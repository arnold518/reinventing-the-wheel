#!/usr/bin/env python3
"""Cross-check a guest fixture's compiled frames and report with a host run.

The same guest C is compiled for the host with stand-in devices
(tests/fixtures/rv32im/games/reference-host.c). Its frames, console and report
are compared with the circuit run (--runner RecordedGuestProbe), or, for a
long fixture with --reference-only, with the computer player's prediction.
RV32IMGameRulesTest independently asserts the rules; the circuit runner
independently checks every instruction and device access.
"""
import argparse, json, pathlib, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/rv32im'))
from long_gameplay.reference import manifest, host_reference

names=[f['name'] for f in manifest()['guests']]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runner',type=pathlib.Path,help='RecordedGuestProbe: run the circuit and compare')
p.add_argument('--reference-only',action='store_true')
p.add_argument('--guest',choices=names,default='bringup')
a=p.parse_args()
if not a.reference_only and not a.runner:p.error('--runner is required for a circuit comparison')
fixture=next(f for f in manifest()['guests'] if f['name']==a.guest)
long=fixture['version']=='long'
script=json.loads((ROOT/'tests/fixtures/rv32im/games'/fixture['script']).read_text())
# The key outcome of each short game (score, final state).
OUTCOMES={'2048-short':(8,0),'sokoban-short':(1,1),'snake-short':(1,0),'pong-short':(0,0),
          'tetris-short':(100,0),'snake-leds-short':(0,2)}
expected=host_reference(fixture)
assert len(expected['frames'])==fixture['presentations'],('Frame count',len(expected['frames']),fixture['presentations'])
report=expected['report']
if a.guest in OUTCOMES:assert tuple(report[4:6])==OUTCOMES[a.guest],('Outcome',a.guest,report)
if long:
    metrics=fixture['metrics']
    assert report[0]==0x4c47534d and report[3]==len(script['batches']) and report[8]==0,('Session report',report)
    assert (report[4],report[5],report[6])==(metrics['totalScore'],metrics['finalStatus'],metrics['finalFingerprint']),('Player prediction',report,metrics)
if a.reference_only:
    print(f"PASS host reference {a.guest}: {len(expected['frames'])} frames, report {report[3:7]} (circuit not run)")
    raise SystemExit(0)

run=subprocess.run([str(a.runner),a.guest,'sram','2000000','100000','compact'],capture_output=True,text=True)
if run.returncode:sys.stderr.write(run.stderr[-4000:]);raise SystemExit(run.returncode)
actual=json.loads(run.stdout)
for key in ['elfSha256','scriptSha256']:assert actual[key]==fixture[key],('Fixture mismatch',key)
assert len(actual['frames'])==len(expected['frames']),(len(actual['frames']),len(expected['frames']))
for i,(frame,pixels) in enumerate(zip(actual['frames'],expected['frames'])):
    assert frame['pixels']==pixels,('Frame differs',i)
    assert set(bytes.fromhex(frame['known']))=={255},('Unknown frame bits',i)
    assert frame['sequence']==i+1,('Sequence differs',i)
assert actual['console']==expected['console'],('Console differs',actual['console'],expected['console'])
words=[int.from_bytes(bytes.fromhex(actual['report'])[i:i+4],'little') for i in range(0,36,4)]
stable=range(7) if fixture['id'] else [1,2,3,4]
for index in stable:assert words[index]==report[index],('Report differs',index,words,report)
if fixture['id']:assert words[8]==report[8]==0,('Input lost or left queued',words[8])
assert actual['setupViolations']==0
print(f"PASS {a.guest}: {len(actual['frames'])} exact frames, console and report match the host run; "
      f"{actual['cycles']} cycles, {actual['instructions']} instructions, {actual['executionSeconds']:.1f} s")
