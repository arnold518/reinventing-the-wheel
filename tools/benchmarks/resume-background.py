#!/usr/bin/env python3
"""Resume an interrupted frozen campaign without rebuilding or replacing results."""
import argparse
import fcntl
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def frozen_runner(output):
    sys.dont_write_bytecode = True
    path = output / 'snapshot/tools/benchmarks/run-full-embench-background.py'
    spec = importlib.util.spec_from_file_location('frozen_campaign', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate_finished(output, manifest, runner):
    signatures = {}
    for job in manifest['jobs']:
        if job['status'] != 'passed':
            continue
        case = output / 'cases' / job['id']
        if not (case / 'COMPLETE').is_file():
            raise RuntimeError('Passed case has no COMPLETE marker: ' + job['id'])
        rows = [json.loads(line) for line in (case / 'runs.jsonl').read_text().splitlines()]
        phases = ['oracle-check', 'measurement']
        if job.get('reference_check'):
            phases.insert(1, 'reference')
        observed = {row['phase']: runner.architectural_signature(row) for row in rows}
        expected = tuple(job['architectural_signature'])
        if any(observed.get(phase) != expected for phase in phases):
            raise RuntimeError('Completed result does not match its manifest: ' + job['id'])
        if signatures.setdefault(job['workload'], expected) != expected:
            raise RuntimeError('Existing cross-core signatures disagree')
    return signatures


def recover(output, manifest, runner, timeout):
    runner.check_snapshot(output / 'snapshot')
    for workload in manifest['workloads']:
        if 'preflight' not in workload:
            raise RuntimeError('Resume requires completed preflight estimates')
        if runner.digest(output / 'snapshot' / workload['elf']) != workload['sha256']:
            raise RuntimeError('Frozen ELF modified: ' + workload['name'])
    signatures = validate_finished(output, manifest, runner)
    archive = output / 'restarts' / (time.strftime('%Y%m%d-%H%M%S') + '-' + str(os.getpid()))
    archive.mkdir(parents=True, exist_ok=False)
    shutil.copy2(output / 'campaign.json', archive / 'campaign.json')
    for marker in ('FINISHED', 'COMPLETE', 'worker.exit', 'worker.pid'):
        path = output / marker
        if path.exists():
            path.rename(archive / marker)
    for job in manifest['jobs']:
        if job['status'] == 'passed':
            continue
        for path in (output / 'cases' / job['id'], (output / 'cases' / job['id']).with_suffix('.log')):
            if path.exists():
                destination = archive / path.relative_to(output)
                destination.parent.mkdir(parents=True, exist_ok=True)
                path.rename(destination)
        job['status'] = 'pending'
        for key in ('started_unix', 'finished_unix', 'error', 'architectural_signature'):
            job.pop(key, None)
        if job.get('reference_check'):
            job['timeout_seconds'] = max(job['timeout_seconds'], timeout)
    for key in ('finished_unix', 'error'):
        manifest.pop(key, None)
    manifest.update(status='running', pid=os.getpid(), current_job=None, resumed_unix=time.time())
    manifest.setdefault('resumptions', []).append({'archive': str(archive.relative_to(output)),
        'time': time.time(), 'reference_timeout_seconds': timeout})
    runner.save(output / 'campaign.json', manifest)
    (output / 'worker.pid').write_text(str(os.getpid()) + '\n')
    return signatures


def worker(output, timeout):
    with (output / 'resume.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        runner = frozen_runner(output)
        manifest = json.loads((output / 'campaign.json').read_text())
        old_pid = manifest.get('pid', 0)
        try:
            old_command = Path(f'/proc/{old_pid}/cmdline').read_bytes()
        except FileNotFoundError:
            old_command = b''
        if old_pid != os.getpid() and str(output).encode() in old_command:
            raise RuntimeError('A worker for this campaign is already alive')
        signatures = recover(output, manifest, runner, timeout)
        os.nice(10)
        lookup = {workload['name']: workload for workload in manifest['workloads']}
        print('Resumed; completed cases preserved, partial attempts archived', flush=True)
        for job in manifest['jobs']:
            if job['status'] != 'pending':
                continue
            job.update(status='running', started_unix=time.time())
            manifest['current_job'] = job['id']
            runner.save(output / 'campaign.json', manifest)
            print(f"Starting {job['id']}: timeout {job['timeout_seconds']}s/process", flush=True)
            try:
                signature = runner.run_job(output, output / 'snapshot', job, lookup[job['workload']])
                if signatures.setdefault(job['workload'], signature) != signature:
                    raise RuntimeError('Cross-core/predictor architectural result mismatch')
                job.update(status='passed', architectural_signature=signature)
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyError, StopIteration) as error:
                job.update(status='failed', error=str(error))
            job['finished_unix'] = time.time()
            runner.save(output / 'campaign.json', manifest)
            print(f"Finished {job['id']}: {job['status']}", flush=True)
        success = all(job['status'] == 'passed' for job in manifest['jobs'])
        manifest.update(status='completed' if success else 'finished-with-failures',
                        finished_unix=time.time(), current_job=None)
        runner.save(output / 'campaign.json', manifest)
        (output / 'FINISHED').write_text(manifest['status'] + '\n')
        (output / 'worker.exit').write_text('0\n' if success else '1\n')
        if success:
            (output / 'COMPLETE').write_text(f"All {len(manifest['jobs'])} cases passed.\n")
        return 0 if success else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference-timeout', type=int, default=21600)
    parser.add_argument('--worker', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve(strict=True)
    if args.reference_timeout <= 0:
        parser.error('Timeout must be positive')
    if args.worker:
        return worker(output, args.reference_timeout)
    with (output / 'resume.log').open('a') as log:
        process = subprocess.Popen([sys.executable, '-u', str(Path(__file__).resolve()), '--worker',
            '--output', str(output), '--reference-timeout', str(args.reference_timeout)],
            cwd=output, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
            start_new_session=True)
    time.sleep(0.3)
    if process.poll() is not None:
        raise RuntimeError('Recovery worker exited; inspect resume.log')
    print(f'Recovery PID {process.pid}: {output}/resume.log')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
