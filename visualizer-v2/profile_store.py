"""Persistent visualizer fidelity choices.

The file stores only exact-path overrides. Circuit topology is derived by C++
from the resulting recursive BuildProfile and is never duplicated here.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import tempfile
import threading
from typing import Any


PROFILE_SCHEMA_VERSION = 1
_PATH_PATTERN = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.\-\[\]]*$")
_FIDELITIES = {"structural", "behavioral"}
_COMPONENT_PATH_PREFIX_MIGRATIONS = {
    "RV32I_SINGLE_CYCLE_SYSTEM_ROOT": "RV32I_PROGRAM_ROOT",
    "RV32I_FIVE_STAGE_PROGRAM_ROOT": "RV32I_PROGRAM_ROOT",
}
_ROOT_FIDELITY_MIGRATIONS = {
    old_root: f"{new_root}.CORE"
    for old_root, new_root
    in _COMPONENT_PATH_PREFIX_MIGRATIONS.items()
}


def validate_overrides(value: Any) -> dict[str, str]:
    if not isinstance(value, dict):
        raise ValueError("exactOverrides must be an object")
    result: dict[str, str] = {}
    for raw_path, raw_fidelity in value.items():
        if not isinstance(raw_path, str) or not _PATH_PATTERN.fullmatch(raw_path):
            raise ValueError(f"Invalid component path {raw_path!r}")
        if raw_fidelity not in _FIDELITIES:
            raise ValueError(
                f"Fidelity for '{raw_path}' must be structural or behavioral"
            )
        result[raw_path] = str(raw_fidelity)
    return dict(sorted(result.items()))


def migrate_component_paths(overrides: dict[str, str]) -> dict[str, str]:
    """Translate saved paths from superseded visual testbench roots.

    Profiles describe selectable implementation nodes, not the testbench
    container itself.  Both RV32I program families now share one fixed root,
    so old CORE subtree choices remain meaningful after a prefix rewrite.
    Canonical paths win if a file happens to contain both forms.
    """
    migrated: dict[str, str] = {}
    for path, fidelity in overrides.items():
        if path in _ROOT_FIDELITY_MIGRATIONS:
            migrated.setdefault(
                _ROOT_FIDELITY_MIGRATIONS[path], fidelity
            )
            continue
        for old_prefix, new_prefix in _COMPONENT_PATH_PREFIX_MIGRATIONS.items():
            if path.startswith(f"{old_prefix}."):
                migrated.setdefault(
                    f"{new_prefix}{path[len(old_prefix):]}",
                    fidelity,
                )
                break
        else:
            migrated[path] = fidelity
    for path, fidelity in overrides.items():
        if any(
            path == new_prefix or path.startswith(f"{new_prefix}.")
            for new_prefix in _COMPONENT_PATH_PREFIX_MIGRATIONS.values()
        ):
            migrated[path] = fidelity
    def ff_path(path):
        match = re.search(r"(^|\.)(REGISTER_FILE|REGISTER_FILE32X32_ROOT)\.X([1-9]|[12][0-9]|3[01])(?=\.|$)", path)
        if not match:
            return path
        tail = path[match.end():]
        tail = re.sub(r"^\.BIT_(\d+)(?=\.|$)", r".CELL_\1", tail)
        tail = re.sub(r"^\.D_SPLIT(?=\.|$)", ".DATA", tail)
        tail = re.sub(r"^\.Q_JOIN(?=\.|$)", ".OUTPUT", tail)
        return path[:match.start()] + match[1] + match[2] + ".STORAGE.WORD_" + str(int(match[3])-1) + tail

    def stage_path(path):
        moves = {
            "CONTROL_FLOW.PC_IMM_ADD": "EXECUTE.BRANCH_TARGET_ADD",
            "CONTROL_FLOW.RS1_IMM_ADD": "EXECUTE.ALU.ARITHMETIC",
            "CONTROL_FLOW.JALR_MASK": "EXECUTE.JALR_MASK",
            "CONTROL_FLOW": "FETCH", "DECODE_CONTROL": "DECODE.CONTROL",
            "REGISTER_FILE": "DECODE.REGISTER_FILE", "ALU_A_MUX": "EXECUTE.OPERAND_A",
            "ALU_B_MUX": "EXECUTE.OPERAND_B", "ALU": "EXECUTE.ALU",
            "WRITEBACK_MUX": "WRITEBACK.RESULT_SELECT", "FETCH_REQUEST": "FETCH.REQUEST",
            "DATA_REQUEST": "MEMORY.REQUEST", "LOAD_STORE": "MEMORY.LANES",
        }
        match = re.match(r"^(.*\.CORE\.|RV32I_SINGLE_CYCLE_CORE_ROOT\.)(.*)$", path)
        if match:
            for old, new in moves.items():
                if match[2] == old or match[2].startswith(old + "."):
                    return match[1] + new + match[2][len(old):]
        return path

    def current_path(path):
        path = stage_path(ff_path(path))
        # Register32 now contains one FFWord. Limit name-based compatibility
        # to its registered root and RV32I state words; unrelated BIT_n helpers
        # (especially MemoryPayloadRegister) must not be rewritten.
        def register_child(match):
            old = match[2]
            child = "CELL_" + match[3] if match[3] is not None else {"D_SPLIT": "DATA", "Q_JOIN": "OUTPUT"}[old]
            return match[1] + ".STORAGE." + child
        return re.sub(r"^(REGISTER32_ROOT|RV32I[^.]*\.(?:[A-Za-z0-9_]+\.)*[A-Za-z0-9_]+_STATE)\.(BIT_([0-9]|[12][0-9]|3[01])|D_SPLIT|Q_JOIN)(?=\.|$)",
                      register_child, path)

    result = {current_path(path): fidelity for path, fidelity in migrated.items()}
    # Explicit choices already using the new hierarchy take precedence.
    result.update({path: fidelity for path, fidelity in migrated.items() if current_path(path) == path})
    return dict(sorted(result.items()))


class ProfileStore:
    def __init__(self, path: Path):
        self.path = path
        self.lock = threading.RLock()
        self.data = self._load()

    def _empty(self) -> dict[str, Any]:
        return {
            "schema_version": PROFILE_SCHEMA_VERSION,
            "scenarios": {},
        }

    def _load(self) -> dict[str, Any]:
        if not self.path.exists():
            return self._empty()
        with self.path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
        if not isinstance(value, dict):
            raise ValueError("profiles.json must contain an object")
        version = value.get("schema_version")
        if version != PROFILE_SCHEMA_VERSION:
            raise ValueError(
                f"Unsupported profiles.json schema {version!r}; "
                f"expected {PROFILE_SCHEMA_VERSION}"
            )
        scenarios = value.get("scenarios")
        if not isinstance(scenarios, dict):
            raise ValueError("profiles.json scenarios must be an object")
        normalized = self._empty()
        for scenario, entry in scenarios.items():
            if not isinstance(scenario, str) or not isinstance(entry, dict):
                raise ValueError("profiles.json contains an invalid scenario entry")
            revision = entry.get("revision", 0)
            if not isinstance(revision, int) or revision < 0:
                raise ValueError(f"Invalid profile revision for '{scenario}'")
            normalized["scenarios"][scenario] = {
                "revision": revision,
                "exact_overrides": migrate_component_paths(
                    validate_overrides(entry.get("exact_overrides", {}))
                ),
            }
        return normalized

    def get(self, scenario: str) -> tuple[int, dict[str, str]]:
        with self.lock:
            entry = self.data["scenarios"].get(scenario)
            if entry is None:
                return 0, {}
            return int(entry["revision"]), dict(entry["exact_overrides"])

    def replace(
        self,
        scenario: str,
        expected_revision: int,
        overrides: dict[str, str],
    ) -> int:
        normalized = validate_overrides(overrides)
        with self.lock:
            current_revision, _ = self.get(scenario)
            if expected_revision != current_revision:
                raise ValueError(
                    f"Profile changed since it was loaded "
                    f"(expected revision {expected_revision}, "
                    f"current revision {current_revision})"
                )
            next_revision = current_revision + 1
            self.data["scenarios"][scenario] = {
                "revision": next_revision,
                "exact_overrides": normalized,
            }
            self._save_locked()
            return next_revision

    def _save_locked(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        handle = tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=self.path.parent,
            prefix=f".{self.path.name}.",
            suffix=".tmp",
            delete=False,
        )
        temporary_path = Path(handle.name)
        try:
            with handle:
                json.dump(self.data, handle, indent=2, sort_keys=True)
                handle.write("\n")
                handle.flush()
                os.fsync(handle.fileno())
            os.replace(temporary_path, self.path)
        finally:
            if temporary_path.exists():
                temporary_path.unlink()
