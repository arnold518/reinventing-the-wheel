#!/usr/bin/env python3
"""Write the chapter-5 result tables from the experiment summaries.

Replaces the text between `<!-- results: NAME -->` and `<!-- /results -->`
in site-docs/system/5.6.md and 5.7.md. Summaries come from
tools/system/run-experiments.py (copied to site-docs/assets/system/results/)
and, for the long games, from the published long-game campaign
(site-docs/assets/system/long-games/manifest.json).

  results-tables.py [--check]
"""
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RESULTS = ROOT / 'site-docs/assets/system/results'
PAGES = [ROOT / 'site-docs/system/5.6.md', ROOT / 'site-docs/system/5.7.md']
LONG = ROOT / 'site-docs/assets/system/long-games/manifest.json'
LONG_VISUAL = ROOT / 'site-docs/assets/system/long-games/visual/manifest.json'
GAMES = ['bringup', '2048-short', 'sokoban-short', 'snake-short', 'pong-short', 'tetris-short', 'snake-leds-short']
TITLES = {'bringup': 'Bring-up', '2048-short': '2048', 'sokoban-short': 'Sokoban', 'snake-short': 'Snake',
          'pong-short': 'Pong', 'tetris-short': 'Tetris', 'snake-leds-short': 'Snake with LEDs',
          # Fixtures retired on 2026-11-03, kept for the old-machine comparison.
          'pong': 'Pong', 'snake': 'Snake'}
SHORT = [('2048-short', 'game-2048', 'Four 2s in the top row', 'LEFT'),
         ('sokoban-short', 'game-sokoban', 'Player, crate and goal in a row', 'RIGHT'),
         ('snake-short', 'game-snake', 'Food two cells ahead', 'UPDATE, UPDATE'),
         ('pong-short', 'game-pong', 'Ball one step from the right paddle', 'UPDATE, UPDATE'),
         ('tetris-short', 'game-tetris', 'Bottom row missing four cells, an I piece above', 'ACTION (hard drop)'),
         ('snake-leds-short', 'game-snake-leds', 'Head one cell from the wall', 'UPDATE, UPDATE')]
LONG_RESULTS = {'2048-long': 'Tile 128, score 852', 'sokoban-long': 'Both puzzles solved, 20 pushes',
                'snake-long': 'Length 10, then the wall', 'pong-long': 'Two points, 12 paddle bounces',
                'tetris-long': '20 pieces, 8 lines, score 800', 'snake-leds-long': 'Length 10, then the wall; LEDs blink'}
MACHINES = ['games-machine', 'games-machine-two-bus', 'games-machine-registered-bridge']


def load(name):
    return json.loads((RESULTS / f'{name}.json').read_text())


def n(x):
    return f'{x:,}'


def duration(microseconds):
    return f'{microseconds / 1000:.2f} ms' if microseconds >= 1000 else f'{microseconds:,.1f} µs'


def short_games():
    rows = ['| Scenario | Prepared board | Presses | Frames | Cycles | Instructions | Simulated time | Host time |',
            '| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |']
    data = {r['game']: r for r in load('embedded')}
    for game, alias, board, presses in SHORT:
        r = data[game]
        simulated = r['cycles'] * r['period'] / 1e6  # ps -> microseconds
        rows.append(f"| [`{alias}`](../../simulator/?scenario={alias}) | {board} | {presses} | {r['presentations']} | "
                    f"{n(r['cycles'])} | {n(r['instructions'])} | {simulated:,.1f} µs | {r['executionSeconds']:.0f} s |")
    return '\n'.join(rows)


def host_time(seconds):
    minutes = round(seconds / 60)
    return f'{minutes} min' if minutes < 90 else f'{minutes // 60} h {minutes % 60} min'


def long_games(manifest=LONG):
    campaign = json.loads(manifest.read_text())
    rows = ['| Recording | Updates | Frames | Result | Cycles | Instructions | Simulated time | Host time |',
            '| --- | ---: | ---: | --- | ---: | ---: | ---: | ---: |']
    for job in campaign['games']:
        name = job['name']
        if job['status'] != 'passed':
            rows.append(f"| `{name}` | {job['frames'] - 1} | {job['frames']} | {LONG_RESULTS[name]} | {job['status']} | | | |")
            continue
        simulated = job['cycles'] * campaign['period'] / 1e6
        folder = manifest.parent.relative_to(ROOT / 'site-docs').as_posix()
        link = f"[`{name}`](../../simulator/?recording=../{folder}/{job['playlist']})"
        rows.append(f"| {link} | {job['frames'] - 1} | {job['frames']} | {LONG_RESULTS[name]} | {n(job['cycles'])} | "
                    f"{n(job['instructions'])} | {duration(simulated)} | {host_time(job['executionSeconds'])} |")
    return '\n'.join(rows)


# Report words that are timer samples (bring-up: words 0 and 5; games: word 7):
# they count clock ticks, so they differ wherever the cycle counts differ.
TIMER_WORDS = {'bringup': (0, 5)}


def program_results(r):
    words = [r['report'][i:i + 8] for i in range(0, len(r['report']), 8)]
    for index in TIMER_WORDS.get(r['game'], (7,)):
        words[index] = None
    return r['frames'], r['console'], words


def machines():
    data = {(r['game'], r['machine']): r for r in load('unit')}
    rows = ['| Game | One bus | Two buses | Registered bridge | Two buses vs one | Registered vs one |',
            '| --- | ---: | ---: | ---: | ---: | ---: |']
    for game in GAMES:
        cells = [data.get((game, m)) for m in MACHINES]
        if not all(cells):
            continue
        base = cells[0]['cycles']
        if any(program_results(c) != program_results(cells[0]) for c in cells):
            raise SystemExit(f'{game}: machines disagree on frames, console or report')
        rows.append(f"| {TITLES[game]} | {n(base)} | {n(cells[1]['cycles'])} | {n(cells[2]['cycles'])} | "
                    f"+{n(cells[1]['cycles'] - base)} (+{(cells[1]['cycles'] / base - 1) * 100:.2f}%) | "
                    f"+{n(cells[2]['cycles'] - base)} (+{(cells[2]['cycles'] / base - 1) * 100:.2f}%) |")
    return '\n'.join(rows)


def baseline():
    data = {(r['game'], r['machine'], r['memory']): r for r in load('unit-retired-fixtures') + load('unit-cached')}
    old = load('old-machine')
    rows = ['| Game | Memory | Old machine cycles | New machine cycles | Same results | Events (old → new) |',
            '| --- | --- | ---: | ---: | :---: | --- |']
    for o in old:
        r = data.get((o['game'], 'games-machine', o['memory']))
        if not r:
            continue
        same = r['frames'] == o['frames'] and r['console'] == o['console'] and r['report'] == o['report'] and r['cycles'] == o['cycles']
        rows.append(f"| {TITLES[o['game']]} | {o['memory']} | {n(o['cycles'])} | {n(r['cycles'])} | {'yes' if same else 'NO'} | "
                    f"{n(o['events'])} → {n(r['events'])} |")
    return '\n'.join(rows)


def clocks():
    rows = ['| Machine | Memory | Unit profile (ticks) | Embedded profile | Embedded clock | Limiting path (embedded) |',
            '| --- | --- | ---: | ---: | ---: | --- |']
    for c in load('clock'):
        rows.append(f"| `{c['machine']}` | {c['memory']} | {c['unit']} | {c['embedded_ps']:,} ps | {1e6 / c['embedded_ps']:.0f} MHz | {c['limit_embedded']} |")
    return '\n'.join(rows)


TABLES = {'short-games': short_games, 'long-games': long_games, 'long-games-visual': lambda: long_games(LONG_VISUAL), 'machines': machines, 'baseline': baseline, 'clocks': clocks}
check = '--check' in sys.argv
stale = False
for page in PAGES:
    text = page.read_text()

    def fill(m):
        try:
            table = TABLES[m[1]]()
        except FileNotFoundError:
            return m[0]  # its campaign has not finished; keep the old table
        return f'<!-- results: {m[1]} -->\n{table}\n<!-- /results -->'
    new = re.sub(r'<!-- results: ([\w-]+) -->.*?<!-- /results -->', fill, text, flags=re.S)
    if new != text:
        stale = True
        if not check:
            page.write_text(new)
if check and stale:
    raise SystemExit('Result tables are stale; run tools/system/results-tables.py')
print('result tables', 'checked' if check else 'written')
