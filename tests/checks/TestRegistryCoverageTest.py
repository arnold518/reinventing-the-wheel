"""Require CTest and the C++ simulation-test registry to stay identical."""

import subprocess
import sys


def duplicates(names):
    seen = set()
    repeated = set()
    for name in names:
        if name in seen:
            repeated.add(name)
        seen.add(name)
    return sorted(repeated)


def main():
    if len(sys.argv) < 3:
        raise RuntimeError("expected runner path followed by CTest simulation names")

    runner, *ctest_names = sys.argv[1:]
    completed = subprocess.run(
        [runner, "--list"],
        check=True,
        capture_output=True,
        text=True,
    )
    registry_names = [line for line in completed.stdout.splitlines() if line]

    ctest_duplicates = duplicates(ctest_names)
    registry_duplicates = duplicates(registry_names)
    if ctest_duplicates or registry_duplicates:
        raise AssertionError(
            f"duplicate CTest names={ctest_duplicates}; "
            f"duplicate registry names={registry_duplicates}"
        )

    if registry_names != ctest_names:
        missing_from_registry = sorted(set(ctest_names) - set(registry_names))
        missing_from_ctest = sorted(set(registry_names) - set(ctest_names))
        raise AssertionError(
            "CTest and C++ registry differ: "
            f"missing from registry={missing_from_registry}; "
            f"missing from CTest={missing_from_ctest}; "
            "their declared order must also match"
        )


if __name__ == "__main__":
    main()
