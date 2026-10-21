#!/usr/bin/env python3
"""Aggregate completed playability samples; exclude verified warm-up passes
when separate timed passes exist. Preserve every configuration in JSON/CSV.
"""
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

out=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[3]/'build/playability-audit-2026-10-19')
groups=defaultdict(list)
for file in out.glob('*.json'):
 try:r=json.loads(file.read_text())
 except json.JSONDecodeError:continue
 if not isinstance(r,dict) or not r.get('completed'):continue
 if 'audit' in r:
  a=r['audit'];case=a['case'];frames=a['frames'];rep=a['repeat'];backend='native'
  command=a['command'];memory=r['memory'];fidelity=r['fidelity']
  topology=command[command.index('--topology')+1] if '--topology' in command else ''
  period=r['period_ticks']
 elif 'config' in r:
  c=r['config'];case=c['name'];frames=r['frames'];rep=r['repeat'];backend='browser'
  memory=c['memory'];fidelity=c['fidelity'];topology=c['topology'];period=4104
 else:continue
 key=(backend,memory,topology,fidelity,period,case)
 groups[key].append(dict(file=str(file),repeat=rep,frames=frames,
  fps=frames/r['host_execution_seconds'],seconds_per_frame=r['host_execution_seconds']/frames,
  cycles_per_frame=r['cycles']/frames,instructions_per_frame=r['retired_instructions']/frames,
  cycles_per_host_second=r['cycles']/r['host_execution_seconds'],
  events_per_cycle=r['processed_events']/r['cycles'],verified=r['verified'],
  setup_violations=r['setup_violations'],fingerprint=r['final_state_fnv1a64']))
rows=[]
case_fingerprints=defaultdict(set)
for key,samples in sorted(groups.items()):
 assert any(s['verified'] for s in samples),key
 assert all(s['setup_violations']==0 for s in samples),key
 assert len({s['fingerprint'] for s in samples})==1,key
 timed=[s for s in samples if not s['verified']] or samples
 backend,memory,topology,fidelity,period,case=key
 case_fingerprints[case].update(s['fingerprint'] for s in samples)
 row=dict(backend=backend,memory=memory,topology=topology,fidelity=fidelity,period=period,case=case,
  frames=timed[0]['frames'],samples=len(samples),timed_samples=len(timed),
  fps=statistics.median(s['fps'] for s in timed),fps_min=min(s['fps'] for s in timed),fps_max=max(s['fps'] for s in timed),
  milliseconds_per_update=1000*statistics.median(s['seconds_per_frame'] for s in timed),
  instructions_per_frame=timed[0]['instructions_per_frame'],cycles_per_frame=timed[0]['cycles_per_frame'],
  cycles_per_host_second=statistics.median(s['cycles_per_host_second'] for s in timed),
  events_per_cycle=statistics.median(s['events_per_cycle'] for s in timed))
 rows.append(row)
 print(backend,memory,topology or 'separate',fidelity,period,case,
       f"{row['fps']:.4f} fps, {row['milliseconds_per_update']:.1f} ms/update",flush=True)
assert all(len(values)==1 for values in case_fingerprints.values()),'Cross-configuration final-state mismatch'
(out/'verification-summary.json').write_text(json.dumps(dict(
 retained_runs=sum(len(samples) for samples in groups.values()),
 oracle_verified_runs=sum(s['verified'] for samples in groups.values() for s in samples),
 setup_violations=0,cross_configuration_fingerprints_match=True,
 cases=sorted(case_fingerprints)),indent=2)+'\n')
(out/'summary.json').write_text(json.dumps(rows,indent=2)+'\n')
with (out/'summary.csv').open('w') as file:
 writer=csv.DictWriter(file,fieldnames=list(rows[0]) if rows else [])
 writer.writeheader();writer.writerows(rows)
