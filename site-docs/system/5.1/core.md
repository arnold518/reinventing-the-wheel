# § 5.1.3. `RV32IMFiveStageCore`

The complete RV32IM processor: the advanced five-stage pipeline of
[§ 4.6](../../rv32i/4.6/index.md) (forwarding, load-use handling, branch
prediction, memory waits) with the M parts of [§ 5.1.2](pipeline.md). It is
the processor of every machine in this chapter, where its two memory ports
become two masters on the bus.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IMFiveStageCore` | RV32I + M, five stages, two request/response ports | Yes | Yes |

Catalog: `rv32im.core.educational-five-stage`; parameters `prediction`
and `prediction_entries` as for the advanced RV32I core.

`prediction` is `not-taken` (default), `taken`, `backward-taken`, `one-bit`
or `two-bit`; `prediction_entries` is a power of two in 1–256 (default 16).
These configure hardware at construction, independently of fidelity.

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; active-high reset (the PC restarts at 0) |
| `ENABLE` | Input | 1 | Low holds the whole pipeline |
| `<P>REQ_VALID`, `<P>REQ_OP`, `<P>REQ_ADDR`, `<P>REQ_WRITE_DATA`, `<P>REQ_WRITE_MASK` | Output | 1, 3, 32, 32, 4 | Requests, for each prefix P = `IMEM_`, `DMEM_` |
| `<P>REQ_READY` | Input | 1 | The corresponding request is taken |
| `<P>RSP_VALID`, `<P>RSP_READ_DATA`, `<P>RSP_STATUS` | Input | 1, 32, 3 | The corresponding answer |
| `<P>RSP_READY` | Output | 1 | The core can take that answer |
| `COMMIT_VALID` | Output | 1 | An instruction retires this cycle |
| `HALTED`, `TRAPPED`, `TRAP_CAUSE` | Output | 1, 1, 4 | `EBREAK` stopped the core; a trap stopped it, and why |
| `IF_ID_VALID`, `ID_EX_VALID`, `EX_MEM_VALID`, `MEM_WB_VALID` | Output | 1 each | Whether each packet register holds an instruction |
| `IF_ID_PC`, `ID_EX_PC`, `EX_MEM_PC`, `MEM_WB_PC`, `FETCH_PC` | Output | 32 each | Packet PCs and current fetch PC |
| `PIPELINE_STALL`, `PIPELINE_FLUSH`, `LOAD_USE_STALL`, `MEMORY_STALL`, `DATA_PORT_STALL`, `EXECUTION_STALL` | Output | 1 each | Why the pipeline is not moving |
| `BRANCH_RESOLVED`, `BRANCH_MISS`, `PREDICTION_RECOVERY`, `INSTRUCTION_ATTEMPT` | Output | 1 each | Prediction and retirement events |

Both ports are the transaction port of [§ 3.4](../../memory/transactions.md):
any memory, cache, arbiter or bus can answer them.

## Behavior

RV32I integer instructions as in § 4.6, plus `MUL`, `MULH`, `MULHSU`,
`MULHU`, `DIV`, `DIVU`, `REM`, `REMU` with the results of the RISC-V M
specification. An M instruction stalls the pipeline for its 33 edges. A fetch
or data response with a nonzero status stops the core with an access-fault
trap. No privilege modes, interrupts, compressed, atomic or floating-point
instructions.

Reset discards pipeline and outstanding-request state, clears the register
file and restarts fetch at address 0. It does not clear an external memory.
After reset, drive `ENABLE = 1` and service both ports; each endpoint must
hold an answer until its own `RSP_READY` handshake. The two ports can be
waiting independently.

`EBREAK` is the clean stop used by the program fixtures. Traps are terminal:
there is no handler entry/return, machine CSR bank, `mtvec` or `MRET`.
Writing rd = x0 still executes the instruction, but architectural x0 remains
zero. The compact-core comparison domain is known binary inputs after reset
with setup met; arbitrary `X`/`Z` clocks and controls are outside that domain.

## Implementation

### Structural implementation

The same five-stage assembly as the advanced RV32I core, with the RV32IM
decode stage, execute stage and coordinator. Packet registers, forwarding,
prediction, memory requesters, writeback and retirement observation are
unchanged. The core offers the register and retirement views the recorded
runs use to check every instruction.

### Behavioral implementation

The compact core runs a compiled gate list of this core's own gates with
characterized register state, and publishes on delays proven on the same
circuit. It holds for known binary inputs after reset with setup met, and
is observed at instruction commits. The independent instruction oracle is
used only to check it.

**Timing.** Alone, with binary inputs changing one tick after falling
edges, the structural core certifies minimum clock periods of 84 unit
ticks, 3900 ps (embedded) and 2716 ps (desktop). Connected to memories,
buses and devices, the whole machine is certified separately
([§ 5.7](../5.7.md)).

The selected default-table paths below assume reset inactive. Setup and
clock-to-output delays describe propagation around an edge; the arithmetic
algorithm's 33-edge latency is separate. The table's internal period is not
the assembled-machine certificate. Prediction variants have their own tables.

<!-- timing-rows
Fetch request after an edge | rv32im.core.educational-five-stage.behavioral | CLK ↑ → IMEM_REQ_VALID
Retirement after an edge | rv32im.core.educational-five-stage.behavioral | CLK ↑ → COMMIT_VALID
Fetch data setup | rv32im.core.educational-five-stage.behavioral | IMEM_RSP_READ_DATA before CLK ↑
Enable setup | rv32im.core.educational-five-stage.behavioral | ENABLE before CLK ↑
-->

## Examples

This recording isolates fetch backpressure, request retention, disable and
reset. The [recorded programs](programs.md) run complete instruction
sequences, and [§ 5.6](../5.6.md) runs the games.

<!-- simulator-examples: rv32im-core RV32IMFiveStageCoreScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rv32im-core&amp;embed=1" title="RV32IMFiveStageCore simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rv32im-core" class="md-button">Open the full visualizer</a>

## Source and tests

- [Interface](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/rv32im/RV32IMCore.hpp), [structural extension and assembly](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32im/RV32IMCore.cpp)
- [Compact core and observations](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32im/RV32IMCoreBehavioral.cpp), [functional tests](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp)

| Test | Coverage |
| --- | --- |
| `RV32IMFiveStageCoreTest` | Programs on the structural and compact cores, identical protocol and retirement timing; every prediction mode |
| `RV32IMCompiledProgramTest` | A compiled C program checked against the instruction oracle |
| `RV32IMTimingTest` | Certified clock periods in every timing profile |
| Programs 23–27 | [Recorded programs](programs.md), every commit checked |
