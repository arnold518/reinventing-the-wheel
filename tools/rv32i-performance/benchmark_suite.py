"""Bounded core-gap panel. Each case is run unchanged on every selected core."""


def core_gap_cases(iterations, size, seed):
    cases = []

    def add(label, workload, **options):
        params = dict(iterations=iterations, size=size, seed=seed) | options
        args = ["--workload", workload]
        for key, value in params.items():
            flag = "--" + key.replace("_", "-")
            if isinstance(value, bool):
                if value:
                    args.append(flag)
            else:
                args.extend([flag, value])
        cases.append((label, args))

    add("independent-short", "independent", iterations=1)
    add("independent-long", "independent")
    for distance in (1, 2, 4, 8):
        add(f"dependency-d{distance}", "dependency", distance=distance)
    for name in ("load-use", "load-scheduled"):
        add(name, name)
    for pattern in ("taken", "not-taken", "alternating", "bursts", "irregular"):
        add("branch-" + pattern, "branch-pattern", pattern=pattern)
    for spacing in (8, 16):
        add(f"alias-spacing{spacing}", "branch-alias", branch_spacing=spacing)
    for name in ("calls", "stream", "pointer-chase", "scan"):
        add(name, name)
    # Sort and bitwise CRC execute much more work per iteration. Direct
    # --workload runs remain available for independently scaling these kernels.
    for name in ("sort", "crc32"):
        add(name, name, iterations=min(iterations, 2))
    for training in (0, 32):
        add(f"branch-body-training{training}", "branch-pattern", pattern="taken",
            measure_body=True, training_iterations=training)
    return cases


def generated_regression_cases(iterations, size, seed):
    """One representative case for every generated workload family."""
    cases = [(name, ["--workload", name, "--iterations", iterations])
             for name in ("dependent", "alternating", "nested")]
    for name in ("independent", "dependency", "load-use", "load-scheduled",
                 "branch-pattern", "branch-alias", "calls", "stream",
                 "pointer-chase", "sort", "crc32", "scan"):
        count = min(iterations, 2) if name in {"sort", "crc32"} else iterations
        cases.append((name, ["--workload", name, "--iterations", count,
                             "--size", size, "--seed", seed]))
    return cases
