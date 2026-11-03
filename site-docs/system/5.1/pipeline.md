# § 5.1.2. Pipeline Parts

The M instructions need three changes to the advanced RV32I pipeline of
[§ 4.6](../../rv32i/4.6/index.md): decode must recognize them, execute must
run a 33-edge multiply or divide instead of a one-cycle ALU operation, and
the pipeline must wait while it runs. Each RV32IM part keeps the RV32I part
inside it unchanged (as a child named `INTEGER`) and adds only the M logic
around it. The decode stage instead reuses the shared decode blueprint with
the M-aware control unit; its register-file child keeps its usual name.
The pins and packed fields retain the RV32I contracts.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IMDecodeControlUnit` | RV32I decode plus the eight M encodings | Yes | Yes, explicit gate-list form |
| `RV32IMDecodeStage` | The decode and register-read stage using that decoder | Yes | No |
| `RV32IMExecuteStage` | Forwarded integer execution plus one M arithmetic slot | Yes | No |
| `RV32IMPipelineCoordinator` | Pipeline movement that also waits for arithmetic | Yes | Yes, explicit gate-list form |

The catalog IDs, in the same order, are `rv32im.decode-control`,
`rv32im.pipeline.stage.decode`, `rv32im.pipeline.stage.execute` and
`rv32im.pipeline.coordinator`. The shared compact registry adds explicit
gate-list forms for the combinational decode-control and coordinator parts;
collapsing those parts can select them, while general compact preferences
keep their gates. Decode and execute stage roots remain structural with
selectable children. The compact whole core includes all four parts' gates.
None of these four has a stored delay-table entry in the current files;
the two explicit compact forms derive their delays when built.

### How an M instruction moves

`MUL x3, x1, x2` reaches execute. The execute stage's M slot launches
`MulDivUnit<32>` with the forwarded values of x1 and x2 and raises
`EXECUTION_WAIT`. While it is high, the coordinator holds every pipeline
register and fetch (`EXECUTION_STALL`); nothing older retires twice. After
33 edges the unit's result replaces the ALU result, `EXECUTION_WAIT` falls,
the instruction moves on to memory and writeback like any `ADD`.

## 5.1.2.1. `RV32IMDecodeControlUnit`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `INSTRUCTION` | Input | 32 | The instruction word |
| `LEGAL`, `TRAP_REQUEST`, `DECODE_TRAP_CAUSE`, `HALT_REQUEST` | Output | 1, 1, 4, 1 | Recognition and terminal requests |
| `RS1_ADDR`, `RS2_ADDR`, `RD_ADDR` | Output | 5 each | Register fields |
| `USES_RS1`, `USES_RS2`, `REG_WRITE` | Output | 1 each | Register use |
| `ALU_OP`, `ALU_A_SEL`, `ALU_B_SEL` | Output | 5, 2, 2 | ALU controls |
| `IMM` | Output | 32 | Immediate |
| `BRANCH_TYPE`, `JUMP_TYPE` | Output | 3, 2 | Control flow |
| `MEM_READ`, `MEM_WRITE`, `MEM_SIZE`, `LOAD_SIGN_EXTEND` | Output | 1, 1, 2, 1 | Memory access |
| `WRITEBACK_SEL` | Output | 2 | Writeback source |

These are the pins of the RV32I decoder ([§ 4.3.4](../../rv32i/4.3/instruction-decoder.md)).

### Behavior

An M instruction has opcode `0x33` (register-register) and funct7 `0x01`;
funct3 selects the operation. It is legal, uses both sources and writes rd;
memory, branch, halt and trap controls stay clear. Every other encoding is
decoded exactly as by the RV32I decoder, which itself still rejects M.
Reserved funct7 patterns stay illegal. Unknown instruction bits follow the
gates.

For binary words, M recognition is `(instruction & 0xfe00007fu) ==
0x02000033u`; the operation is `(instruction >> 12) & 7u`. Register fields
pass through, including rd = 0. The override sets `WRITEBACK_SEL = 1` and
`MEM_SIZE = 2`, and clears the other non-register control fields; execute
recognizes M again from the raw instruction. The verification domain is
known binary words: uncertain opcode bits are not specified to produce a
clean illegal-instruction response.

### Implementation

`INTEGER` is the RV32I decoder; `EXTENSION` recognizes M and overrides the
control fields. ALU operation numbers and packet fields are unchanged.

### Examples

The recording covers the eight M encodings, an integer instruction, an x0
destination, and illegal and reserved encodings.

<!-- simulator-examples: rv32im-decode-control RV32IMDecodeControlScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rv32im-decode-control&amp;embed=1" title="RV32IMDecodeControlUnit simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rv32im-decode-control" class="md-button">Open the full visualizer</a>

## 5.1.2.2. `RV32IMDecodeStage`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Register-file clock and reset |
| `IN_VALID` | Input | 1 | IF/ID packet validity |
| `IN_PC`, `IN_INSTRUCTION`, `IN_FETCH_STATUS` | Input | 32 each | IF/ID packet fields |
| `WB_VALID`, `WB_REG_WRITE`, `WB_REGISTER_WRITE` | Input | 1 each | Writeback validity, decoded write intent, actual register-write enable |
| `WB_RD`, `WB_VALUE` | Input | 5, 32 | Writeback address and value |
| `EX_VALID`, `EX_CONTROL`, `EX_REGISTER_ADDRESSES` | Input | 1, 32, 32 | Older execute packet for load-use detection |
| `OUT_VALID`, `LOAD_USE_HAZARD`, `TERMINAL` | Output | 1 each | Decoded validity, hazard and terminal request |
| `OUT_PC`, `OUT_INSTRUCTION`, `OUT_FETCH_STATUS`, `OUT_CONTROL`, `OUT_REGISTER_ADDRESSES`, `OUT_IMMEDIATE`, `OUT_RS1_VALUE`, `OUT_RS2_VALUE` | Output | 32 each | ID/EX packet fields |

Status, control and register-address fields are packed 32-bit words; they
are not the 3-bit bus status or a single 5-bit register index.

### Behavior

The stage of [§ 4.6](../../rv32i/4.6/index.md) with the RV32IM decoder. The
register file, immediate generator, writeback bypass and hazard fields are
unchanged. The raw instruction travels in the packet; no extra bit marks an M
instruction.

### Implementation

The shared decode blueprint instantiates the M-aware decoder and the same
`REGISTER_FILE`, immediate, writeback-bypass and hazard paths as RV32I.
Reset clears stored registers; decode and register reads settle between
edges. There is no private M register file.

### Examples

<!-- simulator-examples: rv32im-decode-stage RV32IMDecodeStageScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rv32im-decode-stage&amp;embed=1" title="RV32IMDecodeStage simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rv32im-decode-stage" class="md-button">Open the full visualizer</a>

## 5.1.2.3. `RV32IMExecuteStage`

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `IN_VALID` | Input | 1 | ID/EX packet validity |
| `IN_PC`, `IN_INSTRUCTION`, `IN_FETCH_STATUS`, `IN_CONTROL`, `IN_REGISTER_ADDRESSES`, `IN_IMMEDIATE`, `IN_RS1_VALUE`, `IN_RS2_VALUE` | Input | 32 each | ID/EX packet fields |
| `EX_MEM_VALID`, `EX_MEM_REG_WRITE`, `MEM_WB_VALID`, `MEM_WB_REG_WRITE` | Input | 1 each | Forwarding source validity and write intent |
| `EX_MEM_RD`, `MEM_WB_RD` | Input | 5 each | Forwarding register indices |
| `EX_MEM_VALUE`, `MEM_WB_VALUE` | Input | 32 each | Forwarding values |
| `CLK`, `RST` | Input | 1 each | Clock and reset of the M slot |
| `ENABLE` | Input | 1 | Low pauses the arithmetic |
| `TAKE` | Input | 1 | The coordinator accepts a finished result |
| `CANCEL` | Input | 1 | Discard the M operation (an older fault or flush) |
| `EX_MEM_RESULT_READY` | Input | 1 | The older instruction's value exists (not a waiting load) |
| `EXECUTION_WAIT` | Output | 1 | An M operation is running: hold the pipeline |
| `PRETERMINAL` | Output | 1 | The packet will stop the core |
| `OUT_VALID` | Output | 1 | EX/MEM packet validity |
| `OUT_PC`, `OUT_INSTRUCTION`, `OUT_FETCH_STATUS`, `OUT_CONTROL`, `OUT_REGISTER_ADDRESSES`, `OUT_ALU_RESULT`, `OUT_STORE_DATA`, `OUT_NEXT_PC`, `OUT_PC_PLUS_4`, `OUT_EXECUTION_STATUS` | Output | 32 each | EX/MEM packet fields |
| `BRANCH_TAKEN`, `IS_BRANCH`, `CONTROL_TAKEN`, `REDIRECT` | Output | 1 each | Resolved control-flow decisions |
| `CONTROL_TARGET`, `REDIRECT_PC` | Output | 32 each | Resolved target addresses |

### Behavior

A valid M packet launches `MulDivUnit<32>` once both source values are
available; if an older load has not produced its value yet, the slot waits
rather than capture a stale register. `EXECUTION_WAIT` stays high until the
unit is done; then `OUT_ALU_RESULT` carries the M result. Faulted or illegal
packets never launch.

`CANCEL` suppresses launch and discards a running result; `TAKE` consumes a
completed result only when enabled. Integer execution and forwarding stay
combinational; only the arithmetic slot adds clocked state. Keep the input
packet stable while it is held. There is no second M slot or overlap
between two arithmetic instructions.

### Implementation

`INTEGER` is the RV32I execute stage. `M_OPERANDS` reuses its forwarding;
`M_CONTROL` recognizes the instruction and decides launch and wait;
`MULDIV` is the unit; `RESULT_SELECTION` overrides only the ALU result.

### Examples

The recording covers a multiply, a signed divide, a held completion, an
older load that is not ready and then forwarded, fault suppression and
cancellation.

<!-- simulator-examples: rv32im-execute-stage RV32IMExecuteStageScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rv32im-execute-stage&amp;embed=1" title="RV32IMExecuteStage simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rv32im-execute-stage" class="md-button">Open the full visualizer</a>

## 5.1.2.4. `RV32IMPipelineCoordinator`

### IO pins

Every pin here is one bit; the coordinator has no clock or reset input.

| Pins | Direction | Meaning |
| --- | --- | --- |
| `ENABLE`, `HALTED`, `TRAPPED` | Input | Run and terminal state |
| `IMEM_READY`, `DMEM_READY`, `MEM_LOAD`, `MEM_STORE` | Input | Memory progress and access kind |
| `EX_PRETERMINAL`, `EX_REDIRECT`, `LOAD_USE_HAZARD`, `EXECUTION_WAIT` | Input | Execute, dependency and arithmetic controls |
| `MEMORY_FAULT`, `MEMORY_TERMINAL`, `RETIRE_TERMINAL`, `WB_TERMINAL`, `TERMINAL_PENDING` | Input | Older terminal/fault information |
| `ACTIVE`, `COMMIT_ENABLE` | Output | Run and retirement enables |
| `IF_ID_WRITE`, `IF_ID_FLUSH`, `ID_EX_WRITE`, `ID_EX_FLUSH`, `EX_MEM_WRITE`, `EX_MEM_FLUSH`, `MEM_WB_WRITE`, `MEM_WB_FLUSH` | Output | Every packet register's movement controls |
| `FETCH_PC_WRITE`, `FETCH_PC_REDIRECT`, `FETCH_VALID`, `FETCH_RESPONSE_TAKE`, `IMEM_READ_ENABLE` | Output | Fetch movement and memory control |
| `DMEM_READ_ENABLE`, `DMEM_WRITE_ENABLE`, `DATA_REQUEST_ISSUE` | Output | Data access control |
| `PIPELINE_STALL`, `PIPELINE_FLUSH`, `LOAD_USE_STALL`, `MEMORY_STALL`, `DATA_PORT_STALL`, `EXECUTION_STALL` | Output | Movement and stall observations |

### Behavior

While `EXECUTION_WAIT` is high, the coordinator suppresses every packet
write and flush, fetch advance, fetch response consumption and
`COMMIT_ENABLE`, and raises `EXECUTION_STALL`. An older terminal or memory
fault overrides the wait (the M operation is canceled). Older packets are
held too, so writeback never retires twice; a memory access that was already
accepted completes into its held response.

### Implementation

`INTEGER` is the RV32I coordinator; `ARITHMETIC_HOLD` adds the hold, the
override and the movement gates.

### Examples

<!-- simulator-examples: rv32im-coordinator RV32IMPipelineCoordinatorScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=rv32im-coordinator&amp;embed=1" title="RV32IMPipelineCoordinator simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=rv32im-coordinator" class="md-button">Open the full visualizer</a>

## Source and tests

The decode-control and coordinator paths are combinational; their explicit
compact forms evaluate those gates with exact four-state values. The
register file and M slot introduce clocked state. All four parts contribute
to the [whole core's selected paths and clock boundary](core.md#behavioral-implementation).
There is no invented one-tick stage delay. Architectural packet tests use
known binary words after reset; the combinational four-state comparison
does not extend that guarantee to arbitrary X/Z clocks and sequential state.

- [Interfaces and catalog families](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/rv32im/RV32IMCore.hpp)
- [Decode overrides, M launch/wait control and arithmetic hold](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32im/RV32IMCore.cpp)
- [Shared pipeline pin schemas and blueprints](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/rv32i/RV32IPipelineStages.cpp), [verification and scenarios](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp)
- [Shared explicit compact registration](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/components/selection/GateListCompact.cpp) supplies the two combinational compact forms.

| Test | Coverage |
| --- | --- |
| `RV32IMDecodeControlTest` | The eight M encodings (controls, register fields, no side effects) and a reserved funct7 |
| `RV32IMDecodeStageTest` | A written register read back by an M packet |
| `RV32IMExecuteStageTest` | Launch, 33-edge wait and result, no launch on a not-yet-loaded operand, forwarding once it arrives, a faulted packet that never launches |
| `RV32IMPipelineCoordinatorTest` | Normal movement, the arithmetic hold, an older fault overriding it |
| `GateListCompactEquivalenceTest` | Decode-control and coordinator compact forms versus their gates, with binary, dense four-state and single-X/Z vectors |
| `*ScenarioTest` | The recorded examples above |
