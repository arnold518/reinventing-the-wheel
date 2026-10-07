"""Guard complete web-test registration, artifact selection and bounded execution."""
import argparse
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('web_runner', ROOT / 'tools/run-web-tests.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class WebTestRunnerTest(unittest.TestCase):
    def test_manifest_covers_every_external_browser_and_wasm_script(self):
        rows = runner.test_inventory()
        files = {r['file'] for r in rows}
        expected = {p.name for pattern in ('*BrowserTest.py', '*WasmParityTest.mjs', '*WasmRuntimeTest.mjs')
                    for p in (ROOT / 'tests').rglob(pattern)}
        self.assertEqual(files, expected)
        self.assertTrue(all(r['timeout'] > 0 for r in rows))

    def test_suites_and_explicit_selection(self):
        rows = runner.test_inventory()
        self.assertEqual(runner.select_tests(rows, 'all', None), rows)
        self.assertTrue(all(r['kind'] == 'parity' for r in runner.select_tests(rows, 'parity', None)))
        self.assertEqual([r['name'] for r in runner.select_tests(rows, 'smoke', ['ALU32WasmParityTest'])], ['ALU32WasmParityTest'])
        with self.assertRaisesRegex(ValueError, 'Unknown web tests'):
            runner.select_tests(rows, 'all', ['missing'])

    def test_commands_use_selected_artifacts_and_interpreter(self):
        args = argparse.Namespace(wasm_dir=Path('/chosen/wasm'), build_dir=Path('/chosen/native'),
                                  browser_python='/chosen/python', node='/chosen/node')
        rows = runner.test_inventory()
        parity = runner.command_for(next(r for r in rows if r['name'] == 'ALU32WasmParityTest'), args, 'http://test/')
        self.assertEqual(parity[0], '/chosen/node')
        self.assertEqual(parity[2:], ['/chosen/wasm/pages/simulator/circuitsim_wasm.js',
                                     '/chosen/wasm/pages/simulator/circuitsim_wasm.wasm', '/chosen/native/tests/VisualizerSessionProbe'])
        browser = runner.command_for(next(r for r in rows if r['name'] == 'VisualizerScenarioBrowserTest'), args, 'http://test/')
        self.assertEqual(browser[0], '/chosen/python')
        self.assertEqual(browser[-1], 'http://test/simulator/')

    def test_failed_and_timed_out_checks_are_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / 'test.log'
            self.assertEqual(runner.run_check([sys.executable, '-c', 'raise SystemExit(3)'], log, 5, None), 3)
            self.assertEqual(runner.run_check([sys.executable, '-c', 'import time; time.sleep(10)'], log, .1, None), 124)


if __name__ == '__main__':
    unittest.main()
