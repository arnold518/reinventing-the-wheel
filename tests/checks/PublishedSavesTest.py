"""The published long-game saves still open in this build.

A save is refused when the machine it was made on has changed (another part,
pin, width or delay table), and the site's simulator is rebuilt from the
current source on every deploy. This check restores one published save of
Pong from each set (Compact and Visual) into a freshly built machine with
GameSessionProbe, which steps, views, jumps back and seeks, and requires every
path to end in the same state. If it fails after a change to a part the games
machine uses, the long games need re-recording (or the change reverting); the
page itself falls back to showing the recorded frames.

    PublishedSavesTest.py --session GameSessionProbe [--games pong-long ...]
"""
import argparse, gzip, hashlib, json, os, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SETS = {'compact': ROOT / 'site-docs/assets/system/long-games', 'visual': ROOT / 'site-docs/assets/system/long-games/visual'}

p = argparse.ArgumentParser()
p.add_argument('--session', required=True)
p.add_argument('--games', nargs='+', default=['pong-long'])
a = p.parse_args()
checked = []
with tempfile.TemporaryDirectory() as directory:
    for preset, folder in SETS.items():
        for game in a.games:
            index_path = folder / game / 'saves' / 'index.json'
            index = json.loads(index_path.read_text())
            assert index['kind'] == 'circuitsim-machine-saves' and index['guest'] == game, index_path
            assert index['fidelityPreset'] == preset, (index_path, index['fidelityPreset'])
            entry = index['saves'][1] if len(index['saves']) > 1 else index['saves'][0]
            packed = (index_path.parent / entry['file']).read_bytes()
            assert hashlib.sha256(packed).hexdigest() == entry['sha256'], f'{entry["file"]}: bytes differ from the index'
            data = gzip.decompress(packed)
            assert len(data) == entry['bytes'], f'{entry["file"]}: another length than the index says'
            save = Path(directory) / f'{preset}-{game}.bin'
            save.write_bytes(data)
            env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE=index['timingProfile'])
            run = subprocess.run([a.session, game, str(save), preset], capture_output=True, text=True, env=env)
            sys.stdout.write(run.stdout); sys.stderr.write(run.stderr)
            assert run.returncode == 0 and f'PASS {game}' in run.stdout, \
                f'{preset} {game}: the published save no longer opens in this build (exit {run.returncode})'
            checked.append(f'{preset} {game} (cycle {entry["cycle"]})')
print('PASS published saves open:', ', '.join(checked))
