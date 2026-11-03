#!/usr/bin/env python3
"""Bounded gate-heavy complement to the complete RV32IM cache comparison."""
import argparse
import json
from pathlib import Path
import re
import statistics
import experiment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent', type=Path, default=experiment.ROOT /
                        'build-milestone/engine-performance-20261026/implementation')
    parser.add_argument('--rounds', type=int, default=50000)
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.rounds <= 100000 or not 1 <= args.repeats <= 5:
        raise ValueError('Experiment bounds exceeded')
    parent = args.parent.resolve()
    output = parent / 'fanout'
    output.mkdir()
    rows, references = [], {}
    variants = ['prechange', 'baseline', 'combined']
    for repeat in range(args.repeats):
        order = variants[repeat % len(variants):] + variants[:repeat % len(variants)]
        for mode in ['none', 'digest']:
            for name in order:
                directory = experiment.run(output, f'{repeat}-{mode}-{name}',
                    [parent/name/'probe', 'fanout', str(args.rounds), mode])
                result = json.loads((directory/'stdout.txt').read_text())
                experiment.save(directory/'result.json', result)
                metrics = {v['name']: v['value'] for v in result['metrics']}
                signature = dict(fingerprints=result['fingerprints'], ticks=result['simulatedTicks'],
                    counters={k: v for k, v in metrics.items() if k != 'events_per_second'})
                if mode in references and signature != references[mode]:
                    experiment.save(directory/'mismatch.json',
                                    dict(expected=references[mode], actual=signature))
                    raise RuntimeError('Fanout semantic difference at ' + str(directory))
                references.setdefault(mode, signature)
                resource = (directory/'resources.txt').read_text()
                cpu = sum(float(re.search(label + r': ([0-9.]+)', resource)[1])
                          for label in [r'User time \(seconds\)', r'System time \(seconds\)'])
                rows.append(dict(repeat=repeat, mode=mode, variant=name,
                    hostSeconds=result['hostSeconds'], processCpuSeconds=cpu,
                    directory=str(directory)))
                experiment.save(output/'results.partial.json', rows)
                print(name, mode, 'wall', round(result['hostSeconds'], 4), flush=True)
    summary = []
    for mode in ['none', 'digest']:
        for name in variants:
            matched = [r for r in rows if r['mode'] == mode and r['variant'] == name]
            wall = [r['hostSeconds'] for r in matched]
            summary.append(dict(mode=mode, variant=name, medianWallSeconds=statistics.median(wall),
                minWallSeconds=min(wall), maxWallSeconds=max(wall),
                medianProcessCpuSeconds=statistics.median(r['processCpuSeconds'] for r in matched)))
    experiment.save(output/'results.json', dict(rounds=args.rounds, repeats=args.repeats,
                    rows=rows, summary=summary, references=references))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
