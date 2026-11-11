#!/usr/bin/env python3
"""Run the chapter-5 machine experiments serially, one heavy job at a time.

Each run is `system_experiment` for one game on one machine. Results are
written as one JSON file per run under OUTPUT (resumable: an existing result
is kept) and summarized in OUTPUT/summary.json.

  run-experiments.py --tool build/system_experiment --output build/system-experiments \
      [--profile unit|embedded|desktop] [--period TICKS] [--games bringup pong-short ...] \
      [--machines own|games-machine ...] [--memory sram|ff|dram|cached-...] [--fidelity compact] [--record]

`own` runs each fixture on the machine it was built for (the LED games on the
LED machine).
"""
import argparse, json, os, pathlib, subprocess, sys, time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--tool', type=pathlib.Path, required=True)
p.add_argument('--output', type=pathlib.Path, required=True)
p.add_argument('--profile', default='unit')
p.add_argument('--period', type=int, default=1000)
p.add_argument('--games', nargs='+', default=['bringup', '2048-short', 'sokoban-short', 'snake-short', 'pong-short',
                                               'tetris-short'])
p.add_argument('--machines', nargs='+', default=['games-machine', 'games-machine-two-bus', 'games-machine-registered-bridge'])
p.add_argument('--memory', default='sram')
p.add_argument('--fidelity', default='compact')
p.add_argument('--record', action='store_true', help='also write a visualizer recording per run')
p.add_argument('--memory-max', default='1500M')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE=a.profile, XDG_RUNTIME_DIR=f'/run/user/{os.getuid()}')
summary = []
for machine in a.machines:
    for game in a.games:
        stem = f'{game}--{machine}--{a.memory}--{a.fidelity}--{a.profile}--{a.period}'
        result = a.output / f'{stem}.json'
        if not result.exists():
            command = ['systemd-run', '--user', '--scope', '-q', '-p', f'MemoryMax={a.memory_max}', '-p', 'MemorySwapMax=0',
                       str(a.tool), '--guest', game, '--machine', '' if machine == 'own' else machine, '--memory', a.memory,
                       '--fidelity', a.fidelity, '--period', str(a.period)]
            if a.record:
                command += ['--recording', str(a.output / f'{stem}.recording.json')]
            start = time.time()
            run = subprocess.run(command, env=env, capture_output=True, text=True)
            if run.returncode:
                print(f'FAIL {stem}: {run.stderr.strip().splitlines()[-1] if run.stderr.strip() else run.returncode}', flush=True)
                continue
            result.write_text(run.stdout)
            print(f'done {stem} in {time.time() - start:.0f} s', flush=True)
        d = json.loads(result.read_text())
        summary.append({'game': game, 'machine': d['system'], 'memory': a.memory, 'fidelity': a.fidelity, 'profile': a.profile,
                        'period': a.period, 'cycles': d['cycles'], 'instructions': d['instructions'],
                        'presentations': d['presentations'], 'events': d['events']['processed'],
                        'executionSeconds': round(d['executionSeconds'], 1), 'console': d['console'],
                        'report': d['report'], 'frames': [f['pixels'] for f in d['frames']]})
(a.output / 'summary.json').write_text(json.dumps(summary, indent=1) + '\n')
print(f'{len(summary)} results in {a.output / "summary.json"}')
