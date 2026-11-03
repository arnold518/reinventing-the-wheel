"""Run a guest fixture's C on the host with stand-in devices.

The same guest sources are compiled for the host with
tests/fixtures/rv32im/games/reference-host.c; the result holds the report,
console, button levels and pixels at every presented frame. The circuit runs
are compared with it (tests/checks/RV32IMGameReferenceTest.py and
tools/rv32im/run-long-gameplays.py).
"""
import json, os, pathlib, subprocess, tempfile
from .session import session_header

ROOT = pathlib.Path(__file__).resolve().parents[3]
FIXTURES = ROOT / 'tests/fixtures/rv32im/games'


def manifest():
    return json.loads((FIXTURES / 'manifest.json').read_text())


def fixture(name):
    return next(f for f in manifest()['guests'] if f['name'] == name)


def host_reference(row):
    """Compile and run the fixture `row` (a manifest entry) on the host."""
    guest = ROOT / 'tools/rv32im/guest'
    script = json.loads((FIXTURES / row['script']).read_text())
    with tempfile.TemporaryDirectory(prefix='circuitsim-guest-reference-') as directory:
        directory = pathlib.Path(directory)
        defines = [f'-D{k}={v}' for k, v in row['defines'].items()]
        defines += [f'-DREFERENCE_WIDTH={row["width"]}', f'-DREFERENCE_HEIGHT={row["height"]}']
        if row['system'] == 'games-machine-leds':
            defines.append('-DSYSTEM_HEADER="system-leds.h"')
        if row['version'] == 'long':
            plan = {'batches': script['batches'], 'levels': row['levels'], 'players': row['players']}
            (directory / 'session.h').write_text(session_header(plan))
        else:
            events = script['events']
            defines += ['-DREFERENCE_BUTTONS=' + ','.join(str(e['button']) for e in events),
                        '-DREFERENCE_DOWN=' + ','.join(str(int(e['down'])) for e in events)]
        compiler = os.environ.get('CC', 'cc')
        common = ['-std=c11', '-O1', '-fno-builtin', '-Wall', '-Wextra', '-Werror', '-I', str(guest),
                  '-I', str(directory), '-DCIRCUITSIM_GUEST_REFERENCE', *defines]
        sources = ['platform.c', 'bringup.c'] if not row['id'] else ['platform.c', 'games.c', 'main.c']
        objects = []
        for name in sources:
            obj = directory / (name + '.o')
            subprocess.run([compiler, *common, '-Dmain=guest_main', '-c', str(guest / name), '-o', str(obj)], check=True)
            objects.append(str(obj))
        binary = directory / 'reference'
        subprocess.run([compiler, *common, str(FIXTURES / 'reference-host.c'), *objects, '-o', str(binary)], check=True)
        return json.loads(subprocess.check_output([str(binary)], text=True))
