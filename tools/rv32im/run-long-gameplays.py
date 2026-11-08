#!/usr/bin/env python3
"""Launch/resume a frozen, serial native campaign that records the long games.

Jobs are the long fixtures of tests/fixtures/rv32im/games (a computer
player's session each). Each runs through LongGameplayProbe in a memory-capped
background unit with the frozen timing profile, clock period and fidelity; its
frames are compared with the host reference and its circuit is published as a
playlist under site-docs/assets/system/long-games/.

    run-long-gameplays.py launch --output NEW-DIR --runner build/tests/LongGameplayProbe \
        [--fidelity compact] [--timing embedded] [--period 4500] [--games pong-long,...]
    run-long-gameplays.py status|resume|stop --output DIR
    run-long-gameplays.py compress-published --output site-docs/assets/system/long-games

Published games store their topology, scopes, parts and layout as `.json.gz`.
"""
import argparse,fcntl,gzip,hashlib,html,json,os,re,resource,shutil,signal,subprocess,sys,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(path,value):
    temporary=path.with_suffix(path.suffix+'.tmp');temporary.write_text(json.dumps(value,indent=2)+'\n');temporary.replace(path)
def available():return int(next(s for s in Path('/proc/meminfo').read_text().splitlines() if s.startswith('MemAvailable:')).split()[1])//1024
def bus():return {**os.environ,'XDG_RUNTIME_DIR':f'/run/user/{os.getuid()}','DBUS_SESSION_BUS_ADDRESS':f'unix:path=/run/user/{os.getuid()}/bus'}
def read_json(path):
    data=path.read_bytes()
    return json.loads(gzip.decompress(data) if data[:2]==b'\x1f\x8b' else data)
def gz_write(path,data):path.write_bytes(gzip.compress(data,mtime=0))

def compress_ready(ready):
    """Store a published game's large files gzip-compressed (`.json.gz`): the
    topology index, its scopes, the parts and the shared layout. The playlist
    stays plain JSON; references and hashes are rewritten. The viewer
    decompresses them (recorded_circuit_transport.mjs). A Visual frame is
    about 1 MB of JSON and 16 KB compressed."""
    graph=read_json(ready/'graph.json')
    # The build manifest (every part's chosen form) is 7 MB for Visual.
    gz_write(ready/'build-manifest.txt.gz',(ready/'build-manifest.txt').read_bytes());(ready/'build-manifest.txt').unlink()
    graph['buildManifestRef']='build-manifest.txt.gz'
    for scope in graph['scopes']:
        source=ready/scope['file'];target=ready/(scope['file']+'.gz')
        gz_write(target,source.read_bytes());source.unlink()
        scope['file']=target.name;scope['sha256']=sha(target)
    gz_write(ready/'graph.json.gz',(json.dumps(graph,separators=(',',':'))+'\n').encode());(ready/'graph.json').unlink()
    playlist=read_json(ready/'playlist.json');layouts=set()
    for part in playlist['parts']:
        source=ready/part['file'];packet=read_json(source);packet['topologyRef']='graph.json.gz'
        packet['recording']['buildManifestRef']='build-manifest.txt.gz'
        layout=packet.get('presentation',{}).get('layout')
        if layout:layouts.add(layout);packet['presentation']['layout']=layout+'.gz'
        target=ready/(part['file']+'.gz');gz_write(target,(json.dumps(packet,separators=(',',':'))+'\n').encode());source.unlink()
        part['file']=target.name;part['sha256']=sha(target)
    for layout in layouts:gz_write(ready/(layout+'.gz'),(ready/layout).read_bytes());(ready/layout).unlink()
    save(ready/'playlist.json',playlist)

def compress_published(root):
    """Convert already-published games under `root` to compressed files."""
    for game in sorted(p for p in root.iterdir() if (p/'evidence.json').exists()):
        evidence=json.loads((game/'evidence.json').read_text())
        if evidence.get('compressed'):
            if (game/'build-manifest.txt').exists():   # compressed before the manifest was
                gz_write(game/'build-manifest.txt.gz',(game/'build-manifest.txt').read_bytes());(game/'build-manifest.txt').unlink()
                graph=read_json(game/'graph.json.gz');graph['buildManifestRef']='build-manifest.txt.gz'
                gz_write(game/'graph.json.gz',(json.dumps(graph,separators=(',',':'))+'\n').encode())
                playlist=read_json(game/'playlist.json')
                for part in playlist['parts']:
                    packet=read_json(game/part['file']);packet['recording']['buildManifestRef']='build-manifest.txt.gz'
                    gz_write(game/part['file'],(json.dumps(packet,separators=(',',':'))+'\n').encode());part['sha256']=sha(game/part['file'])
                save(game/'playlist.json',playlist)
                evidence.update(topologySha256=sha(game/'graph.json.gz'),playlistSha256=sha(game/'playlist.json'),
                                buildManifestSha256=sha(game/'build-manifest.txt.gz'))
                save(game/'evidence.json',evidence);print('compressed build manifest',game.name,flush=True)
            continue
        compress_ready(game)
        evidence.update(compressed=True,topologySha256=sha(game/'graph.json.gz'),playlistSha256=sha(game/'playlist.json'),
                        buildManifestSha256=sha(game/'build-manifest.txt.gz'))
        save(game/'evidence.json',evidence);print('compressed',game.name,flush=True)

def publish_saves(native,ready,manifest,guest):
    """Machine saves for replays (circuit::state, LongGameplayProbe
    --save-every): each gzip-compressed under saves/ with an index the viewer
    reads ("Step through events" restores the nearest one in the browser)."""
    index_file=native/'saves'/'index.jsonl'
    if not index_file.exists():return None
    target=ready/'saves';target.mkdir()
    rows=[json.loads(line) for line in index_file.read_text().splitlines() if line.strip()]
    rows.sort(key=lambda row:row['cycle'])
    assert rows and len({row['cycle'] for row in rows})==len(rows),'Replay saves are missing or repeated'
    entries=[]
    for row in rows:
        # LongGameplayProbe writes them gzip-compressed already.
        source=native/'saves'/row['file'];data=gzip.decompress(source.read_bytes())
        assert len(data)==row['bytes'],('save size',row['file'])
        packed=target/row['file'];os.link(source,packed)
        entries.append({'cycle':row['cycle'],'settled':row['settled'],'frame':row['frame'],'file':packed.name,
                        'bytes':len(data),'compressedBytes':packed.stat().st_size,'sha256':sha(packed)})
    save(target/'index.json',{'schemaVersion':1,'kind':'circuitsim-machine-saves','guest':guest,
        'fidelityPreset':manifest['fidelityPreset'],'timingProfile':manifest['timingProfile'],'period':manifest['period'],
        'saveEvery':manifest.get('saveEvery',0),'saveFrames':manifest.get('saveFrames',False),
        'window':manifest.get('replayWindow',0),'saves':entries})
    return entries

class Pressure(Exception):pass
class Stop(BaseException):pass

def publish_status(output,manifest):
    public=Path(manifest['publicationRoot']);public.mkdir(parents=True,exist_ok=True)
    title='Long games'
    visible={'schemaVersion':1,'status':manifest['status'],'unit':manifest['unit'],
             'capturePolicy':'all-signal-ticks' if manifest.get('captureTicks') else 'presentation-checkpoints',
             'fidelityPreset':manifest.get('fidelityPreset','compact'),'presetVersion':1,
             'timingProfile':manifest['timingProfile'],'period':manifest['period'],
             'scope':manifest['scope'],'sourceManifest':'source-manifest.json',
             'games':[{k:v for k,v in row.items() if k not in ('attemptDirectory',)} for row in manifest['jobs']]}
    save(public/'manifest.json',visible)
    rows=[]
    for job in manifest['jobs']:
        name=html.escape(job['name']);state=html.escape(job['status']);play='Pending'
        if job['status']=='passed':
            relative=manifest['publicationPath']
            prefix='../'*len(Path(relative).parts)
            entry='ticks.json' if manifest.get('captureTicks') else 'playlist.json'
            play=f'<a href="{prefix}simulator/?recording=../{relative}/{name}/{entry}">Play full session</a>'
        rows.append(f'<tr><td>{name}</td><td>{job["frames"]}</td><td>{state}</td><td>{play}</td></tr>')
    page='<!doctype html><meta charset="utf-8"><title>'+title+'</title><style>body{font:16px system-ui;background:#10151b;color:#e4ebf1;max-width:900px;margin:40px auto;padding:20px}a{color:#83b9ff}td,th{padding:12px;text-align:left;border-bottom:1px solid #35404c}</style>'
    capture='Every signal-change tick is retained, with PRESENT navigation markers.' if manifest.get('captureTicks') else 'All presented frames are retained in one continuous timeline.'
    page+='<h1>'+title+'</h1><p>Fidelity: '+html.escape(manifest.get('fidelityPreset','compact'))+'. Campaign: '+html.escape(manifest['status'])+'. Each game becomes available after its native run and exact reference checks finish. '+capture+'</p>'
    prefix='../'*len(Path(manifest['publicationPath']).parts)
    page+='<table><tr><th>Game</th><th>Planned frames</th><th>Status</th><th>Recording</th></tr>'+''.join(rows)+f'</table><p><a href="{prefix}system/5.6/">§ 5.6 Games</a> · <a href="manifest.json">Run manifest</a></p>'
    temporary=public/'index.html.tmp';temporary.write_text(page);temporary.replace(public/'index.html')

def run_command(command,attempt,label,manifest,job,output,native=False):
    while available()<1536:
        job['status']='waiting-for-memory';save(output/'campaign.json',manifest);time.sleep(5)
    job['status']='running' if native else 'verifying';started=time.monotonic();last_update=0
    def cap():
        if native:
            space=manifest['provenance']['limits']['nativeAddressSpaceMiB']*1024**2
            resource.setrlimit(resource.RLIMIT_AS,(space,space))
    env={**os.environ,'NODE_OPTIONS':'--max-old-space-size=256'}
    with (attempt/(label+'.log')).open('w') as stream:
        process=subprocess.Popen(['/usr/bin/time','-v','-o',str(attempt/(label+'-resources.txt')),*command],
            cwd=output,stdin=subprocess.DEVNULL,stdout=stream,stderr=subprocess.STDOUT,
            start_new_session=True,preexec_fn=cap,env=env)
        job['pid']=process.pid;save(output/'campaign.json',manifest)
        try:
            while process.poll() is None:
                if available()<1024:raise Pressure('Host available memory below 1024 MiB')
                if shutil.disk_usage(output).free<1024**3:raise RuntimeError('Less than 1 GiB free disk space')
                wall_days=manifest['provenance']['limits'].get('nativeWallDays',7) if native else 7
                if time.monotonic()-started>wall_days*86400:raise RuntimeError(f'{wall_days}-day command wall budget exhausted')
                if native and time.monotonic()-last_update>=15:
                    if manifest.get('captureTicks'):
                        trace_bytes=sum(p.stat().st_size for p in output.glob('jobs/*/attempt-*/native/tick-*.bin.gz'))
                        if trace_bytes>manifest['provenance']['limits']['traceDiskBytes']:raise RuntimeError('Campaign tick trace disk budget exhausted')
                        job['campaignTraceBytes']=trace_bytes
                        progress=attempt/'native/ticks-progress.json'
                        if progress.exists():job['tickProgress']=json.loads(progress.read_text())
                    with (attempt/(label+'.log')).open('rb') as log:
                        log.seek(0,2);log.seek(max(0,log.tell()-8192));tail=log.read().decode(errors='replace')
                    matches=re.findall(r'(\d+) cycles, (\d+) retired, (\d+) presented',tail)
                    if matches:job['progress']=dict(zip(('cycles','instructions','presented'),map(int,matches[-1])))
                    job['elapsedSeconds']=round(time.monotonic()-started,1);job['heartbeatUnix']=time.time()
                    save(output/'campaign.json',manifest)
                    last_update=time.monotonic()
                time.sleep(1)
            if process.returncode:raise RuntimeError(f'{label} exited {process.returncode}; see {attempt/(label+".log")}')
        finally:
            if process.poll() is None:
                os.killpg(process.pid,signal.SIGTERM)
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
            job.pop('pid',None)

def finalize(output,manifest,job,attempt):
    frozen=output/'frozen';prepared=frozen/'prepared'/job['name'];native=attempt/'native'
    actual=json.loads((native/'recording.json').read_text());expected=json.loads((prepared/'expected.json').read_text())
    assert actual.get('fidelityPreset')==manifest['fidelityPreset'],'Recorded fidelity differs from the frozen campaign'
    plan={'batches':json.loads((prepared/'input.json').read_text())['batches']};words=[int.from_bytes(bytes.fromhex(actual['report'])[i:i+4],'little') for i in range(0,36,4)]
    assert len(actual['frames'])==job['frames']==len(expected['frames'])
    assert set(bytes.fromhex(actual['reportKnown']))=={255}
    for i,(frame,pixels,levels) in enumerate(zip(actual['frames'],expected['frames'],expected['levels'])):
        assert frame['pixels']==pixels,(job['name'],'pixels',i)
        assert frame['sequence']==i+1
        assert set(bytes.fromhex(frame['known']))=={255}
        assert int(frame['input']['levels'],2)==levels and int(frame['input']['count'],2)==0 and frame['input']['overflow']=='0'
    for i in [0,1,2,3,4,5,6,8]:assert words[i]==expected['report'][i],(job['name'],'report',i)
    assert actual['setupViolations']==0 and actual['presentations']==job['frames']
    assert [(e['button'],e['down']) for e in actual['inputs']]==[(b,d) for batch in plan['batches'] for b,d in batch]
    recording_name=job['name']
    ready=attempt/'ready';ready.mkdir();parts=[];offset=0
    topology=json.loads((native/'topology.json').read_text())
    assert topology['profileFingerprint']==actual['profileFingerprint'],'Topology fidelity differs'
    topology['profile'].update(fidelityPreset=actual['fidelityPreset'],presetVersion=1,custom=False)
    (ready/'build-manifest.txt').write_text(actual['buildManifest'])
    scopes=[]
    for path in sorted(native.glob('scope-*.json'),key=lambda p:int(p.stem.split('-')[1])):
        scope=json.loads(path.read_text())
        scopes.append({'file':path.name,'ownerIds':scope['scopeIds'],'sha256':sha(path)})
        os.link(path,ready/path.name)
    graph={'schemaVersion':1,'kind':'circuitsim-recorded-topology','topology':topology,'scopes':scopes,
           'fidelityPreset':actual['fidelityPreset'],'presetVersion':1,'buildManifestRef':'build-manifest.txt'}
    (ready/'graph.json').write_text(json.dumps(graph,separators=(',',':'))+'\n')
    assert (ready/'graph.json').stat().st_size<=32*1024**2,'Topology index exceeds byte limit'
    del topology,graph,scope
    files=sorted(native.glob('part-*.partial.json'),key=lambda p:int(p.name.split('-')[1].split('.')[0]))
    shared_layout=None
    for number,path in enumerate(files):
        packet=json.loads(path.read_text());count=len(packet['checkpoints'])
        recording={**actual,'guest':recording_name,'frames':actual['frames'][offset:offset+count],
                   'sourcePresentations':actual['presentations'],'presentations':count,'buildManifestRef':'build-manifest.txt'}
        recording.pop('buildManifest',None)
        packet.update(schemaVersion=2,topologyRef='ready/graph.json',kind='circuitsim-circuit-recording',capturePolicy='presentation-checkpoints',sourceRuntime='native',recording=recording)
        raw=attempt/f'packet-{number}.json';raw.write_text(json.dumps(packet,separators=(',',':'))+'\n')
        target=ready/f'part-{number:04}.json'
        del packet,recording
        run_command([manifest.get('nodeExecutable','node'),str(frozen/'tools/package-recorded-circuit.mjs'),str(raw),str(frozen/'visualizer-v2/layout.json'),str(target)],attempt,f'package-{number}',manifest,job,output)
        raw.unlink() # Actual native partials and the full recording are retained.
        # Every part gets the same layout; keep the first and point later parts
        # at it (the viewer reads the first part's layout).
        layout=ready/(target.stem+'.layout.json')
        if shared_layout is None:shared_layout=layout
        elif layout.read_bytes()==shared_layout.read_bytes():
            layout.unlink();packet=json.loads(target.read_text());packet['presentation']['layout']=shared_layout.name
            target.write_text(json.dumps(packet,separators=(',',':'))+'\n')
            package=ready/(target.stem+'.package.json');meta=json.loads(package.read_text())
            meta.update(preparedCaptureSha256=sha(target),sharedLayout=shared_layout.name)
            package.write_text(json.dumps(meta,indent=2)+'\n');del packet,meta
        parts.append({'file':target.name,'start':offset,'count':count,'sha256':sha(target)});offset+=count
    assert offset==job['frames']
    playlist={'schemaVersion':1,'kind':'circuitsim-circuit-playlist','guest':recording_name,'parts':parts,
              'fidelityPreset':manifest['fidelityPreset'],'presetVersion':1,
              'checkpoints':[{'time':f['time'],'cycle':f['cycle']} for f in actual['frames']]}
    saves=publish_saves(native,ready,manifest,recording_name)
    if saves:playlist['saves']='saves/index.json'
    save(ready/'playlist.json',playlist)
    # Full-tick packages keep their own file contract; others are compressed.
    if not manifest.get('captureTicks'):compress_ready(ready)
    graph_file='graph.json' if manifest.get('captureTicks') else 'graph.json.gz'
    if manifest.get('captureTicks'):
        shutil.copy2(native/'ticks.json',ready/'ticks.json')
        os.link(native/'pin-sources.json',ready/'pin-sources.json')
        ticks=json.loads((ready/'ticks.json').read_text())
        for chunk in ticks['chunks']:os.link(native/chunk['file'],ready/chunk['file'])
        run_command([manifest['nodeExecutable'],str(frozen/'tools/package-tick-recording.mjs'),str(ready)],attempt,'package-ticks',manifest,job,output)
    # An interrupted capture's resource file has no peak (its process was
    # stopped before /usr/bin/time wrote one); the resumed capture's has.
    peaks=[int(line.split(':')[1]) for path in attempt.glob('capture*-resources.txt')
           for line in path.read_text().splitlines() if 'Maximum resident set size' in line]
    if not peaks:raise RuntimeError('No capture reported its peak memory')
    peak=max(peaks)
    evidence={'schemaVersion':1,'guest':job['name'],'exactHostReference':True,'allFramesRetained':True,
              'fidelityPreset':actual['fidelityPreset'],'profileName':actual['profileName'],'profileFingerprint':actual['profileFingerprint'],
              'frames':job['frames'],'parts':len(parts),'metrics':job['metrics'],'cycles':actual['cycles'],
              'instructions':actual['instructions'],'executionSeconds':actual['executionSeconds'],'peakNativeRssKiB':peak,
              'setupViolations':actual['setupViolations'],'events':actual['events'],
              'runnerSha256':manifest['provenance']['runnerSha256'],'elfSha256':sha(prepared/'guest.elf'),
              'batchesSha256':sha(prepared/'input.json'),'expectedSha256':sha(prepared/'expected.json'),
              'timingProfile':manifest['timingProfile'],'period':manifest['period'],
              'topologySha256':sha(ready/graph_file),
              'buildManifestSha256':sha(ready/('build-manifest.txt' if manifest.get('captureTicks') else 'build-manifest.txt.gz')),
              'playlistSha256':sha(ready/'playlist.json'),'sourceManifest':'../source-manifest.json',
              'compressed':not manifest.get('captureTicks')}
    if saves:
        evidence.update(saves=len(saves),savesCompressedBytes=sum(e['compressedBytes'] for e in saves),
                        savesIndexSha256=sha(ready/'saves'/'index.json'))
    if manifest.get('captureTicks'):
        evidence.update(capturePolicy='all-signal-ticks',tickManifestSha256=sha(ready/'ticks.json'),
                        tickPackage=json.loads((ready/'ticks.package.json').read_text()))
    save(ready/'evidence.json',evidence)
    public=Path(manifest['publicationRoot'])/job['name']
    if public.exists():raise RuntimeError('Published long recording already exists: '+str(public))
    staging=attempt/'publish-ready'
    shutil.copytree(ready,staging,copy_function=os.link)
    staging.rename(public)
    job.update(status='passed',playlist=f'{job["name"]}/playlist.json',evidence=f'{job["name"]}/evidence.json',
               cycles=actual['cycles'],instructions=actual['instructions'],executionSeconds=actual['executionSeconds'],peakNativeRssKiB=peak,
               finishedUnix=time.time(),partCount=len(parts))
    if manifest.get('captureTicks'):job['ticks']=f'{job["name"]}/ticks.json'

def worker(output):
    with (output/'worker.lock').open('w') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        manifest=json.loads((output/'campaign.json').read_text());manifest.update(status='running',pid=os.getpid())
        def stop(_sig,_frame):raise Stop()
        signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
        try:
            for job in manifest['jobs']:
                published=Path(manifest['publicationRoot'])/job['name']
                if published.exists():
                    evidence=json.loads((published/'evidence.json').read_text())
                    assert evidence['exactHostReference'] and evidence['frames']==job['frames']
                    for key in ('elfSha256','batchesSha256','expectedSha256'):assert evidence[key]==job[key]
                    assert evidence['runnerSha256']==manifest['provenance']['runnerSha256']
                    assert sha(published/'playlist.json')==evidence['playlistSha256']
                    if 'topologySha256' in evidence:
                        graph_file=published/('graph.json.gz' if evidence.get('compressed') else 'graph.json')
                        assert sha(graph_file)==evidence['topologySha256']
                        assert sha(published/('build-manifest.txt.gz' if evidence.get('compressed') else 'build-manifest.txt'))==evidence['buildManifestSha256']
                        graph=read_json(graph_file)
                        for scope in graph['scopes']:assert sha(published/scope['file'])==scope['sha256']
                    playlist=json.loads((published/'playlist.json').read_text())
                    for part in playlist['parts']:assert sha(published/part['file'])==part['sha256']
                    if 'savesIndexSha256' in evidence:
                        assert sha(published/'saves'/'index.json')==evidence['savesIndexSha256']
                        for entry in json.loads((published/'saves'/'index.json').read_text())['saves']:
                            assert sha(published/'saves'/entry['file'])==entry['sha256']
                    if manifest.get('captureTicks'):
                        assert sha(published/'ticks.json')==evidence['tickManifestSha256']
                        ticks=json.loads((published/'ticks.json').read_text())
                        assert sha(published/ticks['pinSources'])==ticks['pinSourcesSha256']
                        for chunk in ticks['chunks']:assert sha(published/chunk['file'])==chunk['sha256']
                    job.update(status='passed',playlist=f'{job["name"]}/playlist.json',evidence=f'{job["name"]}/evidence.json')
                    continue
                if job['status']=='passed':raise RuntimeError('Completed published record is missing: '+job['name'])
                for name,digest in manifest['provenance']['frozenFiles'].items():
                    if sha(output/'frozen'/name)!=digest:raise RuntimeError('Frozen executor changed: '+name)
                case=output/'frozen/prepared'/job['name']
                for name,key in [('guest.elf','elfSha256'),('input.json','batchesSha256'),('expected.json','expectedSha256')]:
                    if sha(case/name)!=job[key]:raise RuntimeError('Frozen fixture changed: '+job['name'])
                for retry in range(3):
                    # An interrupted capture (memory guard, stop, reboot) continues
                    # from its last resume save (taken at each circuit part)
                    # in the same attempt instead of starting at cycle 0.
                    previous=Path(job['attemptDirectory']) if job.get('attemptDirectory') else None
                    resume=(previous is not None and job['status']!='failed' and not manifest.get('captureTicks')
                            and (previous/'native'/'resume.bin').exists())
                    if resume:attempt=previous
                    else:attempt=output/'jobs'/job['name']/f'attempt-{time.time_ns()}';attempt.mkdir(parents=True)
                    job.update(status='running',attemptDirectory=str(attempt),startedUnix=time.time());job.pop('error',None)
                    save(output/'campaign.json',manifest);publish_status(output,manifest)
                    prepared=output/'frozen/prepared'/job['name']
                    try:
                        print('RESUME' if resume else 'RUN',job['name'],job['frames'],'frames',flush=True)
                        label='capture-'+str(len(list(attempt.glob('capture*-resources.txt')))) if resume else 'capture'
                        run_command([str(output/'frozen/bin/LongGameplayProbe'),job['name'],str(attempt/'native'),
                                     '--period',str(manifest['period']),'--fidelity',manifest['fidelityPreset'],'--part-frames','16',
                                     *(['--save-every',str(manifest['saveEvery'])] if manifest.get('saveEvery') else []),
                                     *(['--save-at-frames'] if manifest.get('saveFrames') else []),
                                     *(['--resume'] if resume else []),
                                     *(['--ticks'] if manifest.get('captureTicks') else [])],attempt,label,manifest,job,output,native=True)
                        finalize(output,manifest,job,attempt)
                        print('PASS',job['name'],flush=True);break
                    except Pressure as error:
                        job.update(status='waiting-for-memory',error=str(error));save(output/'campaign.json',manifest)
                        if retry<2:time.sleep(60);continue
                        job['status']='resource-stopped'
                    except Exception as error:
                        job.update(status='failed',error=str(error));print('FAIL',job['name'],str(error),flush=True)
                    break
                save(output/'campaign.json',manifest);publish_status(output,manifest)
            manifest.update(status='completed' if all(j['status']=='passed' for j in manifest['jobs']) else 'finished-with-failures',finishedUnix=time.time())
        except Stop:
            manifest.update(status='interrupted',finishedUnix=time.time())
            for job in manifest['jobs']:
                if job['status'] in ('running','verifying','waiting-for-memory'):job['status']='interrupted'
        except Exception as error:
            manifest.update(status='failed',error=str(error),finishedUnix=time.time())
        save(output/'campaign.json',manifest);publish_status(output,manifest)
        return 0 if manifest['status']=='completed' else 1

def start(output):
    m=json.loads((output/'campaign.json').read_text());unit=m['unit']
    if subprocess.run(['systemctl','--user','is-active','--quiet',unit],env=bus()).returncode==0:raise RuntimeError('Campaign is already active')
    limits=m['provenance']['limits']
    command=['systemd-run','--user','--collect','--unit',unit,f'--property=MemoryHigh={limits["memoryHighMiB"]}M',f'--property=MemoryMax={limits["memoryMaxMiB"]}M',
             '--property=MemorySwapMax=0','--property=CPUQuota=90%','--property=Nice=10','--property=OOMPolicy=stop',
             '--property=KillMode=control-group','--setenv=CIRCUITSIM_TIMING_PROFILE='+m['timingProfile'],
             '--working-directory='+str(output),sys.executable,'-u',
             str(output/'frozen/tools/rv32im/run-long-gameplays.py'),'worker','--output',str(output)]
    subprocess.run(command,env=bus(),check=True)

def launch(a):
    sys.path.insert(0,str(ROOT/'tools/rv32im'))
    from long_gameplay.reference import manifest as fixture_manifest, host_reference, FIXTURES
    output=a.output.resolve()
    if output.exists():raise RuntimeError('Use a new campaign output directory')
    saves=a.save_every>0 or a.save_at_frames
    if saves!=(a.replay_window>0) or a.save_every<0 or a.replay_window<0:
        raise RuntimeError('Replay saves (--save-every, --save-at-frames) and --replay-window go together')
    if saves and a.ticks:raise RuntimeError('Replay saves are for checkpoint recordings, not signal-tick captures')
    # The smoke fixture checks the equipment; it runs only when named.
    rows=[f for f in fixture_manifest()['guests'] if f['version']=='long' and (f['name']!='smoke-long' or a.games)]
    if a.games:
        wanted=a.games.split(',');rows=[f for f in rows if f['name'] in wanted]
        assert len(rows)==len(wanted),'Unknown long fixture in --games'
    # Cheapest first, so finished games publish early.
    order=['smoke-long','sokoban-long','snake-long','snake-leds-long','2048-long','tetris-long','pong-long']
    rows.sort(key=lambda f:order.index(f['name']) if f['name'] in order else len(order))
    output.mkdir(parents=True);frozen=output/'frozen';(frozen/'bin').mkdir(parents=True)
    games=[]
    for row in rows:
        directory=frozen/'prepared'/row['name'];directory.mkdir(parents=True)
        shutil.copy2(FIXTURES/row['elf'],directory/'guest.elf');shutil.copy2(FIXTURES/row['script'],directory/'input.json')
        assert sha(directory/'guest.elf')==row['elfSha256'] and sha(directory/'input.json')==row['scriptSha256']
        expected=host_reference(row);assert len(expected['frames'])==row['presentations']
        metrics=row['metrics'];report=expected['report']
        assert (report[4],report[5],report[6])==(metrics['totalScore'],metrics['finalStatus'],metrics['finalFingerprint'])
        save(directory/'expected.json',expected)
        games.append({'name':row['name'],'system':row['system'],'frames':row['presentations'],'metrics':metrics,
                      'elfSha256':sha(directory/'guest.elf'),'batchesSha256':sha(directory/'input.json'),
                      'expectedSha256':sha(directory/'expected.json')})
        print('PASS host reference',row['name'],row['presentations'],'frames',flush=True)
    shutil.copy2(a.runner,frozen/'bin/LongGameplayProbe')
    shutil.copy2(Path(shutil.which('node')).resolve(),frozen/'bin/node')
    copied=['tools/rv32im/run-long-gameplays.py','tools/package-recorded-circuit.mjs','tools/package-tick-recording.mjs',
                 'visualizer-v2/static/tick_recording.mjs',
                 'visualizer-v2/static/recorded_circuit_transport.mjs','visualizer-v2/layout.json']
    for name in copied:
        target=frozen/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/name,target)
    names=subprocess.check_output(['git','ls-files','-co','--exclude-standard'],cwd=ROOT,text=True).splitlines()
    sources={name:sha(ROOT/name) for name in sorted(set(names)) if (name=='CMakeLists.txt' or name.startswith(('include/','src/','tools/rv32im/','tests/fixtures/rv32im/'))
             or name in ('tests/CMakeLists.txt','tests/cpp/LongGameplayProbe.cpp','tests/cpp/TickTraceWriter.hpp','tools/package-tick-recording.mjs','visualizer-v2/static/tick_recording.mjs','visualizer-v2/static/recorded_circuit_transport.mjs','visualizer-v2/layout.json')) and (ROOT/name).is_file()}
    provenance={'sourceSha256':sources,'runnerSha256':sha(frozen/'bin/LongGameplayProbe'),
                'frozenFiles':{name:sha(frozen/name) for name in ['bin/LongGameplayProbe','bin/node',*copied]},
                'baseCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'limits':{'memoryMaxMiB':a.memory_mib,'memoryHighMiB':a.memory_mib*5//6,'nativeAddressSpaceMiB':a.memory_mib*5//6,'swap':False,
                          'startAvailableMiB':1536,'stopAvailableMiB':1024,'serial':True,'partCheckpoints':16,
                          'traceDiskBytes':a.trace_disk_gib*1024**3 if a.ticks else 0,
                          'nativeWallDays':a.native_wall_days or (90 if a.fidelity in ('gate','logic','visual') else 7)}}
    relative='assets/system/long-games'+('' if a.fidelity=='compact' and not a.ticks else '/'+a.fidelity+('-ticks' if a.ticks else ''))
    # --publication puts an equipment check outside the site.
    public=(a.publication.resolve() if a.publication else ROOT/'site-docs'/relative);public.mkdir(parents=True,exist_ok=True)
    if output.stat().st_dev!=public.stat().st_dev:raise RuntimeError('Output and publication must share a filesystem for atomic publication')
    if (public/'manifest.json').exists() and not a.games:raise RuntimeError('Long game publication already exists')
    save(public/'source-manifest.json',provenance);save(output/'source-manifest.json',provenance)
    # List the set in the site's index of recordings (the viewer's menu).
    sets_root=ROOT/'site-docs/assets/system/long-games'
    if public!=sets_root and public.is_relative_to(sets_root):
        index_file=sets_root/'recordings.json'
        index=json.loads(index_file.read_text()) if index_file.exists() else {'schemaVersion':1,'sets':[]}
        entry=(public/'manifest.json').relative_to(sets_root).as_posix()
        if all(s['manifest']!=entry for s in index['sets']):
            label=' · '+a.fidelity.capitalize()+(' · every signal' if a.ticks else '')
            index['sets'].append({'manifest':entry,'label':label});save(index_file,index)
    manifest={'version':2,'status':'queued','unit':('circuitsim-long-games-ticks-' if a.ticks else 'circuitsim-long-games-')+str(int(time.time())),
              'fidelityPreset':a.fidelity,'presetVersion':1,'timingProfile':a.timing,'period':a.period,
              'publicationPath':relative,'captureTicks':a.ticks,'saveEvery':a.save_every,'saveFrames':a.save_at_frames,'replayWindow':a.replay_window,
              'scope':'Computer players\' sessions on the 64 x 48 games machine; every frame retained.',
              'publicationRoot':str(public),'nodeExecutable':str(frozen/'bin/node'),
              'provenance':{k:v for k,v in provenance.items() if k!='sourceSha256'},
              'jobs':[{**row,'status':'pending'} for row in games]}
    save(output/'campaign.json',manifest);publish_status(output,manifest)
    if not a.prepare_only:start(output)
    print('Background unit:',manifest['unit']);print('Status:',output/'campaign.json')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('command',choices=['launch','worker','resume','status','stop','compress-published'])
    p.add_argument('--output',type=Path,required=True);p.add_argument('--runner',type=Path)
    p.add_argument('--games',help='Comma-separated long fixtures (default: the six games)')
    p.add_argument('--publication',type=Path,help='Publish here instead of site-docs (equipment checks)')
    p.add_argument('--timing',choices=['unit','embedded','desktop'],default='embedded')
    p.add_argument('--period',type=int,default=4500,help='Clock period in ticks (default 4500: 4.5 ns at embedded timing)')
    p.add_argument('--memory-mib',type=int,default=1024,help='Memory cap of the background unit (default 1024 MiB)')
    p.add_argument('--native-wall-days',type=int,help='Native per-game wall budget (default 90 for structural presets, 7 for compact)')
    p.add_argument('--prepare-only',action='store_true',help='Freeze and publish queued status; resume starts the background unit')
    p.add_argument('--fidelity',choices=['gate','logic','visual','compact'],default='compact')
    p.add_argument('--ticks',action='store_true',help='Retain every signal-change tick in bounded compressed chunks')
    p.add_argument('--save-every',type=int,default=0,help='Machine save for replays about every N cycles (0: none)')
    p.add_argument('--save-at-frames',action='store_true',help='Machine save for replays right after each frame')
    p.add_argument('--replay-window',type=int,default=0,help='Cycles a replay from a save runs in the browser')
    p.add_argument('--trace-disk-gib',type=int,default=12,help='Total campaign compressed trace disk budget (default 12 GiB)')
    a=p.parse_args()
    if a.native_wall_days is not None and not 1<=a.native_wall_days<=180:p.error('Native wall budget must be 1-180 days')
    if not 1<=a.trace_disk_gib<=64:p.error('Trace disk budget must be 1-64 GiB')
    if a.command=='launch':
        if not a.runner:p.error('launch requires --runner')
        if a.period<100 or a.period%2:p.error('The period must be even and at least 100 ticks')
        if not 512<=a.memory_mib<=3072:p.error('Memory cap must be 512-3072 MiB')
        launch(a);return
    output=a.output.resolve()
    if a.command=='compress-published':compress_published(output);return
    if a.command=='worker':raise SystemExit(worker(output))
    m=json.loads((output/'campaign.json').read_text())
    if a.command=='status':print(json.dumps({'status':m['status'],'unit':m['unit'],'jobs':m['jobs']},indent=2))
    elif a.command=='resume':start(output)
    elif a.command=='stop':subprocess.run(['systemctl','--user','stop',m['unit']],env=bus(),check=True)

if __name__=='__main__':main()
