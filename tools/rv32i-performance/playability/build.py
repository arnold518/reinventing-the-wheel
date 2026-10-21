#!/usr/bin/env python3
"""Build original RV32I game audit workloads with host-checked results."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'build/playability-audit-2026-10-19').resolve()
SRC = Path(__file__).with_name('game-kernels.c')
GCC = ROOT/'build/toolchains/xpack-riscv-none-elf-gcc-12.4.0-2/bin/riscv-none-elf-gcc'
NM = GCC.with_name('riscv-none-elf-nm')
PORT = ROOT/'tools/rv32i-benchmarks/embench'
CASES = [
 ('pong-logic',0,0,64,48,512),('pong-dirty',0,1,64,48,128),
 ('pong-full64',0,2,64,48,4),('pong-full160',0,2,160,120,1),
 ('tetris-logic',1,0,64,48,64),('tetris-dirty',1,1,64,48,32),
 ('tetris-full64',1,2,64,48,2),('tetris-lineclear',2,1,64,48,4),
 ('pong-logic-short',0,0,64,48,1),('pong-dirty-short',0,1,64,48,1),
 ('tetris-logic-short',1,0,64,48,1),('tetris-dirty-short',1,1,64,48,1),
]

def main():
 (OUT/'elfs').mkdir(parents=True,exist_ok=True)
 rows=[]
 for name,game,render,w,h,frames in CASES:
  defines=[f'-DGAME={game}',f'-DRENDER={render}',f'-DWIDTH={w}',f'-DHEIGHT={h}',f'-DFRAMES={frames}']
  host=OUT/(name+'-host')
  subprocess.run(['cc','-std=c11','-O2','-fno-strict-aliasing','-DAUDIT_HOST',*defines,str(SRC),'-o',str(host)],check=True)
  expected,n,points,locks=map(int,subprocess.check_output([str(host)],text=True).split())
  assert n==frames+1 and (game!=2 or points==frames+1 and locks==frames+1)
  elf=OUT/'elfs'/(name+'.elf')
  command=[str(GCC),'-march=rv32i','-mabi=ilp32','-O2','-fno-strict-aliasing','-ffunction-sections',
   '-fdata-sections','-fno-builtin','-fno-common','-nostdlib','-nostartfiles',*defines,f'-DEXPECTED={expected}u',
   '-Wl,--build-id=none','-Wl,--no-relax','-Wl,--gc-sections','-Wl,-T,'+str(PORT/'link.ld'),
   str(PORT/'start.S'),str(SRC),'-lgcc','-o',str(elf)]
  subprocess.run(command,check=True)
  symbols={r.split()[2]:int(r.split()[0],16) for r in subprocess.check_output([str(NM),'-n',str(elf)],text=True).splitlines() if len(r.split())==3}
  row=dict(name=name,game=game,render=render,width=w,height=h,frames=frames,expected=expected,
   expected_points=points,expected_locks=locks,elf=str(elf),begin=symbols['audit_begin'],end=symbols['audit_end'],
   sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),command=command)
  rows.append(row)
  print(name,hex(row['begin']),hex(row['end']),expected,flush=True)
 manifest=dict(source_sha256=hashlib.sha256(SRC.read_bytes()).hexdigest(),cases=rows)
 (OUT/'workloads.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (OUT/'audit_cases.inc').write_text('\n'.join(
  '{"'+r['name']+'",'+str(r['begin'])+','+str(r['end'])+','+str(r['frames'])+'},' for r in rows)+'\n')

if __name__=='__main__': main()
