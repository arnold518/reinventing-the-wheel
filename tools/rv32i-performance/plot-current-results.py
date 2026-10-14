#!/usr/bin/env python3
"""Rebuild the four §4.7 figures from the completed October 2026 campaigns."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize, TwoSlopeNorm
from matplotlib.patches import Rectangle


ROOT = Path(__file__).resolve().parents[2]
EXPERIMENTS = ROOT / "build/experiments"
CURRENT = EXPERIMENTS / "20261014-nonembench-full"
HISTORICAL = EXPERIMENTS / "20260927-full-embench"
REPORT = ROOT / "site-docs/rv32i/experiments.md"
OUTPUT = ROOT / "site-docs/rv32i/4.7"

SOURCE_SHA256 = "4da38232d8dd68278bf56f27d587a25728aca0bf12b8e7591acb48f67c8213e2"
BINARY_SHA256 = "e8ae5aa55e7a60bcf842eb7013d67becae25278c9d1dcfdf08ab9dcd2857b0f1"
HISTORICAL_SOURCE_SHA256 = "3d8092f2a130d85b0109b127eb65a9664f045b580ef9146f6b2cab6111c82d43"
TARGETS = (
    ("single", "not-taken", "Single", 166),
    ("baseline", "not-taken", "Baseline", 80),
    ("advanced", "not-taken", "Adv NT", 100),
    ("advanced", "taken", "Adv T", 100),
    ("advanced", "backward-taken", "Adv BT", 100),
    ("advanced", "one-bit", "Adv 1b", 100),
    ("advanced", "two-bit", "Adv 2b", 100),
)
TARGET_KEYS = tuple((core, policy) for core, policy, _, _ in TARGETS)
SHORT_NAMES = tuple(name for _, _, name, _ in TARGETS)
PERIODS = {key: period for (core, policy, _, period) in TARGETS for key in [(core, policy)]}
PATTERNS = ("taken", "not-taken", "alternating", "bursts", "irregular")
GENERATED_CASES = (
    "dependent", "alternating", "nested", "independent", "dependency",
    "load-use", "load-scheduled", "branch-pattern", "branch-alias",
    "calls", "stream", "pointer-chase", "sort", "crc32", "scan",
)
CORE_GAP_CASES = (
    "independent-short", "independent-long", "dependency-d1", "dependency-d2",
    "dependency-d4", "dependency-d8", "load-use", "load-scheduled",
    "branch-taken", "branch-not-taken", "branch-alternating", "branch-bursts",
    "branch-irregular", "alias-spacing8", "alias-spacing16",
    "branch-body-training0", "branch-body-training32", "calls", "stream",
    "pointer-chase", "scan", "sort", "crc32",
)

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "svg.fonttype": "none",
    "svg.hashsalt": "rv32i-results-2026-10-14",
    "savefig.facecolor": "white",
})


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_json(path: Path):
    return json.loads(path.read_text())


def row_key(row: dict) -> tuple[str, str]:
    return row["core"], row["policy"]


def load_campaign(path: Path, expected_cases: tuple[str, ...]) -> dict[str, dict[tuple[str, str], int]]:
    require((path / "COMPLETE").is_file(), f"incomplete campaign: {path}")
    provenance = read_json(path / "provenance.json")
    require(provenance["source_sha256"] == SOURCE_SHA256, f"source changed: {path}")
    require(provenance["binary_sha256"] == BINARY_SHA256, f"binary changed: {path}")
    clocks = read_json(path / "clocks.json")
    require(len(clocks) == len(TARGETS), f"clock grid changed: {path}")
    require({row_key(c) for c in clocks} == set(TARGET_KEYS), f"clock targets changed: {path}")
    for clock in clocks:
        target = row_key(clock)
        period = PERIODS[target]
        require(clock["period"] == clock["certified_period"] == period,
                f"clock period changed: {path}, {target}")
        require(clock["safe_clock"]["status"] == "safe-under-contracts",
                f"uncertified clock: {path}, {target}")

    summary = read_json(path / "summary.json")
    require(len(summary) == len(expected_cases) * len(TARGETS), f"summary size changed: {path}")
    grid: dict[str, dict[tuple[str, str], int]] = defaultdict(dict)
    for row in summary:
        case, target = row["workload"], row_key(row)
        require(case in expected_cases and target in PERIODS, f"unexpected summary row: {path}, {case}, {target}")
        require(target not in grid[case], f"duplicate summary row: {path}, {case}, {target}")
        require(row["entries"] == 16, f"predictor capacity changed: {path}, {case}, {target}")
        require(row["run_status"] == "completed" and row["structural_checked"] is True,
                f"unchecked summary: {path}, {case}, {target}")
        require(row["clock_basis"] == "binary-startup-safe-clock-v1",
                f"clock basis changed: {path}, {case}, {target}")
        require(row["period_ticks"] == PERIODS[target], f"period changed: {path}, {case}, {target}")
        ticks = row["modeled_time_ticks"]
        require(ticks == row["cycles"] * PERIODS[target] and ticks > 0,
                f"tick arithmetic failed: {path}, {case}, {target}")
        grid[case][target] = ticks
    require(set(grid) == set(expected_cases), f"case set changed: {path}")
    require(all(set(row) == set(TARGET_KEYS) for row in grid.values()), f"target grid incomplete: {path}")

    runs = [json.loads(line) for line in (path / "runs.jsonl").read_text().splitlines()]
    require(len(runs) == len(summary) * 3, f"run phase count changed: {path}")
    phase_grid: dict[tuple[str, tuple[str, str]], dict[str, dict]] = defaultdict(dict)
    for run in runs:
        key = (run["label"], row_key(run))
        phase = run["phase"]
        require(key[0] in grid and key[1] in grid[key[0]], f"unexpected run: {path}, {key}")
        require(phase in ("oracle-check", "reference", "measurement") and phase not in phase_grid[key],
                f"duplicate/unknown phase: {path}, {key}, {phase}")
        require(run["completed"] is True and run["setup_violations"] == 0,
                f"incomplete/violating run: {path}, {key}, {phase}")
        if phase != "measurement":
            require(run["verified"] is True, f"unverified run: {path}, {key}, {phase}")
        phase_grid[key][phase] = run
    require(len(phase_grid) == len(summary), f"run grid incomplete: {path}")
    for (case, target), phases in phase_grid.items():
        require(set(phases) == {"oracle-check", "reference", "measurement"},
                f"missing phase: {path}, {case}, {target}")
        measurement = phases["measurement"]
        require(measurement["modeled_time_ticks"] == grid[case][target],
                f"summary/measurement mismatch: {path}, {case}, {target}")
        require(phases["reference"]["cycles"] == measurement["cycles"],
                f"reference cycle mismatch: {path}, {case}, {target}")
        require(phases["reference"]["final_state_fnv1a64"] == measurement["final_state_fnv1a64"],
                f"reference final-state mismatch: {path}, {case}, {target}")
    return dict(grid)


def load_projection(path: Path) -> dict[str, dict[tuple[str, str], int]]:
    require(read_json(HISTORICAL / "campaign.json")["status"] == "completed",
            "historical full Embench campaign is incomplete")
    grid: dict[str, dict[tuple[str, str], int]] = defaultdict(dict)
    with path.open(newline="") as handle:
        for row in csv.DictReader(handle):
            case, target = row["workload"], row_key(row)
            require(target in PERIODS and target not in grid[case], f"unexpected projected row: {case}, {target}")
            cycles = int(row["historical_cycles"])
            ticks, period = int(row["projected_ticks"]), int(row["projected_period"])
            require(cycles > 0 and period == PERIODS[target] and ticks == cycles * period,
                    f"projection arithmetic failed: {case}, {target}")
            old_dir = HISTORICAL / "cases" / case / f"{target[0]}-{target[1]}"
            require((old_dir / "COMPLETE").is_file(), f"historical Embench case incomplete: {case}, {target}")
            require(read_json(old_dir / "provenance.json")["source_sha256"] == HISTORICAL_SOURCE_SHA256,
                    f"historical Embench source changed: {case}, {target}")
            old_rows = read_json(old_dir / "summary.json")
            require(len(old_rows) == 1, f"historical Embench case count changed: {case}, {target}")
            old = old_rows[0]
            old_period = {"single": 116, "baseline": 62, "advanced": 78}[target[0]]
            require(row_key(old) == target and old["cycles"] == cycles and old["period_ticks"] == old_period,
                    f"projected cycles differ from historical result: {case}, {target}")
            require(old["run_status"] == "completed" and old["structural_checked"] is False,
                    f"historical Embench case status changed: {case}, {target}")
            old_runs = [json.loads(line) for line in (old_dir / "runs.jsonl").read_text().splitlines()]
            checks = [run for run in old_runs if run["phase"] == "oracle-check"]
            require(len(checks) == 1 and checks[0]["completed"] is True and checks[0]["verified"] is True,
                    f"historical Embench oracle check missing: {case}, {target}")
            grid[case][target] = ticks
    require(len(grid) == 19 and all(set(row) == set(TARGET_KEYS) for row in grid.values()),
            "full Embench projection is not 19 × 7")
    return dict(grid)


def markdown_table(report: str, marker: str) -> tuple[list[str], dict[str, list[int]]]:
    require(marker in report, f"missing report marker: {marker}")
    lines = report[report.index(marker) + len(marker):].splitlines()
    start = next((i for i, line in enumerate(lines) if line.startswith("| ")), None)
    require(start is not None, f"missing table after: {marker}")
    table = []
    for line in lines[start:]:
        if not line.startswith("| "):
            break
        table.append([cell.strip() for cell in line.strip().strip("|").split("|")])
    require(len(table) >= 3, f"short report table after: {marker}")
    headings = table[0]
    rows: dict[str, list[int]] = {}
    for cells in table[2:]:
        name = re.sub(r"^\[([^]]+)\]\([^)]+\)$", r"\1", cells[0])
        require(name not in rows, f"duplicate report row: {name}")
        rows[name] = [int(value.replace(",", "")) for value in cells[1:]]
    return headings, rows


def verify_public_tables(report: str, panels: dict[str, dict], projection: dict) -> int:
    definitions = (
        ("fixed", "**Modeled ticks, complete 22 × 7 FF panel:**"),
        ("generated", "**Modeled ticks, complete 15 × 7 FF panel:**"),
        ("core-gap-dependencies", "**Modeled ticks, eight controlled settings:**"),
        ("core-gap-branches", "**Modeled ticks, nine controlled settings:**"),
        ("core-gap-other", "**Modeled ticks, six controlled settings:**"),
        ("fixtures", "**Modeled ticks, complete 2 × 7 FF panel:**"),
        ("projection", "**Conditional projected ticks, all 19 × 7 target cells:**"),
    )
    core_gap_rows: dict[str, list[int]] = {}
    checked = 0
    for group, marker in definitions:
        headings, rows = markdown_table(report, marker)
        require(headings == ["Case" if group != "projection" else "Workload", *SHORT_NAMES],
                f"report target order changed: {group}")
        if group.startswith("core-gap-"):
            require(not (set(core_gap_rows) & set(rows)), "duplicate core-gap report row")
            core_gap_rows.update(rows)
            source = panels["core-gap"]
        elif group == "projection":
            source = projection
        else:
            source = panels[group]
        if not group.startswith("core-gap-"):
            require(set(rows) == set(source), f"report case set changed: {group}")
        for case, values in rows.items():
            require(case in source, f"unexpected report row: {group}, {case}")
            require(values == [source[case][target] for target in TARGET_KEYS],
                    f"report values differ from results: {group}, {case}")
            checked += len(values)
    require(set(core_gap_rows) == set(panels["core-gap"]), "core-gap report rows incomplete")

    headings, rows = markdown_table(report, "Each cell below is **modeled ticks**.")
    require(headings == ["Target", *PATTERNS] and set(rows) == set(SHORT_NAMES),
            "long-branch report grid changed")
    for name, values in rows.items():
        target = TARGET_KEYS[SHORT_NAMES.index(name)]
        require(values == [panels["branches"][pattern][target] for pattern in PATTERNS],
                f"long-branch report values differ: {name}")
        checked += len(values)
    return checked


def appearance(target: tuple[str, str]) -> tuple[str, str]:
    if target[0] == "single":
        return "#2563a6", "o"
    if target[0] == "baseline":
        return "#b96113", "s"
    return "#087e72", "D"


def draw_rows(ax, values: list[float], xlim: tuple[float, float], *, digits: int = 3) -> None:
    ax.axvline(1, color="#65737e", lw=1, ls="--", zorder=0)
    for y, (target, value) in enumerate(zip(TARGET_KEYS, values)):
        color, marker = appearance(target)
        ax.scatter(value, y, s=52, color=color, marker=marker, zorder=3)
        ax.text(value + (xlim[1] - xlim[0]) * 0.014, y, f"{value:.{digits}f}×",
                va="center", fontsize=8.6, color="#1d2932")
    ax.set_yticks(range(len(TARGETS)), SHORT_NAMES)
    ax.set_ylim(len(TARGETS) - 0.5, -0.5)
    ax.set_xlim(*xlim)
    ax.grid(axis="x", color="#e3e7ea", lw=0.8)
    ax.set_axisbelow(True)
    ax.spines[["top", "right", "left"]].set_visible(False)
    ax.tick_params(axis="y", length=0)


def save(fig, path: Path, title: str, description: str, preview_dir: Path | None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, format="svg", bbox_inches="tight", pad_inches=0.22,
                metadata={"Title": title, "Description": description, "Date": None})
    if preview_dir is not None:
        preview_dir.mkdir(parents=True, exist_ok=True)
        fig.savefig(preview_dir / f"{path.stem}.png", dpi=120, bbox_inches="tight", pad_inches=0.22)
    plt.close(fig)


def plot_overview(panels: dict[str, dict], output: Path, preview_dir: Path | None) -> None:
    groups = (
        ("Fixed programs without expected traps", "fixed", tuple(f"program-{i:02d}" for i in (*range(1, 9), *range(17, 23)))),
        ("Generated families", "generated", GENERATED_CASES),
        ("Controlled core-gap settings", "core-gap", CORE_GAP_CASES),
        ("Longer branch patterns", "branches", PATTERNS),
        ("Checked-in ELF fixtures", "fixtures", ("self-check.elf", "compiled.elf")),
    )
    fig, axes = plt.subplots(5, 1, figsize=(11.7, 15.4), sharex=True)
    for ax, (title, group, cases) in zip(axes, groups):
        values = [math.exp(sum(math.log(panels[group][case][target] /
                                        panels[group][case][TARGET_KEYS[0]]) for case in cases) / len(cases))
                  for target in TARGET_KEYS]
        draw_rows(ax, values, (0.55, 1.75))
        ax.set_title(f"{title}  ·  {len(cases)} matched cases", loc="left", fontsize=11.5,
                     fontweight="bold", pad=9)
    fig.suptitle("Measured FF timing across workload groups", fontsize=17, fontweight="bold", y=0.995)
    fig.supxlabel("Geometric mean of modeled ticks / single-cycle modeled ticks  ·  lower is faster", y=0.022)
    fig.subplots_adjust(left=0.13, right=0.96, top=0.963, bottom=0.055, hspace=0.34)
    save(fig, output / "measured-ff-overview.svg", "Measured FF timing across workload groups",
         "Five groups, seven visible targets each. Values are unweighted geometric means of casewise modeled-tick ratios to single-cycle. The groups are not combined.", preview_dir)


def plot_contrasts(core_gap: dict, output: Path, preview_dir: Path | None) -> None:
    contrasts = (
        ("Distance-one dependency / independent long", "dependency-d1", "independent-long", (0.88, 3.82)),
        ("Load-use / scheduled loads", "load-use", "load-scheduled", (0.96, 1.48)),
    )
    fig, axes = plt.subplots(2, 1, figsize=(11.4, 8.3))
    for ax, (title, stressed, control, xlim) in zip(axes, contrasts):
        values = [core_gap[stressed][target] / core_gap[control][target] for target in TARGET_KEYS]
        draw_rows(ax, values, xlim)
        ax.set_title(title, loc="left", fontsize=12, fontweight="bold", pad=10)
        ax.set_xlabel("Stressed case ticks / control case ticks  ·  above 1 means more time")
    fig.suptitle("Matched workload contrasts · measured FF timing", fontsize=16, fontweight="bold", y=0.997)
    fig.subplots_adjust(left=0.15, right=0.96, top=0.92, bottom=0.08, hspace=0.39)
    save(fig, output / "measured-core-gap-contrasts.svg", "Matched workload contrasts",
         "Two measured FF contrasts, each showing all seven targets. The distance-one dependency affects baseline most; scheduling loads reduces time on the pipelines.", preview_dir)


def plot_branches(branches: dict, output: Path, preview_dir: Path | None) -> None:
    ticks = [[branches[pattern][target] for target in TARGET_KEYS] for pattern in PATTERNS]
    ratios = [[value / min(row) for value in row] for row in ticks]
    fig, ax = plt.subplots(figsize=(11.4, 5.8))
    image = ax.imshow(ratios, cmap="YlGnBu", norm=Normalize(vmin=1, vmax=2.2), aspect="auto")
    ax.set_xticks(range(len(TARGETS)), SHORT_NAMES)
    ax.set_yticks(range(len(PATTERNS)), PATTERNS)
    ax.xaxis.tick_top()
    ax.tick_params(length=0, pad=8)
    for y, row in enumerate(ticks):
        minimum = min(row)
        for x, value in enumerate(row):
            ax.text(x, y, f"{value:,}", ha="center", va="center", fontsize=9,
                    color="white" if ratios[y][x] > 1.56 else "#17232e",
                    fontweight="bold" if value == minimum else "normal")
            if value == minimum:
                ax.add_patch(Rectangle((x - 0.47, y - 0.45), 0.94, 0.9,
                                       fill=False, edgecolor="#17232e", linewidth=2))
    ax.set_title("Longer branch patterns · measured modeled ticks", loc="left", pad=44,
                 fontsize=15, fontweight="bold")
    bar = fig.colorbar(image, ax=ax, fraction=0.025, pad=0.025)
    bar.set_label("Ticks / fastest target within pattern")
    fig.subplots_adjust(left=0.13, right=0.94, top=0.72, bottom=0.08)
    save(fig, output / "measured-long-branches.svg", "Longer branch patterns",
         "Five 64-iteration patterns by seven targets. Each cell prints absolute modeled ticks; color compares it with the fastest target in that row. Outlines mark winners and ties.", preview_dir)


def plot_projection(projection: dict, output: Path, preview_dir: Path | None) -> None:
    names = sorted(projection)
    ratios = [[projection[name][target] / projection[name][TARGET_KEYS[0]]
               for target in TARGET_KEYS] for name in names]
    fig, ax = plt.subplots(figsize=(11.4, 10.7))
    image = ax.imshow(ratios, cmap="RdBu_r", norm=TwoSlopeNorm(vmin=0.55, vcenter=1, vmax=1.6),
                      aspect="auto")
    ax.set_xticks(range(len(TARGETS)), SHORT_NAMES)
    ax.set_yticks(range(len(names)), names)
    ax.xaxis.tick_top()
    ax.tick_params(length=0, pad=7)
    for y, row in enumerate(ratios):
        for x, value in enumerate(row):
            ax.text(x, y, f"{value:.2f}×", ha="center", va="center", fontsize=8.5,
                    color="white" if value < 0.74 or value > 1.35 else "#17232e")
    fig.suptitle("FULL EMBENCH · CONDITIONAL PROJECTION", x=0.17, y=0.992,
                 ha="left", fontsize=15, fontweight="bold", color="#96302b")
    fig.text(0.17, 0.964, "Older cycles × current FF periods; no current-source rerun",
             fontsize=10.5, color="#96302b", fontweight="bold")
    bar = fig.colorbar(image, ax=ax, fraction=0.025, pad=0.025)
    bar.set_label("Projected ticks / projected single-cycle ticks")
    fig.subplots_adjust(left=0.17, right=0.94, top=0.895, bottom=0.035)
    save(fig, output / "conditional-embench-projection.svg", "Full Embench conditional projection",
         "Nineteen workloads by seven targets. Ratios are projected from historical cycle counts and current FF clock periods; they are not current-source measurements.", preview_dir)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=OUTPUT)
    parser.add_argument("--preview-dir", type=Path, help="also write PNG previews for visual inspection")
    args = parser.parse_args()

    panels = {
        "fixed": load_campaign(CURRENT / "fixed", tuple(f"program-{i:02d}" for i in range(1, 23))),
        "generated": load_campaign(CURRENT / "generated", GENERATED_CASES),
        "core-gap": load_campaign(CURRENT / "core-gap", CORE_GAP_CASES),
        "fixtures": {},
        "branches": {},
    }
    for directory, case in (("compiled-smoke", "self-check.elf"),
                            ("compiled-compiled", "compiled.elf")):
        panels["fixtures"].update(load_campaign(CURRENT / directory, (case,)))
    for pattern in PATTERNS:
        directory = EXPERIMENTS / f"20261014-branch64-structural-{pattern}"
        panel = load_campaign(directory, ("branch-pattern",))
        panels["branches"][pattern] = panel["branch-pattern"]
    projection = load_projection(CURRENT / "embench-clock-projection.csv")
    checked = verify_public_tables(REPORT.read_text(), panels, projection)

    output = args.output_dir
    plot_overview(panels, output, args.preview_dir)
    plot_contrasts(panels["core-gap"], output, args.preview_dir)
    plot_branches(panels["branches"], output, args.preview_dir)
    plot_projection(projection, output, args.preview_dir)
    print(f"Validated {checked} public table cells and generated four SVG figures in {output}")


if __name__ == "__main__":
    main()
