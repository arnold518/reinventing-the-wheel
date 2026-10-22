#!/usr/bin/env python3
"""Run an audited scripted guest through the native circuit and save evidence."""
import argparse, hashlib, json, pathlib, resource, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('guest',choices=['bringup','2048','sokoban','snake','pong','tetris','2048-merge','tetris-line-clear','pong-rally','snake-wall'])
p.add_argument('--runner',type=pathlib.Path,required=True)
p.add_argument('--machine',choices=['ff','cached-ff','cached-dram'],default='ff')
p.add_argument('--output',type=pathlib.Path,required=True)
p.add_argument('--width',type=int,default=64)
p.add_argument('--height',type=int,default=48)
p.add_argument('--max-cycles',type=int,default=500000)
p.add_argument('--timeout',type=int,default=3600)
p.add_argument('--memory-mib',type=int,default=768)
p.add_argument('--quantum-events',type=int,default=100000)
p.add_argument('--logic-profile',choices=['compact','expanded-core','structural'],default='compact')
a=p.parse_args()
if not 1<=a.width<=4096 or not 1<=a.height<=4096 or a.width*a.height>262144:p.error('Invalid surface geometry')
if a.memory_mib<128 or a.memory_mib>4096 or not 1<=a.max_cycles<=10000000 or not 1<=a.quantum_events<=1000000 or a.timeout<=0:p.error('Invalid resource limit')
sha=lambda data:hashlib.sha256(data).hexdigest()
fixtures=ROOT/'tests/fixtures/rv32im/games';manifest=json.loads((fixtures/'manifest.json').read_text())
f=next(f for f in manifest['guests'] if f['name']==a.guest)
for key,file_key in [('elfSha256','elf'),('scriptSha256','script')]:
    if sha((fixtures/f[file_key]).read_bytes())!=f[key]:raise SystemExit('Fixture provenance mismatch: '+file_key)
for source,digest in manifest['sources'].items():
    if sha((ROOT/'tools/rv32im/guest'/source).read_bytes())!=digest:raise SystemExit('Guest source changed; rebuild fixtures: '+source)
a.output.mkdir(parents=True,exist_ok=True)
if any((a.output/name).exists() for name in ('recording.json','evidence.json')):
    raise SystemExit('Output already contains a recording or evidence; choose a fresh directory')
runner=a.runner.resolve();rss=a.output/'peak-rss-kib.txt'
def source_hashes():
    files=subprocess.check_output(['git','ls-files','-co','--exclude-standard'],cwd=ROOT,text=True).splitlines()
    return {name:sha((ROOT/name).read_bytes()) for name in sorted(set(files)) if (name=='CMakeLists.txt' or name.startswith(('include/','src/','tools/rv32im/','tools/build/'))) and (ROOT/name).is_file()}
def build_configuration():
    cache=runner.parent.parent/'CMakeCache.txt'
    if not cache.is_file():return None
    keys={'CMAKE_BUILD_TYPE','CMAKE_CXX_FLAGS','CMAKE_CXX_FLAGS_RELEASE','CMAKE_CXX_COMPILER',
          'CMAKE_CXX_COMPILER_LAUNCHER','CMAKE_INTERPROCEDURAL_OPTIMIZATION',
          'CMAKE_SHARED_LINKER_FLAGS','CMAKE_MODULE_LINKER_FLAGS',
          'CIRCUITSIM_DEPLOY_PYTHON','CIRCUITSIM_SPLIT_DELAY_TABLES'}
    values={}
    for line in cache.read_text().splitlines():
        if not line or line.startswith(('#','//')) or '=' not in line:continue
        declaration,value=line.split('=',1);key=declaration.split(':',1)[0]
        if key in keys:values[key]=value
    result={'cacheSha256':sha(cache.read_bytes()),'values':values}
    launcher=pathlib.Path(values.get('CMAKE_CXX_COMPILER_LAUNCHER','').split(';',1)[0])
    if launcher.is_file():
        result['compilerLauncherSha256']=sha(launcher.read_bytes())
        (a.output/'compiler-launcher.txt').write_bytes(launcher.read_bytes())
    return result
viewer_before={str(path.relative_to(ROOT)):sha(path.read_bytes()) for path in sorted((ROOT/'visualizer-v2/static').rglob('*')) if path.is_file()}
viewer_before.update({name:sha((ROOT/name).read_bytes()) for name in ('visualizer-v2/layout.json','visualizer-v2/palette.json','visualizer-v2/profiles.defaults.json')})
build_before=build_configuration()
source_before=source_hashes();runner_before=sha(runner.read_bytes());manifest_before=sha((fixtures/'manifest.json').read_bytes())

def cap():
    limit=a.memory_mib*1024*1024;resource.setrlimit(resource.RLIMIT_AS,(limit,limit))
command=[str(runner),a.guest,a.machine,str(a.max_cycles),str(a.quantum_events),a.logic_profile,str(a.width),str(a.height)]
# Keep the simulator as the direct child so subprocess kills and reaps the
# actual runner on timeout. An intervening timing wrapper could leave it alive.
with (a.output/'stderr.log').open('w') as error_log:
    try:
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=error_log,text=True,timeout=a.timeout,preexec_fn=cap)
    except subprocess.TimeoutExpired:
        rss.write_text(str(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)+'\n')
        error_log.write('\nGuest wall-time budget exhausted\n')
        raise SystemExit(f'Guest timed out; see {a.output}/stderr.log')
rss.write_text(str(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)+'\n')
if result.returncode:raise SystemExit(f'Guest failed ({result.returncode}); see {a.output}/stderr.log')
record=json.loads(result.stdout)
if record.get('elfSha256')!=f['elfSha256'] or record.get('scriptSha256')!=f['scriptSha256']:raise SystemExit('Runner embeds different guest fixtures; rebuild the native target')
(a.output/'recording.json').write_text(json.dumps(record,separators=(',',':'))+'\n')
frame_hashes=[sha(bytes.fromhex(frame['pixels'])) for frame in record['frames']]
if source_hashes()!=source_before or sha(runner.read_bytes())!=runner_before or sha((fixtures/'manifest.json').read_bytes())!=manifest_before:
    raise SystemExit('Source, native binary or fixture manifest changed during execution; recording retained without success evidence')
evidence={'version':1,'guest':a.guest,'machine':a.machine,'logicProfile':a.logic_profile,'surface':{'width':a.width,'height':a.height},'quantumEvents':a.quantum_events,'command':command,'baseCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'sourceSha256':source_before,'buildConfiguration':build_before,'viewerSourceSha256AtStart':viewer_before,'runnerSha256':runner_before,'fixtureManifestSha256':manifest_before,'recordingSha256':sha((a.output/'recording.json').read_bytes()),'limits':{'cycles':a.max_cycles,'timeoutSeconds':a.timeout,'addressSpaceMiB':a.memory_mib},'peakRssKiB':int(rss.read_text()),'frameSha256':frame_hashes,'cycles':record['cycles'],'instructions':record['instructions'],'executionSeconds':record['executionSeconds'],'captureSeconds':record['captureSeconds'],'setupViolations':record['setupViolations'],'events':record['events']}
(a.output/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
print(f"{a.guest}: {len(frame_hashes)} frames, {record['cycles']} cycles, {record['executionSeconds']:.3f}s; {a.output}")
