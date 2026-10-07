"""Recovery bookkeeping only; no large benchmarks are executed."""
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('resume', ROOT / 'tools/benchmarks/resume-background.py')
resume = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resume)


class ResumeTest(unittest.TestCase):
    def fixture(self, root):
        (root / 'snapshot').mkdir()
        (root / 'snapshot/program.elf').write_bytes(b'fixture')
        manifest = {'pid': 0, 'status': 'running', 'workloads': [
            {'name': 'program', 'elf': 'program.elf', 'sha256': 'hash', 'preflight': {}}],
            'jobs': [
                {'id': 'program/single', 'workload': 'program', 'status': 'passed',
                 'architectural_signature': ['image', 'state', 19, 0], 'timeout_seconds': 7200},
                {'id': 'program/baseline', 'workload': 'program', 'status': 'running',
                 'reference_check': True, 'timeout_seconds': 7200, 'started_unix': 1},
                {'id': 'program/advanced', 'workload': 'program', 'status': 'pending',
                 'reference_check': True, 'timeout_seconds': 7200}]}
        case = root / 'cases/program/single'
        case.mkdir(parents=True)
        (case / 'COMPLETE').write_text('verified')
        rows = [{'phase': phase, 'signature': ['image', 'state', 19, 0]}
                for phase in ('oracle-check', 'measurement')]
        (case / 'runs.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in rows))
        partial = root / 'cases/program/baseline'
        partial.mkdir()
        (partial / 'runs.jsonl').write_text('partial attempt\n')
        partial.with_suffix('.log').write_text('old timeout\n')
        (root / 'campaign.json').write_text(json.dumps(manifest))
        runner = SimpleNamespace(check_snapshot=lambda path: None, digest=lambda path: 'hash',
            architectural_signature=lambda row: tuple(row['signature']),
            save=lambda path, value: path.write_text(json.dumps(value)))
        return manifest, runner

    def test_preserve_passed_archive_partial_and_increase_timeout(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest, runner = self.fixture(root)
            complete = root / 'cases/program/single/runs.jsonl'
            original = complete.read_bytes(), complete.stat().st_mtime_ns
            signatures = resume.recover(root, manifest, runner, 21600)
            self.assertEqual(signatures, {'program': ('image', 'state', 19, 0)})
            self.assertEqual(original, (complete.read_bytes(), complete.stat().st_mtime_ns))
            self.assertEqual([j['status'] for j in manifest['jobs']], ['passed', 'pending', 'pending'])
            self.assertEqual([j['timeout_seconds'] for j in manifest['jobs']], [7200,21600,21600])
            self.assertFalse((root / 'cases/program/baseline').exists())
            archive = root / manifest['resumptions'][-1]['archive']
            self.assertEqual((archive / 'cases/program/baseline/runs.jsonl').read_text(), 'partial attempt\n')
            self.assertEqual((archive / 'cases/program/baseline.log').read_text(), 'old timeout\n')

    def test_missing_complete_refuses_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest, runner = self.fixture(root)
            (root / 'cases/program/single/COMPLETE').unlink()
            with self.assertRaisesRegex(RuntimeError, 'no COMPLETE'):
                resume.recover(root, manifest, runner, 21600)
            self.assertFalse((root / 'restarts').exists())

    def test_missing_reference_refuses_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest, runner = self.fixture(root)
            manifest['jobs'][0]['reference_check'] = True
            with self.assertRaisesRegex(RuntimeError, 'does not match'):
                resume.recover(root, manifest, runner, 21600)
            self.assertFalse((root / 'restarts').exists())


if __name__ == '__main__':
    unittest.main()
