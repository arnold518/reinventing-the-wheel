"""Exact wide ticks through the Python session and portable C++ JSON facade."""
import json,pathlib,subprocess,sys,tempfile
import circuit_backend
import server
probe=pathlib.Path(sys.argv[1]).resolve()
base=(1<<54)+3
alias='simulation-wide-time'
# Keep verification equipment out of the product scenario menu. Add this one
# test factory only to this process's session map.
server.SCENARIOS[alias]=lambda:circuit_backend.create_test_by_name('SimulationTimePlaybackTest')
with tempfile.TemporaryDirectory(prefix='circuitsim-time-binding-') as directory:
    # Outside the menu it has no catalog profile: name one on both sides.
    session=server.CircuitSession(alias,server.LayoutManager(pathlib.Path(directory)/'layout.json'),fidelity='gate')
    actual=session.topology_response()
    expected=json.loads(subprocess.check_output([str(probe),'topology','SimulationTimePlaybackTest',alias,'--fidelity','gate'],text=True))
    for field in ['rootId','rootType','timestamps','checkpoints','state']:
        assert actual[field]==expected[field],field
    assert str(base) in actual['timestamps']
    assert [c['time'] for c in actual['checkpoints']]==[str(base+2),str(base+10)]
    for index in [len(actual['timestamps'])-1,0,*[c['index'] for c in actual['checkpoints']]]:
        expected_state=json.loads(subprocess.check_output([str(probe),'state','SimulationTimePlaybackTest',alias,str(index),'--fidelity','gate'],text=True))
        assert session.state_at_index(index)==expected_state,index
    assert server._json_time((1<<53)-1)==(1<<53)-1
    assert server._json_time(1<<53)==str(1<<53)
    assert server._json_time((1<<64)-1)==str((1<<64)-1)
print('PASS: Python/native exact ticks above 2^53 and recorded rewind')
