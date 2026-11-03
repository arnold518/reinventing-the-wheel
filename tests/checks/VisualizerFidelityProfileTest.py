"""Named preset selection, independent timing and isolated explorer overrides."""
import json
from pathlib import Path
import subprocess
import tempfile
import circuit_backend as backend
import server
from profile_store import FIDELITY_PRESETS, ProfileStore, normalize_fidelity, profile_storage_key

ROOT = Path(__file__).resolve().parents[2]

def main():
    assert backend.fidelity_preset_names() == list(FIDELITY_PRESETS)
    script = """import {FIDELITY_PRESETS,profileStorageKey} from './visualizer-v2/static/fidelity_profiles.mjs';
process.stdout.write(JSON.stringify({names:FIDELITY_PRESETS.map(([key])=>key),
gateKey:profileStorageKey('register32','gate'),visualKey:profileStorageKey('register32','visual')}));"""
    result = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', script], cwd=ROOT, text=True))
    assert result == dict(names=list(FIDELITY_PRESETS), gateKey='register32::gate', visualKey='register32::visual')
    assert 'scenario' not in FIDELITY_PRESETS
    # Every scenario opens in one named profile, available for it.
    for option in server.SCENARIO_OPTIONS:
        assert option['fidelity'] in FIDELITY_PRESETS, option['key']
    assert server.default_fidelity('register32') == 'gate'
    assert server.default_fidelity('computer-system') == 'compact'
    assert server.default_fidelity('computer-system-structure') == 'visual'
    assert server.default_fidelity('rv32i-five-stage-program1') == 'visual'
    try:
        normalize_fidelity('unknown')
    except ValueError:
        pass
    else:
        raise AssertionError('Unknown preset accepted')
    with tempfile.TemporaryDirectory() as directory:
        store = server.SessionStore()
        store.profile_store = ProfileStore(Path(directory) / 'profiles.json')
        store.max_sessions = 5
        try:
            store.get('computer-system', fidelity='gate')
        except ValueError as error:
            assert 'main memory' in str(error)
        else:
            raise AssertionError('Unsupported system gate profile was constructed')
        gate = store.get('register32', fidelity='gate')
        visual = store.get('register32', fidelity='visual')
        assert gate.root.get_selected_fidelity() == 'structural'
        assert visual.root.get_selected_fidelity() == 'behavioral'
        assert gate.session_id != visual.session_id
        assert store.get('register32', fidelity='visual').session_id == visual.session_id
        embedded = store.get('register32', fidelity='visual', timing='embedded')
        assert embedded.session_id != visual.session_id and embedded.fidelity == 'visual'
        assert embedded.root.get_selected_fidelity() == 'behavioral' and embedded.timing == 'embedded'
        custom = store.apply_profile('register32', 0, {gate.root.get_id(): 'behavioral'}, fidelity='gate')
        info = custom.topology_response()['profile']
        assert info['fidelityPreset'] == 'gate' and info['custom'] and info['presetVersion'] == 1
        assert store.get('register32').fidelity == 'gate'
        # A profile saved before named defaults (no preset in its key) moves
        # to the scenario's default profile.
        legacy = Path(directory) / 'legacy.json'
        legacy.write_text(json.dumps({'schema_version': 1, 'scenarios': {'register32': {'revision': 3, 'exact_overrides': {}}}}))
        assert ProfileStore(legacy, server.default_fidelity).get('register32::gate')[0] == 3
        assert store.profile_store.get(profile_storage_key('register32', 'visual')) == (0, {})
        assert store.get('register32', fidelity='visual').profile_overrides == {}
        assert store.get('register32', fidelity='gate').profile_overrides == {gate.root.get_id(): 'behavioral'}
    print('Native/JS preset identity, actual selections, timing independence and isolated custom overrides pass')

if __name__ == '__main__':
    main()
