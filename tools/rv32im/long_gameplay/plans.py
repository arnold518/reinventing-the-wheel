"""Deterministic players for the existing C rules; produce real device scripts."""
import ctypes as C
import heapq
import itertools
import random
from collections import deque

class Game(C.Structure):
    _fields_ = [(n,C.c_uint32) for n in ('kind','score','status','seed','paused','steps')] + [
        (n,C.c_int32) for n in ('x','y','dx','dy','other','ball_x','ball_y','ball_dx','ball_dy','piece','rotation')] + [
        ('cells',C.c_uint8*128),('body_x',C.c_uint8*64),('body_y',C.c_uint8*64),
        ('length',C.c_uint32),('food_x',C.c_uint32),('food_y',C.c_uint32)]

class Rules:
    def __init__(self,path):
        self.lib=C.CDLL(str(path));assert C.sizeof(Game)==336
        self.lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
        self.lib.game_action.argtypes=[C.POINTER(Game),C.c_uint32,C.c_uint32]
        self.lib.game_fingerprint.argtypes=[C.POINTER(Game)];self.lib.game_fingerprint.restype=C.c_uint32
    def init(self,kind,episode=0):
        g=Game();self.lib.game_init(C.byref(g),kind);g.seed=(0x12345678+episode*7919)&0xffffffff;return g
    def action(self,g,b,levels=0):self.lib.game_action(C.byref(g),b,levels)
    def clone(self,g):return Game.from_buffer_copy(bytes(g))

class Plan:
    def __init__(self,rules,kind):
        self.r=rules;self.kind=kind;self.g=rules.init(kind);self.batches=[]
        self.held=set();self.episode=0;self.total_score=0;self.levels=[];self.players=[];self.metrics={}
    def add(self,button,controls=None):
        controls=set(controls or ());wanted=controls|{button}
        release=(self.held-wanted)|({button} if button in self.held else set())
        batch=[(b,False) for b in sorted(release)]
        for b in sorted(controls-self.held):batch.append((b,True))
        batch.append((button,True));assert len(batch)<=7
        levels=sum(1<<b for b in wanted)
        if button==5:
            self.total_score+=self.g.score;self.episode+=1;self.g=self.r.init(self.kind,self.episode)
            if self.kind==2:self.load_level(self.episode)
        else:
            if self.kind==3:
                for b,down in batch:
                    if down and b<4:self.r.action(self.g,b,levels)
            self.r.action(self.g,button,levels)
        self.batches.append(batch);self.held=wanted
    def load_level(self,index):
        for i,v in enumerate(self.levels[index]):self.g.cells[i]=v
        self.g.x,self.g.y=self.players[index]
    def finish(self):
        assert 16<=len(self.batches)<=1023
        return {'kind':self.kind,'batches':self.batches,'levels':self.levels,'players':self.players,
                'metrics':{**self.metrics,'updates':len(self.batches),'episodes':self.episode+1,
                           'totalScore':self.total_score+self.g.score,'finalStatus':self.g.status,
                           'finalFingerprint':int(self.r.lib.game_fingerprint(C.byref(self.g)))}}

def plan2048(r,goal=128):
    """Lookahead player until the first `goal` tile."""
    p=Plan(r,1);merges=0;largest=0
    def utility(g):
        a=list(g.cells[:16]);smooth=sum(abs(a[y*4+x]-a[y*4+x+1]) for y in range(4) for x in range(3))
        return a.count(0)*5000+g.score*2+max(a)*200-smooth*40+(1500 if max(a) in [a[i] for i in (0,3,12,15)] else 0)
    while largest<goal:
        assert len(p.batches)<600 and not p.g.status,'2048 player did not reach its goal'
        candidates=[]
        for b in range(4):
            g=r.clone(p.g);r.action(g,b)
            if list(g.cells[:16])==list(p.g.cells[:16]):continue
            future=[]
            for n in range(4):
                h=r.clone(g);r.action(h,n);future.append(utility(h))
            candidates.append((utility(g)+max(future)*.7,-b,b))
        assert candidates
        before=p.g.score;p.add(max(candidates)[2]);merges+=p.g.score>before
        largest=max(largest,1<<max(p.g.cells[:16]))
    p.metrics={'mergingMoves':merges,'largestTile':largest};return p.finish()

def plan_snake(r,goal=10):
    """Follow a route through every cell until the snake is `goal` long, then
    turn up and run into the wall (or its own body)."""
    p=Plan(r,3)
    route=[(x,0) for x in range(8)]
    for y in range(1,8):route.extend((x,y) for x in (range(7,0,-1) if y%2 else range(1,8)))
    route.extend((0,y) for y in range(7,0,-1));assert len(set(route))==64
    lookup={xy:i for i,xy in enumerate(route)};turns=0;previous=1
    for tick in range(700):
        assert not p.g.status,'Snake player lost before its goal'
        x,y=p.g.body_x[0],p.g.body_y[0]
        if p.g.length>=goal:direction=0
        else:
            target=(x,y-1) if tick==0 else route[(lookup[x,y]+1)%64]
            delta=(target[0]-x,target[1]-y);direction=[(0,-1),(1,0),(0,1),(-1,0)].index(delta)
        turns+=direction!=previous;previous=direction;p.add(6,{direction})
        if p.g.status:break
    assert p.g.status==2 and p.g.length>=goal,'Snake did not grow to its goal and crash'
    p.metrics={'foodEaten':p.g.score,'length':p.g.length,'turns':turns};return p.finish()

def plan_pong(r,goal=2):
    """Both paddles track the ball; after a long rally one side lets it pass.
    Ends when the `goal`-th point is scored."""
    p=Plan(r,4);round_ticks=0;bounces=0;points=0;paddle_moves=0
    while True:
        assert len(p.batches)<900,'Pong player did not finish'
        if p.g.status:
            points+=1
            if points>=goal:break
            p.add(5);round_ticks=0;continue
        g=p.g;next_y=g.ball_y+g.ball_dy
        if next_y<0 or next_y>=16:next_y=g.ball_y-g.ball_dy
        left=right=max(0,min(12,next_y-1))
        if round_ticks>160:
            if g.ball_dx>0 and g.ball_x>22:right=0 if next_y>7 else 12
            if g.ball_dx<0 and g.ball_x<9:left=0 if next_y>7 else 12
        keys=set()
        if g.y!=left:keys.add(2 if g.y<left else 0)
        if g.other!=right:keys.add(8 if g.other<right else 7)
        paddle_moves+=len(keys);old_dx=g.ball_dx;p.add(6,keys);round_ticks+=1
        bounces+=p.g.ball_dx!=old_dx
    assert bounces>=8 and paddle_moves>=50,'Pong rally too short'
    p.metrics={'points':points,'paddleBounces':bounces,'paddleMoves':paddle_moves};return p.finish()

def plan_tetris(r,goal_pieces=20,goal_lines=3):
    """Placement search; ends after `goal_pieces` pieces with `goal_lines` cleared lines."""
    p=Plan(r,5);pieces=0;lines=0;rotations=0;shifts=0
    def value(g,old_score):
        heights=[];holes=0
        for x in range(8):
            occupied=False;height=0
            for y in range(16):
                if g.cells[y*8+x]:
                    if not occupied:height=16-y
                    occupied=True
                elif occupied:holes+=1
            heights.append(height)
        return (g.score-old_score)*5-holes*400-sum(heights)*8-max(heights)*12-sum(abs(a-b) for a,b in zip(heights,heights[1:]))*10-(1000000 if g.status else 0)
    while pieces<goal_pieces or lines<goal_lines:
        assert len(p.batches)<600 and not p.g.status,'Tetris player did not reach its goal'
        choices=[]
        for rotation in range(4):
            for x in range(-2,8):
                g=r.clone(p.g)
                for _ in range(rotation):r.action(g,0)
                if g.rotation!=rotation:continue
                for _ in range(12):
                    if g.x==x:break
                    before=g.x;r.action(g,1 if g.x<x else 3)
                    if g.x==before:break
                if g.x!=x:continue
                r.action(g,4);choices.append((value(g,p.g.score),-rotation,-abs(x-2),rotation,x))
        assert choices
        _,_,_,rotation,x=max(choices)
        for _ in range(rotation):p.add(0);rotations+=1
        while p.g.x!=x:p.add(1 if p.g.x<x else 3);shifts+=1
        before=p.g.score;seed=p.g.seed
        for _ in range(2+pieces%3):
            p.add(6)
            if p.g.seed!=seed or p.g.status:break
        if p.g.seed==seed and not p.g.status:p.add(4)
        lines+=(p.g.score-before)//100;pieces+=1
    assert shifts>=10
    p.metrics={'pieces':pieces,'clearedLines':lines,'rotations':rotations,'lateralMoves':shifts};return p.finish()

DELTAS=((0,-1),(1,0),(0,1),(-1,0))
def paths(player,boxes,floor):
    found={player:[]};pending=deque([player])
    while pending:
        at=pending.popleft()
        for b,(dx,dy) in enumerate(DELTAS):
            q=(at[0]+dx,at[1]+dy)
            if q in floor and q not in boxes and q not in found:
                found[q]=found[at]+[b];pending.append(q)
    return found

def solve_sokoban(floor,goals,boxes,player):
    def estimate(bs):return min(sum(abs(a[0]-b[0])+abs(a[1]-b[1]) for a,b in zip(bs,gs)) for gs in itertools.permutations(goals))
    def key(bs,pos):return tuple(sorted(bs)),min(paths(pos,bs,floor))
    initial=key(boxes,player);best={initial:0};parents={};serial=itertools.count()
    todo=[(estimate(initial[0]),0,next(serial),initial,player)]
    while todo and len(best)<50000:
        _,cost,_,state,pos=heapq.heappop(todo)
        if best.get(state)!=cost:continue
        bs=set(state[0])
        if bs==goals:
            result=[]
            while state!=initial:
                previous,segment=parents[state];result.append(segment);state=previous
            return [b for segment in reversed(result) for b in segment]
        reachable=paths(pos,bs,floor)
        for box in sorted(bs):
            for b,(dx,dy) in enumerate(DELTAS):
                behind=(box[0]-dx,box[1]-dy);front=(box[0]+dx,box[1]+dy)
                if behind not in reachable or front not in floor or front in bs:continue
                if front not in goals and any((front[0]+x,front[1]) not in floor for x in (-1,1)) and any((front[0],front[1]+y) not in floor for y in (-1,1)):continue
                new=(bs-{box})|{front};nkey=key(new,box)
                if cost+1>=best.get(nkey,1000000):continue
                best[nkey]=cost+1;parents[nkey]=(state,reachable[behind]+[b])
                heapq.heappush(todo,(cost+1+estimate(nkey[0]),cost+1,next(serial),nkey,box))
    return None

def plan_sokoban(r,puzzles=2):
    """Generated puzzles (solvable by construction), solved by search."""
    rng=random.Random(39217);levels=[];players=[];solutions=[]
    for index in range(puzzles):
        for attempt in range(100):
            floor={(x,y) for y in range(1,7) for x in range(1,7)}
            # Varied internal walls; reverse generation guarantees a solution.
            if index%3==1:floor-={(3,2),(4,5)}
            if index%3==2:floor-={(2,4),(5,3)}
            goals=set(rng.sample(sorted(floor-{(1,1),(6,6),(1,6),(6,1)}),3))
            boxes=set(goals);player=rng.choice(sorted(floor-boxes))
            for _ in range(45):
                reachable=paths(player,boxes,floor);pulls=[]
                for box in sorted(boxes):
                    for dx,dy in DELTAS:
                        near=(box[0]-dx,box[1]-dy);far=(box[0]-2*dx,box[1]-2*dy)
                        if near in reachable and far in floor and far not in boxes:pulls.append((box,near,far))
                if not pulls:break
                box,near,player=rng.choice(pulls);boxes=(boxes-{box})|{near}
            if boxes==goals:continue
            solution=solve_sokoban(floor,goals,boxes,player)
            if solution and 32<=len(solution)<=95:
                level=[1]*64
                for x,y in floor:level[y*8+x]=3 if (x,y) in goals else 0
                for x,y in boxes:level[y*8+x]=4 if (x,y) in goals else 2
                levels.append(level);players.append(player);solutions.append(solution);break
        else:raise RuntimeError('Could not generate a substantial solvable puzzle')
    p=Plan(r,2);p.levels=levels;p.players=players;p.load_level(0);pushes=0
    for index,solution in enumerate(solutions):
        if index:p.add(5)
        for button in solution:
            old=(p.g.x,p.g.y);before=p.g.score;p.add(button)
            assert (p.g.x,p.g.y)!=old,'Solver emitted a blocked move'
            pushes+=p.g.score-before
        assert p.g.status==1,'Puzzle not solved by the actual C rules'
    assert pushes>=8
    p.metrics={'solvedPuzzles':puzzles,'cratePushes':pushes,'movesPerPuzzle':[len(s) for s in solutions]};return p.finish()

PLAYERS={'2048':plan2048,'sokoban':plan_sokoban,'snake':plan_snake,'pong':plan_pong,'tetris':plan_tetris}
