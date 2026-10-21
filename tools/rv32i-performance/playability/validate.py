#!/usr/bin/env python3
"""Check long prototype runs with ASan/UBSan and exact renderer parity."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'build/playability-audit-2026-10-19')
SOURCE=Path(__file__).with_name('host-check.c')
rows=[]
for game in range(3):
 outputs=[]
 for render in (1,2):
  name=f'host-check-{game}-{render}'
  executable=OUT/name
  command=['cc','-O1','-g','-fno-strict-aliasing','-fno-omit-frame-pointer',
   '-fsanitize=address,undefined',f'-DGAME={game}',f'-DRENDER={render}',
   '-DFRAMES=4096',str(SOURCE),'-o',str(executable)]
  subprocess.run(command,check=True)
  result=subprocess.run([str(executable)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
  assert not result.stderr,result.stderr.decode()
  (OUT/(name+'.bin')).write_bytes(result.stdout)
  outputs.append(result.stdout)
  rows.append(dict(game=game,render=render,updates=4097,
   sha256=hashlib.sha256(result.stdout).hexdigest(),command=command))
 assert outputs[0]==outputs[1],f'Game {game}: incremental/full pixel mismatch'
 print('PASS',game,'4097 updates per renderer, sanitizers and exact pixel/state parity',flush=True)
(OUT/'host-validation.json').write_text(json.dumps(rows,indent=2)+'\n')
