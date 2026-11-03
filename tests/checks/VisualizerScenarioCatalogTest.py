"""Registry visibility, Python/JS presentation parity and program configuration."""
import gc
import json
from pathlib import Path
import subprocess
import unittest

import circuit_backend as backend
import server
import scenario_catalog as catalog

ROOT = Path(__file__).resolve().parents[2]


class VisualizerScenarioCatalogTest(unittest.TestCase):
    def test_overall_component_learning_order(self):
        options = server.SCENARIO_OPTIONS
        groups = list(dict.fromkeys(option['group'] for option in options))
        self.assertEqual(groups, ['logic', 'wiring', 'alu', 'memory-helpers',
            'ff', 'sram', 'dram', 'rom', 'flash', 'memory-interface', 'memory-cache',
            'registers', 'rv32i-alu', 'rv32i-blocks', 'single', 'baseline',
            'prediction', 'advanced', 'memory-systems', 'examples', 'faults',
            'performance', 'rv32im', 'rv32im-programs', 'system', 'games', 'developer'])
        def aliases(group):
            return [option['key'] for option in options if option['group'] == group]
        def classes(group):
            return [option['className'] for option in options if option['group'] == group]
        self.assertEqual(classes('ff'), ['FFWordTest', 'FFStorageBankTest',
            'FFMemoryTest', 'NativeMemoryDeviceTest'])
        self.assertEqual([name for name in classes('wiring') if name.startswith('Mux')],
            ['Mux2Width1Test', 'Mux2Width4Test', 'Mux2Width32Test',
             'Mux4Width1Test', 'Mux4Width32Test', 'Mux8Width1Test',
             'Mux8Width8Test', 'Mux8Width32Test', 'Mux32Width1Test', 'Mux32Width32Test'])
        self.assertEqual(classes('single'), [f'RV32ISingleCycle{stage}StageTest'
            for stage in ('Fetch', 'Decode', 'Execute', 'Memory', 'Writeback', 'NextPC')]
            + ['RV32ISingleCycleCoreTest'])
        self.assertEqual(classes('baseline')[-1], 'RV32IBaselinePipelineCoreTest')
        self.assertEqual(classes('advanced')[-1], 'RV32IFiveStageCoreTest')
        self.assertEqual(aliases('rv32im'), ['multiply-step', 'divide-step', 'multiplier-w32',
            'divider-w32', 'muldiv-w32', 'rv32im-decode-control', 'rv32im-decode-stage',
            'rv32im-execute-stage', 'rv32im-coordinator', 'rv32im-core'])
        self.assertEqual(aliases('rv32im-programs'), ['rv32im-program-arithmetic',
            'rv32im-program-dependencies', 'rv32im-program-divide-edges', 'rv32im-program26', 'rv32im-program27'])
        self.assertEqual(aliases('system'), ['bus-router', 'bus-interconnect', 'bus-bridge', 'bus-port-checker',
            'memory-subsystem', 'device-register-port-w32', 'clocked-counter', 'synchronous-fifo', 'console-device',
            'timer-device', 'button-device', 'framebuffer-display', 'led-panel', 'computer-system-structure',
            'computer-system', 'computer-system-two-bus', 'computer-system-registered-bridge', 'computer-system-leds',
            'computer-system-checked'])
        self.assertEqual(aliases('games'), ['game-2048', 'game-sokoban', 'game-snake', 'game-pong',
            'game-tetris', 'game-snake-leds'])

    def test_explicit_ranks_are_complete_and_registry_order_independent(self):
        groups = [group['id'] for group in catalog.CATALOG['groups']]
        self.assertEqual(len(groups), len(set(groups)))
        for group in groups:
            rows = [row for row in catalog.CATALOG['components'].values() if row['group'] == group]
            ranks = [row['order'] for row in rows]
            self.assertTrue(all(type(rank) is int for rank in ranks), group)
            self.assertEqual(sorted(ranks), list(range(len(rows))), group)
        for program in catalog.CATALOG['programs']:
            self.assertEqual(program['order'], int(program['id'].split('-')[1]))
        visible = set(server.visualizable_test_names())
        descriptors = [d for d in backend.get_registered_test_descriptors() if d['name'] in visible]
        self.assertEqual(catalog.scenario_options(descriptors[::-1]), server.SCENARIO_OPTIONS)

    def test_no_empty_or_retired_scenarios_and_complete_presentation(self):
        descriptors = backend.get_registered_test_descriptors()
        visible = set(server.visualizable_test_names())
        for name in ('SimulatorAdvanceAndRecordTest', 'SimulatorUnwiredOutputPinHistoryTest',
                     'RV32IALUSliceTest', 'ALUWidthContractTest', 'WidthComponentsTest'):
            self.assertIn(name, backend.get_registered_test_names())
            self.assertNotIn(name, visible)
        for name in ('Memory64Kx32Test', 'MemoryBusAdapter32Test', 'Memory4x32Test',
                     'Memory32x32Test', 'SRAMContainerTest', 'ROMContainerTest'):
            self.assertNotIn(name, visible)
        components = {d['name'] for d in descriptors if d['name'] in visible and '/' not in d['name']}
        self.assertEqual(components, set(catalog.CATALOG['components']))
        self.assertEqual(len({o['key'] for o in server.SCENARIO_OPTIONS}), len(visible))
        groups = {g['id'] for g in catalog.CATALOG['groups']}
        self.assertTrue(all(o['title'] and o['group'] in groups for o in server.SCENARIO_OPTIONS))

    def test_every_component_menu_entry_exposes_a_real_topology(self):
        # Build only; do not precompute CPU programs or run verification suites.
        for name in catalog.CATALOG['components']:
            with self.subTest(name=name):
                root = server.scenario_topology(name)
                pins = len(root.get_input_pins()) + len(root.get_output_pins())
                self.assertTrue(pins or root.get_children() or root.get_wires())
                del root
        gc.collect()

    def test_program_titles_and_all_core_policy_combinations(self):
        names = set(backend.get_registered_test_names())
        root_layouts = json.loads((ROOT / 'visualizer-v2/layout.json').read_text())['root_layouts']
        required_root_children = {'CORE', 'CLOCK', 'INSTRUCTION_MEMORY', 'DATA_MEMORY'}
        for number, program in enumerate(catalog.CATALOG['programs'], 1):
            self.assertEqual(program['id'], f'program-{number:02d}')
            doc = (ROOT / f'site-docs/rv32i/4.2/programs/program-{number:02d}.md').read_text().splitlines()[0]
            self.assertEqual(doc, f'# Program {number}: {program["title"]}')
            for alias in (f'rv32i-program{number}', f'rv32i-baseline-program{number}',
                          f'rv32i-five-stage-program{number}'):
                self.assertTrue(required_root_children <= root_layouts.get(alias, {}).get('children', {}).keys(), alias)
            for policy in ('not-taken', 'taken', 'backward-taken', 'one-bit', 'two-bit'):
                name = f'RV32IPredictionProgramTest/{policy}/program-{number:02d}'
                self.assertIn(name, names)
                alias = f'rv32i-predict-{policy}-program{number}'
                self.assertEqual(server.normalize_scenario(alias), catalog.aliases_for(name)[0])
                self.assertTrue(required_root_children <= root_layouts.get(alias, {}).get('children', {}).keys(), alias)

    def test_javascript_and_python_use_the_same_catalog_and_legacy_aliases(self):
        script = """
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {scenarioOptions, menuOptions, findScenario, resolveProgram, aliasesFor, defaultAlias}
  from './visualizer-v2/static/scenario_catalog.mjs';
const input = JSON.parse(fs.readFileSync(0, 'utf8'));
const options = scenarioOptions(input.descriptors, input.catalog);
assert.deepEqual(scenarioOptions([...input.descriptors].reverse(), input.catalog), options);
const groups = menuOptions(options, input.catalog.groups);
assert.deepEqual(groups.map(g => g.id), input.catalog.groups.map(g => g.id));
const menu = groups.flatMap(g => g.options);
assert.deepEqual(menu, options.filter(o => !o.workload || o.core === 'single'));
assert.equal(menu.filter(o => o.workload).length, 22);
assert.equal(new Set(menu.map(o => o.key)).size, menu.length);
for (const o of options) {
  for (const alias of new Set([o.key, ...aliasesFor(o.className, input.catalog), defaultAlias(o.className), o.className.toLowerCase()]))
    assert.equal(findScenario(options, alias, input.catalog)?.className, o.className, alias);
  if (o.workload) for (const core of ['single', 'baseline', 'advanced']) {
    for (const policy of ['not-taken', 'taken', 'backward-taken', 'one-bit', 'two-bit'])
      assert(resolveProgram(options, o.workload, core, policy), `${o.workload}/${core}/${policy}`);
  }
}
assert.equal(findScenario(options, 'unregistered', input.catalog), undefined);
process.stdout.write(JSON.stringify(options));
"""
        descriptors = [d for d in backend.get_registered_test_descriptors() if d['name'] in server.visualizable_test_names()]
        output = subprocess.run(['node', '--input-type=module', '-e', script], cwd=ROOT,
            input=json.dumps(dict(descriptors=descriptors, catalog=catalog.CATALOG)), text=True,
            capture_output=True, timeout=30)
        self.assertEqual(output.returncode, 0, output.stderr)
        self.assertEqual(json.loads(output.stdout), server.SCENARIO_OPTIONS)

    def test_sequential_and_memory_classification(self):
        rows = {d['name']: d for d in backend.get_registered_test_descriptors()}
        for name in ('MemoryResponseBufferTest', 'RV32IBranchPredictorTest', 'RV32IPredictionPathTest',
                     'RV32IFetchStageTest', 'RV32IBaselineFetchStageTest'):
            self.assertEqual(rows[name]['kind'], 'sequential', name)
        self.assertEqual(rows['NativeMemoryDeviceTest']['kind'], 'memory')
        self.assertEqual(rows['RV32IBaselinePipelineCoreTest']['kind'], 'program')

    def test_static_layout_variants_match_python_topology(self):
        probe = Path(backend.__file__).resolve().parent / 'tests/VisualizerSessionProbe'
        self.assertTrue(probe.exists(), probe)
        for name, suffix in (
            ('FFAlwaysReadyControllerTest', ',always-ready>'),
            ('RegisterFileWidth32Count32Test', '<5,zero-disabled>'),
            ('RV32IBaselinePipelineCoreTest', '<compact>'),
            # Structure-keyed floorplans: memory geometry and policy variants.
            ('CacheScalableScenarioTest', '[SRAMStorageBank<32,8,2>]'),
            ('FFMemoryTest', '[MemoryStorageBank<FFArray,8,4>]'),
            ('RV32IBranchPredictorTest', ']'),
        ):
            with self.subTest(name=name):
                root = server.scenario_topology(name)
                expected = {}
                stack = [root]
                while stack:
                    component = stack.pop()
                    stack.extend(component.get_children())
                    kind = server._component_layout_type(component)
                    if kind.endswith(suffix):
                        expected[component.get_id()] = kind
                self.assertTrue(expected, name)
                alias = server.scenario_canonical_alias(name)
                topology = json.loads(subprocess.check_output(
                    [str(probe), 'topology', name, alias], text=True, timeout=90))
                native = {component['id']: component.get('layoutType', component['type'])
                          for component in topology['components']}
                self.assertEqual({key: native.get(key) for key in expected}, expected)
                del root
                gc.collect()

    def test_legacy_client_uses_the_same_registry_and_aliases(self):
        descriptors = backend.get_registered_test_descriptors()
        for option in server.SCENARIO_OPTIONS:
            for alias in (option['key'], option['className']):
                self.assertEqual(catalog.registered_scenario(descriptors, alias), option['className'])
                self.assertEqual(server.normalize_scenario(alias), option['key'])
        for name in ('Memory64Kx32Test', 'RV32IALUSliceTest', 'unknown'):
            with self.assertRaises(ValueError):
                catalog.registered_scenario(descriptors, name)

    def test_ctest_program_and_benchmark_selection(self):
        result = subprocess.run(['ctest', '--test-dir', str(Path(backend.__file__).parent),
                                 '-N', '--show-only=json-v1'], text=True, capture_output=True,
                                check=True, timeout=30)
        tests = {t['name']: t for t in json.loads(result.stdout)['tests']}
        labels = {name: set(next((p['value'] for p in t['properties'] if p['name'] == 'LABELS'), []))
                  for name, t in tests.items()}
        for name in ('RV32IBenchmarkTest', 'RV32IBenchmarkCLITest', 'RV32IDifferentialTest',
                     'RV32IExperimentTest', 'RV32IExperimentCLITest', 'RV32IBinaryClockCLITest'):
            self.assertIn('benchmark', labels[name], name)
        for d in backend.get_registered_test_descriptors():
            if catalog.program_identity(d['name']):
                self.assertIn(d['name'], tests)
                self.assertEqual(labels[d['name']], set(d['labels']), d['name'])


if __name__ == '__main__':
    unittest.main()
