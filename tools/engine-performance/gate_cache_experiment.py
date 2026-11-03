#!/usr/bin/env python3
"""Compare complete-layout-compatible gate/cache controls using native Clang."""
import argparse
import json
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import experiment

def build(output,build_dir,compiler):
    if (output/'builds.json').exists():raise RuntimeError('Completed comparison build already exists')
    snap=output/'snapshot';snap.mkdir()
    shutil.copytree(experiment.ROOT/'include',snap/'include')
    shutil.copytree(experiment.ROOT/'src',snap/'src')
    shutil.copy2(build_dir/'libSimulatorLib.a',snap/'libSimulatorLib.a')
    shutil.copy2(experiment.ROOT/'tests/cpp/TickTraceWriter.hpp',snap/'TickTraceWriter.hpp')
    shutil.copy2(experiment.ROOT/'tools/engine-performance/EnginePerformanceProbe.cpp',snap/'probe.cpp')
    flags=['-std=gnu++20','-O1','-g0','-DNDEBUG','-fPIC','-UNDEBUG','-Wno-unknown-warning-option',
           '-DCIRCUITSIM_SOURCE_DIR="'+str(experiment.ROOT)+'"',
           '-I'+str(snap/'include'),'-I'+str(snap/'src'),'-I'+str(snap)]
    sources={
        'gate-named':output/'before/src/modules/basic/Gate.cpp',
        'gate-cached':snap/'src/modules/basic/Gate.cpp',
        'state-original':output/'before/src/simulator/Simulator.cpp',
        'state-reused':snap/'src/simulator/Simulator.cpp',
    }
    for name,source in sources.items():
        experiment.run(output,'compile-'+name,[compiler,*flags,'-c',source,'-o',output/(name+'.o')])
    source=(snap/'src/runtime/RecordedGuest.cpp').read_text()
    source='#include <ctime>\n#include <iostream>\n'+source
    source=source.replace('const auto start=Clock::now();','const auto start=Clock::now();const auto cpu_start=std::clock();')
    old='result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();return result;'
    assert source.count(old)==1
    source=source.replace(old,'result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();std::cerr << "ENGINE_EXEC_CPU " << double(std::clock()-cpu_start)/CLOCKS_PER_SEC << "\\n";return result;')
    (snap/'execution-cpu.cpp').write_text(source)
    experiment.run(output,'compile-execution-timer',[compiler,*flags,'-c',snap/'execution-cpu.cpp','-o',output/'execution-cpu.o'])
    experiment.run(output,'compile-cache-benchmark-driver',[compiler,*flags,'-c',snap/'probe.cpp','-o',output/'driver.o'])
    combinations={'baseline':('gate-named','state-original'),'gate-cache':('gate-cached','state-original'),
                  'buffer-reuse':('gate-named','state-reused'),'combined':('gate-cached','state-reused')}
    builds={}
    for name,(gate,state) in combinations.items():
        d=output/name;d.mkdir()
        objects=[output/(gate+'.o'),output/(state+'.o'),output/'execution-cpu.o',output/'driver.o']
        exe=d/'probe'
        experiment.run(output,'link-'+name,[compiler,*objects,snap/'libSimulatorLib.a','-lz','-o',exe])
        builds[name]=dict(binarySha256=experiment.sha(exe),objects={str(p):experiment.sha(p) for p in objects})
    # A separate executable keeps the complete pre-change headers/archive
    # together, so the net comparison never mixes incompatible class layouts.
    old=output.parent/'snapshot';before=output/'prechange';before.mkdir()
    old_flags=['-std=gnu++20','-O1','-g0','-DNDEBUG','-fPIC','-UNDEBUG','-Wno-unknown-warning-option',
               '-DCIRCUITSIM_SOURCE_DIR="'+str(experiment.ROOT)+'"','-I'+str(old/'include'),'-I'+str(old/'src'),'-I'+str(old)]
    source=(old/'src/runtime/RecordedGuest.cpp').read_text()
    source='#include <ctime>\n#include <iostream>\n'+source
    source=source.replace('const auto start=Clock::now();','const auto start=Clock::now();const auto cpu_start=std::clock();')
    assert source.count(old_marker:= 'result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();return result;')==1
    source=source.replace(old_marker,'result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();std::cerr << "ENGINE_EXEC_CPU " << double(std::clock()-cpu_start)/CLOCKS_PER_SEC << "\\n";return result;')
    (before/'execution-cpu.cpp').write_text(source)
    experiment.run(output,'compile-prechange-timer',[compiler,*old_flags,'-c',before/'execution-cpu.cpp','-o',before/'execution-cpu.o'])
    experiment.run(output,'compile-prechange-driver',[compiler,*old_flags,'-c',old/'probe.cpp','-o',before/'driver.o'])
    experiment.run(output,'link-prechange',[compiler,before/'execution-cpu.o',before/'driver.o',old/'libSimulatorLib.a','-lz','-o',before/'probe'])
    builds['prechange']=dict(binarySha256=experiment.sha(before/'probe'),archiveSha256=experiment.sha(old/'libSimulatorLib.a'),
        headers=str(old/'include'),flags=old_flags)
    experiment.save(output/'builds.json',dict(variants=builds,compiler=str(compiler),
        compilerVersion=subprocess.check_output([compiler,'--version'],text=True),flags=flags,
        archiveSha256=experiment.sha(snap/'libSimulatorLib.a'),
        sourceHashes={str(f.relative_to(snap)):experiment.sha(f) for f in snap.rglob('*') if f.is_file() and f.suffix in ('.hpp','.cpp','.tpp')}))

def measure(output,repeats,iterations):
    if (output/'results.json').exists():raise RuntimeError('Completed comparison results already exist')
    variants=['prechange','baseline','gate-cache','buffer-reuse','combined'];rows=[];references={}
    for repeat in range(repeats):
        order=variants[repeat%len(variants):]+variants[:repeat%len(variants)]
        for mode in ['none','stream']:
            for name in order:
                label=f'measure-{repeat}-{mode}-{name}';args=[output/name/'probe','guest',str(iterations),mode]
                trace=output/'traces'/label
                if mode=='stream':trace.parent.mkdir(exist_ok=True);args.append(trace)
                d=experiment.run(output,label,args)
                result=json.loads((d/'stdout.txt').read_text());experiment.save(d/'result.json',result)
                metrics={r['name']:r['value'] for r in result['metrics']}
                signature=dict(fingerprints=result['fingerprints'],ticks=result['simulatedTicks'],
                    counters={k:v for k,v in metrics.items() if k not in ('setup_seconds','events_per_second')})
                if mode=='stream':signature['traceFiles']={p.name:experiment.sha(p) for p in sorted(trace.iterdir()) if p.is_file()}
                if mode in references and signature!=references[mode]:
                    experiment.save(d/'mismatch.json',dict(expected=references[mode],actual=signature))
                    raise RuntimeError('Semantic difference at '+str(d))
                references.setdefault(mode,signature)
                cpu=float(re.search(r'ENGINE_EXEC_CPU ([0-9.]+)',(d/'stderr.txt').read_text())[1])
                row=dict(repeat=repeat,mode=mode,variant=name,cpuSeconds=cpu,hostSeconds=result['hostSeconds'],
                         events=metrics['processed_events'],directory=str(d))
                rows.append(row);experiment.save(output/'results.partial.json',rows)
                print(name,mode,'CPU',cpu,'wall',round(result['hostSeconds'],3),flush=True)
    summary=[]
    for mode in ['none','stream']:
        for name in variants:
            values=[r for r in rows if r['mode']==mode and r['variant']==name]
            cpu=[r['cpuSeconds'] for r in values]
            summary.append(dict(mode=mode,variant=name,repeats=len(values),medianCpuSeconds=statistics.median(cpu),
                minCpuSeconds=min(cpu),maxCpuSeconds=max(cpu),medianWallSeconds=statistics.median(r['hostSeconds'] for r in values)))
    experiment.save(output/'results.json',dict(iterations=iterations,rows=rows,summary=summary,references=references))
    print(json.dumps(summary,indent=2))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=experiment.ROOT/'build-milestone/engine-performance-20261026/implementation')
    p.add_argument('--build-dir',type=Path,default=experiment.ROOT/'build-milestone')
    p.add_argument('--compiler',type=Path,help='Actual compiler selected by the native launcher, required when building controls')
    p.add_argument('--repeats',type=int,default=3);p.add_argument('--iterations',type=int,default=16)
    p.add_argument('stage',choices=['build','measure']);a=p.parse_args()
    if not 1<=a.repeats<=5 or not 1<=a.iterations<=64:raise ValueError('Experiment bounds exceeded')
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if a.stage=='build':
        if a.compiler is None:raise ValueError('Pass --compiler after inspecting the actual native compiler launcher')
        # clang++ can be a symlink to clang. Preserve the invocation name:
        # resolving it changes driver mode and omits the C++ runtime at link.
        build(output,a.build_dir.resolve(),a.compiler.absolute())
    else:measure(output,a.repeats,a.iterations)

if __name__=='__main__':main()
