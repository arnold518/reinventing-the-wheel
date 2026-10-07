"""Profile persistence and profile-aware visualizer session checks."""

from pathlib import Path
import json
import tempfile

import server
from profile_store import ProfileStore, validate_overrides


def expect_value_error(action, message_fragment: str):
    try:
        action()
    except ValueError as error:
        assert message_fragment in str(error)
    else:
        raise AssertionError(f"Expected ValueError containing {message_fragment!r}")


def main():
    assert validate_overrides(
        {"ROOT.CHILD": "behavioral", "ROOT": "structural"}
    ) == {
        "ROOT": "structural",
        "ROOT.CHILD": "behavioral",
    }
    expect_value_error(
        lambda: validate_overrides({"ROOT": "automatic"}),
        "must be structural or behavioral",
    )
    expect_value_error(
        lambda: validate_overrides({"ROOT/CHILD": "structural"}),
        "Invalid component path",
    )

    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "profiles.json"
        profiles = ProfileStore(path)
        assert profiles.get("flip-flop-cell") == (0, {})
        assert profiles.replace(
            "flip-flop-cell",
            0,
            {"FLIP_FLOP_CELL_ROOT": "behavioral"},
        ) == 1
        assert ProfileStore(path).get("flip-flop-cell") == (
            1,
            {"FLIP_FLOP_CELL_ROOT": "behavioral"},
        )

        legacy_path = Path(directory) / "legacy-rv32i-profiles.json"
        legacy_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "scenarios": {
                        "rv32i-program9": {
                            "revision": 4,
                            "exact_overrides": {
                                "RV32I_SINGLE_CYCLE_SYSTEM_ROOT": "behavioral",
                                "RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.ALU": "behavioral",
                            },
                        },
                        "rv32i-five-stage-program1": {
                            "revision": 2,
                            "exact_overrides": {
                                "RV32I_FIVE_STAGE_PROGRAM_ROOT.CORE.FETCH": "structural",
                            },
                        },
                    },
                }
            ),
            encoding="utf-8",
        )
        migrated = ProfileStore(legacy_path)
        assert migrated.get("rv32i-program9") == (
            4,
            {
                "RV32I_PROGRAM_ROOT.CORE": "behavioral",
                "RV32I_PROGRAM_ROOT.CORE.EXECUTE.ALU": "behavioral",
            },
        )
        assert migrated.get("rv32i-five-stage-program1") == (
            2,
            {"RV32I_PROGRAM_ROOT.CORE.FETCH": "structural"},
        )
        expect_value_error(
            lambda: profiles.replace("flip-flop-cell", 0, {}),
            "expected revision 0, current revision 1",
        )
        assert ProfileStore(path).get("flip-flop-cell") == (
            1,
            {"FLIP_FLOP_CELL_ROOT": "behavioral"},
        )

        store = server.SessionStore()
        store.profile_store = ProfileStore(
            Path(directory) / "session-profiles.json"
        )
        initial = store.get("flip-flop-cell")
        assert initial.root.get_selected_fidelity() == "structural"
        assert initial.profile_revision == 0

        expect_value_error(
            lambda: store.apply_profile(
                "flip-flop-cell",
                0,
                {
                    "FLIP_FLOP_CELL_ROOT.WRITE_MUX.NOT_SEL": "behavioral",
                },
            ),
            "does not provide behavioral fidelity",
        )
        assert store.profile_store.get("flip-flop-cell") == (0, {})
        assert store.get("flip-flop-cell") is initial

        changed = store.apply_profile(
            "flip-flop-cell",
            0,
            {"FLIP_FLOP_CELL_ROOT": "behavioral"},
        )
        payload = changed.topology_response()
        assert payload["sessionId"] == changed.session_id
        assert payload["profile"] == {
            "editable": True,
            "revision": 1,
            "exactOverrides": {
                "FLIP_FLOP_CELL_ROOT": "behavioral",
            },
            "fingerprint": changed.profile_fingerprint,
        }
        assert payload["components"][0]["fidelity"] == "behavioral"
        assert payload["components"][0]["profileSelectable"]
        assert payload["components"][0]["availableFidelities"] == [
            "structural",
            "behavioral",
        ]
        assert store.get_by_id(changed.session_id) is changed

        expect_value_error(
            lambda: store.apply_profile(
                "flip-flop-cell",
                1,
                {
                    "FLIP_FLOP_CELL_ROOT": "behavioral",
                    "FLIP_FLOP_CELL_ROOT.UNKNOWN": "structural",
                },
            ),
            "unknown component",
        )
        assert store.profile_store.get("flip-flop-cell") == (
            1,
            {"FLIP_FLOP_CELL_ROOT": "behavioral"},
        )
        assert store.get("flip-flop-cell") is changed

        expect_value_error(
            lambda: store.apply_profile(
                "flip-flop-cell",
                0,
                {"FLIP_FLOP_CELL_ROOT": "structural"},
            ),
            "Profile changed since it was loaded",
        )
        assert store.profile_store.get("flip-flop-cell") == (
            1,
            {"FLIP_FLOP_CELL_ROOT": "behavioral"},
        )


def test_transient_reference_profile():
    with tempfile.TemporaryDirectory() as directory:
        store = server.SessionStore()
        store.profile_store = ProfileStore(Path(directory) / "profiles.json")
        scenario = "rv32i-program9"
        reference = store.get(scenario, "behavioral")
        payload = reference.topology_response()
        core = next(c for c in payload["components"] if c["id"] == "RV32I_PROGRAM_ROOT.CORE")
        assert core["fidelity"] == "behavioral"
        assert store.profile_store.get(scenario) == (0, {})
        assert store.get(scenario, "behavioral") is reference
        assert store.get_by_id(reference.session_id) is reference
        normal = store.get(scenario)
        assert normal.profile_overrides == {}
        assert normal is not reference
        expect_value_error(lambda: store.get("flip-flop-cell", "behavioral"), "core_fidelity")
        expect_value_error(lambda: store.get(scenario, "automatic"), "core_fidelity")


if __name__ == "__main__":
    main()
    test_transient_reference_profile()
