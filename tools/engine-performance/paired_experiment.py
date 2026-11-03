#!/usr/bin/env python3
"""Retain ABBA comparisons, fixed CPU affinity, load context and drift flags."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import statistics
import time
import experiment as e
from wire_timing_experiment import signature


def host():
    frequencies={}
    for p in Path('/sys/devices/system/cpu').glob('cpu[0-9]*/cpufreq/scaling_cur_freq'):
        try:frequencies[p.parents[1].name]=int(p.read_text())
        except OSError:pass
    temperatures={}
    for p in Path('/sys/class/thermal').glob('thermal_zone*/temp'):
        try:temperatures[p.parent.name]=int(p.read_text())
        except OSError:pass
    return dict(unix=time.time(),loadAverage=os.getloadavg(),availableMiB=e.available(),
                cpuTicks=[s for s in Path('/proc/stat').read_text().splitlines() if s.startswith('cpu')],
                frequenciesKHz=frequencies,temperaturesMilliC=temperatures)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=e.ROOT/'build-milestone/engine-performance-20261026/wire-timing')
    p.add_argument('--name',default='paired');p.add_argument('--iterations',type=int,default=4)
    p.add_argument('--blocks',type=int,default=2)
    p.add_argument('--control',default='baseline')
    p.add_argument('--variants',nargs='+',default=['wire','baseline-o2','memo-stateful'])
    p.add_argument('--cpu',type=int,default=max(os.sched_getaffinity(0)));a=p.parse_args()
    if not 1<=a.iterations<=16 or not 1<=a.blocks<=3 or a.cpu not in os.sched_getaffinity(0):raise ValueError('Bounds exceeded')
    out=a.output.resolve()
    if(out/(a.name+'.json')).exists()or(out/(a.name+'.partial.json')).exists():raise RuntimeError('Use a fresh matrix name')
    golden=json.loads((out/'allocation-calibration.json').read_text())['references'] if a.iterations==4 else {}
    references={};rows=[];blocks=[]
    e.save(out/(a.name+'-protocol.json'),dict(iterations=a.iterations,blocks=a.blocks,variants=a.variants,
        cpu=a.cpu,control=a.control,order='control,candidate,candidate,control',maxEndpointControlDrift=0.05,
        note='All results retained; endpoint stability does not guarantee an otherwise isolated host'))
    for repeat in range(a.blocks):
        variants=list(a.variants)
        if repeat%2:variants.reverse()
        for mode in ['none','stream']:
            for variant in variants:
                current=[]
                for slot,name in enumerate([a.control,variant,variant,a.control]):
                    label=f'{a.name}-{repeat}-{mode}-{variant}-{slot}-{name}';trace=None
                    command=['taskset','-c',str(a.cpu),out/name/'probe','guest',str(a.iterations),mode]
                    if mode=='stream':trace=out/'traces'/label;command.append(trace)
                    before=host();d=e.run(out,label,command);after=host()
                    result=json.loads((d/'stdout.txt').read_text());e.save(d/'result.json',result)
                    observed=signature(result,trace)
                    expected=references.get(mode,golden.get(mode))
                    if expected and observed!=expected:
                        e.save(d/'mismatch.json',dict(expected=expected,actual=observed));raise RuntimeError('Paired semantic mismatch')
                    references.setdefault(mode,observed)
                    receipt=json.loads((d/'receipt.json').read_text());receipt['semanticCheckStatus']='passed';e.save(d/'receipt.json',receipt)
                    stderr=(d/'stderr.txt').read_text()
                    row=dict(repeat=repeat,mode=mode,candidate=variant,slot=slot,variant=name,
                        cpuSeconds=float(re.search(r'ENGINE_EXEC_CPU ([0-9.]+)',stderr)[1]),hostSeconds=result['hostSeconds'],
                        directory=str(d),hostBefore=before,hostAfter=after)
                    rows.append(row);current.append(row);e.save(out/(a.name+'.partial.json'),dict(rows=rows,blocks=blocks))
                    print(label,row['cpuSeconds'],flush=True)
                base=[current[i]['cpuSeconds'] for i in [0,3]];candidate=[current[i]['cpuSeconds'] for i in [1,2]]
                drift=max(base)/min(base)-1
                block=dict(repeat=repeat,mode=mode,variant=variant,controlCpu=base,candidateCpu=candidate,
                    controlDrift=drift,stable=drift<=0.05,candidateSpread=max(candidate)/min(candidate)-1,
                    cpuRatio=math.sqrt(candidate[0]*candidate[1]/(base[0]*base[1])))
                blocks.append(block);e.save(out/(a.name+'.partial.json'),dict(rows=rows,blocks=blocks));print(json.dumps(block),flush=True)
    summary=[]
    for mode in ['none','stream']:
        for variant in a.variants:
            matches=[b for b in blocks if b['mode']==mode and b['variant']==variant];stable=[b['cpuRatio'] for b in matches if b['stable']]
            summary.append(dict(mode=mode,variant=variant,blocks=len(matches),stableBlocks=len(stable),
                allBlockRatios=[b['cpuRatio'] for b in matches],
                medianStableRatio=statistics.median(stable) if stable else None))
    e.save(out/(a.name+'.json'),dict(iterations=a.iterations,rows=rows,blocks=blocks,summary=summary,references=references))
    print(json.dumps(summary,indent=2))


if __name__=='__main__':main()
