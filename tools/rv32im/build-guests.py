#!/usr/bin/env python3
"""Build and audit the RV32IM guest fixtures: bring-up, short games, long games.

Short fixtures run on a 32 x 24 screen (bring-up on 8 x 8) from a prepared board; their button
presses are queued at fixed clock cycles. Long fixtures run on the 64 x 48
screen; a computer player (long_gameplay/plans.py) plays the real C rules to
a goal and its moves become batches of button changes, one batch after each
presented frame.
"""
import argparse, hashlib, json, pathlib, re, shutil, subprocess, sys, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/rv32im'))
from long_gameplay.plans import Rules, PLAYERS
from long_gameplay.session import session_header

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',type=pathlib.Path,required=True)
p.add_argument('--output',type=pathlib.Path,default=ROOT/'tests/fixtures/rv32im/games')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
guest=ROOT/'tools/rv32im/guest'
flags=['-march=rv32im','-mabi=ilp32','-msmall-data-limit=0','-mno-relax','-Os','-ffreestanding','-fno-builtin','-fno-stack-protector','-nostdlib','-nostartfiles','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Wl,--no-relax','-Wl,--build-id=none','-T',str(guest/'link.ld')]
allowed=set('lui auipc jal jalr beq bne blt bge bltu bgeu lb lh lw lbu lhu sb sh sw addi slti sltiu xori ori andi slli srli srai add sub sll slt sltu xor srl sra or and fence ecall ebreak mul mulh mulhsu mulhu div divu rem remu'.split())
sha=lambda data:hashlib.sha256(data).hexdigest()
compiler=a.compiler.resolve();objdump=compiler.with_name(compiler.name.removesuffix('gcc')+'objdump')
manifest={'version':2,'isa':'rv32im','abi':'ilp32','compiler':subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0],'compilerSha256':sha(compiler.read_bytes()),'flags':flags[:-1]+['guest/link.ld'],'sources':{f.name:sha(f.read_bytes()) for f in sorted(guest.iterdir()) if f.suffix in ('.c','.h','.S','.ld')},'guests':[]}
manifest['sources']['plans.py']=sha((ROOT/'tools/rv32im/long_gameplay/plans.py').read_bytes())

GAMES={'2048':1,'sokoban':2,'snake':3,'pong':4,'tetris':5}
SYSTEM_HEADERS={'games-machine':'system.h','games-machine-leds':'system-leds.h'}
SHORT=(32,24);LONG=(64,48)
# name, kind, version, machine, presses (short) or player (long), defines
cases=[
    ('bringup',0,'short','games-machine',[(1,True)],{}),
    ('bringup-leds',0,'short','games-machine-leds',[(1,True)],{}),
    # Short: a prepared board, the game's key event within 1-2 presses.
    ('2048-short',1,'short','games-machine',[(3,True)],{'GAME_ACTIONS':1,'GAME_PRESET':1}),
    ('sokoban-short',2,'short','games-machine',[(1,True)],{'GAME_ACTIONS':1,'GAME_PRESET':0}),
    ('snake-short',3,'short','games-machine',[(6,True),(6,True)],{'GAME_ACTIONS':2,'GAME_PRESET':5}),
    ('pong-short',4,'short','games-machine',[(6,True),(6,True)],{'GAME_ACTIONS':2,'GAME_PRESET':3}),
    ('tetris-short',5,'short','games-machine',[(4,True)],{'GAME_ACTIONS':1,'GAME_PRESET':2}),
    ('snake-leds-short',3,'short','games-machine-leds',[(6,True),(6,True)],{'GAME_ACTIONS':2,'GAME_PRESET':4}),
    # Long: a computer player's whole session.
    ('2048-long',1,'long','games-machine','2048',{}),
    ('sokoban-long',2,'long','games-machine','sokoban',{}),
    ('snake-long',3,'long','games-machine','snake',{}),
    ('pong-long',4,'long','games-machine','pong',{}),
    ('tetris-long',5,'long','games-machine','tetris',{}),
    ('snake-leds-long',3,'long','games-machine-leds','snake',{}),
    # Campaign equipment check: Pong's session cut to two updates (minutes in
    # Visual). Not one of the games; the campaign runs it only when named.
    ('smoke-long',4,'long','games-machine','pong-smoke',{}),
]

def smoke_plan(rules):
    """The first two batches of the Pong player's session, replayed through
    the real rules for the expected report."""
    import ctypes
    plan=PLAYERS['pong'](rules);batches=plan['batches'][:2]
    g=rules.init(4);levels=0;episode=0
    for batch in batches:
        for b,down in batch:levels=(levels&~(1<<b))|(int(down)<<b)
        for b,down in batch:
            if not down:continue
            if b==5:episode+=1;g=rules.init(4,episode)
            elif b==6:rules.action(g,6,levels)
    return {'kind':4,'batches':batches,'levels':[],'players':[],
            'metrics':{'updates':len(batches),'episodes':episode+1,'totalScore':g.score,'finalStatus':g.status,
                       'finalFingerprint':int(rules.lib.game_fingerprint(ctypes.byref(g)))}}

work=pathlib.Path(tempfile.mkdtemp(prefix='circuitsim-guests-'))
subprocess.run(['cc','-std=c11','-O1','-shared','-fPIC','-Wall','-Wextra','-Werror',str(guest/'games.c'),'-o',str(work/'rules.so')],check=True)
rules=Rules(work/'rules.so');plans={}
for name,kind,version,system,inputs,defines in cases:
    elf=a.output/(name+'.elf')
    sources=['start.S','platform.c','bringup.c'] if not kind else ['start.S','platform.c','games.c','main.c']
    defines={'GAME_KIND':kind,'GAME_SESSION':int(version=='long'),**defines}
    include=[]
    if version=='long':
        if inputs not in plans:plans[inputs]=smoke_plan(rules) if inputs=='pong-smoke' else PLAYERS[inputs](rules)
        plan=plans[inputs];session=work/name;session.mkdir()
        (session/'session.h').write_text(session_header(plan));include=['-I',str(session)]
    header=[] if system=='games-machine' else [f'-DSYSTEM_HEADER="{SYSTEM_HEADERS[system]}"']
    subprocess.run([str(compiler),*flags,*include,*[f'-D{k}={v}' for k,v in defines.items()],*header,*[str(guest/f) for f in sources],'-o',str(elf)],check=True)
    disasm=subprocess.check_output([str(objdump),'-d','-M','no-aliases',str(elf)],text=True).replace(str(elf),name+'.elf')
    ops=[]
    for line in disasm.splitlines():
        m=re.match(r'\s*[0-9a-f]+:\s+([0-9a-f]+)\s+(\w+)',line)
        if m:
            bits,op=m.groups()
            if len(bits)!=8 or op not in allowed:raise SystemExit('Outside RV32IM: '+line)
            ops.append(op)
    (a.output/(name+'.disasm.txt')).write_text(disasm)
    # The bring-up program runs on the whole-computer scenarios: an 8 x 8 screen.
    width,height=(8,8) if not kind else SHORT if version=='short' else LONG
    if version=='short':
        # Unit clock: reset release at 2000, origin 3000, period 1000. Presses
        # arrive after falling edges, before the guest first polls the queue.
        script={'version':1,'clockPeriod':'1000','resetRelease':'2000','origin':'3000','events':[{'tick':str(3000+(i+1)*1000+501),'sequence':i+1,'device':'buttons','button':button,'down':down} for i,(button,down) in enumerate(inputs)]}
        frames=(defines.get('GAME_ACTIONS',0)+1) if kind else 2
        extra={'downEvents':sum(down for _,down in inputs)}
    else:
        # One batch of button changes after each presented frame.
        script={'version':2,'paced':True,'player':inputs,'metrics':plan['metrics'],'batches':plan['batches']}
        frames=len(plan['batches'])+1
        extra={'player':inputs,'metrics':plan['metrics'],'levels':plan['levels'],'players':plan['players']}
    script_bytes=(json.dumps(script,indent=1 if version=='long' else 2)+'\n').encode();(a.output/(name+'.input.json')).write_bytes(script_bytes)
    manifest['guests'].append({'id':kind,'name':name,'version':version,'elf':elf.name,'elfSha256':sha(elf.read_bytes()),
        'script':name+'.input.json','scriptSha256':sha(script_bytes),'instructions':len(ops),'operations':sorted(set(ops)),
        'defines':defines,'presentations':frames,'system':system,'width':width,'height':height,
        'buttonDepth':7 if version=='long' else 4,**extra})
    print(name,len(ops),'instructions',len(elf.read_bytes()),'ELF bytes',frames,'frames')
(a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
# Embed the exact fixture bytes and inputs for FS-free WebAssembly.
lines=['// Generated by tools/rv32im/build-guests.py; do not edit.',
       'const std::vector<Fixture>& fixtures() {', '    static const std::vector<Fixture> value{']
for f in manifest['guests']:
    script=json.loads((a.output/f['script']).read_text());raw=(a.output/f['elf']).read_bytes().hex()
    lines.append('        {"'+f['name']+'","'+f['system']+'","'+f['elfSha256']+'","'+f['scriptSha256']+'",'+str(f['width'])+','+str(f['height'])+','+str(f['buttonDepth'])+',')
    lines.extend('            "'+raw[i:i+128]+'"' for i in range(0,len(raw),128))
    events=','.join('{'+e['tick']+'ULL,'+str(e['sequence'])+'ULL,'+str(e['button'])+','+str(e['down']).lower()+'}' for e in script.get('events',[]))
    batches=','.join('{'+','.join('{'+str(b)+','+str(d).lower()+'}' for b,d in batch)+'}' for batch in script.get('batches',[]))
    lines.append('            ,{'+events+'},{'+batches+'}},')
lines.extend(['    };','    return value;','}'])
(ROOT/'src/runtime/RecordedGuestFixtures.generated.inc').write_text('\n'.join(lines)+'\n')
shutil.rmtree(work)
