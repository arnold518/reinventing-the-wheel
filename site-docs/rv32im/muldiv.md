# MulDivUnit32

Shared RV32M launch/result interface.

Catalog: `rv32im.mul-div.32`. The structural implementation remains available.
A compact form executes that circuit's own four-state gate list with
characterized register state and a delay table proven from the same circuit.
It preserves iterative latency and ownership; no host multiply/divide or
instruction oracle is used. The newly added form is undergoing equivalence
and timing validation.

## Pins

| Pin | Direction | Bits |
| --- | --- | --- |
| `A` | Input | 32 |
| `B` | Input | 32 |
| `CANCEL` | Input | 1 |
| `CLK` | Input | 1 |
| `ENABLE` | Input | 1 |
| `OP` | Input | 3 |
| `RST` | Input | 1 |
| `START` | Input | 1 |
| `TAKE` | Input | 1 |
| `BUSY` | Output | 1 |
| `DONE` | Output | 1 |
| `READY` | Output | 1 |
| `RESULT` | Output | 32 |

## Behavior

OP uses instruction funct3: 0 MUL, 1 MULH, 2 MULHSU, 3 MULHU, 4 DIV, 5 DIVU, 6 REM, 7 REMU. Latch OP and operands on launch; expose RESULT only when DONE. Latency, pause, completion retention and cancellation match the underlying units.

## Internal structure

Multiplier32 and Divider32, an operation register and result-selection gates. Exactly one arithmetic engine accepts each launch.

## Edge cases and integration

Changing OP/A/B after launch does not change the owned result. Cancellation is synchronous and works while disabled. Results are not shortened to host arithmetic latency.

## Standalone timing boundary

Under the same synchronous binary input boundary as the core, the structural
unit certifies at 64 unit ticks, 2322 ps (embedded) and 1620 ps (desktop).
`RV32IMTimingTest` derives these periods independently for each profile.
Launch latency remains one launch edge plus 32 enabled iteration edges.

## Recorded example and checks

`MulDivUnit32Test` owns the independent functional checks; `MulDivUnit32ScenarioTest` retains this smaller recorded example. Both use the same logical component owner.

<!-- simulator-examples: mul-div-unit32 MulDivUnit32ScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=mul-div-unit32&amp;embed=1" title="MulDivUnit32" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=mul-div-unit32" class="md-button">Open the recorded circuit</a>
