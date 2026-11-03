#!/usr/bin/env python3
"""Exact-repeat memoization of stateless GateTwin calls, with owned caches."""
import argparse
import json
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import experiment as e
import wire_timing_experiment as w

WRAPPER = r'''
#ifdef ENGINE_MEMO_DIAGNOSTICS
struct MemoStats {
    uint64_t calls=0,hits=0,stateful=0;
    ~MemoStats(){std::fprintf(stderr,"ENGINE_MEMO {\"calls\":%llu,\"hits\":%llu,\"statefulBypass\":%llu}\n",
        (unsigned long long)calls,(unsigned long long)hits,(unsigned long long)stateful);}
};
MemoStats memoStats;
#define MEMO_COUNT(member) (++memoStats.member)
#else
#define MEMO_COUNT(member) ((void)0)
#endif
class MemoGateTwin final : public circuit::DelayTwin {
    std::shared_ptr<const GateTwin> inner_;
    bool enabled_;
    struct MemoState final : State {
        bool valid=false,answered=false;
        long long now=0;
        std::vector<std::vector<LogicValue>> inputs;
        std::vector<std::vector<long long>> changed;
        std::vector<long long> settled;
        std::vector<std::vector<uint8_t>> outputs;
    };
public:
    explicit MemoGateTwin(std::shared_ptr<const GateTwin> inner)
        :inner_(std::move(inner)),enabled_(inner_->flops.empty()&&inner_->clock_input<0){
        inputs=inner_->inputs;outputs=inner_->outputs;
    }
    std::unique_ptr<State> start()const override {
        if(!enabled_)return inner_->start();
        return std::make_unique<MemoState>();
    }
    bool settle(State* state,long long now,const std::vector<std::vector<LogicValue>>& values,
                const std::vector<std::vector<long long>>& changed,std::vector<long long>& settled,
                std::vector<std::vector<uint8_t>>& result)const override {
        MEMO_COUNT(calls);
        if(!enabled_){MEMO_COUNT(stateful);return inner_->settle(state,now,values,changed,settled,result);}
        if(!state)return inner_->settle(nullptr,now,values,changed,settled,result);
        auto& memo=*static_cast<MemoState*>(state);
        if(memo.valid&&memo.now==now&&memo.inputs==values&&memo.changed==changed){
            MEMO_COUNT(hits);settled=memo.settled;result=memo.outputs;return memo.answered;
        }
        const bool answered=inner_->settle(nullptr,now,values,changed,settled,result);
        memo.now=now;memo.inputs=values;memo.changed=changed;memo.settled=settled;
        memo.outputs=result;memo.answered=answered;memo.valid=true;return answered;
    }
};
#undef MEMO_COUNT
'''


def build(out,compiler,prefix='memo',wrapper=WRAPPER):
    if (out/(prefix+'-builds.json')).exists():raise RuntimeError('Completed memo controls already exist')
    info=json.loads((out/'builds.json').read_text());snap=out/'snapshot'
    source=(snap/'src/analysis/DelayTwin.cpp').read_text()
    source='#include <cstdio>\n'+source
    source=w.replace(source,'};\n} // namespace\n\nstd::shared_ptr<const circuit::DelayTwin> buildDelayTwin',
        '};\n'+wrapper+'\n} // namespace\n\nstd::shared_ptr<const circuit::DelayTwin> buildDelayTwin')
    source=w.replace(source,'    return twin;','    return std::make_shared<MemoGateTwin>(std::move(twin));')
    source=w.replace(source,'#include "TimingGraph.hpp"','#include "analysis/TimingGraph.hpp"')
    prototype=out/('DelayTwin-'+prefix+'.cpp');prototype.write_text(source)
    builds={}
    for name,level,diagnostic in [(prefix,1,False),(prefix+'-o2',2,False),(prefix+'-diagnostic',1,True)]:
        obj=out/'objects'/('DelayTwin-'+name+'.o')
        flags=[*info['flags'],'-O'+str(level),'-I'+str(snap/'include'),'-I'+str(snap/'src')]
        if diagnostic:flags.append('-DENGINE_MEMO_DIAGNOSTICS')
        e.run(out,'compile-'+name,[compiler,*flags,'-c',prototype,'-o',obj])
        original='baseline-o2' if level==2 else 'baseline'
        objects=[obj if Path(p).name.startswith('DelayTwin-') else Path(p)
                 for p in info['variants'][original]['objects']]
        target=out/name;target.mkdir()
        e.run(out,'link-'+name,[compiler,*objects,out/'objects/execution-cpu.o',out/'objects/driver.o',
              snap/'libSimulatorLib.a','-lz','-o',target/'probe'])
        builds[name]=dict(optimization=level,diagnostic=diagnostic,binarySha256=e.sha(target/'probe'),
                         objects={str(p):e.sha(p) for p in objects})
    e.save(out/(prefix+'-builds.json'),dict(variants=builds,sourceSha256=e.sha(prototype),
        compiler=str(compiler),compilerVersion=subprocess.check_output([compiler,'--version'],text=True)))


def verify(out,compiler,prefix='memo'):
    snap=out/'snapshot';info=json.loads((out/(prefix+'-builds.json')).read_text())
    baseline=json.loads((out/'builds.json').read_text())['variants']['baseline']
    info['variants']['baseline']=baseline
    obj=out/'objects'/('verify-'+prefix+'.o')
    e.run(out,'compile-'+prefix+'-regression',[compiler,'-std=gnu++20','-O1','-g0','-DNDEBUG','-UNDEBUG','-fPIC',
          '-I'+str(snap/'include'),'-I'+str(snap/'src'),'-c',e.ROOT/'tools/engine-performance/MemoTimingProbe.cpp','-o',obj])
    hashes={};rows=[]
    for name,build in info['variants'].items():
        exe=out/name/('verify-'+prefix)
        core=list(build['objects'])
        e.run(out,'link-memo-regression-'+name,[compiler,*core,obj,snap/'libSimulatorLib.a','-lz','-o',exe])
        d=e.run(out,'verify-memo-regression-'+name,[exe]);hashes[name]=e.sha(d/'stdout.txt');rows.append(str(d))
        if len(set(hashes.values()))!=1:raise RuntimeError('Memo timing signature differs: '+str(d))
        if name=='baseline':continue
        for equipment in ['gate','order','trace','runner']:
            target=out/name/('verify-'+equipment)
            e.run(out,'link-memo-'+name+'-'+equipment,[compiler,*core,out/'objects'/('verify-'+equipment+'.o'),
                  snap/'libSimulatorLib.a','-lz','-o',target])
            if equipment=='trace':
                d=e.run(out,'verify-memo-'+name+'-tick-decoder',['node','--max-old-space-size=128',
                    e.ROOT/'tests/checks/TickRecordingTest.mjs',target],address_space_mib=0);rows.append(str(d))
            elif equipment=='runner':
                for scenario in ['RegisterTest','SimulatorUnwiredOutputPinHistoryTest','SimulatorDrainUntilIdleTest','RV32IALUAddSubTest']:
                    d=e.run(out,'verify-memo-'+name+'-'+scenario,[target,scenario]);rows.append(str(d))
            else:
                d=e.run(out,'verify-memo-'+name+'-'+equipment,[target]);rows.append(str(d))
    e.save(out/(prefix+'-verification.json'),dict(checks=len(rows),hashes=hashes,directories=rows))


def measure(out,name,iterations,repeats):
    if(out/(name+'.json')).exists():raise RuntimeError('Completed memo matrix already exists')
    variants=['baseline','memo','baseline-o2','memo-o2'];rows=[];references={}
    for repeat in range(repeats):
        order=variants[repeat%4:]+variants[:repeat%4]
        if repeat%2:order.reverse()
        for mode in ['none','stream']:
            for variant in order:
                label=f'{name}-{repeat}-{mode}-{variant}';args=[out/variant/'probe','guest',str(iterations),mode];trace=None
                if mode=='stream':trace=out/'traces'/label;args.append(trace)
                d=e.run(out,label,args);result=json.loads((d/'stdout.txt').read_text());e.save(d/'result.json',result)
                signature=w.signature(result,trace)
                if mode in references and signature!=references[mode]:
                    e.save(d/'mismatch.json',dict(expected=references[mode],actual=signature));raise RuntimeError('Memo semantic mismatch')
                references.setdefault(mode,signature)
                stderr=(d/'stderr.txt').read_text()
                rows.append(dict(repeat=repeat,mode=mode,variant=variant,
                    cpuSeconds=float(re.search(r'ENGINE_EXEC_CPU ([0-9.]+)',stderr)[1]),hostSeconds=result['hostSeconds'],directory=str(d)))
                e.save(out/(name+'.partial.json'),rows);print(variant,mode,rows[-1]['cpuSeconds'],flush=True)
    summary=[]
    for mode in ['none','stream']:
        for variant in variants:
            values=[r for r in rows if r['mode']==mode and r['variant']==variant];cpu=[r['cpuSeconds'] for r in values]
            summary.append(dict(variant=variant,mode=mode,medianCpuSeconds=statistics.median(cpu),minCpuSeconds=min(cpu),
                                maxCpuSeconds=max(cpu),medianHostSeconds=statistics.median(r['hostSeconds'] for r in values)))
    e.save(out/(name+'.json'),dict(iterations=iterations,repeats=repeats,rows=rows,summary=summary,references=references))
    print(json.dumps(summary,indent=2))


def diagnostics(out,prefix='memo'):
    if(out/(prefix+'-diagnostics.json')).exists():raise RuntimeError('Completed diagnostics already exist')
    rows=[];golden=json.loads((out/'allocation-calibration.json').read_text())['references']
    for mode in ['none','stream']:
        for variant in ['baseline',prefix,prefix+'-o2',prefix+'-diagnostic']:
            label=prefix+'-alloc-'+mode+'-'+variant;args=[out/variant/'probe','guest','4',mode];trace=None
            if mode=='stream':trace=out/'traces'/label;args.append(trace)
            d=e.run(out,label,args,extra_env={'LD_PRELOAD':str(out/'allocations.so')})
            result=json.loads((d/'stdout.txt').read_text());e.save(d/'result.json',result)
            if w.signature(result,trace)!=golden[mode]:raise RuntimeError('Memo diagnostic differs from golden: '+str(d))
            stderr=(d/'stderr.txt').read_text();row=dict(mode=mode,variant=variant,directory=str(d),
                allocations=json.loads(re.search(r'ENGINE_ALLOC (\{.*\})',stderr)[1]))
            match=re.search(r'ENGINE_MEMO (\{.*\})',stderr)
            if match:row['wholeProcessMemo']=json.loads(match[1])
            rows.append(row);e.save(out/(prefix+'-diagnostics.partial.json'),rows);print(json.dumps(row),flush=True)
    e.save(out/(prefix+'-diagnostics.json'),dict(rows=rows,allGoldenSignaturesMatch=True,
        timingUse='Allocation/stat counters are diagnostic; do not use these durations for throughput'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=e.ROOT/'build-milestone/engine-performance-20261026/wire-timing')
    parser.add_argument('--compiler',type=Path,default=Path('/home/arnold/.cache/circuitsim-emsdk/upstream/bin/clang++'))
    parser.add_argument('--iterations',type=int,default=16);parser.add_argument('--repeats',type=int,default=3)
    parser.add_argument('--name',default='memo-throughput');parser.add_argument('stage',choices=['build','verify','measure','diagnostics'])
    a=parser.parse_args();out=a.output.resolve()
    if not 1<=a.iterations<=64 or not 1<=a.repeats<=5:raise ValueError('Experiment bounds exceeded')
    if a.stage=='build':build(out,a.compiler.absolute())
    elif a.stage=='verify':verify(out,a.compiler.absolute())
    elif a.stage=='diagnostics':diagnostics(out)
    else:measure(out,a.name,a.iterations,a.repeats)


if __name__=='__main__':main()
