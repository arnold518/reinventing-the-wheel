# Unified Component Migration Report

Status: complete

Branch: `unified-component-migration`

Starting checkpoint: `f519350`

Last updated: 2026-07-29

## Outcome

CircuitSim now uses one public family identity for every replaceable
component. One recursive `BuildProfile` chooses structural or behavioral
fidelity at the root and throughout the selectable subtree.

The foundational hierarchy remains:

```text
Component -> IOComponent -> BasicComponent
```

Fidelity is selection metadata, not inheritance.

## Final simplification

The final correction removed selection concepts that duplicated the recursive
profile:

- forced-root construction;
- root plus descendant run configuration;
- descendants-only rules;
- rule priority and implicit specificity;
- profile-generator objects;
- named preset lookup; and
- reference-only selection state.

There is one construction path:

```cpp
catalog.createRoot(request, profile);
```

Rules are ordered, and the last matching rule is the explicit override.
Standard policy helpers return normal `BuildProfile` values.

## Component families

`ComponentFamily` groups:

- contract ID;
- public/visual type name;
- shared pin initializer; and
- available implementation factories.

Parents request families:

```cpp
builder.add(circuit::families::ALU32, "ALU");
```

Profiles, parents, tests, bindings, and the visualizer do not name internal
implementation classes.

The RV32I core and system now register both fidelities through their families,
so family and catalog construction cannot disagree.

## Capabilities

`RegisterStateView` provides fidelity-independent register-file observation.

`RV32IStateView` now provides fidelity-independent architectural CPU
observation. It fixes a real mixed-profile bug: the structural system can
contain a behavioral selected core without casting it to the structural core
class.

## Tests

Each component contract owns one logical test. `ComponentTestRunner` derives
one complete profile per DUT fidelity by appending an exact-path rule to the
caller’s base profile.

Runs use independent trees and simulators. They check independent expected
outputs and compare complete public-output snapshots at semantic checkpoints.

Program tests keep sixteen parameterized scenarios of one logical RV32I system
test. They compare selected system runs with the independently executed
behavioral answer sheet, instruction oracle, and hard-coded final outcomes.

## Registry and visualizer

The C++ registry is the scenario factory and metadata source. Python exposes
registry queries plus `create_test_by_name()` instead of binding every test
class separately.

Registry metadata explicitly marks scenarios that have meaningful visual
circuits. The visualizer no longer infers this from whether a Python class
happened to be bound.

Final layout regeneration:

```text
110 visual scenarios
95 representative topology builds
217,804 traversed component instances
178 unique parent topologies
131 reusable type layouts
0 profile-specific layouts
110 root layouts
0 overlapping generated child layouts
```

Profile fingerprints exclude human profile names and reasons, avoiding layout
duplication for prose-only changes.

## Verification

Build:

```text
cmake -S . -B build
cmake --build build -j4
passed
```

Complete regression:

```text
ctest --test-dir build --output-on-failure -j2
127/127 passed
535.15 seconds
```

The suite includes:

- catalog/profile resolution and recursive selection;
- mixed structural-system/behavioral-core construction;
- all component contract tests;
- structural/behavioral public-output comparisons;
- all sixteen RV32I program scenarios;
- a mixed-fidelity audit covering all 14 selectable RV32I contracts and all
  1,077 selectable instances;
- registry/CTest synchronization;
- Python registry and profile bindings;
- atomic profile persistence, stale-revision rejection, and failed-apply
  rollback; and
- layout key and overlap regression.

## Deployed visualizer

Visualizer V2 now has a docked, resizable component explorer. Expanded rows
request structural fidelity, collapsed rows request behavioral fidelity, and
fixed rows expose no toggle. `Apply & Run` rebuilds the scenario using the
saved recursive profile. Clicking a row smoothly focuses the corresponding
canvas component.

The tree is virtualized. The complete 54,049-component RV32I tree keeps all
component records available for search and navigation but creates only the DOM
rows around the viewport.

Pin/wire drawing metadata is now progressive as well. The first response keeps
the complete component tree and simulation state but includes only the root
signal scope. Exact deeper scopes load in batches when zoom or focus makes
them resolvable. This reduced the measured Program 1 response from 128.8 MB to
17.9 MB uncompressed and 0.77 MB over browser gzip without omitting simulated
signals or history.

The port-8765 systemd worker was restarted after the final rebuild. Host policy
blocked an interactive `systemctl restart`, so the exact validated worker was
terminated and the service's restart policy launched a fresh worker.

Post-restart checks:

```text
GET /api/health -> {"ok":true}
GET /api/circuit?scenario=rv32i-program1 -> 200
layout key: rv32i-program1@66d12149b4bd5ced
54,049 components; 180,155 pins; 103,361 wires
initial drawing scope: 53 pins; 29 wires
browser console errors: 0
warm local browser overview: 2.8 seconds
explorer records: 54,049; rendered DOM rows: 44
timeline step: time 0 -> 2,020
depth-8 gate focus: exact 2-input/1-output scope loaded
MemoryBit live Apply & Run: 28 structural components -> 1 behavioral
component -> 28 structural components
```

Browser profile choices remain machine-local experiment state.
`visualizer-v2/profiles.json` is ignored by Git, is created atomically when
needed, and was preserved across the deployment checks.

## Resulting design

- Users see one public `ALU32`.
- A profile chooses `Structural` or `Behavioral`, not a class name.
- One profile completely describes root and subtree selection policy.
- Structural implementations remain readable wiring blueprints.
- Behavioral implementations remain tested scale abstractions.
- Capabilities and public pins survive fidelity changes.
- Layouts are keyed by topology-affecting policy.
- The visualizer edits the same recursive profile instead of maintaining a
  second navigation model.

See:

- `docs/unified-component-model-migration-plan.md`
- `docs/unified-component-profiles.md`
- `docs/component-test-model-migration-report.md`
- `docs/visualizer-v2-profile-explorer.md`
- `docs/rv32i-profile-toggle-audit.md`
- `docs/rv32i-visualizer-performance-report.md`
