#!/usr/bin/env python3
"""Interleave repeated candidates, checking event counters and trace bytes."""
import argparse
import json
from pathlib import Path
import re
import statistics
import experiment

def metrics(result):
    return {x['name']:x['value'] for x in result['metrics']}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--repeats',type=int,default=3)
    p.add_argument('--variants',nargs='+',default=['baseline','wire-fast','hot-o2'])
    p.add_argument('--name',default='measurements')
    a=p.parse_args();output=a.output.resolve()
    if not 1<=a.repeats<=5:raise ValueError('Repeat count out of bounds')
    if (output/(a.name+'.json')).exists():raise ValueError('Measurement result already exists; use a new --name')
    cases=[('wire1-changing',['wire1','50000','digest']),
           ('wire32-unchanged',['wire32','50000','digest','unchanged']),
           ('fanout64',['fanout','10000','digest']),
           ('rv32im-none',['guest','4','none']),
           ('rv32im-digest',['guest','4','digest']),
           ('rv32im-stream',['guest','4','stream'])]
    results=[];references={}
    for repeat in range(a.repeats):
        order=a.variants[repeat%len(a.variants):]+a.variants[:repeat%len(a.variants)]
        for case,arguments in cases:
            for variant in order:
                label=f'{a.name}-{repeat}-{case}-{variant}'
                args=list(arguments);trace=None
                if case.endswith('stream'):
                    trace=output/'traces'/label
                    trace.parent.mkdir(exist_ok=True)
                    args.append(str(trace))
                directory=experiment.run(output,label,[output/variant/'probe',*args])
                result=json.loads((directory/'stdout.txt').read_text())
                match=re.search(r'ENGINE_EXEC_CPU ([0-9.]+)',(directory/'stderr.txt').read_text())
                execution_cpu=float(match[1]) if match else None
                experiment.save(directory/'result.json',result)
                m=metrics(result)
                signature={'fingerprints':result['fingerprints'],'ticks':result['simulatedTicks'],
                    'counters':{k:v for k,v in m.items() if k not in ('setup_seconds','events_per_second')}}
                if trace:
                    signature['traceFiles']={x.name:experiment.sha(x) for x in sorted(trace.iterdir()) if x.is_file()}
                if case in references and signature!=references[case]:
                    experiment.save(directory/'mismatch.json',{'expected':references[case],'actual':signature})
                    receipt=json.loads((directory/'receipt.json').read_text())
                    receipt['semanticCheckStatus']='failed'
                    experiment.save(directory/'receipt.json',receipt)
                    raise RuntimeError('Semantic mismatch at '+str(directory))
                receipt=json.loads((directory/'receipt.json').read_text())
                receipt['semanticCheckStatus']='passed'
                experiment.save(directory/'receipt.json',receipt)
                references.setdefault(case,signature)
                row=dict(repeat=repeat,case=case,variant=variant,hostSeconds=result['hostSeconds'],
                         executionCpuSeconds=execution_cpu,eventsPerSecond=m['events_per_second'],directory=str(directory),semanticMatch=True)
                results.append(row)
                experiment.save(output/(a.name+'.partial.json'),results)
                print(case,variant,round(result['hostSeconds'],4),round(m['events_per_second']),flush=True)
    summary=[]
    for case,_ in cases:
        for variant in a.variants:
            rows=[r for r in results if r['case']==case and r['variant']==variant]
            times=[r['hostSeconds'] for r in rows]
            summary.append(dict(case=case,variant=variant,repeats=len(rows),
                medianSeconds=statistics.median(times),minSeconds=min(times),maxSeconds=max(times),
                medianExecutionCpuSeconds=statistics.median(r['executionCpuSeconds'] for r in rows) if all(r['executionCpuSeconds'] is not None for r in rows) else None,
                medianEventsPerSecond=statistics.median(r['eventsPerSecond'] for r in rows)))
    experiment.save(output/(a.name+'.json'),dict(results=results,summary=summary,references=references))
    print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
