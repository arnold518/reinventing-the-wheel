#!/usr/bin/env python3
"""Serial, resumable native acceptance for the scripted RV32IM guest fixtures."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARIANTS = (('baseline', 100000, 'compact'), ('repeat', 100000, 'compact'),
            ('quantum', 97, 'compact'), ('expanded', 100000, 'expanded-core'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def sources():
    names = subprocess.check_output(['git', 'ls-files', '-co', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    return {name: sha(ROOT / name) for name in sorted(set(names))
            if (name == 'CMakeLists.txt' or name.startswith(('include/', 'src/', 'tools/rv32im/', 'tools/build/')))
            and (ROOT / name).is_file()}


def normalized(record, variant):
    record = json.loads(json.dumps(record))
    for key in ('setupSeconds', 'executionSeconds', 'captureSeconds'):
        record.pop(key, None)
    for frame in record['frames']:
        frame.pop('hostSeconds', None)
    if variant != 'repeat':
        record['configuration'].pop('quantumEvents', None)
    if variant == 'expanded':
        for key in ('profileName', 'profileFingerprint', 'events'):
            record.pop(key, None)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runner', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--guest', action='append', help='Select guests; the default is all ten')
    parser.add_argument('--timeout', type=int, default=3600)
    parser.add_argument('--memory-mib', type=int, default=768)
    args = parser.parse_args()
    args.runner = args.runner.resolve()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    fixture_root = ROOT / 'tests/fixtures/rv32im/games'
    fixture_file = fixture_root / 'manifest.json'
    manifest = json.loads(fixture_file.read_text())
    names = [item['name'] for item in manifest['guests']]
    if args.guest and (len(set(args.guest)) != len(args.guest) or set(args.guest) - set(names)):
        parser.error('Guests must be distinct fixture names')
    selected = args.guest or names
    provenance = {'sourceSha256': sources(), 'runnerSha256': sha(args.runner),
                  'fixtureManifestSha256': sha(fixture_file)}
    for fixture in manifest['guests']:
        for file_key, digest_key in (('elf', 'elfSha256'), ('script', 'scriptSha256')):
            if sha(fixture_root / fixture[file_key]) != fixture[digest_key]:
                raise RuntimeError('Fixture provenance mismatch: ' + fixture['name'])
    rows = []
    for guest in selected:
        baseline = None
        for variant, quantum, profile in VARIANTS:
            output = args.output / guest / variant
            if (output / 'recording.json').exists() and not (output / 'evidence.json').exists():
                archive = args.output / '_incomplete'
                archive.mkdir(exist_ok=True)
                output.rename(archive / f'{guest}-{variant}-{time.time_ns()}')
            evidence_file = output / 'evidence.json'
            if evidence_file.exists():
                evidence = json.loads(evidence_file.read_text())
                for key, value in provenance.items():
                    if evidence[key] != value:
                        raise RuntimeError(f'{guest}/{variant}: {key} differs; choose a fresh campaign directory')
                if (evidence['guest'] != guest or evidence['machine'] != 'ff'
                        or evidence['logicProfile'] != profile or evidence['quantumEvents'] != quantum
                        or evidence['recordingSha256'] != sha(output / 'recording.json')
                        or evidence['limits']['cycles'] != 500000):
                    raise RuntimeError(f'{guest}/{variant}: cached evidence differs')
                print('RESUME checked provenance', guest, variant, flush=True)
            else:
                if sources() != provenance['sourceSha256'] or sha(args.runner) != provenance['runnerSha256']:
                    raise RuntimeError('Source or binary changed during acceptance')
                output.mkdir(parents=True, exist_ok=True)
                command = [sys.executable, str(ROOT / 'tools/rv32im/run-recorded.py'), guest,
                           '--runner', str(args.runner), '--output', str(output),
                           '--quantum-events', str(quantum), '--logic-profile', profile,
                           '--memory-mib', str(args.memory_mib), '--timeout', str(args.timeout)]
                print('RUN', guest, variant, flush=True)
                with (output / 'run.log').open('w') as log:
                    subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                   timeout=args.timeout + 60, check=True)
                evidence = json.loads(evidence_file.read_text())
            with (output / 'reference.log').open('w') as log:
                subprocess.run([sys.executable, str(ROOT / 'tests/checks/RV32IMGameReferenceTest.py'),
                                '--recording', str(output / 'recording.json'), '--guest', guest,
                                '--output', str(output / 'reference')], cwd=ROOT, stdout=log,
                               stderr=subprocess.STDOUT, timeout=60, check=True)
            record = json.loads((output / 'recording.json').read_text())
            if baseline is None:
                baseline = record
            elif normalized(record, variant) != normalized(baseline, variant):
                raise RuntimeError(f'{guest}/{variant}: deterministic recording differs')
            rows.append({'guest': guest, 'comparison': variant, 'cycles': record['cycles'],
                         'instructions': record['instructions'], 'presentations': record['presentations'],
                         'changedFrames': record['changedFrames'], 'executionSeconds': record['executionSeconds'],
                         'captureSeconds': record['captureSeconds'], 'peakRssKiB': evidence['peakRssKiB'],
                         'events': record['events']})
            (args.output / 'comparisons.json').write_text(json.dumps(rows, indent=2) + '\n')
            print('PASS', guest, variant, record['cycles'], 'cycles', record['executionSeconds'], 'seconds', flush=True)
    performance = []
    for guest in selected:
        output = args.output / guest / 'baseline'
        record = json.loads((output / 'recording.json').read_text())
        evidence = json.loads((output / 'evidence.json').read_text())
        frames = record['frames']
        updates = len(frames) - 1
        if updates < 1:
            raise RuntimeError(guest + ': insufficient captured updates')
        seconds = frames[-1]['hostSeconds'] - frames[0]['hostSeconds']
        cycles = frames[-1]['cycle'] - frames[0]['cycle']
        changed = sum((a['pixels'], a['known']) != (b['pixels'], b['known']) for a, b in zip(frames, frames[1:]))
        performance.append({'guest': guest, 'cycles': record['cycles'], 'instructions': record['instructions'],
            'firstFrameSeconds': frames[0]['hostSeconds'], 'setupSeconds': record['setupSeconds'],
            'executionSeconds': record['executionSeconds'], 'captureSeconds': record['captureSeconds'],
            'capturePercentOfExecution': 100 * record['captureSeconds'] / record['executionSeconds'],
            'events': record['events'], 'ramAllocatedBytes': record['ramAllocatedBytes'],
            'videoAllocatedBytes': record['videoAllocatedBytes'], 'peakRssMiB': evidence['peakRssKiB'] / 1024,
            'laterUpdates': updates, 'laterChangedUpdates': changed, 'meanCyclesPerUpdate': cycles / updates,
            'presentedUpdatesPerSecond': updates / seconds, 'changedUpdatesPerSecond': changed / seconds,
            'elfSha256': record['elfSha256'], 'scriptSha256': record['scriptSha256'],
            'runnerSha256': evidence['runnerSha256'], 'frameSha256': evidence['frameSha256']})
    (args.output / 'performance.json').write_text(json.dumps(performance, indent=2) + '\n')
    (args.output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(f'PASS {len(selected)} guests x 4 runs: exact references, repeats, event quanta and expanded core', flush=True)
    print('Batch update rates include ISA/device checking; these are not delivered live FPS.', flush=True)


if __name__ == '__main__':
    main()
