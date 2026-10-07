#!/usr/bin/env python3
"""Serial isolated validation with persistent status/logs (no preview deployment)."""
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
LOGS = ROOT / 'build/validation' / time.strftime('cleanup-%Y%m%d-%H%M%S')
BUILD = ROOT / 'build-cleanup'
WASM = ROOT / 'build-wasm-cleanup'
env = dict(os.environ)
env['PYTHONPATH'] = f'{BUILD}:{ROOT / "visualizer-v2"}'
env['CIRCUITSIM_BUILD_JOBS'] = '1'
env['EMCC_CORES'] = '1'
env['NODE_OPTIONS'] = '--max-old-space-size=512'
env['PATH'] = '/home/arnold/venv/bin:/home/arnold/.cache/circuitsim-emsdk/upstream/emscripten:' + env['PATH']
completed = []
scope = 'full'
WEB_TESTS = json.loads((ROOT / 'tests/web-tests.json').read_text())['tests']
BROWSER_TESTS = tuple((r['name'], 'simulator/' if r['args'] == ['{simulator}'] else '')
                      for r in WEB_TESTS if r['kind'] == 'browser' and 'cleanup' in r['suites'])


def status(phase, **extra):
    data = dict(phase=phase, scope=scope, pid=os.getpid(), completed=completed, updated=time.time(), **extra)
    temporary = LOGS / 'status.tmp'
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.replace(LOGS / 'status.json')
    print(phase, flush=True)


def run(name, command, timeout=7200):
    status(name)
    with (LOGS / (name + '.log')).open('w') as output:
        process = subprocess.Popen(command, cwd=ROOT, env=env, stdout=output,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            result = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            raise
    if result:
        raise RuntimeError(f'{name} failed ({result}); see {name}.log')
    completed.append(name)


def native_and_wasm_checks():
    run('native-configure', ['cmake', '-S', str(ROOT), '-B', str(BUILD),
        '-DCIRCUITSIM_DEPLOY_PYTHON=OFF', '-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF'])
    # GCC's binding compilation needs more than 2 GiB of virtual address
    # space. The service cgroup still caps actual RAM/swap for the whole job.
    run('native-build', ['prlimit', '--as=4294967296', 'cmake', '--build', str(BUILD), '-j1'])
    run('backend-import', ['/usr/bin/python3', '-c',
        'import circuit_backend; print(circuit_backend.__file__); '
        'assert "/build-cleanup/" in circuit_backend.__file__; '
        'assert not hasattr(circuit_backend,"Memory64Kx32"); '
        'assert not hasattr(circuit_backend,"create_memory_array")'])
    pattern = ('Memory|Register|Catalog|BuildProfile|TestRegistry|DocumentationExamples|'
               'Visualizer|RV32ILayoutMigration|RV32IBuildingBlockLayout|Benchmark|RV32IDifferential|RV32IExperiment|RV32IBinaryClockCLI|BackgroundCampaignResume|CleanupValidationRunner|WebTestRunner|'
               'RV32IProgramLoader|RV32IInstructionOracle|RV32IReference|SafeClock|StartupTiming|'
               '^(RV32ISingleCycleSystemTest|RV32IFiveStageCoreProgramTest|'
               'RV32IBaselinePipelineProgramTest)/program-(01|03|17)$')
    # Node/browser processes reserve large virtual arenas. Bound resident
    # usage with the launching service's cgroup, not RLIMIT_AS on CTest.
    run('native-tests', ['ctest', '--test-dir', str(BUILD), '-j1',
        '--output-on-failure', '-R', pattern])
    run('storage-benchmark-smoke', [str(BUILD / 'tests/MemoryStorageBenchmark')])
    run('wasm-pages-build', ['bash', 'tools/build-pages.sh', str(WASM)], timeout=10800)
    js, wasm = [str(WASM / 'pages/simulator' / ('circuitsim_wasm.' + suffix))
                for suffix in ('js', 'wasm')]
    probe = str(BUILD / 'tests/VisualizerSessionProbe')
    for name in ('FFMemoryWasmParityTest', 'MemoryEndpointWasmParityTest',
                 'MemoryInspectorWasmParityTest', 'RV32IMemoryWasmParityTest', 'VisualizerScenarioWasmParityTest'):
        run(name, ['node', f'tests/web/{name}.mjs', js, wasm, probe], timeout=1200)
    run('loader-wasm', ['node', 'tests/web/VisualizerWasmRuntimeTest.mjs', js, wasm,
                        'RV32IProgramLoaderTest', 'rv32i-program-loader'])


def browser_preflight(python):
    # Use the same interpreter and launch options as the tests. Fail before
    # costly builds if either Playwright or its browser is unavailable.
    run('browser-preflight', [python, '-c',
        'import shutil, sys\n'
        'from playwright.sync_api import sync_playwright\n'
        'with sync_playwright() as p:\n'
        '    b = p.chromium.launch(executable_path=shutil.which("google-chrome") '
        'or shutil.which("chromium"), headless=True, args=["--no-sandbox"])\n'
        '    print(sys.executable, b.version)\n'
        '    b.close()\n'], timeout=120)


def main(argv=None):
    global scope
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser-only', action='store_true',
                        help='Check the already-built site; does not claim build/native/parity validation')
    parser.add_argument('--browser-python', default=os.environ.get('CIRCUITSIM_BROWSER_PYTHON', sys.executable),
                        help='Python with Playwright installed (default: this runner\'s Python)')
    parser.add_argument('--browser-test', action='append', choices=[name for name, _ in BROWSER_TESTS],
                        help='With --browser-only, rerun selected checks (repeatable; default: all)')
    args = parser.parse_args(argv)
    if args.browser_test and not args.browser_only:
        parser.error('--browser-test requires --browser-only')
    scope = 'browser-only' if args.browser_only else 'full'
    # Distinct directory per validation attempt: never discard an earlier log.
    LOGS.mkdir(parents=True, exist_ok=False)
    try:
        browser_preflight(args.browser_python)
        if not args.browser_only:
            native_and_wasm_checks()
        for artifact in ('index.html', 'simulator/index.html', 'simulator/circuitsim_wasm.wasm'):
            if not (WASM / 'site' / artifact).is_file():
                raise RuntimeError(f'Missing built site artifact: {artifact}; run full validation first')
        handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(WASM / 'site'))
        with http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler) as server:
            threading.Thread(target=server.serve_forever, daemon=True).start()
            base = f'http://127.0.0.1:{server.server_port}/'
            try:
                for name, path in BROWSER_TESTS:
                    if args.browser_test and name not in args.browser_test:
                        continue
                    run(name, [args.browser_python, 'tools/run-web-tests.py', '--test', name,
                        '--browser-python', args.browser_python, '--build-dir', str(BUILD),
                        '--wasm-dir', str(WASM), '--base-url', base,
                        '--output', str(LOGS / name)], timeout=1200)
            finally:
                server.shutdown()
        status('passed')
    except Exception as error:
        status('failed', error=str(error))
        raise


if __name__ == '__main__':
    main()
