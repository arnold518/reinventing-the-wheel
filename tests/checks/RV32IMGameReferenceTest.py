#!/usr/bin/env python3
"""Cross-check exact compiled guest frames/reports with host C reference runs.

Host C is a compiler/renderer reference. RV32IMGameRulesTest independently
asserts the rules; the circuit runner independently checks ISA and devices.
"""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, sys, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runner',type=pathlib.Path)
p.add_argument('--recording',type=pathlib.Path,help='Compare an existing circuit recording without running a binary')
p.add_argument('--reference-only',action='store_true')
p.add_argument('--guest',choices=['bringup','2048','sokoban','snake','pong','tetris','2048-merge','tetris-line-clear','pong-rally','snake-wall'],default='bringup')
p.add_argument('--output',type=pathlib.Path)
a=p.parse_args()
if not a.reference_only and not (a.runner or a.recording):p.error('--runner or --recording is required for circuit comparison')
if a.runner and a.recording:p.error('Choose --runner or --recording')
guest=ROOT/'tools/rv32im/guest';fixtures=ROOT/'tests/fixtures/rv32im/games'
fixture=next(f for f in json.loads((fixtures/'manifest.json').read_text())['guests'] if f['name']==a.guest)
kind=fixture['id'];events=json.loads((fixtures/fixture['script']).read_text())['events']
compiler=os.environ.get('CC','cc')
native_output=None
if a.runner and not a.reference_only:
    # Use the same bounded recording CLI as the acceptance campaign. Preserve
    # each fresh run rather than discarding its JSON and resource/provenance
    # evidence after the assertions; successful artifacts can be compared
    # again without executing the same native workload unnecessarily.
    artifact_root=a.runner.resolve().parent.parent/'recorded-guest-native'
    artifact_root.mkdir(parents=True,exist_ok=True)
    native_output=pathlib.Path(tempfile.mkdtemp(prefix=a.guest+'-',dir=artifact_root))
    subprocess.run([sys.executable,str(ROOT/'tools/rv32im/run-recorded.py'),a.guest,
                    '--runner',str(a.runner.resolve()),'--output',str(native_output),
                    '--timeout','3500'],check=True,timeout=3550)
with tempfile.TemporaryDirectory(prefix='circuitsim-guest-reference-') as directory:
    directory=pathlib.Path(directory);objects=[]
    source_names=['platform.c','bringup.c'] if not kind else ['platform.c','games.c','main.c']
    common=['-std=c11','-O1','-fno-builtin','-Wall','-Wextra','-Werror','-I',str(guest),'-DCIRCUITSIM_GUEST_REFERENCE',*[f'-D{k}={v}' for k,v in fixture['defines'].items()],'-DREFERENCE_BUTTONS='+','.join(str(e['button']) for e in events),'-DREFERENCE_DOWN='+','.join(str(int(e['down'])) for e in events)]
    for name in source_names:
        obj=directory/(name+'.o');subprocess.run([compiler,*common,'-Dmain=guest_main','-c',str(guest/name),'-o',str(obj)],check=True);objects.append(str(obj))
    binary=directory/'reference';subprocess.run([compiler,*common,str(ROOT/'tests/fixtures/rv32im/games/reference-host.c'),*objects,'-o',str(binary)],check=True)
    expected=json.loads(subprocess.check_output([str(binary)],text=True))
    acceptance={'2048-merge':(8,0),'tetris-line-clear':(100,0),'snake-wall':(0,2),'pong-rally':(0,0)}
    if a.guest in acceptance:
        assert tuple(expected['report'][4:6])==acceptance[a.guest],('Acceptance rule outcome',a.guest,expected['report'])
    assert len(expected['frames'])==fixture['presentations']
    if a.reference_only:
        if a.output:
            a.output.mkdir(parents=True,exist_ok=True)
            (a.output/'expected.json').write_text(json.dumps(expected))
        print(f"PASS host guest/reference build {a.guest}: {len(expected['frames'])} frames (circuit not run)")
        raise SystemExit(0)
    # Keep the circuit's output/errors visible if a budget or check fails.
    if a.recording:
        recording_text=a.recording.read_text()
    else:
        recording_text=(native_output/'recording.json').read_text()
    actual=json.loads(recording_text)
    for key in ['elfSha256','scriptSha256']:assert actual[key]==fixture[key],('Fixture mismatch',key)
    if a.runner or 'inputs' in actual:
        expected_inputs=[{'time':int(e['tick']),'sequence':int(e['sequence']),'button':e['button'],'down':e['down']} for e in events]
        actual_inputs=[{**e,'time':int(e['time']),'sequence':int(e['sequence'])} for e in actual['inputs']]
        assert actual_inputs==expected_inputs,('Sampled input log differs',actual_inputs,expected_inputs)

    assert len(actual['frames'])==len(expected['frames']),(len(actual['frames']),len(expected['frames']))
    for i,(frame,pixels) in enumerate(zip(actual['frames'],expected['frames'])):
        assert frame['pixels']==pixels,('Frame differs',i)
        assert set(bytes.fromhex(frame['known']))=={255},('Unknown frame bits',i)
        assert frame['sequence']==i+1,('Sequence differs',i)
    words=[int.from_bytes(bytes.fromhex(actual['report'])[i:i+4],'little') for i in range(0,36,4)]
    stable=range(7) if kind else [1,2,3,4]
    for index in stable:assert words[index]==expected['report'][index],('Report differs',index,words,expected['report'])
    if kind:assert words[8]==expected['report'][8]==0,('Input lost or left queued',words[8])
    assert actual['setupViolations']==0
    if a.output:
        a.output.mkdir(parents=True,exist_ok=True)
        (a.output/'recording.json').write_text(recording_text)
        (a.output/'expected.json').write_text(json.dumps(expected))
        (a.output/'frame-hashes.json').write_text(json.dumps([hashlib.sha256(bytes.fromhex(f['pixels'])).hexdigest() for f in actual['frames']],indent=2))
    if native_output:
        (native_output/'expected.json').write_text(json.dumps(expected))
        verdict={'guest':a.guest,'artifactDirectory':str(native_output),
                 'referenceCheckerSha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
                 'exactFrames':len(actual['frames']),'cycles':actual['cycles'],'setupViolations':actual['setupViolations']}
        (native_output/'reference-verdict.json').write_text(json.dumps(verdict,indent=2)+'\n')
        # Only publish the index after every exact frame/report assertion passes.
        (native_output.parent/(a.guest+'.json')).write_text(json.dumps(verdict,indent=2)+'\n')
    print(f"PASS {a.guest}: {len(actual['frames'])} exact frames, stable reports, no lost input, ISA/device checks and zero setup violations; {actual['cycles']} cycles / {actual['executionSeconds']}s")
