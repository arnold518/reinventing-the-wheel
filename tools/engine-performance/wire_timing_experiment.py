#!/usr/bin/env python3
"""Matched current-engine wire/timing prototypes; never deploys an engine."""
import argparse
import json
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import experiment as e

SOURCES = ['simulator/Event.cpp', 'basic/TemplateInstantiations.cpp',
           'components/BasicComponent.cpp', 'simulator/Simulator.cpp',
           'basic/Wire.cpp', 'basic/Pin.cpp', 'modules/basic/Gate.cpp',
           'analysis/DelayTwin.cpp', 'components/IOComponent.cpp']
VARIANTS = {'baseline': (1, False, False), 'wire': (1, True, False),
            'timing': (1, False, True), 'combined': (1, True, True),
            'baseline-o2': (2, False, False), 'combined-o2': (2, True, True)}


def replace(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError('Prototype source context changed: ' + old[:90])
    return source.replace(old, new)


def timing_source(source):
    source = replace(source, '#include "TimingGraph.hpp"', '#include "analysis/TimingGraph.hpp"')
    source = replace(source, '    static void settleLeaf(const Leaf& leaf, std::vector<uint8_t>& value, std::vector<Tick>& time) {', '''    struct Scratch {
        struct Late { Tick at; LogicValue* bit; Tick delay; };
        std::vector<Tick> by_bit, found;
        std::vector<uint8_t> waits, moves;
        std::vector<Late> late;
    };
    static void settleLeaf(const Leaf& leaf, std::vector<uint8_t>& value, std::vector<Tick>& time, Scratch& scratch) {''')
    source = replace(source, '        std::vector<Tick> by_bit(leaf.nodes.size(), at);',
                     '        auto& by_bit = scratch.by_bit; by_bit.assign(leaf.nodes.size(), at);')
    source = replace(source, '            struct Late { Tick at; LogicValue* bit; Tick delay; };\n            std::vector<Late> late;',
                     '            using Late = Scratch::Late;\n            auto& late = scratch.late; late.clear();')
    source = replace(source, '            const Primitive::Values final_in = in;\n            const auto final_out = out[leaf.output];',
                     '            const auto& final_out = out[leaf.output];')
    source = replace(source, '            std::vector<uint8_t> waits(leaf.nodes.size(), 0);\n            std::vector<Tick> found(leaf.nodes.size(), NEVER);', '''            auto& waits = scratch.waits; waits.assign(leaf.nodes.size(), 0);
            auto& found = scratch.found; found.assign(leaf.nodes.size(), NEVER);
            // These exact concrete types overwrite every output on every call.
            // Unknown/custom primitives retain their fresh-map contract.
            const bool reuse = typeid(*leaf.part) == typeid(MemoryOutputBuffer) ||
                               typeid(*leaf.part) == typeid(StaticReadColumn);
            Primitive::Values tried;
            Primitive::Deciders unused;''')
    source = replace(source, '                std::vector<uint8_t> moves(waits.size(), 0);',
                     '                auto& moves = scratch.moves; moves.assign(waits.size(), 0);')
    source = replace(source, '                    Primitive::Values tried;\n                    Primitive::Deciders unused;',
                     '                    if (!reuse) tried.clear();\n                    unused.clear();')
    source = replace(source, '            in = final_in;', '            late.clear(); // Do not retain pointers into local input buffers.')
    source = replace(source, '        for (const int node : order) {',
                     '        Scratch scratch; // Owned by this settle call; reused only across its leaves.\n        for (const int node : order) {')
    source = replace(source, 'settleLeaf(leaves[gate.a], value, time);',
                     'settleLeaf(leaves[gate.a], value, time, scratch);')
    return source


def build(out, compiler):
    if (out/'builds.json').exists():
        raise RuntimeError('Completed controls already exist')
    snap = out/'snapshot'
    if not snap.exists():
        previous = out.parent/'implementation/snapshot'
        if e.sha(previous/'libSimulatorLib.a') != e.sha(e.ROOT/'build-milestone/libSimulatorLib.a'):
            raise RuntimeError('Native archive changed; inspect/rebuild before freezing controls')
        for directory in ['include', 'src']:
            for file in (previous/directory).rglob('*'):
                if file.is_file() and e.sha(file) != e.sha(e.ROOT/file.relative_to(previous)):
                    raise RuntimeError('Baseline source changed: '+str(file))
        snap.mkdir()
        for directory in ['include', 'src']:
            shutil.copytree(previous/directory, snap/directory)
        shutil.copy2(previous/'libSimulatorLib.a', snap/'libSimulatorLib.a')
        for name in ['TickTraceWriter.hpp', 'probe.cpp']:
            shutil.copy2(previous/name, snap/name)
    wire = out/'wire-source'
    (wire/'include/simulator').mkdir(parents=True, exist_ok=True)
    text = (snap/'include/simulator/Event.tpp').read_text()
    text = replace(text, '#include <utility>', '#include <utility>\n#include <typeinfo>')
    text = replace(text, '''    const std::vector<LogicValue> values(new_values.begin(), new_values.end());
    processWireUpdateEvent(time, wire_to_update, values, sim);''', '''    // Derived wires may override the value API; retain the generic contract.
    if (typeid(*wire_to_update) != typeid(Wire<WIDTH>)) {
        const std::vector<LogicValue> values(new_values.begin(), new_values.end());
        processWireUpdateEvent(time, wire_to_update, values, sim);
        return;
    }
    bool changed = false;
    for (size_t i = 0; i < WIDTH; ++i) {
        // Qualified template access avoids a virtual call for every bit.
        if (wire_to_update->Wire<WIDTH>::getBit(i) != new_values[i]) { changed = true; break; }
    }
    if (!changed) return;
    const std::vector<LogicValue> values(new_values.begin(), new_values.end());
    processChangedWireUpdateEvent(time, wire_to_update, values, sim);''')
    (wire/'include/simulator/Event.tpp').write_text(text)
    header = (snap/'include/simulator/Event.hpp').read_text()
    header = replace(header, 'void processWireUpdateEvent(',
        'void processChangedWireUpdateEvent(SimulationTime, const std::shared_ptr<WireBase>&,\n'
        '                                   const std::vector<LogicValue>&, Simulator&);\n\nvoid processWireUpdateEvent(')
    (wire/'include/simulator/Event.hpp').write_text(header)
    (wire/'Event.cpp').write_text((snap/'src/simulator/Event.cpp').read_text()+'''
void processChangedWireUpdateEvent(SimulationTime t, const std::shared_ptr<WireBase>& w,
                                  const std::vector<LogicValue>& v, Simulator& sim) {
    w->setValueVector(v); sim.recordChange(t,w,v); w->propagateChange(sim,t);
}
''')
    (out/'DelayTwin-reuse.cpp').write_text(timing_source((snap/'src/analysis/DelayTwin.cpp').read_text()))
    common = ['-std=gnu++20', '-g0', '-DNDEBUG', '-fPIC', '-UNDEBUG',
              '-Wno-unknown-warning-option', '-DCIRCUITSIM_SOURCE_DIR="'+str(e.ROOT)+'"']

    def compile_one(label, source, level=1, wire_headers=False):
        obj = out/'objects'/(label+'.o')
        obj.parent.mkdir(exist_ok=True)
        if not obj.exists():
            flags = [*common, '-O'+str(level)]
            if wire_headers: flags.append('-I'+str(wire/'include'))
            flags += ['-I'+str(snap/'include'), '-I'+str(snap/'src'), '-I'+str(snap)]
            e.run(out, 'compile-'+label, [compiler, *flags, '-c', source, '-o', obj])
        return obj

    runtime = '#include <ctime>\n#include <iostream>\n'+(snap/'src/runtime/RecordedGuest.cpp').read_text()
    runtime = 'extern "C" void engineAllocBegin() __attribute__((weak));\nextern "C" void engineAllocEnd() __attribute__((weak));\n'+runtime
    runtime = replace(runtime, 'const auto start=Clock::now();',
        'const auto start=Clock::now();const auto cpu_start=std::clock();if(engineAllocBegin)engineAllocBegin();')
    runtime = replace(runtime, 'result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();return result;',
        'result.execution_seconds=std::chrono::duration<double>(Clock::now()-start).count();'
        'const double cpu=double(std::clock()-cpu_start)/CLOCKS_PER_SEC;if(engineAllocEnd)engineAllocEnd();'
        'std::cerr<<"ENGINE_EXEC_CPU "<<cpu<<"\\n";return result;')
    (out/'execution-cpu.cpp').write_text(runtime)
    timer = compile_one('execution-cpu', out/'execution-cpu.cpp')
    driver = compile_one('driver', snap/'probe.cpp')
    builds = {}
    for name, (level, wires, timing) in VARIANTS.items():
        objects = []
        for relative in SOURCES:
            wire_headers = wires and relative in SOURCES[:3]
            source = snap/'src'/relative
            tag = Path(relative).stem+'-o'+str(level)
            if wire_headers:
                tag += '-wire'
                if relative == 'simulator/Event.cpp': source = wire/'Event.cpp'
            if timing and relative == 'analysis/DelayTwin.cpp':
                tag += '-reuse'; source = out/'DelayTwin-reuse.cpp'
            objects.append(compile_one(tag, source, level, wire_headers))
        target = out/name
        target.mkdir(exist_ok=True)
        # Event's explicit width definitions precede every archive/driver weak
        # template definition in all variants. Allocation diagnostics verify use.
        e.run(out, 'link-'+name, [compiler, *objects, timer, driver,
              snap/'libSimulatorLib.a', '-lz', '-o', target/'probe'])
        builds[name] = dict(binarySha256=e.sha(target/'probe'), optimization=level,
            wire=wires, timing=timing, objects={str(o): e.sha(o) for o in objects})
    e.run(out, 'compile-allocation-counter', [compiler, '-std=gnu++20', '-O1', '-g0', '-shared', '-fPIC',
          e.ROOT/'tools/engine-performance/AllocationCounter.cpp', '-o', out/'allocations.so'])
    e.save(out/'builds.json', dict(variants=builds, compiler=str(compiler),
        compilerVersion=subprocess.check_output([compiler, '--version'], text=True), flags=common,
        optimizedSources=SOURCES, archiveSha256=e.sha(snap/'libSimulatorLib.a'),
        sourceHashes={str(p.relative_to(snap)): e.sha(p) for p in snap.rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.tpp')}))


def signature(result, trace=None):
    metrics = {v['name']: v['value'] for v in result['metrics']}
    answer = dict(fingerprints=result['fingerprints'], ticks=result['simulatedTicks'],
        counters={k: v for k, v in metrics.items() if k not in ('events_per_second', 'setup_seconds')})
    if trace: answer['traceFiles'] = {p.name: e.sha(p) for p in sorted(trace.iterdir()) if p.is_file()}
    return answer


def verify(out, compiler):
    snap=out/'snapshot'; info=json.loads((out/'builds.json').read_text())
    equipment={
        'timing':e.ROOT/'tools/engine-performance/TimingTwinProbe.cpp',
        'wire':e.ROOT/'tools/engine-performance/WirePayloadProbe.cpp',
        'gate':e.ROOT/'tests/cpp/GateInputCacheTest.cpp',
        'order':e.ROOT/'tools/engine-performance/SchedulerContractProbe.cpp',
        'trace':e.ROOT/'tests/cpp/SignalTraceProbe.cpp',
        'runner':e.ROOT/'tests/cpp/SimulationTestRunner.cpp'}
    objects={}
    for name,source in equipment.items():
        obj=out/'objects'/('verify-'+name+'.o')
        e.run(out,'compile-verify-'+name,[compiler,'-std=gnu++20','-O1','-g0','-DNDEBUG','-UNDEBUG','-fPIC',
            '-DCIRCUITSIM_SOURCE_DIR="'+str(e.ROOT)+'"','-I'+str(snap/'include'),'-I'+str(snap/'src'),
            '-I'+str(e.ROOT/'tests/cpp'),'-c',source,'-o',obj])
        objects[name]=obj
    rows=[];references={}
    for variant in VARIANTS:
        selected=['timing','wire']
        if variant in ('wire','timing','combined-o2'):selected+=['gate','order','trace','runner']
        for name in selected:
            exe=out/variant/('verify-'+name)
            core=list(info['variants'][variant]['objects'])
            e.run(out,'link-verify-'+variant+'-'+name,[compiler,*core,objects[name],snap/'libSimulatorLib.a','-lz','-o',exe])
            if name=='runner':
                for scenario in ['SimulatorUnwiredOutputPinHistoryTest','SimulatorDrainUntilIdleTest','RV32IALUAddSubTest']:
                    d=e.run(out,'verify-'+variant+'-'+scenario,[exe,scenario]);rows.append(str(d))
            elif name=='trace':
                d=e.run(out,'verify-'+variant+'-tick-decoder',['node','--max-old-space-size=128',
                    e.ROOT/'tests/checks/TickRecordingTest.mjs',exe],address_space_mib=0)
                rows.append(str(d))
            else:
                d=e.run(out,'verify-'+variant+'-'+name,[exe]);rows.append(str(d))
                if name in ('timing','wire'):
                    value=e.sha(d/'stdout.txt')
                    if name in references and value!=references[name]:raise RuntimeError('Probe signature differs: '+str(d))
                    references.setdefault(name,value)
    e.save(out/'verification.json',dict(checks=len(rows),directories=rows,exactProbeHashes=references))


def measure(out, name, iterations, repeats, diagnostic=False):
    if (out/(name+'.json')).exists(): raise RuntimeError('Completed results already exist')
    rows, references = [], {}
    variants = list(VARIANTS)
    for repeat in range(repeats):
        order = variants[repeat % len(variants):]+variants[:repeat % len(variants)]
        for mode in ['none', 'stream']:
            for variant in order:
                label = f'{name}-{repeat}-{mode}-{variant}'
                args = [out/variant/'probe', 'guest', str(iterations), mode]
                trace = None
                if mode == 'stream':
                    trace = out/'traces'/label; trace.parent.mkdir(exist_ok=True); args.append(trace)
                env = {'LD_PRELOAD': str(out/'allocations.so')} if diagnostic else None
                directory = e.run(out, label, args, extra_env=env)
                result = json.loads((directory/'stdout.txt').read_text())
                e.save(directory/'result.json', result)
                current = signature(result, trace)
                if mode in references and current != references[mode]:
                    e.save(directory/'mismatch.json', dict(expected=references[mode], actual=current))
                    receipt=json.loads((directory/'receipt.json').read_text())
                    receipt['semanticCheckStatus']='failed';e.save(directory/'receipt.json',receipt)
                    raise RuntimeError('Semantic mismatch at '+str(directory))
                receipt=json.loads((directory/'receipt.json').read_text())
                receipt['semanticCheckStatus']='passed';e.save(directory/'receipt.json',receipt)
                references.setdefault(mode, current)
                stderr = (directory/'stderr.txt').read_text()
                row = dict(variant=variant, mode=mode, repeat=repeat,
                    cpuSeconds=float(re.search(r'ENGINE_EXEC_CPU ([0-9.]+)', stderr)[1]),
                    hostSeconds=result['hostSeconds'], directory=str(directory))
                if diagnostic: row['allocations'] = json.loads(re.search(r'ENGINE_ALLOC (\{.*\})', stderr)[1])
                rows.append(row); e.save(out/(name+'.partial.json'), rows)
                print(variant, mode, 'CPU', row['cpuSeconds'], row.get('allocations',''), flush=True)
    summary = []
    for mode in ['none','stream']:
        for variant in variants:
            selected = [r for r in rows if r['mode']==mode and r['variant']==variant]
            times = [r['cpuSeconds'] for r in selected]
            summary.append(dict(variant=variant, mode=mode, medianCpuSeconds=statistics.median(times),
                minCpuSeconds=min(times), maxCpuSeconds=max(times),
                medianHostSeconds=statistics.median(r['hostSeconds'] for r in selected)))
    e.save(out/(name+'.json'), dict(iterations=iterations, repeats=repeats, diagnostic=diagnostic,
        rows=rows, summary=summary, references=references))
    print(json.dumps(summary, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=e.ROOT/'build-milestone/engine-performance-20261026/wire-timing')
    parser.add_argument('--compiler', type=Path, default=Path('/home/arnold/.cache/circuitsim-emsdk/upstream/bin/clang++'))
    parser.add_argument('--name', default='measurements')
    parser.add_argument('--iterations', type=int, default=16)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('stage', choices=['build','verify','measure','allocations'])
    args = parser.parse_args()
    if not 1 <= args.iterations <= 64 or not 1 <= args.repeats <= 5: raise ValueError('Experiment bounds exceeded')
    out = args.output.resolve(); out.mkdir(exist_ok=True)
    if args.stage == 'build': build(out, args.compiler.absolute())
    elif args.stage == 'verify': verify(out, args.compiler.absolute())
    else: measure(out, args.name, args.iterations, args.repeats, args.stage=='allocations')


if __name__ == '__main__': main()
