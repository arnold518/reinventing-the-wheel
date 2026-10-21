#!/usr/bin/env python3
"""Measure the audit ELF frame region with the existing experiment runner."""
import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--out',type=Path,default=ROOT/'build/playability-audit-2026-10-19')
 p.add_argument('--cases',nargs='+',required=True)
 p.add_argument('--memory',default='ff')
 p.add_argument('--topology',default='')
 p.add_argument('--fidelity',default='behavioral')
 p.add_argument('--period',type=int,default=4104)
 p.add_argument('--repeats',type=int,default=3)
 p.add_argument('--timeout',type=int,default=600)
 a=p.parse_args();out=a.out.resolve()
 manifest=json.loads((out/'workloads.json').read_text())
 byname={r['name']:r for r in manifest['cases']}
 tag=a.fidelity+'-'+a.memory+('-'+a.topology if a.topology else '')
 if a.period!=4104:tag+='-period'+str(a.period)
 binary=ROOT/'build-cleanup/rv32i_experiment'
 metadata=dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
  head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
  timing_profile='embedded',fidelity=a.fidelity,memory=a.memory,topology=a.topology,period=a.period)
 (out/(tag+'-metadata.json')).write_text(json.dumps(metadata,indent=2)+'\n')
 for name in a.cases:
  case=byname[name];fingerprint=None
  for rep in range(a.repeats):
   stem=f'{tag}-{name}-{rep}'
   command=[str(binary),'--core','advanced','--elf',case['elf'],'--memory',a.memory,
    '--fidelity',a.fidelity,'--period',str(a.period),'--start-pc',str(case['begin']),'--stop-pc',str(case['end']),
    '--max-cycles','3000000','--max-events','3000000000','--progress-cycles','10000']
   if a.topology:command+=['--topology',a.topology]
   if rep:command+=['--no-verify']
   started=time.perf_counter()
   binary_hash=hashlib.sha256(binary.read_bytes()).hexdigest()
   print('START',stem,flush=True)
   with (out/(stem+'.json')).open('w') as stdout,(out/(stem+'.stderr')).open('w') as stderr:
    completed=subprocess.run(command,stdout=stdout,stderr=stderr,
     env=dict(os.environ,CIRCUITSIM_TIMING_PROFILE='embedded'),timeout=a.timeout)
   if completed.returncode:
    raise RuntimeError(stem+': '+(out/(stem+'.stderr')).read_text())
   r=json.loads((out/(stem+'.json')).read_text())
   assert r['completed'] and r['setup_violations']==0,stem
   if rep==0:assert r['verified'],stem
   if fingerprint is None:fingerprint=r['final_state_fnv1a64']
   assert r['final_state_fnv1a64']==fingerprint,stem
   r.update(audit=dict(case=name,frames=case['frames'],repeat=rep,
    binary_sha256=binary_hash,
    fps=case['frames']/r['host_execution_seconds'],
    seconds_per_frame=r['host_execution_seconds']/case['frames'],
    cycles_per_frame=r['cycles']/case['frames'],
    instructions_per_frame=r['retired_instructions']/case['frames'],
    total_wall_seconds=time.perf_counter()-started,command=command))
   (out/(stem+'.json')).write_text(json.dumps(r,indent=2)+'\n')
   print('DONE',stem,json.dumps({k:r['audit'][k] for k in ('fps','seconds_per_frame','instructions_per_frame','cycles_per_frame','total_wall_seconds')}),flush=True)

if __name__=='__main__':main()
