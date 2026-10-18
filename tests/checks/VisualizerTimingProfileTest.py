"""The visualizer builds and simulates a scenario under a chosen timing profile.

The profile is one setting of the whole C++ library: a session sets it while
it builds and runs its circuit, under one lock, and restores the previous
value. Sessions are kept per scenario and profile.
"""
from pathlib import Path
import tempfile

import circuit_backend  # The isolated build, before server adds visualizer-v2.
import server


def expect_value_error(action, fragment):
    try:
        action()
    except ValueError as error:
        assert fragment in str(error), error
    else:
        raise AssertionError(f"Expected ValueError containing {fragment!r}")


def main():
    assert {"unit", "embedded", "desktop"} <= set(server.timing_profile_names())
    assert server.normalize_timing(None) == "unit"
    expect_value_error(lambda: server.normalize_timing("fastest"), "Unknown timing profile")
    assert server.timing_profile_info("unit")["tickPicoseconds"] == 0
    assert server.timing_profile_info("embedded")["tickPicoseconds"] == 1

    with tempfile.TemporaryDirectory() as directory:
        layouts = server.LayoutManager(Path(directory) / "layout.json")
        unit = server.CircuitSession("adder8", layouts)
        embedded = server.CircuitSession("adder8", layouts, timing="embedded")
        # The profile is restored after each build.
        assert circuit_backend.timing_profile() == "unit"
        assert unit.topology_response()["timing"]["name"] == "unit"
        info = embedded.topology_response()["timing"]
        assert info["name"] == "embedded" and info["tickPicoseconds"] == 1
        # Same checkpoints, later times: realistic gates are slower.
        assert [c["label"] for c in unit.checkpoints] == [c["label"] for c in embedded.checkpoints]
        assert embedded.timestamps[-1] > 10 * unit.timestamps[-1]

        # A program scenario runs at the profile's certified clock.
        program = server.CircuitSession("rv32i-program1", layouts, timing="embedded")
        assert program.checkpoints, "program scenario recorded no checkpoints under embedded"
        assert circuit_backend.timing_profile() == "unit"

    store = server.SessionStore()
    a = store.get("adder8", timing="unit")
    b = store.get("adder8", timing="embedded")
    assert a.session_id != b.session_id and b.timing == "embedded"
    assert store.get("adder8", timing="embedded").session_id == b.session_id
    print("visualizer timing profiles: ok")


if __name__ == "__main__":
    main()
