#!/usr/bin/env python3
"""Private owned-buffer and dirty-node GateTwin experiments."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import experiment as e
from wire_timing_experiment import replace, signature

STATS = r'''
#ifdef ENGINE_INCREMENTAL_DIAGNOSTICS
struct IncrementalStats {
    uint64_t calls=0,early=0,incrementalCalls=0,fullCalls=0;
    uint64_t visited=0,evaluated=0,skipped=0,csrBytes=0,dirtyBytes=0;
    ~IncrementalStats(){std::fprintf(stderr,
        R"(ENGINE_INCREMENTAL {"calls":%llu,"early":%llu,"incrementalCalls":%llu,"fullCalls":%llu,"visited":%llu,"evaluated":%llu,"skipped":%llu,"csrBytes":%llu,"dirtyBytes":%llu})" "\n",
        (unsigned long long)calls,(unsigned long long)early,(unsigned long long)incrementalCalls,
        (unsigned long long)fullCalls,(unsigned long long)visited,(unsigned long long)evaluated,
        (unsigned long long)skipped,(unsigned long long)csrBytes,(unsigned long long)dirtyBytes);}
};
IncrementalStats incrementalStats;
#define INC_ADD(field,amount) (incrementalStats.field+=(amount))
#else
#define INC_ADD(field,amount) ((void)0)
#endif
'''

PREPARE = '''    std::vector<uint32_t> fanout_offsets_,fanout_nodes_;
    bool incremental_ready_=false;
    void prepareIncremental() {
#ifdef ENGINE_INCREMENTAL
        // Leaves retain the original whole-list evaluation contract.
        if(flops.empty()||!leaves.empty())return;
        fanout_offsets_.assign(gates.size()+1,0);
        for(int node:order){
            const auto& gate=gates[node];
            if(gate.a>=0)++fanout_offsets_[gate.a+1];
            if(gate.b>=0&&gate.b!=gate.a)++fanout_offsets_[gate.b+1];
        }
        for(size_t i=1;i<fanout_offsets_.size();++i)fanout_offsets_[i]+=fanout_offsets_[i-1];
        fanout_nodes_.resize(fanout_offsets_.back());auto cursor=fanout_offsets_;
        for(int node:order){
            const auto& gate=gates[node];
            if(gate.a>=0)fanout_nodes_[cursor[gate.a]++]=node;
            if(gate.b>=0&&gate.b!=gate.a)fanout_nodes_[cursor[gate.b]++]=node;
        }
        incremental_ready_=true;
        INC_ADD(csrBytes,(fanout_offsets_.capacity()+fanout_nodes_.capacity())*sizeof(uint32_t));
#endif
    }
'''


def prototype(source):
    source='#include <cstdio>\n'+source
    source=replace(source,'#include "TimingGraph.hpp"','#include "analysis/TimingGraph.hpp"')
    source=replace(source,'class GateTwin final : public circuit::DelayTwin {',STATS+'\nclass GateTwin final : public circuit::DelayTwin {')
    source=replace(source,'        std::vector<uint8_t> value, held;',
        '        std::vector<uint8_t> dirty;\n        std::vector<uint8_t> value, held;')
    source=replace(source,'    int clock_input = -1;','    int clock_input = -1;\n'+PREPARE)
    source=replace(source,'''        thread_local std::vector<uint8_t> value;
        thread_local std::vector<Tick> time;''','''        INC_ADD(calls,1);
        thread_local std::vector<uint8_t> full_value;
        thread_local std::vector<Tick> full_time;''')
    source=replace(source,'        auto* memory = static_cast<Memory*>(state);','''        auto* memory = static_cast<Memory*>(state);
        auto& value=memory?memory->value:full_value;
        auto& time=memory?memory->time:full_time;
#ifdef ENGINE_INCREMENTAL
        const bool incremental=memory&&memory->started&&incremental_ready_;
#else
        constexpr bool incremental=false;
#endif''')
    source=replace(source,'if (alone) { memory->clock = clock; return memory->answered; }',
        'if (alone) { INC_ADD(early,1); memory->clock = clock; return memory->answered; }')
    source=replace(source,'''        value.resize(gates.size());
        time.resize(gates.size());
        for (size_t node = 0; node < gates.size(); ++node) { value[node] = gates[node].constant; time[node] = NEVER; }''','''        if(!incremental){
            INC_ADD(fullCalls,1);
            value.resize(gates.size());time.resize(gates.size());
            for(size_t node=0;node<gates.size();++node){value[node]=gates[node].constant;time[node]=NEVER;}
            if(memory&&incremental_ready_){memory->dirty.assign(gates.size(),0);INC_ADD(dirtyBytes,gates.size());}
        }else INC_ADD(incrementalCalls,1);
        const auto notify=[&](int node){
            if(incremental)for(uint32_t at=fanout_offsets_[node];at<fanout_offsets_[node+1];++at)
                memory->dirty[fanout_nodes_[at]]=1;
        };
        const auto assign=[&](int node,uint8_t next,Tick at){
            const bool changed=value[node]!=next||time[node]!=at;
            value[node]=next;time[node]=at;if(changed)notify(node);
        };''')
    source=replace(source,'''            value[node] = code(values[pin][bit]);
            time[node] = changed[pin][bit];''','''            assign(node,code(values[pin][bit]),changed[pin][bit]);''')
    source=replace(source,'''            if (f.q >= 0) { value[f.q] = held; time[f.q] = memory->held_since[flop]; }
            if (f.q_bar >= 0) { value[f.q_bar] = held > 1 ? UNKNOWN : 1 - held; time[f.q_bar] = memory->held_since[flop]; }''','''            if(f.q>=0)assign(f.q,held,memory->held_since[flop]);
            if(f.q_bar>=0)assign(f.q_bar,held>1?UNKNOWN:1-held,memory->held_since[flop]);''')
    source=replace(source,'''        for (const int node : order) {
            const auto& gate = gates[node];''','''        for (const int node : order) {
            INC_ADD(visited,1);
            if(incremental){
                if(!memory->dirty[node]){INC_ADD(skipped,1);continue;}
                memory->dirty[node]=0;
            }
            INC_ADD(evaluated,1);
            const auto previous=incremental?value[node]:uint8_t(0);
            const auto previous_time=incremental?time[node]:NEVER;
            const auto& gate = gates[node];''')
    source=replace(source,'''        }
        if (memory) {
            // A reset in progress''','''            if(incremental&&(value[node]!=previous||time[node]!=previous_time))notify(node);
        }
        if (memory) {
            // A reset in progress''')
    source=replace(source,'''            memory->value = value;
            memory->time = time;
''','')
    source=replace(source,'    return twin;','    twin->prepareIncremental();\n    return twin;')
    return source


def setup(out,compiler):
    if(out/'builds.json').exists():raise RuntimeError('Completed incremental controls already exist')
    parent=out.parent;snap=parent/'snapshot';info=json.loads((parent/'builds.json').read_text())
    if e.sha(e.ROOT/'build-milestone/libSimulatorLib.a')!=info['archiveSha256']:raise RuntimeError('Baseline archive changed')
    for directory in ['include','src']:
        for p in (snap/directory).rglob('*'):
            if p.is_file() and e.sha(p)!=e.sha(e.ROOT/p.relative_to(snap)):raise RuntimeError('Baseline source changed: '+str(p))
    for name in ['baseline','baseline-o2']:
        if not(out/name).exists():(out/name).symlink_to(parent/name,target_is_directory=True)
    for name in ['allocations.so','allocation-calibration.json']:
        if not(out/name).exists():(out/name).symlink_to(parent/name)
    source=out/'DelayTwin-incremental.cpp';source.write_text(prototype((snap/'src/analysis/DelayTwin.cpp').read_text()))
    builds={};(out/'objects').mkdir(exist_ok=True)
    for name,level,incremental,diagnostic in [('inplace',1,False,False),('incremental',1,True,False),
        ('incremental-o2',2,True,False),('incremental-diagnostic',1,True,True)]:
        obj=out/'objects'/(name+'.o')
        flags=[*info['flags'],'-O'+str(level),'-I'+str(snap/'include'),'-I'+str(snap/'src')]
        if incremental:flags.append('-DENGINE_INCREMENTAL')
        if diagnostic:flags.append('-DENGINE_INCREMENTAL_DIAGNOSTICS')
        e.run(out,'compile-'+name,[compiler,*flags,'-c',source,'-o',obj])
        base='baseline-o2' if level==2 else 'baseline'
        objects=[obj if Path(p).name.startswith('DelayTwin-')else Path(p) for p in info['variants'][base]['objects']]
        target=out/name;target.mkdir()
        e.run(out,'link-'+name,[compiler,*objects,parent/'objects/execution-cpu.o',parent/'objects/driver.o',
            snap/'libSimulatorLib.a','-lz','-o',target/'probe'])
        builds[name]=dict(objects={str(p):e.sha(p) for p in objects},binarySha256=e.sha(target/'probe'),
                         optimization=level,incremental=incremental,diagnostic=diagnostic)
    e.save(out/'builds.json',dict(variants=builds,sourceSha256=e.sha(source),archiveSha256=info['archiveSha256'],
        compiler=str(compiler),compilerVersion=subprocess.check_output([compiler,'--version'],text=True)))


def bitset_source(source):
    source='#include <bit>\n'+source
    source=replace(source,'        std::vector<uint8_t> dirty;','        std::vector<uint64_t> dirty_words;')
    source=replace(source,'''        for(int node:order){
            const auto& gate=gates[node];
            if(gate.a>=0)fanout_nodes_[cursor[gate.a]++]=node;
            if(gate.b>=0&&gate.b!=gate.a)fanout_nodes_[cursor[gate.b]++]=node;
        }''','''        for(uint32_t rank=0;rank<order.size();++rank){
            const auto& gate=gates[order[rank]];
            if(gate.a>=0)fanout_nodes_[cursor[gate.a]++]=rank;
            if(gate.b>=0&&gate.b!=gate.a)fanout_nodes_[cursor[gate.b]++]=rank;
        }''')
    source=replace(source,'memory->dirty.assign(gates.size(),0);INC_ADD(dirtyBytes,gates.size());',
        'memory->dirty_words.assign((order.size()+63)/64,0);INC_ADD(dirtyBytes,memory->dirty_words.size()*sizeof(uint64_t));')
    source=replace(source,'''            if(incremental)for(uint32_t at=fanout_offsets_[node];at<fanout_offsets_[node+1];++at)
                memory->dirty[fanout_nodes_[at]]=1;''','''            if(incremental)for(uint32_t at=fanout_offsets_[node];at<fanout_offsets_[node+1];++at){
                const auto rank=fanout_nodes_[at];
                memory->dirty_words[rank/64]|=uint64_t(1)<<(rank%64);
            }''')
    source=replace(source,'''        for (const int node : order) {
            INC_ADD(visited,1);
            if(incremental){
                if(!memory->dirty[node]){INC_ADD(skipped,1);continue;}
                memory->dirty[node]=0;
            }
            INC_ADD(evaluated,1);''','''        size_t position=0,evaluated_count=0;
        INC_ADD(visited,order.size());
        while(true){
            int node;
            if(incremental){
                while(position<memory->dirty_words.size()&&!memory->dirty_words[position])++position;
                if(position==memory->dirty_words.size())break;
                const auto bit=std::countr_zero(memory->dirty_words[position]);
                memory->dirty_words[position]&=memory->dirty_words[position]-1;
                node=order[position*64+bit];
            }else{
                if(position==order.size())break;
                node=order[position++];
            }
            ++evaluated_count;INC_ADD(evaluated,1);''')
    source=replace(source,'''        if (memory) {
            // A reset in progress''','''        INC_ADD(skipped,order.size()-evaluated_count);
        if (memory) {
            // A reset in progress''')
    return source


def build_bits(out,compiler):
    if(out/'bitset-builds.json').exists():raise RuntimeError('Completed bitset controls already exist')
    parent=out.parent;snap=parent/'snapshot';info=json.loads((parent/'builds.json').read_text())
    source=out/'DelayTwin-bitset.cpp';source.write_text(bitset_source((out/'DelayTwin-incremental.cpp').read_text()))
    builds={}
    for name,level,diagnostic in [('bitset',1,False),('bitset-o2',2,False),('bitset-diagnostic',1,True)]:
        obj=out/'objects'/(name+'.o')
        flags=[*info['flags'],'-O'+str(level),'-DENGINE_INCREMENTAL','-I'+str(snap/'include'),'-I'+str(snap/'src')]
        if diagnostic:flags.append('-DENGINE_INCREMENTAL_DIAGNOSTICS')
        e.run(out,'compile-'+name,[compiler,*flags,'-c',source,'-o',obj])
        base='baseline-o2' if level==2 else 'baseline'
        objects=[obj if Path(p).name.startswith('DelayTwin-')else Path(p) for p in info['variants'][base]['objects']]
        target=out/name;target.mkdir()
        e.run(out,'link-'+name,[compiler,*objects,parent/'objects/execution-cpu.o',parent/'objects/driver.o',
            snap/'libSimulatorLib.a','-lz','-o',target/'probe'])
        builds[name]=dict(objects={str(p):e.sha(p) for p in objects},binarySha256=e.sha(target/'probe'),optimization=level)
    e.save(out/'bitset-builds.json',dict(variants=builds,sourceSha256=e.sha(source)))


def verify(out,compiler,bits=False):
    prefix='bitset-' if bits else ''
    if(out/(prefix+'verification.json')).exists():raise RuntimeError('Completed verification already exists')
    parent=out.parent;snap=parent/'snapshot';info=json.loads((out/('bitset-builds.json' if bits else 'builds.json')).read_text())
    baseline=json.loads((parent/'builds.json').read_text())['variants']['baseline']
    info['variants']={'baseline':baseline,**info['variants']}
    driver=out/'objects/verify-incremental.o'
    e.run(out,'compile-incremental-probe',[compiler,'-std=gnu++20','-O1','-g0','-DNDEBUG','-UNDEBUG','-fPIC',
        '-I'+str(snap/'include'),'-I'+str(snap/'src'),'-c',e.ROOT/'tools/engine-performance/IncrementalTimingProbe.cpp','-o',driver])
    hashes={};rows=[]
    for name,config in info['variants'].items():
        core=list(config['objects']);target=out/(name if name!='baseline'else 'control');target.mkdir(exist_ok=True)
        exe=target/'verify-incremental'
        e.run(out,'link-probe-'+name,[compiler,*core,driver,snap/'libSimulatorLib.a','-lz','-o',exe])
        d=e.run(out,'verify-probe-'+name,[exe]);hashes[name]=e.sha(d/'stdout.txt');rows.append(str(d))
        if len(set(hashes.values()))!=1:raise RuntimeError('Incremental timing signature differs at '+str(d))
        if name=='baseline':continue
        for check in ['gate','order','trace','runner']:
            exe=target/('verify-'+check)
            e.run(out,'link-'+check+'-'+name,[compiler,*core,parent/'objects'/('verify-'+check+'.o'),snap/'libSimulatorLib.a','-lz','-o',exe])
            if check=='trace':
                d=e.run(out,'verify-decoder-'+name,['node','--max-old-space-size=128',e.ROOT/'tests/checks/TickRecordingTest.mjs',exe],address_space_mib=0);rows.append(str(d))
            elif check=='runner':
                for scenario in ['RegisterTest','SimulatorUnwiredOutputPinHistoryTest','SimulatorDrainUntilIdleTest','RV32IALUAddSubTest']:
                    d=e.run(out,'verify-'+name+'-'+scenario,[exe,scenario]);rows.append(str(d))
            else:
                d=e.run(out,'verify-'+check+'-'+name,[exe]);rows.append(str(d))
    e.save(out/(prefix+'verification.json'),dict(checks=len(rows),exactProbeHashes=hashes,directories=rows))


def diagnostics(out,bits=False):
    prefix='bitset-' if bits else ''
    if(out/(prefix+'diagnostics.json')).exists():raise RuntimeError('Completed diagnostics already exist')
    golden=json.loads((out/'allocation-calibration.json').read_text())['references'];rows=[]
    for mode in ['none','stream']:
        for name in (['baseline','bitset','bitset-o2','bitset-diagnostic'] if bits else ['baseline','inplace','incremental','incremental-o2','incremental-diagnostic']):
            label=prefix+'diagnostic-'+mode+'-'+name;args=[out/name/'probe','guest','4',mode];trace=None
            if mode=='stream':trace=out/'traces'/label;trace.parent.mkdir(exist_ok=True);args.append(trace)
            d=e.run(out,label,args,extra_env={'LD_PRELOAD':str(out/'allocations.so')})
            result=json.loads((d/'stdout.txt').read_text());e.save(d/'result.json',result)
            observed=signature(result,trace)
            if observed!=golden[mode]:
                e.save(d/'mismatch.json',dict(expected=golden[mode],actual=observed));raise RuntimeError('Incremental diagnostic differs from golden')
            stderr=(d/'stderr.txt').read_text();row=dict(variant=name,mode=mode,directory=str(d),
                allocations=json.loads(re.search(r'ENGINE_ALLOC (\{.*\})',stderr)[1]))
            m=re.search(r'ENGINE_INCREMENTAL (\{.*\})',stderr)
            if m:row['wholeProcessNodes']=json.loads(m[1])
            rows.append(row);e.save(out/(prefix+'diagnostics.partial.json'),rows);print(json.dumps(row),flush=True)
    e.save(out/(prefix+'diagnostics.json'),dict(rows=rows,allGoldenSignaturesMatch=True,
        note='Node counters cover the process; allocation counters cover execution only. Durations are not throughput.'))


def kernels(out,compiler):
    if(out/'kernels.json').exists():raise RuntimeError('Completed kernel comparisons already exist')
    parent=out.parent;snap=parent/'snapshot';base=json.loads((parent/'builds.json').read_text())
    variants={'control':base['variants']['baseline']}
    variants.update({k:v for k,v in json.loads((out/'builds.json').read_text())['variants'].items() if k in ('inplace','incremental')})
    variants['bitset']=json.loads((out/'bitset-builds.json').read_text())['variants']['bitset']
    reference=(snap/'src/analysis/DelayTwin.cpp').read_text()
    reference=replace(reference,'#include "TimingGraph.hpp"','#include "analysis/TimingGraph.hpp"')
    reference=replace(reference,'namespace circuit::analysis {','namespace circuit::analysis::reference {')
    ref_source=out/'DelayTwin-reference.cpp';ref_source.write_text(reference)
    common=[*base['flags'],'-O1','-I'+str(snap/'include'),'-I'+str(snap/'src')]
    ref_obj=out/'objects/reference.o';driver=out/'objects/kernel-driver.o'
    e.run(out,'compile-kernel-reference',[compiler,*common,'-c',ref_source,'-o',ref_obj])
    e.run(out,'compile-kernel-driver',[compiler,*common,'-c',e.ROOT/'tools/engine-performance/TimingKernelCompare.cpp','-o',driver])
    rows=[]
    for name,config in variants.items():
        target=out/('kernel-'+name)
        e.run(out,'link-kernel-'+name,[compiler,*config['objects'],ref_obj,driver,snap/'libSimulatorLib.a','-lz','-o',target])
        d=e.run(out,'measure-kernel-'+name,['taskset','-c','3',target])
        values=[json.loads(line) for line in (d/'stdout.txt').read_text().splitlines()]
        rows.append(dict(variant=name,directory=str(d),results=values))
        e.save(out/'kernels.partial.json',rows)
        print(name,[(v['profile'],v['width'],v['activity'],v['cpuRatio']) for v in values],flush=True)
    e.save(out/'kernels.json',dict(rows=rows,
        protocol='Same process, 32 alternating ABBA/BAAB chunks of 256 calls; every case validated before timing; chunk endpoints compared',
        note='Timing-model microbenchmarks, not full-engine or game throughput'))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=e.ROOT/'build-milestone/engine-performance-20261026/wire-timing/incremental')
    p.add_argument('--compiler',type=Path,default=Path('/home/arnold/.cache/circuitsim-emsdk/upstream/bin/clang++'))
    p.add_argument('stage',choices=['build','verify','diagnostics','build-bits','verify-bits','diagnose-bits','kernels']);a=p.parse_args();out=a.output.resolve();out.mkdir(exist_ok=True)
    if a.stage=='build':setup(out,a.compiler.absolute())
    elif a.stage=='verify':verify(out,a.compiler.absolute())
    elif a.stage=='diagnostics':diagnostics(out)
    elif a.stage=='build-bits':build_bits(out,a.compiler.absolute())
    elif a.stage=='verify-bits':verify(out,a.compiler.absolute(),True)
    elif a.stage=='diagnose-bits':diagnostics(out,True)
    else:kernels(out,a.compiler.absolute())


if __name__=='__main__':main()
