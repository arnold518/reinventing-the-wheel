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
                "exact_overrides": validate_overrides(
                    entry.get("exact_overrides", {})
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
