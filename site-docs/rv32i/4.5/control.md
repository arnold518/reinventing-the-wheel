# § 4.5.7. Dependencies and Control

The baseline has two separate control components: one detects unavailable register operands; the other decides which pipeline registers hold, advance, or receive bubbles. Neither stores an instruction or predicts a branch.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IBaselineDependencyUnit` | RAW dependencies against three older instruction slots | Yes | Yes |
| `RV32IBaselineControl` | Enable, stall, bubble, branch-wait and terminal arbitration | Yes | Yes |
| `RV32IBaselinePacketStatus` | Internal shared packet-status gate group | Yes | No standalone variant |

HALTED and TRAPPED come from the separate [execution-status block](writeback.md#execution-status), not storage inside Writeback. They keep control inactive after the terminal packet leaves MEM/WB.

## Control responsibilities

`CONTROL` is the main coordinator: it decides how instructions move, but does not calculate register results or jump addresses. Detection, calculation, and state updates remain separate jobs.

| Child in the complete core | Responsibility |
| --- | --- |
| `DEPENDENCIES` | Report whether Decode needs a register value that an older instruction has not written yet |
| `DECODE` | Identify source registers and recognize branches/jumps |
| `EXECUTE.DATAPATH` and `EXECUTE.NEXT_PC` | Evaluate the branch condition and calculate/select the resolved next address |
| `CONTROL` | Decide which pipeline registers hold, advance, or receive bubbles, and when Fetch may update its PC |
| `FETCH` and the four pipeline registers | Carry out those decisions at the clock edge |

The `CONTROL` child inside `DECODE` is the instruction decoder, not this top-level pipeline coordinator. In the visualizer, inspect the complete core's `DEPENDENCIES` and `CONTROL` blocks for hazard handling.

## 4.5.7.1. `RV32IBaselineDependencyUnit`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `D_VALID`, `E_VALID`, `M_VALID`, `W_VALID` | Input | 1 each | Decode/Execute/Memory/Writeback validity |
| `D_CONTROL`, `E_CONTROL`, `M_CONTROL`, `W_CONTROL` | Input | 32 each | Packed decoded control |
| `D_REGISTER_ADDRESSES`, `E_REGISTER_ADDRESSES`, `M_REGISTER_ADDRESSES`, `W_REGISTER_ADDRESSES` | Input | 32 each | Packed register addresses |
| `WAIT_EX`, `WAIT_MEM`, `WAIT_WB` | Output | 1 each | At least one actually used source depends on that older slot |
| `DATA_HAZARD` | Output | 1 | OR of the three wait outputs |

### Behavior

A match requires a valid consumer, a valid producer with REG_WRITE=1 and RD≠0, and a matching source whose USES_RS bit is one. Encoded but unused source bits do not cause a stall. x0 never produces a dependency. All older writers matter, not only loads.

The comparisons use **register numbers**, not register values: “Decode reads x1, and an older instruction will write x1.” WAIT_EX, WAIT_MEM, and WAIT_WB check the three older slots independently; their OR is DATA_HAZARD. The detector reports the conflict; it does not hold registers itself.

WAIT_WB deliberately includes a producer currently in WB. Without a bypass, the old register value is still present before that producer's edge. An immediately dependent instruction normally waits three cycles: producer in EX, then MEM, then WB.

#### C++ example

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:dependency"
```

### Implementation

#### Structural implementation

DECODE_FIELDS/SOURCE_REGISTERS unpack the consumer. WAIT_EX, WAIT_MEM, and WAIT_WB reuse the tested two-source comparison circuit in RV32IHazardDetectionUnit. That older component names its producer-enable pin EX_MEM_READ; here REG_WRITE drives it so the comparator checks **all** register producers. This reuse does not turn the baseline into a load-only hazard detector.

#### Behavioral implementation

Three independent register-match checks produce the same outputs. The registered family is `rv32i.baseline.dependency`.

### Examples

Every destination register, including x0, is tested in each producer position, followed by disabled/bubble producers and ignored sources.

<!-- simulator-examples: rv32i-baseline-dependency RV32IBaselineDependencyUnitTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-dependency&amp;embed=1" title="Baseline dependency unit" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-dependency" class="md-button">Open dependency unit</a>

### Tests

`RV32IBaselineDependencyUnitTest` compares both implementations. Dependent-ALU and load-use program cases verify the resulting stalls in the complete pipeline.

## 4.5.7.2. `RV32IBaselineControl`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `ENABLE`, `RST`, `HALTED`, `TRAPPED` | Input | 1 each | Global execution permission and state |
| `DATA_HAZARD` | Input | 1 | Decode needs an older register result |
| `D_CONTROL_FLOW`, `E_CONTROL_FLOW` | Input | 1 each | Valid branch/jump in Decode or Execute |
| `D_TERMINAL`, `E_TERMINAL`, `M_TERMINAL`, `W_TERMINAL` | Input | 1 each | Valid terminal reason at each stage |
| `ACTIVE`, `COMMIT_ENABLE`, `MEMORY_ENABLE` | Output | 1 each | Global advance, WB commit, permitted native memory operation |
| `FETCH_VALID`, `PC_WRITE`, `PC_REDIRECT` | Output | 1 each | Accept instruction; update fetch PC; use resolved PC |
| `IF_ID_WRITE`, `ID_EX_WRITE`, `EX_MEM_WRITE`, `MEM_WB_WRITE` | Output | 1 each | Enable each pipeline register |
| `IF_ID_FLUSH`, `ID_EX_FLUSH`, `EX_MEM_FLUSH`, `MEM_WB_FLUSH` | Output | 1 each | Capture bubble when that register is enabled |
| `DATA_STALL`, `BRANCH_WAIT`, `PIPELINE_FLUSH` | Output | 1 each | Active-cycle diagnostic conditions |

### Behavior

#### A stall does not stop the clock

The controller drives the [pipeline registers' WRITE_ENABLE and FLUSH pins](registers.md#advance-hold-and-bubble). WRITE_ENABLE=0 holds the stored instruction. WRITE_ENABLE=1 captures the next packet; FLUSH=1 makes that captured packet invalid, creating a bubble. The payload bits need not be zero, but an invalid slot cannot write registers or memory or retire.

For a data dependency, CONTROL holds the fetch PC and IF/ID, inserts a bubble into ID/EX, and lets EX/MEM and MEM/WB advance. This keeps the waiting instruction in Decode while older instructions finish. Holding the whole pipeline would prevent the producer from writing the value the consumer needs.

The controller does not turn a clock off or run a software wait loop. In the structural implementation, combinational gates compute the enables and flushes from the current stage signals. All storage then reacts to the same rising edge.

#### Movement and priority

| Situation | Fetch / IF-ID | ID-EX | Older stages |
| --- | --- | --- | --- |
| Normal | Accept and advance | Advance | Advance |
| RAW dependency | Hold | Bubble | Advance so producer can finish |
| Branch in Decode, operands ready | Stop fetch; clear younger slot | Accept branch | Advance |
| Branch in Execute | Resolve PC; do not accept old fetch | Bubble | Branch advances |
| Terminal instruction | Stop younger work | Bubble younger slots | Terminal and older instructions drain to WB |
| Disabled, reset, halted, trapped | No advancing writes | Hold except reset | No retirement or memory write |

Terminal priority follows age. A terminal instruction in WB suppresses a younger MEM store. A MEM terminal instruction suppresses a younger Execute redirect. Reset also reaches the storage components directly; their asynchronous resets are not implemented by these flush outputs.

FLUSH outputs describe bubble conditions even while inactive; the corresponding WRITE outputs are zero, so storage holds. PIPELINE_FLUSH is a cycle-level condition, not a unique branch-event pulse. BRANCH_WAIT can overlap DATA_STALL when a branch waits for a register operand.

#### C++ example

This includes every control output and the priority rules, including impossible input combinations used by the exhaustive component test. Real integrated stage terminal/control-flow signals are validity-qualified.

```cpp
--8<-- "site-docs/rv32i/4.5/examples.hpp:control"
```

### Implementation

#### Structural implementation

AND/OR/NOT gates form execution permission, dependency wait, branch resolution, age-prioritized terminal conditions, and stage enables. `MEM_WB_FLUSH` is `W_TERMINAL` wired straight through. The complete core keeps this logic in the CONTROL block below the datapath.

#### Behavioral implementation

`control` evaluates the same combinational Boolean equations. It has no clock or pending state. The family is `rv32i.baseline.control`.

### Examples

The following walkthroughs describe active cycles after reset, with ENABLE=1, no faults, and no halt/trap. A cycle means the work **before its ending rising edge**; captures and writes occur at that edge. These cycle numbers explain the instruction sequence, not simulator timestamps or checkpoint numbers. The complete recorded test rows follow the walkthroughs.

#### Data hazard: hold the consumer, advance the producer

```asm
addi x1, x0, 5     # A: produces x1
add  x2, x1, x1    # B: consumes x1
```

Start with an empty pipeline. In cycle 3, A is in Execute and B is in Decode. DEPENDENCIES sees that B reads the register A will write and raises DATA_HAZARD.

At that cycle's ending edge:

1. PC_WRITE=0 holds the fetch PC; FETCH_VALID=0 accepts no new fetched instruction.
2. IF_ID_WRITE=0 keeps B in IF/ID, so Decode continues to see B.
3. ID_EX_WRITE=1 and ID_EX_FLUSH=1 put a bubble into Execute instead of B's not-yet-ready operands.
4. EX_MEM_WRITE=1 and MEM_WB_WRITE=1 let older instructions advance. Their flush inputs remain zero; A moves into Memory.

| Cycle | A | B | Dependency / ending-edge action |
| --- | --- | --- | --- |
| 1 | Fetch | Not fetched | IF/ID captures A |
| 2 | Decode | Fetch | ID/EX captures A; IF/ID captures B |
| 3 | Execute | Decode, waiting | WAIT_EX=1; hold B and insert a bubble into ID/EX |
| 4 | Memory | Decode, waiting | WAIT_MEM=1; hold B and insert another bubble |
| 5 | Writeback | Decode, waiting | WAIT_WB=1; A writes x1=5 at the ending edge; B still does not advance on that edge |
| 6 | Finished | Decode, ready | DATA_HAZARD=0; B reads the updated x1 and captures both operands into ID/EX |
| 7 | Finished | Execute | B computes 5+5 |
| 8 | Finished | Memory | B carries its arithmetic result through Memory |
| 9 | Finished | Writeback | B writes x2=10 |

The important distinction is that **B waits, but A keeps moving**. There is no forwarding path and no same-edge Writeback-to-Decode bypass. Stores and branches also wait if their actually used source registers depend on an older writer.

#### Jump: stop younger fetches, let the jump resolve

```asm
jal  x0, target    # PC=0: jump to PC=8; discard the link value
addi x1, x0, 99    # PC=4: skipped
target:
addi x2, x0, 7     # PC=8: target instruction
```

This example also starts with an empty pipeline. The jump has no source-register dependency.

| Cycle | Jump's stage | Fetch and ending-edge action |
| --- | --- | --- |
| 1 | Fetch | Accept the jump into IF/ID and advance the fetch PC to 4 |
| 2 | Decode | D_CONTROL_FLOW=1 stops fetch acceptance. ID/EX captures the jump; IF/ID captures a bubble. The fetch PC stays 4 |
| 3 | Execute | EXECUTE selects address 8. PC_REDIRECT=1 and PC_WRITE=1 load 8 into the fetch-PC register; younger slots remain bubbles |
| 4 | Memory | Fetch accepts the target instruction at address 8 |
| 5 | Writeback | The jump retires; the target instruction is already in Decode |

In cycle 2, IF_ID_WRITE=1 and IF_ID_FLUSH=1 clear the old Decode slot, while ID_EX_WRITE=1 and ID_EX_FLUSH=0 transfer the jump into Execute. All registers capture from pre-edge signals, so the jump is copied forward before the changed IF/ID outputs propagate; it is not lost or executed twice.

In cycle 3, FETCH_VALID remains zero. The redirect updates the PC at the edge; target fetching starts in cycle 4, not during the resolution cycle. The instruction at address 4 may appear on the memory wires, but it is never accepted as a valid younger instruction.

Conditional branches use the same wait: Execute selects the target if taken, or PC+4 if untaken. An untaken branch still incurs the wait. JALR also resolves in Execute, using its captured source operand. A JAL/JALR with a nonzero destination writes its link value later in Writeback; Fetch does not wait for that retirement.

#### A branch or JALR can wait for data too

For `addi x1,x0,8` followed by `jalr x0,0(x1)`, both DATA_HAZARD and D_CONTROL_FLOW are initially one while JALR is in Decode. The controller keeps IF_ID_WRITE=0, even though IF_ID_FLUSH=1. A flush does not override a hold, so the waiting JALR remains in IF/ID rather than disappearing.

Once x1 has been written and DATA_HAZARD clears, the controller transfers JALR into Execute and bubbles IF/ID. Execute then resolves address 8; Fetch resumes afterward. BRANCH_WAIT can therefore overlap DATA_STALL: they are conditions, not counts that should simply be added together.

#### Signals behind the walkthroughs

These are the named checkpoints in the component simulation below. `WRITE / FLUSH` describes what the corresponding pipeline register does at an edge; EX/MEM and MEM/WB advance without flushing in all five rows.

<!-- baseline-control-actions:start -->
| Checkpoint | FETCH_VALID | PC_WRITE | PC_REDIRECT | IF_ID WRITE / FLUSH | ID_EX WRITE / FLUSH |
| --- | ---: | ---: | ---: | --- | --- |
| `normal-flow` | 1 | 1 | 0 | 1 / 0: advance | 1 / 0: advance |
| `raw-wait` | 0 | 0 | 0 | 0 / 0: hold consumer | 1 / 1: bubble |
| `branch-waits-for-operand` | 0 | 0 | 0 | 0 / 1: hold branch | 1 / 1: bubble |
| `branch-enters-execute` | 0 | 0 | 0 | 1 / 1: bubble | 1 / 0: accept branch |
| `branch-resolves` | 0 | 1 | 1 | 1 / 1: bubble | 1 / 1: bubble |
<!-- baseline-control-actions:end -->

`RV32IBaselineDocumentationTest` checks this signal summary against the simulator's exported checkpoint values. The walkthroughs describe normal operation; the full behavior and C++ example above also cover older faults, reset, and disabled execution.

#### Complete checkpoint table

The first rows name common transitions. The remaining rows enumerate all **2,048** binary combinations of the eleven inputs; the scrollable table includes every row and checkpoint time. To inspect the decisions above, use `raw-wait`, `branch-waits-for-operand`, `branch-enters-execute`, and `branch-resolves` before the exhaustive rows.

<!-- simulator-examples: rv32i-baseline-control RV32IBaselineControlTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-baseline-control&amp;embed=1" title="Baseline pipeline control" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32i-baseline-control" class="md-button">Open pipeline control</a>

### Tests

`RV32IBaselineControlTest` checks the complete binary truth table and structural/behavioral equivalence. Connected-core tests check that the enables actually hold/bubble the intended pipeline registers. Sources: `RV32IBaselinePipeline.cpp`, `RV32IBaselinePipelineBehavioral.cpp`.

## Shared structural helper: `RV32IBaselinePacketStatus`

Decode, Execute, and Memory contain a `PACKET_STATUS` child. This is a grouping of existing Boolean gates and field rewiring, not a new stage or storage element. It keeps the main datapath visible without a row of small status gates dominating each stage view.

Its inputs are VALID (1 bit) and CONTROL, FETCH_STATUS, EXECUTION_STATUS (32 bits each); its outputs are TERMINAL and CONTROL_FLOW (1 bit each). Only the defined status/control bits matter. Decode supplies zero EXECUTION_STATUS; Execute supplies its newly calculated status; Memory supplies the captured EX/MEM status.

```cpp
bool terminal = valid && ((fetch_status & 3) || !(control & 1) ||
                          (control & ((1u << 5) | (1u << 6))) ||
                          (execution_status & 1));
bool control_flow = valid && (((control >> 13) & 7) || ((control >> 16) & 3));
```

The helper is internal, with no independent fidelity selector or behavioral variant. Its equations are included in each enclosing stage's behavioral implementation and verified through those stages' equivalence tests. Inspect PACKET_STATUS inside any of the three embedded stage simulations to see the structural gates.
