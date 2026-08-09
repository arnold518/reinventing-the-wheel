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
        assert profiles.get("memory-bit") == (0, {})
        assert profiles.replace(
            "memory-bit",
            0,
            {"MEMORY_BIT_ROOT": "behavioral"},
        ) == 1
        assert ProfileStore(path).get("memory-bit") == (
            1,
            {"MEMORY_BIT_ROOT": "behavioral"},
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
                "RV32I_PROGRAM_ROOT.CORE.ALU": "behavioral",
            },
        )
        assert migrated.get("rv32i-five-stage-program1") == (
            2,
            {"RV32I_PROGRAM_ROOT.CORE.FETCH": "structural"},
        )
        expect_value_error(
            lambda: profiles.replace("memory-bit", 0, {}),
            "expected revision 0, current revision 1",
        )
        assert ProfileStore(path).get("memory-bit") == (
            1,
            {"MEMORY_BIT_ROOT": "behavioral"},
        )

        store = server.SessionStore()
        store.profile_store = ProfileStore(
            Path(directory) / "session-profiles.json"
        )
        initial = store.get("memory-bit")
        assert initial.root.get_selected_fidelity() == "structural"
        assert initial.profile_revision == 0

        expect_value_error(
            lambda: store.apply_profile(
                "memory-bit",
                0,
                {
                    "MEMORY_BIT_ROOT.WRITE_MUX.NOT_SEL": "behavioral",
                },
            ),
            "does not provide behavioral fidelity",
        )
        assert store.profile_store.get("memory-bit") == (0, {})
        assert store.get("memory-bit") is initial

        changed = store.apply_profile(
            "memory-bit",
            0,
            {"MEMORY_BIT_ROOT": "behavioral"},
        )
        payload = changed.topology_response()
        assert payload["sessionId"] == changed.session_id
        assert payload["profile"] == {
            "editable": True,
            "revision": 1,
            "exactOverrides": {
                "MEMORY_BIT_ROOT": "behavioral",
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
                "memory-bit",
                1,
                {
                    "MEMORY_BIT_ROOT": "behavioral",
                    "MEMORY_BIT_ROOT.UNKNOWN": "structural",
                },
            ),
            "unknown component",
        )
        assert store.profile_store.get("memory-bit") == (
            1,
            {"MEMORY_BIT_ROOT": "behavioral"},
        )
        assert store.get("memory-bit") is changed

        expect_value_error(
            lambda: store.apply_profile(
                "memory-bit",
                0,
                {"MEMORY_BIT_ROOT": "structural"},
            ),
            "Profile changed since it was loaded",
        )
        assert store.profile_store.get("memory-bit") == (
            1,
            {"MEMORY_BIT_ROOT": "behavioral"},
        )


if __name__ == "__main__":
    main()
