"""The game-replay scenario natively: a save made by MachineStateProbe is
restored by GameReplayScenarioTest (CIRCUITSIM_REPLAY_SAVE) and run for a
window with history, every check of the checked runner continuing.

    GameReplayScenarioTest.py --probe MachineStateProbe --runner SimulationTestRunner
"""
import argparse, os, subprocess, sys, tempfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--probe', required=True); p.add_argument('--runner', required=True); p.add_argument('--session')
a = p.parse_args()
env = dict(os.environ, CIRCUITSIM_TIMING_PROFILE='embedded')
with tempfile.TemporaryDirectory() as directory:
    subprocess.run([a.probe, 'pong-short', '--save', '300', '--compare', '400', '--out', directory], env=env, check=True,
                   stdout=subprocess.DEVNULL)
    save = Path(directory) / 'save-300.bin'
    result = subprocess.run([a.runner, 'GameReplayScenarioTest'], capture_output=True, text=True,
                            env=dict(env, CIRCUITSIM_REPLAY_SAVE=str(save), CIRCUITSIM_REPLAY_FIXTURE='pong-short',
                                     CIRCUITSIM_REPLAY_CYCLES='200'))
    sys.stdout.write(result.stdout); sys.stderr.write(result.stderr)
    assert result.returncode == 0 and '[PASS] GameReplayScenarioTest: pong-short, 500 cycles' in result.stdout, result.returncode
    if a.session:
        stepped = subprocess.run([a.session, 'pong-short', str(save)], capture_output=True, text=True, env=env)
        sys.stdout.write(stepped.stdout); sys.stderr.write(stepped.stderr)
        assert stepped.returncode == 0 and 'PASS pong-short' in stepped.stdout, stepped.returncode
        # With a 40-cycle history window the session trims as it goes; the
        # machine's state is untouched, so the end must still be equal.
        trimmed = subprocess.run([a.session, 'pong-short', str(save)], capture_output=True, text=True,
                                 env=dict(env, CIRCUITSIM_GAME_HISTORY_CYCLES='40'))
        sys.stdout.write(trimmed.stdout); sys.stderr.write(trimmed.stderr)
        assert trimmed.returncode == 0 and 'PASS pong-short' in trimmed.stdout, trimmed.returncode
    # Without a save the scenario refuses to run instead of playing a whole game.
    refused = subprocess.run([a.runner, 'GameReplayScenarioTest'], capture_output=True, text=True, env=env)
    assert refused.returncode != 0 and 'opens from a long recording' in refused.stdout + refused.stderr
print('PASS game-replay restored a save and ran 200 cycles with history')
