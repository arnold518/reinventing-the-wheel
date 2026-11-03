#!/usr/bin/env python3
"""Container review against the frozen engine baseline, with all attempts logged."""
import argparse
import json
from pathlib import Path
import re
import shutil
import statistics
import experiment

def metric(result,name):
    return next(x['value'] for x in result['metrics'] if x['name']==name)

def build_headless(output,parent):
    snap=parent/'snapshot';directory=output/'headless-reuse'
    if directory.exists():raise RuntimeError('Headless variant already exists')
    directory.mkdir()
    source=(snap/'src/simulator/Simulator.cpp').read_text()
    old='''            state.clear();
            state.push_back({time, value});'''
    new='''            if(state.empty())state.push_back({time,value});
            else { state.back().first=time; state.back().second=value; }'''
    if source.count(old)!=1:raise RuntimeError('Headless state context changed')
    source_path=directory/'Simulator.cpp';source_path.write_text(source.replace(old,new))
    obj=directory/'Simulator.o'
    experiment.run(output,'headless-reuse-compile',['/usr/bin/c++','-std=c++20','-O1','-g0','-UNDEBUG',
        '-I'+str(snap/'include'),'-I'+str(snap/'src'),'-c',source_path,'-o',obj])
    binary=directory/'probe'
    experiment.run(output,'headless-reuse-link',['/usr/bin/c++',obj,parent/'execution-cpu.o',parent/'baseline-cpu/driver.o',snap/'libSimulatorLib.a','-lz','-o',binary])
    builds=json.loads((output/'builds.json').read_text())
    builds['headless-reuse']=dict(binarySha256=experiment.sha(binary),objects=[str(obj)],sourceHashes={source_path.name:experiment.sha(source_path)})
    experiment.save(output/'builds.json',builds)

def build_pool(output,parent):
    snap=parent/'snapshot';directory=output/'trace-pool'
    if directory.exists():raise RuntimeError('Pool variant already exists')
    directory.mkdir()
    shutil.copy2(snap/'probe.cpp',directory/'probe.cpp')
    header=(snap/'TickTraceWriter.hpp').read_text()
    header='#include <memory_resource>\n'+header
    header=header.replace('std::map<size_t,uint64_t>','std::pmr::map<size_t,uint64_t>')
    header=header.replace('auto& changes=pending_[time];','auto& changes=pending_.try_emplace(time,&pending_pool_).first->second;')
    marker='    std::map<SimulationTime,std::pmr::map<size_t,uint64_t>> pending_;'
    if header.count(marker)!=1:raise RuntimeError('Pool declaration context changed')
    header=header.replace(marker,'    std::pmr::unsynchronized_pool_resource pending_pool_{std::pmr::pool_options{256,128}};\n'+marker)
    (directory/'TickTraceWriter.hpp').write_text(header)
    flags=['-std=c++20','-O1','-g0','-UNDEBUG','-I'+str(directory),'-I'+str(snap/'include'),'-I'+str(snap/'src')]
    driver=directory/'driver.o'
    experiment.run(output,'trace-pool-compile-driver',['/usr/bin/c++',*flags,'-c',directory/'probe.cpp','-o',driver])
    binary=directory/'probe'
    experiment.run(output,'trace-pool-link',['/usr/bin/c++',parent/'execution-cpu.o',driver,snap/'libSimulatorLib.a','-lz','-o',binary])
    experiment.run(output,'compile-pool-container-probe',['/usr/bin/c++','-std=c++20','-O2','-g0','-I'+str(snap/'include'),
        experiment.ROOT/'tools/engine-performance/STLContainerProbe.cpp',snap/'libSimulatorLib.a','-o',output/'containers-pool'])
    builds=json.loads((output/'builds.json').read_text())
    builds['trace-pool']=dict(binarySha256=experiment.sha(binary),objects=[],sourceHashes={p.name:experiment.sha(p) for p in directory.iterdir() if p.suffix in ('.cpp','.hpp')})
    experiment.save(output/'builds.json',builds)

def build(output,parent):
    if (output/'builds.json').exists():raise RuntimeError('Completed builds already exist')
    snap=parent/'snapshot';includes=['-I'+str(snap/'include'),'-I'+str(snap/'src')]
    flags=['-std=c++20','-O1','-g0','-UNDEBUG']
    builds={}
    experiment.run(output,'compile-container-probe',['/usr/bin/c++','-std=c++20','-O2','-g0',*includes,
        experiment.ROOT/'tools/engine-performance/STLContainerProbe.cpp',snap/'libSimulatorLib.a','-o',output/'containers'])
    for name in ['output-linear','byte-flags','trace-flat','trace-objects']:
        directory=output/name;directory.mkdir()
        objects=[]
        shutil.copy2(snap/'probe.cpp',directory/'probe.cpp')
        header=(snap/'TickTraceWriter.hpp').read_text()
        if name in ('output-linear','byte-flags'):
            source=(snap/'src/components/BasicComponent.cpp').read_text()
            if name=='output-linear':
                helper='''
namespace {
class SmallOutputIndex {
    std::vector<std::pair<std::string,size_t>> values;
public:
    size_t size() const { return values.size(); }
    auto end() { return values.end(); }
    auto end() const { return values.end(); }
    auto find(const std::string& key) {
        return std::find_if(values.begin(),values.end(),[&](const auto& p){return p.first==key;});
    }
    auto find(const std::string& key) const {
        return std::find_if(values.begin(),values.end(),[&](const auto& p){return p.first==key;});
    }
    void emplace(const std::string& key,size_t value) {
        if(find(key)==end())values.emplace_back(key,value);
    }
};
}
'''
                source=source.replace('// The delay table, prepared for use while simulating.',helper+'\n// The delay table, prepared for use while simulating.',1)
                source=source.replace('std::unordered_map<std::string, size_t> output_index;','SmallOutputIndex output_index;')
            else:
                source='#include <cstdint>\n'+source.replace('std::vector<bool>','std::vector<uint8_t>')
            source_path=directory/'BasicComponent.cpp';source_path.write_text(source)
            obj=directory/'BasicComponent.o'
            experiment.run(output,name+'-compile',['/usr/bin/c++',*flags,*includes,'-c',source_path,'-o',obj])
            objects.append(obj)
        elif name=='trace-flat':
            old='''        if(!changes.contains(index)&&++pending_values_>2*1024*1024)throw std::length_error("Pending tick trace exceeds memory budget");
        changes[index]=bits;'''
            new='''        auto it=std::lower_bound(changes.begin(),changes.end(),index,[](const auto& entry,size_t key){return entry.first<key;});
        if(it!=changes.end()&&it->first==index)it->second=bits;
        else {
            if(++pending_values_>2*1024*1024)throw std::length_error("Pending tick trace exceeds memory budget");
            changes.insert(it,{index,bits});
        }'''
            if header.count(old)!=1:raise RuntimeError('Trace queue context changed')
            header='#include <algorithm>\n'+header.replace(old,new)
            header=header.replace('const std::map<size_t,uint64_t>& changes','const std::vector<std::pair<size_t,uint64_t>>& changes')
            header=header.replace('std::map<SimulationTime,std::map<size_t,uint64_t>> pending_;','std::map<SimulationTime,std::vector<std::pair<size_t,uint64_t>>> pending_;')
        else:
            header=header.replace('registers_[component->getID()]','registers_[component]')
            header=header.replace('registers_.find(owner->getID())','registers_.find(owner)')
            header=header.replace('std::map<std::string,size_t> registers_;','std::unordered_map<std::shared_ptr<Component>,size_t> registers_;')
        (directory/'TickTraceWriter.hpp').write_text(header)
        driver=directory/'driver.o'
        experiment.run(output,name+'-compile-driver',['/usr/bin/c++',*flags,'-I'+str(directory),*includes,'-c',directory/'probe.cpp','-o',driver])
        binary=directory/'probe'
        experiment.run(output,name+'-link',['/usr/bin/c++',*objects,parent/'execution-cpu.o',driver,snap/'libSimulatorLib.a','-lz','-o',binary])
        builds[name]=dict(binarySha256=experiment.sha(binary),objects=[str(x) for x in objects],
            sourceHashes={p.name:experiment.sha(p) for p in directory.iterdir() if p.suffix in ('.cpp','.hpp')})
    experiment.save(output/'builds.json',builds)
    build_headless(output,parent)
    build_pool(output,parent)

def pool_micro(output,repeats):
    if (output/'pool-containers.json').exists():raise RuntimeError('Completed pooled measurements already exist')
    results=[]
    for repeat in range(repeats):
        for size in [8,64,512,4096]:
            for order in ['random','ascending']:
                for variant in (['map','pool-map'] if repeat%2==0 else ['pool-map','map']):
                    label=f'pool-{repeat}-{size}-{order}-{variant}'
                    d=experiment.run(output,label,[output/'containers-pool','changes',variant,str(size),str(max(32,524288//size)),order])
                    result=json.loads((d/'stdout.txt').read_text());experiment.save(d/'result.json',result)
                    results.append(dict(repeat=repeat,size=size,order=order,variant=variant,cpuSeconds=metric(result,'cpu_seconds'),hostSeconds=result['hostSeconds'],directory=str(d)))
                    experiment.save(output/'pool-containers.partial.json',results)
    experiment.save(output/'pool-containers.json',results)

def build_pic(output,parent):
    snap=parent/'snapshot';root=output/'pic-control'
    if root.exists():raise RuntimeError('PIC controls already exist')
    root.mkdir()
    flags=['-std=gnu++20','-O1','-g0','-DNDEBUG','-fPIC','-UNDEBUG','-I'+str(snap/'include'),'-I'+str(snap/'src')]
    objects={}
    sources={'baseline-basic':snap/'src/components/BasicComponent.cpp','baseline-sim':snap/'src/simulator/Simulator.cpp',
             'output-linear':output/'output-linear/BasicComponent.cpp','byte-flags':output/'byte-flags/BasicComponent.cpp',
             'headless-reuse':output/'headless-reuse/Simulator.cpp'}
    for name,source in sources.items():
        obj=root/(name+'.o');objects[name]=obj
        experiment.run(output,'pic-compile-'+name,['/usr/bin/c++',*flags,'-c',source,'-o',obj])
    builds={}
    for name in ['baseline','output-linear','byte-flags','headless-reuse','trace-flat','trace-objects','trace-pool']:
        directory=root/name;directory.mkdir()
        basic=objects[name] if name in ('output-linear','byte-flags') else objects['baseline-basic']
        sim=objects['headless-reuse'] if name=='headless-reuse' else objects['baseline-sim']
        driver=output/name/'driver.o' if name.startswith('trace-') else parent/'baseline-cpu/driver.o'
        binary=directory/'probe'
        experiment.run(output,'pic-link-'+name,['/usr/bin/c++',basic,sim,parent/'execution-cpu.o',driver,snap/'libSimulatorLib.a','-lz','-o',binary])
        builds[name]=dict(binarySha256=experiment.sha(binary),basicObject=str(basic),simulatorObject=str(sim),
                          driverObject=str(driver),coreCompileFlags=flags)
    experiment.save(root/'builds.json',builds)

def micro(output,repeats):
    if (output/'containers.json').exists():raise RuntimeError('Completed container measurements already exist')
    cases=[]
    for size in [2,8,32,128,1024]:
        cases.append(('lookup',size,1000000 if size<1024 else 200000,'random',['map','hash','flat','linear','indexed']))
    for size in [8,64,512,4096]:
        for order in ['random','ascending']:
            cases.append(('changes',size,max(32,524288//size),order,['map','hash-sort','flat','append-sort']))
    for size in [32,256,4096]:cases.append(('flags',size,10000000,'random',['bits','bytes']))
    results=[]
    for repeat in range(repeats):
        for family,size,count,order,variants in cases:
            order_variants=variants[repeat%len(variants):]+variants[:repeat%len(variants)]
            for variant in order_variants:
                label=f'containers-{repeat}-{family}-{size}-{order}-{variant}'
                d=experiment.run(output,label,[output/'containers',family,variant,str(size),str(count),order])
                result=json.loads((d/'stdout.txt').read_text());experiment.save(d/'result.json',result)
                results.append(dict(repeat=repeat,family=family,size=size,order=order,variant=variant,
                    cpuSeconds=metric(result,'cpu_seconds'),hostSeconds=result['hostSeconds'],checksum=result['fingerprints']['checksum'],directory=str(d)))
                experiment.save(output/'containers.partial.json',results)
            print(family,size,order,'repeat',repeat,'passed',flush=True)
    groups={}
    for row in results:groups.setdefault((row['family'],row['size'],row['order'],row['variant']),[]).append(row)
    summary=[]
    for key,rows in groups.items():
        cpu=[r['cpuSeconds'] for r in rows];wall=[r['hostSeconds'] for r in rows]
        summary.append(dict(family=key[0],size=key[1],order=key[2],variant=key[3],medianCpuSeconds=statistics.median(cpu),
            minCpuSeconds=min(cpu),maxCpuSeconds=max(cpu),medianWallSeconds=statistics.median(wall),repeats=len(rows)))
    experiment.save(output/'containers.json',dict(results=results,summary=summary))

def engine(output,parent,repeats,controlled=False):
    variants=['baseline','output-linear','byte-flags','trace-flat','trace-objects','headless-reuse','trace-pool']
    results=[];references={}
    stem='engine-pic' if controlled else 'engine'
    if (output/(stem+'.json')).exists():raise RuntimeError('Completed engine measurements already exist')
    for repeat in range(repeats):
        order=variants[repeat%len(variants):]+variants[:repeat%len(variants)]
        for trace in ['none','stream']:
            for name in order:
                label=f'{stem}-{repeat}-{trace}-{name}'
                binary=output/'pic-control'/name/'probe' if controlled else parent/'baseline-cpu/probe' if name=='baseline' else output/name/'probe'
                args=[binary,'guest','4',trace]
                trace_path=output/'traces'/label
                if trace=='stream':trace_path.parent.mkdir(exist_ok=True);args.append(trace_path)
                d=experiment.run(output,label,args)
                result=json.loads((d/'stdout.txt').read_text());experiment.save(d/'result.json',result)
                metrics={v['name']:v['value'] for v in result['metrics']}
                match=re.search(r'ENGINE_EXEC_CPU ([0-9.]+)',(d/'stderr.txt').read_text())
                if not match:raise RuntimeError('Missing execution CPU timer')
                signature=dict(fingerprints=result['fingerprints'],ticks=result['simulatedTicks'],
                    counters={k:v for k,v in metrics.items() if k not in ('setup_seconds','events_per_second')})
                if trace=='stream':signature['files']={p.name:experiment.sha(p) for p in sorted(trace_path.iterdir()) if p.is_file()}
                if trace in references and signature!=references[trace]:
                    experiment.save(d/'mismatch.json',dict(expected=references[trace],actual=signature))
                    raise RuntimeError('Engine semantic mismatch: '+str(d))
                references.setdefault(trace,signature)
                results.append(dict(repeat=repeat,trace=trace,variant=name,hostSeconds=result['hostSeconds'],cpuSeconds=float(match[1]),directory=str(d)))
                experiment.save(output/(stem+'.partial.json'),results)
                print(name,trace,'CPU',match[1],'wall',round(result['hostSeconds'],4),flush=True)
    summary=[]
    for trace in ['none','stream']:
        for name in variants:
            rows=[r for r in results if r['trace']==trace and r['variant']==name]
            summary.append(dict(trace=trace,variant=name,medianCpuSeconds=statistics.median(r['cpuSeconds'] for r in rows),
                minCpuSeconds=min(r['cpuSeconds'] for r in rows),maxCpuSeconds=max(r['cpuSeconds'] for r in rows),
                medianHostSeconds=statistics.median(r['hostSeconds'] for r in rows)))
    experiment.save(output/(stem+'.json'),dict(results=results,summary=summary,references=references))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--parent',type=Path,default=experiment.ROOT/'build-milestone/engine-performance-20261026')
    p.add_argument('--output',type=Path)
    p.add_argument('--repeats',type=int,default=3)
    p.add_argument('stage',choices=['build','headless','pool','pic','micro','pool-micro','engine','engine-pic'])
    a=p.parse_args();parent=a.parent.resolve();output=(a.output or parent/'stl-review').resolve();output.mkdir(parents=True,exist_ok=True)
    if not 1<=a.repeats<=5:raise ValueError('Invalid repeat count')
    if a.stage=='build':build(output,parent)
    elif a.stage=='headless':build_headless(output,parent)
    elif a.stage=='pool':build_pool(output,parent)
    elif a.stage=='pic':build_pic(output,parent)
    elif a.stage=='micro':micro(output,a.repeats)
    elif a.stage=='pool-micro':pool_micro(output,a.repeats)
    else:engine(output,parent,a.repeats,a.stage=='engine-pic')

if __name__=='__main__':main()
