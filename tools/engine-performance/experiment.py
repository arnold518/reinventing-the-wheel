#!/usr/bin/env python3
"""Serial, resource-bounded research equipment; retains every attempted command."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()

def available():
    return int(next(x for x in Path('/proc/meminfo').read_text().splitlines()
                    if x.startswith('MemAvailable:')).split()[1]) // 1024

def save(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')

def run(output, label, command, seconds=180, cwd=None, extra_env=None, address_space_mib=768):
    attempts = output/'attempts'
    attempts.mkdir(exist_ok=True)
    identifier = f'{time.time_ns()}-{label}'
    directory = attempts/identifier
    directory.mkdir()
    unit = 'circuitsim-engine-'+str(time.time_ns())
    receipt = dict(label=label, command=list(map(str, command)), status='waiting-for-memory',
                   startedUnix=time.time(), memoryMaxMiB=768, memoryHighMiB=640,
                   swapMaxBytes=0, addressSpaceMiB=address_space_mib, startAvailableMiB=1536,
                   stopAvailableMiB=1024, wallLimitSeconds=seconds, unit=unit)
    save(directory/'receipt.json', receipt)
    start = time.monotonic()
    while available() < 1536:
        if time.monotonic()-start > 60:
            receipt.update(status='resource-deferred', finishedUnix=time.time())
            save(directory/'receipt.json', receipt)
            raise RuntimeError('Insufficient headroom; attempt retained at '+str(directory))
        time.sleep(1)
    env = {**os.environ, 'XDG_RUNTIME_DIR':f'/run/user/{os.getuid()}',
           'DBUS_SESSION_BUS_ADDRESS':f'unix:path=/run/user/{os.getuid()}/bus'}
    launch = ['systemd-run','--user','--quiet','--wait','--pipe','--collect',
              '--unit='+unit,'-p','MemoryMax=768M','-p','MemoryHigh=640M',
              '-p','MemorySwapMax=0','-p','TasksMax=32','-p','CPUQuota=90%',
              '-p','Nice=10','-p',f'RuntimeMaxSec={seconds}',
              '--working-directory='+str(cwd or ROOT)]
    if address_space_mib:launch.extend(['/usr/bin/prlimit','--as='+str(address_space_mib*1024*1024)])
    launch.extend(['/usr/bin/time','-v','-o',
              str(directory/'resources.txt'),'/usr/bin/env','CIRCUITSIM_TIMING_PROFILE=unit']
    )
    launch.extend(f'{k}={v}' for k,v in (extra_env or {}).items())
    launch.extend(map(str,command))
    receipt.update(status='running', launchedUnix=time.time())
    save(directory/'receipt.json', receipt)
    print('RUN',label,flush=True)
    with (directory/'stdout.txt').open('w') as stdout, (directory/'stderr.txt').open('w') as stderr:
        child = subprocess.Popen(launch, env=env, stdout=stdout, stderr=stderr)
        try:
            while child.poll() is None:
                if available()<1024:
                    receipt['status']='resource-stopped'
                    subprocess.run(['systemctl','--user','stop',unit],env=env,capture_output=True)
                    break
                if time.monotonic()-start>seconds+70:
                    receipt['status']='timed-out'
                    subprocess.run(['systemctl','--user','stop',unit],env=env,capture_output=True)
                    break
                time.sleep(.2)
            child.wait(timeout=15)
        except BaseException:
            subprocess.run(['systemctl','--user','stop',unit],env=env,capture_output=True)
            child.wait(timeout=15)
            receipt.update(status='interrupted',finishedUnix=time.time())
            save(directory/'receipt.json',receipt)
            raise
    receipt.update(returncode=child.returncode, finishedUnix=time.time())
    if receipt['status']=='running':
        resource_file=directory/'resources.txt'
        resource_text=resource_file.read_text() if resource_file.exists() else ''
        completion=re.search(r'^\s*Exit status:\s*(\d+)\s*$',resource_text,re.M)
        if child.returncode:
            receipt['status']='failed'
        elif not completion or 'Command terminated by signal' in resource_text:
            # systemd-run can exit zero after an external service stop even
            # though the timed command never completed.
            receipt.update(status='interrupted',reason='Missing successful timed-command completion')
        else:
            receipt['status']='passed' if int(completion[1])==0 else 'failed'
    save(directory/'receipt.json',receipt)
    with (output/'ledger.jsonl').open('a') as stream:
        stream.write(json.dumps({**receipt,'directory':str(directory)})+'\n')
    if receipt['status']!='passed':
        raise RuntimeError('Attempt failed; inspect '+str(directory))
    return directory

def snapshot(output, build):
    snap=output/'snapshot'
    if snap.exists(): return snap
    snap.mkdir()
    shutil.copytree(ROOT/'include',snap/'include')
    shutil.copytree(ROOT/'src',snap/'src')
    shutil.copy2(ROOT/'tests/cpp/TickTraceWriter.hpp',snap/'TickTraceWriter.hpp')
    shutil.copy2(ROOT/'tools/engine-performance/EnginePerformanceProbe.cpp',snap/'probe.cpp')
    shutil.copy2(build/'libSimulatorLib.a',snap/'libSimulatorLib.a')
    save(output/'provenance.json',dict(baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
         compiler=subprocess.check_output(['/usr/bin/c++','--version'],text=True).splitlines()[0],
         baselineFlags='-O1 -g0 -DNDEBUG', librarySha256=sha(snap/'libSimulatorLib.a'),
         sourceHashes={str(p.relative_to(snap)):sha(p) for p in snap.rglob('*') if p.is_file() and p.suffix in ('.hpp','.tpp','.cpp')}))
    return snap

def build_variant(output, build, variant):
    snap=snapshot(output,build)
    kind=variant.removesuffix('-cpu')
    destination=output/variant
    destination.mkdir(exist_ok=True)
    if (destination/'build.json').exists():
        raise RuntimeError('Completed variant already exists; use a fresh experiment output directory')
    includes=['-I'+str(destination/'include'),'-I'+str(destination),'-I'+str(snap/'include'),'-I'+str(snap/'src'),'-I'+str(snap)]
    flags=['-std=c++20','-O1','-g0','-DNDEBUG','-UNDEBUG']
    sources=[]
    if kind in ('wire-fast','wire-array'):
        target=destination/'include/simulator/Event.tpp';target.parent.mkdir(parents=True,exist_ok=True)
        text=(snap/'include/simulator/Event.tpp').read_text()
        old='    const std::vector<LogicValue> values(new_values.begin(), new_values.end());\n    processWireUpdateEvent(time, wire_to_update, values, sim);'
        new='''    bool changed = false;
    for (size_t i = 0; i < WIDTH; ++i) {
        if (wire_to_update->getBit(i) != new_values[i]) { changed = true; break; }
    }
    if (!changed) return;
    const std::vector<LogicValue> values(new_values.begin(), new_values.end());
    processChangedWireUpdateEvent(time, wire_to_update, values, sim);'''
        if kind=='wire-array':
            new='''    if (wire_to_update->valueEquals(new_values)) return;
    const std::vector<LogicValue> values(new_values.begin(), new_values.end());
    processChangedWireUpdateEvent(time, wire_to_update, values, sim);'''
            wire=(snap/'include/basic/Wire.hpp').read_text()
            marker='    size_t getWidth() const override;'
            if wire.count(marker)!=1:raise RuntimeError('Wire comparison context changed')
            wire=wire.replace(marker,'    bool valueEquals(const std::array<LogicValue, WIDTH>& other) const { return value == other; }\n\n'+marker)
            wh=destination/'include/basic/Wire.hpp';wh.parent.mkdir(parents=True,exist_ok=True);wh.write_text(wire)
        if text.count(old)!=1:raise RuntimeError('Wire candidate context changed')
        target.write_text(text.replace(old,new))
        header=(snap/'include/simulator/Event.hpp').read_text()
        declaration='void processChangedWireUpdateEvent(SimulationTime time, const std::shared_ptr<WireBase>& wire,\n                                  const std::vector<LogicValue>& values, Simulator& sim);\n\n'
        (target.parent/'Event.hpp').write_text(header.replace('void processWireUpdateEvent(',declaration+'void processWireUpdateEvent(',1))
        source=(snap/'src/simulator/Event.cpp').read_text()
        source+='\nvoid processChangedWireUpdateEvent(SimulationTime t, const std::shared_ptr<WireBase>& w, const std::vector<LogicValue>& v, Simulator& sim) {\n w->setValueVector(v); sim.recordChange(t,w,v); w->propagateChange(sim,t);\n}\n'
        (destination/'Event.cpp').write_text(source)
        sources=['simulator/Event.cpp']
    elif kind=='hot-o2':
        flags[1]='-O2'
        sources=['simulator/Simulator.cpp','simulator/Event.cpp','basic/Wire.cpp','basic/Pin.cpp',
                 'components/BasicComponent.cpp','modules/basic/Gate.cpp','basic/TemplateInstantiations.cpp']
    elif kind=='profiled-o2':
        flags[1]='-O2'
        sources=['analysis/DelayTwin.cpp','components/IOComponent.cpp']
    elif kind=='buckets':
        source=(snap/'src/simulator/Simulator.cpp').read_text()
        helper='''
#include <deque>
namespace {
// Research-only side storage avoids rebuilding the archive for a private
// Simulator layout change. Production would require owned member storage.
class BucketQueue {
    struct Bucket { std::deque<std::shared_ptr<Event>> wire, eval; };
    std::map<SimulationTime, Bucket> buckets;
    size_t count=0;
public:
    bool empty() const { return count==0; }
    size_t size() const { return count; }
    const std::shared_ptr<Event>& top() const {
        const auto& b=buckets.begin()->second;
        return b.wire.empty()?b.eval.front():b.wire.front();
    }
    void push(std::shared_ptr<Event> event) {
        auto& b=buckets[event->time];
        (event->getPriority()==EventPriority::WIRE_UPDATE?b.wire:b.eval).push_back(std::move(event));
        ++count;
    }
    void pop() {
        auto i=buckets.begin();auto& b=i->second;
        (b.wire.empty()?b.eval:b.wire).pop_front();--count;
        if(b.wire.empty()&&b.eval.empty())buckets.erase(i);
    }
};
thread_local std::map<const Simulator*, BucketQueue> experimentalQueues;
BucketQueue& bucketQueue(const Simulator& sim) { return experimentalQueues[&sim]; }
}
'''
        source=source.replace('namespace {',helper+'\nnamespace {',1)
        source=re.sub(r'\bevent_queue\b','bucketQueue(*this)',source)
        source=source.replace('Simulator::Simulator() : current_time(0) {}','Simulator::Simulator() : current_time(0) { experimentalQueues.erase(this); }')
        (destination/'Simulator.cpp').write_text(source)
        sources=['simulator/Simulator.cpp']
    elif kind not in ('baseline','sampled'):raise ValueError('Unknown variant')
    objects=[]
    if kind=='profiled-o2':
        objects=[output/'hot-o2'/(name+'.o') for name in ['Simulator','Event','Wire','Pin','BasicComponent','Gate','TemplateInstantiations']]
        if not all(p.exists() for p in objects):raise RuntimeError('Build hot-o2 first')
    for relative in sources:
        obj=destination/(Path(relative).stem+'.o')
        source=destination/'Event.cpp' if kind in ('wire-fast','wire-array') else destination/'Simulator.cpp' if kind=='buckets' else snap/'src'/relative
        run(output,variant+'-compile-'+obj.stem,['/usr/bin/c++',*flags,*includes,'-c',source,'-o',obj])
        objects.append(obj)
    driver=destination/'driver.o'
    driver_flags=flags+(['-pg'] if kind=='sampled' else [])
    run(output,variant+'-compile-driver',['/usr/bin/c++',*driver_flags,*includes,'-c',snap/'probe.cpp','-o',driver])
    binary=destination/'probe'
    common=[]
    if variant.endswith('-cpu'):
        common=[output/'execution-cpu.o']
        if not common[0].exists():
            source=(snap/'src/runtime/RecordedGuest.cpp').read_text()
            source='#include <ctime>\n#include <iostream>\n'+source
            source=source.replace('const auto start=Clock::now();','const auto start=Clock::now(); const auto execution_cpu_start=std::clock();')
            marker='result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();return result;'
            if source.count(marker)!=1:raise RuntimeError('Runtime timer context changed')
            source=source.replace(marker,'result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count(); std::cerr << "ENGINE_EXEC_CPU " << double(std::clock()-execution_cpu_start)/CLOCKS_PER_SEC << "\\n";return result;')
            path=output/'execution-cpu.cpp';path.write_text(source)
            run(output,'common-compile-execution-cpu',['/usr/bin/c++','-std=c++20','-O1','-g0','-UNDEBUG',
                '-I'+str(snap/'include'),'-I'+str(snap/'src'),'-c',path,'-o',common[0]])
    run(output,variant+'-link',['/usr/bin/c++',*(['-pg','-no-pie'] if kind=='sampled' else []),
        *objects,*common,driver,snap/'libSimulatorLib.a','-lz','-o',binary])
    save(destination/'build.json',dict(variant=variant,flags=flags,overriddenSources=sources,
        binarySha256=sha(binary),objects={str(p):sha(p) for p in objects+common+[driver]}))
    return binary

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--build-dir',type=Path,default=ROOT/'build-milestone')
    sub=p.add_subparsers(dest='action',required=True)
    b=sub.add_parser('build');b.add_argument('variant',choices=['baseline','sampled','wire-fast','wire-array','hot-o2','profiled-o2','buckets','baseline-cpu','wire-array-cpu','profiled-o2-cpu','buckets-cpu'])
    r=sub.add_parser('run');r.add_argument('variant');r.add_argument('label');r.add_argument('arguments',nargs=argparse.REMAINDER)
    a=p.parse_args();output=a.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if a.action=='build':build_variant(output,a.build_dir.resolve(),a.variant)
    else:
        extra={'GMON_OUT_PREFIX':str(output/'sampled'/'gmon')} if a.variant=='sampled' else {}
        directory=run(output,a.label,[output/a.variant/'probe',*a.arguments],extra_env=extra)
        result=json.loads((directory/'stdout.txt').read_text())
        save(directory/'result.json',result)

if __name__=='__main__':main()
