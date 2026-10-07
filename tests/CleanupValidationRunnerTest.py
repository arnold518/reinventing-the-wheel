"""Check cleanup resumption without launching builds, browsers or benchmarks."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]


class CleanupValidationRunnerTest(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('cleanup_runner', ROOT / 'tools/validate-cleanup.py')
        self.runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.runner)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        base = Path(temporary.name)
        self.runner.LOGS = base / 'logs'
        self.runner.WASM = base / 'wasm'
        for artifact in ('index.html', 'simulator/index.html', 'simulator/circuitsim_wasm.wasm'):
            path = self.runner.WASM / 'site' / artifact
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        self.runner.run = Mock(side_effect=lambda name, *_args, **_kwargs: self.runner.completed.append(name))
        self.runner.native_and_wasm_checks = Mock()
        self.http = self.enterContext(patch.object(self.runner.http.server, 'ThreadingHTTPServer'))
        self.http.return_value.__enter__.return_value.server_port = 12345
        self.enterContext(patch.object(self.runner.threading, 'Thread'))

    def test_browser_resume_uses_selected_python_and_labels_its_scope(self):
        self.runner.main(['--browser-only', '--browser-python', '/test/python'])
        self.runner.native_and_wasm_checks.assert_not_called()
        calls = self.runner.run.call_args_list
        self.assertEqual(calls[0].args[0], 'browser-preflight')
        self.assertEqual(len(calls), 1 + len(self.runner.BROWSER_TESTS))
        for call in calls:
            self.assertEqual(call.args[1][0], '/test/python')
        for call in calls[1:]:
            self.assertIn('tools/run-web-tests.py', call.args[1])
        status = json.loads((self.runner.LOGS / 'status.json').read_text())
        self.assertEqual(status['phase'], 'passed')
        self.assertEqual(status['scope'], 'browser-only')
        self.assertNotIn('native-tests', status['completed'])
        self.http.return_value.__enter__.return_value.shutdown.assert_called_once()

    def test_preflight_failure_prevents_builds_and_browser_suite(self):
        self.runner.run.side_effect = RuntimeError('Playwright unavailable')
        with self.assertRaisesRegex(RuntimeError, 'Playwright unavailable'):
            self.runner.main([])
        self.runner.native_and_wasm_checks.assert_not_called()
        self.http.assert_not_called()
        status = json.loads((self.runner.LOGS / 'status.json').read_text())
        self.assertEqual(status['phase'], 'failed')
        self.assertEqual(status['scope'], 'full')

    def test_resume_requires_built_site(self):
        self.runner.WASM = self.runner.WASM / 'missing'
        with self.assertRaisesRegex(RuntimeError, 'Missing built site artifact'):
            self.runner.main(['--browser-only'])
        self.runner.native_and_wasm_checks.assert_not_called()
        self.http.assert_not_called()

    def test_full_mode_keeps_build_and_parity_phase(self):
        self.runner.main([])
        self.runner.native_and_wasm_checks.assert_called_once()
        self.assertEqual(json.loads((self.runner.LOGS / 'status.json').read_text())['scope'], 'full')

    def test_selected_browser_retry_does_not_repeat_completed_suites(self):
        self.runner.main(['--browser-only', '--browser-test', 'VisualizerWasmBrowserTest'])
        self.assertEqual(self.runner.completed, ['browser-preflight', 'VisualizerWasmBrowserTest'])
        self.runner.native_and_wasm_checks.assert_not_called()


if __name__ == '__main__':
    unittest.main()
