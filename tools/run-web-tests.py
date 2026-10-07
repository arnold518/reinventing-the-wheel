#!/usr/bin/env python3
"""Run registered browser/WASM checks serially against one chosen artifact."""
import argparse
import functools
import http.server
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'tests/web-tests.json'


class StaticSiteHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format, *_args):
        # Browser checks log assertions to their own files; asset GETs would
        # otherwise bury the test names and final result in the runner output.
        pass


def test_inventory():
    data = json.loads(MANIFEST.read_text())
    if data['schema'] != 1:
        raise ValueError('Unsupported web-test manifest')
    rows = data['tests']
    if len({r['name'] for r in rows}) != len(rows):
        raise ValueError('Duplicate web-test name')
    for row in rows:
        if not (ROOT / 'tests' / row['file']).is_file():
            raise ValueError('Missing web test: ' + row['file'])
    return rows


def select_tests(rows, suite, names):
    unknown = set(names or ()) - {r['name'] for r in rows}
    if unknown:
        raise ValueError('Unknown web tests: ' + ', '.join(sorted(unknown)))
    return [r for r in rows if r['name'] in names] if names else [
        r for r in rows if suite == 'all' or suite in r['suites'] or suite == r['kind']]


def command_for(row, args, base):
    values = dict(js=str(args.wasm_dir / 'pages/simulator/circuitsim_wasm.js'),
                  wasm=str(args.wasm_dir / 'pages/simulator/circuitsim_wasm.wasm'),
                  probe=str(args.build_dir / 'tests/VisualizerSessionProbe'), base=base,
                  simulator=base + 'simulator/')
    executable = args.browser_python if row['kind'] == 'browser' else args.node
    return [executable, str(ROOT / 'tests' / row['file']),
            *[argument.format(**values) for argument in row['args']]]


def run_check(command, log, timeout, env):
    with log.open('w') as output:
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            return process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            return 124


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=['smoke', 'cleanup', 'browser', 'parity', 'all'], default='smoke')
    parser.add_argument('--test', action='append', help='Select named checks instead of a suite (repeatable)')
    parser.add_argument('--list', action='store_true', help='Print selected checks without running them')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--wasm-dir', type=Path, default=ROOT / 'build-wasm')
    parser.add_argument('--browser-python', default=os.environ.get('CIRCUITSIM_BROWSER_PYTHON', sys.executable))
    parser.add_argument('--node', default='node')
    parser.add_argument('--base-url', help='Use an existing served site; otherwise serve the selected site locally')
    parser.add_argument('--output', type=Path, help='New result directory (existing results are never overwritten)')
    parser.add_argument('--keep-going', action='store_true', help='Continue after a failed check')
    args = parser.parse_args(argv)
    try:
        selected = select_tests(test_inventory(), args.suite, args.test)
    except ValueError as error:
        parser.error(str(error))
    if not selected:
        parser.error('No tests selected')
    if args.list:
        print('\n'.join(f'{r["name"]}\t{r["kind"]}\t{r["file"]}' for r in selected))
        return 0
    args.build_dir, args.wasm_dir = args.build_dir.resolve(), args.wasm_dir.resolve()
    # Fail before starting any checks if required artifacts are missing.
    required = []
    if any(r['kind'] != 'browser' for r in selected):
        required += [args.wasm_dir / 'pages/simulator' / ('circuitsim_wasm.' + suffix) for suffix in ('js', 'wasm')]
    if any('{probe}' in r['args'] for r in selected):
        required.append(args.build_dir / 'tests/VisualizerSessionProbe')
    if any(r['kind'] == 'browser' for r in selected):
        required.append(args.wasm_dir / 'pages/simulator/example-rows.json')
        if not args.base_url:
            required += [args.wasm_dir / 'site/index.html', args.wasm_dir / 'site/simulator/index.html']
    missing = [str(p) for p in required if not p.is_file()]
    if missing:
        parser.error('Missing artifacts: ' + ', '.join(missing))
    output = args.output or ROOT / 'build/validation' / time.strftime('web-%Y%m%d-%H%M%S')
    output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, PYTHONPATH=f'{args.build_dir}:{ROOT / "visualizer-v2"}',
               CIRCUITSIM_EXAMPLE_ROWS=str(args.wasm_dir / 'pages/simulator/example-rows.json'))
    env.setdefault('NODE_OPTIONS', '--max-old-space-size=512')
    rows = []

    def checks(base):
        for test in selected:
            print(test['name'], flush=True)
            started = time.monotonic()
            code = run_check(command_for(test, args, base), output / (test['name'] + '.log'), test['timeout'], env)
            rows.append(dict(name=test['name'], exit=code, seconds=time.monotonic() - started))
            (output / 'results.json').write_text(json.dumps(dict(schema=1, selected=[r['name'] for r in selected], results=rows), indent=2) + '\n')
            if code and not args.keep_going:
                break

    if args.base_url or not any(r['kind'] == 'browser' for r in selected):
        checks((args.base_url or '').rstrip('/') + '/')
    else:
        handler = functools.partial(StaticSiteHandler, directory=str(args.wasm_dir / 'site'))
        with http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler) as server:
            threading.Thread(target=server.serve_forever, daemon=True).start()
            try:
                checks(f'http://127.0.0.1:{server.server_port}/')
            finally:
                server.shutdown()
    passed = len(rows) == len(selected) and all(r['exit'] == 0 for r in rows)
    print(f'{"PASS" if passed else "FAIL"}: {len(rows)}/{len(selected)} executed; logs in {output}')
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
