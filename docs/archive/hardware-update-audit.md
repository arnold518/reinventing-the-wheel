# Hardware update audit and implementation record

Date: 2026-09-30. Status: **implemented in the working tree; validation in progress**.

This audit covers the single-cycle, baseline pipeline, and advanced pipeline
cores; their arithmetic, decode, register, prediction, and memory-access
components; and the shared FF/SRAM/DRAM/ROM/NOR-flash device/controller hierarchy.
It is a source and topology audit, not an exhaustive correctness proof or a
physical synthesis/area/power analysis.

The main finding is incomplete specialization: reusable components instantiate
logic for capabilities that a particular parent does not use. There is also
repeated combinational logic and excessive pipeline-packet storage. None of
these findings alone demonstrates incorrect instruction execution.

A follow-up storage audit found an abstraction gap as well: request/response
payloads rebuild words from individual FF cells without a selectable compact
word implementation. That is different from redundant hardware. See H23/H24
and the [storage inventory](#storage-abstraction-follow-up).

## Original audit evidence and limits

- Reviewed current implementations, standalone contracts/tests, retirement
  observers, and the existing circuit-optimization reports.
- Before implementation, built topology only, using the existing `build-cleanup` Python module, for
  program 01 on single-cycle, baseline, and advanced/one-bit configurations;
  and for `RV32IDecodeControlUnitTest`, `RV32IMemoryLanesTest`,
  `MatrixWriteDistributorTest`, `SRAMColumnSelectorTest`, and
  `SRAMPrechargeTest`. No simulation or benchmark was run.
- Those original diagnostic processes were serial, limited to 60 seconds and
  768 MiB virtual memory. No build or background campaign ran during that audit.
- The topology checks corroborate source findings; their profiles contain
  behavioral storage abstractions. Visible component counts are not physical
  gate counts, and a behavioral register still represents storage bits.
- The duplicate-gate check grouped same-parent gates with identical types and
  source pins. It was deliberately shallow: it does not prove complete
  transitive equivalence or find every possible optimization.
- Unconnected circuit outputs are not automatically dead. Host-side observers,
  visualizers, standalone component contracts, and timing analysis also consume
  them.
- The storage follow-up reviewed FF/latch construction sites in `src/modules`
  and `include/modules`, their catalog/family registration, build profiles,
  timing-model support, and existing storage/endpoint tests. Its new findings
  are source-derived; no additional topology probe, simulation, or benchmark
  was run for that follow-up.

## Findings and disposition

P1/P2/P3 are the original priorities, not current severity. The implementation
preserves standalone public contracts where a parent uses a narrower internal
variant. “Implemented” describes the working-tree code, not a measured area,
timing, or power improvement. Focused and full regression tests remain the
validation gate. The detailed H01–H24 sections below retain the original
source-derived rationale and proposed design; the table records disposition.

| ID | Priority | Implementation in this working tree |
| --- | --- | --- |
| H01 | P1 | Read-only fetch requester: constant operation, data, mask |
| H02 | P1 | Saved fetch address doubles as request tag |
| H03 | P1 | Compact internal packet fields; standalone wide packet contract retained |
| H04 | P1 | One state bit and one read tree per one-bit predictor entry |
| H05 | P1 | Advanced fetch selects external next PC without its own incrementer |
| H06 | P1 | Dedicated WB-to-decode bypass |
| H07 | P1 | Balanced control reductions, shared recognition prefixes, and shared repeated output terms |
| H08 | P1 | Shared load sign fill and halfword masks |
| H09 | P2 | Shared 2:1 bus-mux select complement |
| H10 | P2 | Register32 word storage and 31-word register-file bank |
| H11 | P2 | Power-of-two high-bit range check; balanced general check |
| H12 | P2 | Factored matrix/SRAM row validation |
| H13 | P2 | Shared matrix byte write enables |
| H14 | P2 | Shared SRAM precharge and column controls; per-bit write-driver gates retained to preserve integrated timing |
| H15 | P2 | Always-clocked state-bit family for eligible control state |
| H16 | P3 | `ALU32Core` omits unused public flags in CPU Execute |
| H17 | P3 | Packet fields narrowed after last use in integrated cores |
| H18 | P3 | Data-only memory lanes and fetch word alignment; architectural fault checks remain separate |
| H19 | P3 | Dedicated parallel-prefix `IncrementBy4` at constant-four sites; general adders remain where needed |
| H20 | P3 | FF-only always-ready response path for fixed-cycle cores; buffered path retained |
| H21 | P3 | Balanced ALU8 zero detection and selection |
| H22 | P3 | Decoder predecode and register-file x0-omission mode |
| H23 | P1 | Selectable width-aware payload register including small fields |
| H24 | P2 | Four-bit trap-cause payload register |

## Recommended core updates

### H01. Fetch requester stores constant fields

Location: advanced `CORE.FETCH_REQUEST.REQUEST_OP`, `REQUEST_WRITE_DATA`,
`REQUEST_WRITE_MASK`, and their `SELECT_*` muxes.

The general requester captures 3 operation bits, 32 data bits, and 4 mask bits.
Fetch permanently supplies Read, zero write data, and mask 15. Thus **39 stored
bits and their selection logic** are unnecessary in this integration.

Add an explicit read-only/full-word requester configuration, retaining the
universal bus port. Do not remove write capability from the data requester or
change the standalone general requester's contract. Preserve same-edge
zero-wait completion, delayed acceptance, and response draining. Reset and
pre-reset unknown outputs need an explicit equivalence domain; replacing a
reset register with a constant is not automatically waveform-identical.

Replacing `REQUEST_WRITE_DATA` with `Register32` would only change its
implementation, not remove the unnecessary state. Apply this specialization
before migrating surviving fields to the reusable storage proposed in H23.

Evidence: [requester construction](../../src/modules/rv32i/RV32IMemoryRequest.cpp),
[`wireRequestFields`](../../src/modules/rv32i/RV32IMemoryRequestWiring.hpp).

### H02. Fetch address and tag are the same saved word

`RV32IFiveStageCore` connects the same fetch address wire to requester `ADDR` and
`TAG`. Both 32-bit registers use the same capture enable, clock, and reset.
The saved address can therefore also supply the saved tag: **32 more bits**
can be removed from the fetch configuration.

Retain the comparison with the current fetch address and abandonment state.
They discard stale responses after a redirect. Do not blindly apply this to
the data requester: its tag is the original byte address while the bus address
comes from lane/alignment formatting, so those inputs are not generally equal.

Evidence: [`IMEM_ADDR` and `DMEM_ADDR` wiring](../../src/modules/rv32i/RV32IFiveStageCore.cpp),
[`REQUEST_*` capture loop](../../src/modules/rv32i/RV32IMemoryRequest.cpp).

### H03. Pipeline packets store padding in full registers

All four pipeline-register families allocate `Register32` for every field.
The current producers define considerably fewer bits in several fields:

| Field | Allocated bits per instance | Defined bits | Instances per pipeline | Padding bits |
| --- | ---: | ---: | ---: | ---: |
| `FETCH_STATUS` | 32 | 2 | 4 | 120 |
| `EXECUTION_STATUS` | 32 | 1 | 2 | 62 |
| `MEMORY_STATUS` | 32 | 2 | 1 | 30 |
| `REGISTER_ADDRESSES` | 32 | 15 | 3 | 51 |
| `CONTROL` | 32 | 31 | 3 | 3 |
| Total | | | | **266** |

This applies separately to baseline and advanced. Their four packet banks
currently represent 1,060 bits, including four valid bits. Storing only the
defined fields would reduce that to 794, before any further field-liveness
optimization. These are logical storage-bit counts, not measured area savings.

Important correction to a simplistic cleanup proposal: the standalone packet
tests deliberately capture arbitrary 32-bit values, including status-field
values such as `0x100 + index`. Flush clears validity but still captures the
payload. Dropping upper bits or changing invalid-packet contents would break
that existing public contract.

Introduce a compact packet schema/configuration and migrate the cores,
behavioral equivalents, observers, tests, docs, and layouts together; preserve
the old contract where still exposed, or version it explicitly. Use supported
small fields, e.g. three 5-bit register addresses and named control groups.
Current dynamic pin/wire/Rewire dispatch supports widths 1, 2, 3, 4, 5, 8, 16,
32, **not arbitrary widths 15 or 31**. A new `Register<15>` alone is insufficient.
Keeping `CONTROL` at 32 initially still saves 263 of the identified 266 bits.

Evidence: [field lists and registers](../../src/modules/rv32i/RV32IPipelineRegisters.cpp),
[packet encoding](../../include/modules/rv32i/RV32IPipelineEncoding.hpp),
[`pipelineRegisterSpec`](../../src/tests/RV32IPipelineBlockTests.cpp),
[dynamic pin widths](../../src/components/IOComponent.cpp),
[dynamic wire widths](../../src/components/ComponentBuilder.cpp).

### H04. One-bit prediction currently uses two bits per entry

`PredictionEntry` always creates two FFs and the general saturating-counter
network. With one-bit mode selected, both next-state bits equal `TAKEN`.
The table also builds two read-selection trees for those identical columns.

For one-bit policy, instantiate one FF per entry, a direct update path, and one
read tree. If the public `COUNTER[2]` representation remains 00/11, replicate
the selected bit at the output rather than storing it twice. A 16-entry table
saves **16 FFs**, besides update/read logic. Keep the two-bit saturating design
for two-bit policy; static policies already omit the table entirely.

Test reset/unknown controls, aliases, same-edge lookup/update, saturation for
the retained two-bit policy, and training suppression on killed instructions.

Evidence: [`PredictionEntry`, `PredictionTable`, `RV32IPredictionCounter`](../../src/modules/rv32i/RV32IPrediction.cpp).

### H05. Advanced fetch has an unselected PC+4 datapath

`RV32IFetchStage` builds `PC_PLUS_4` and `NEXT_PC_MUX`. The advanced core ties
`PC_REDIRECT` high and supplies the predicted/recovered address to the mux's
other input. Its local PC+4 result is therefore never selected.

Provide an externally-selected-next-PC fetch configuration, eliminating this
adder and mux. Keep the sequential addition in `PREDICTION.PREDECODE`, which
actually supplies fallthrough prediction, and keep sequential-PC behavior in
the standalone generic fetch component if that remains its contract.

The execute-stage PC+4 adder is a different case: it operates on a different
instruction concurrently and is not removable by sharing the current fetch PC.

Evidence: [`PREDICTED_PC_SELECT`](../../src/modules/rv32i/RV32IFiveStageCore.cpp),
[`RV32IFetchStage::buildInternals`](../../src/modules/rv32i/RV32IFetchStage.cpp).

### H06. Decode bypass uses a disabled half of full forwarding

Advanced `DECODE.WB_BYPASS` is an `RV32IForwardingUnit` with EX/MEM valid,
write-enable, ready, and destination tied to zero. Only the WB producer is
usable, yet the circuit builds both producers' comparators/priority logic and
two four-way 32-bit operand muxes. Binary selections can only be 0 or 2.

Introduce a two-source-register WB bypass helper: two 5-bit equality checks,
shared nonzero/valid/write qualification, and two 2:1 data muxes. Keep full
forwarding in Execute. Preserve the current distinction between bypass
eligibility and the register file's actual write authorization.

Evidence: [`RV32IDecodeStage::buildInternals`](../../src/modules/rv32i/RV32IDecodeStage.cpp),
[`RV32IForwardingUnit`](../../src/modules/rv32i/RV32IForwardingUnit.cpp).

### H07. Decoder still has long serial output reductions

`RV32IDecodeControlField` chains OR gates over matching instructions. The
topology check found **39 serial OR gates** in the `LEGAL` field (40 matches).
A balanced tree needs six OR levels, with the same 39 gates. Other control
fields also use serial reductions. This is a local depth opportunity, not a
claim that the complete core's period will fall by the same amount.

The same topology check found four immediately duplicated gates: shared
recognition prefixes for ADD/SUB, SRL/SRA, SRLI/SRAI, and one repeated ALU-op
output term. Share exact predicates where it improves clarity.

Keep all illegal-instruction and reserved-encoding checks. OR reassociation
preserves settled four-state values, but changes transient timing; regenerate
timing certificates and timestamped examples. The source's comment preserving
old depth is not evidence that the old depth is architecturally necessary.

Evidence: [`RV32IDecodeControlField` and instruction recognition](../../src/modules/rv32i/RV32IDecodeControlUnit.cpp).

### H08. Load extension repeats identical sign terms

Each `RV32ILoadByte` recomputes the same sign-fill AND gate for every bit of an
extended byte. The topology check found 35 duplicate gates within the byte
groups, plus two identical halfword-mask pairs: **37 excess same-parent gates**
in the current standalone memory-lanes topology. Further cross-byte sharing is
possible but is not included in that count.

Compute masked byte/halfword sign fills once and fan out. Share halfword mask
terms for each lane pair. Keep bit-specific data masking and preserve reserved
size, unsigned zero-extension, unknown input, and illegal-address behavior.
There is no need to change the public lane interface for this improvement.

Evidence: [`RV32ILoadByte`, `RV32ILoadExtender`, `RV32IByteWriteMask`](../../src/modules/rv32i/RV32IMemoryLanes.cpp).

## Shared storage, mux, and memory improvements

### H09. Share select handling across bus-mux bits

`buildBusMux` instantiates a complete one-bit mux tree for every bit. This
replicates select splitters and select inversion. A 32-bit 2:1 mux has 32
identical select NOT gates where one shared NOT would suffice.

Keep expandable data-bit slices, but provide shared select/complement signals.
Start with the 2:1 bus mux; generalize only after four-state and timing checks.
`MemoryPayloadMux` already shares its select inverter and is a useful pattern.
Do not globally replace gate muxes with C++ conditionals: that changes unknown
select behavior. Sharing increases fanout, which this simulator does not price
as electrical delay.

Evidence: [`buildBusMux`](../../src/modules/basic/Mux.cpp),
[`PayloadMux`](../../src/modules/memory/MemoryHandshakeWiring.hpp).

### H10. Registers do not need byte-mask gates

`Register32` and `RegisterFile32x32` use `FFWord<32>` with mask 15. Each word
still constructs four `WE AND mask[byte]` gates. Add a whole-word/unmasked
storage configuration using the same FF-cell foundation; retain byte masks for
actual memory devices.

The register file has 31 stored words, so this accounts for **124 byte-enable
AND gates** when fully expanded, plus four for each other expanded Register32.
These are source-derived counts, not extra gates executed inside a behavioral
register-file abstraction. Also remove the unconnected legacy `ZERO` constant
constructed by every structural `FFWord` if no diagnostic depends on its path.

Do not bypass a gate blindly for `Z` inputs: `Z AND 1` becomes `X`, while an
identity Rewire preserves `Z`. Preserve the storage contract's defined domain.

Evidence: historical `src/modules/memory/Register.cpp` (retired),
[register file](../../src/modules/rv32i/RegisterFile32x32.cpp),
[FFWord](../../include/modules/memory/FFWord.hpp).

### H11. Simplify constant-capacity FF address checks

`FFRequestCheck` constructs a 32-step address-versus-constant recurrence. For
power-of-two byte capacities, a high-bit OR reduction gives the range error.
At 64K words of 32 bits, capacity is 2^18 bytes and bits 18–31 suffice.

Use balanced constant comparison for other depths; do not assume all sizes are
powers of two. Retain alignment, unknown-field detection, mask validation, and
status priority. These checks belong to the controller even if the native
device also protects its own standalone address interface.

This affects structural FF controllers; current experiment memory macros are
behavioral, so it is not automatically a benchmark speedup.

Evidence: [`FFRequestCheck`](../../src/modules/memory/MemoryFFController.hpp).

### H12. Factor matrix and SRAM row decoding

`MatrixRowDecoder` compares all 32 address bits separately for every row.
`SRAMRowDecoder` does the same above its column-address bits. High address bits
that must be zero are repeated across rows.

Compute a shared high-address-valid term, then decode only the low row-index
bits. Preserve non-power-of-two geometry and out-of-range behavior. The matrix
helper also appears inside column read/write selection, so this benefits
DRAM, ROM, and flash periphery as well as their row decoders.

A single-row or single-column component still has a public 32-bit address
contract. It cannot become an unconditional wire unless its parent proves the
address valid; otherwise invalid/unknown addresses change behavior.

Evidence: [MatrixRowDecoder](../../src/modules/memory/DigitalMemory.cpp),
[SRAMRowDecoder](../../src/modules/memory/SRAM.cpp).

### H13. Matrix writes repeat one byte enable eight times

`MatrixWriteDistributor` computes `selected_word AND mask[byte]` for every bit.
The same result applies to eight bits. Compute it once per selected word/byte.

The observed `MatrixWriteDistributor<8,2>` has 14 immediately duplicate write
AND gates (16 down to 2). The same topology has 15 further immediately repeated
decoder gates covered by H12; do not double-count them. At W=32, the write-enable
saving alone is 28 gates per word slot.

Evidence: [`MatrixWriteDistributor::buildInternals`](../../src/modules/memory/DigitalMemory.cpp).

### H14. SRAM has repeated peripheral fanout gates

Three source-confirmed opportunities:

- `SRAMPrecharge<C>` computes `ENABLE AND 1` C times. One normalization gate
  can drive all C modeled precharge outputs; the two-column fixture confirmed
  one duplicate.
- `SRAMColumnSelector<W,K>` repeats `WRITE_BL AND 1` and its complement-line
  counterpart for each word slot. Share each normalized bit across slots;
  the W=8, K=2 fixture confirmed 16 duplicate gates. Share repeated inverted
  column-address bits as K grows.
- The W `SRAMWriteDriver` instances repeat `ENABLE AND mask[byte]` eight times
  per byte. Group byte authorization while retaining each bit's differential
  data driver and the visible row/column architecture.

This is digital-logic sharing, not a claim that a physical SRAM needs one
electrical driver for an arbitrarily large array. Keep individual modeled
bitline/precharge connections. First share identical gates rather than replacing
all gates with zero-delay rewires, which would alter timing and Z normalization.

Evidence: [SRAMPrecharge/WriteDriver](../../src/modules/memory/SRAM.cpp),
[SRAMColumnSelector/device assembly](../../include/modules/memory/SRAM.tpp).

### H15. Some state FFs have an unnecessary enable mux

Requester `BUSY`, `SENT`, `ABANDONED`, transaction `PENDING`, and response-buffer
`OCCUPIED` use `FlipFlopCell` with WE permanently high. The structural FF cell
adds a write/hold feedback mux even though their next-state equations already
decide when to hold.

An always-clocked state-bit abstraction backed by the existing DFlipFlop can
omit that mux. It must have a matching compact implementation and retain reset
and unknown-input behavior. Do not remove these state bits themselves: accepted,
pending, abandoned, and occupied are different transaction facts.

Evidence: [requester state](../../src/modules/rv32i/RV32IMemoryRequest.cpp),
[controller/buffer state](../../src/modules/memory/MemoryController.cpp),
[FlipFlopCell](../../src/modules/memory/FlipFlopCell.cpp).

## Conditional specializations and architectural choices

### H16. ALU flags unused by the CPU

The Execute datapath consumes ALU result, EQ, signed LT, and unsigned LT, but
not ALU32's public ZERO/NEGATIVE/CARRY_OUT/OVERFLOW outputs. Topology confirmed
those four unconnected ALU outputs in all three cores.

A CPU-specific datapath could omit `ALU32FlagUnit` while the general ALU32
retains its complete interface. Do not remove AddSub32's internal carry or
overflow: the comparison unit uses them. Preserve inspection expectations if
the same ALU32 identity remains visible. This is lower priority than removing
duplicate registers and forwarding logic.

Evidence: [ALU32 assembly](../../src/modules/composite/ALU32.cpp),
[Execute usage](../../src/modules/rv32i/RV32ISingleCycleStages.cpp).

### H17. Trim packets by stage, but preserve retirement evidence

After Execute, ordinary hazard/forwarding/writeback logic only needs the
destination register number, not both source numbers. Beyond H03's padding,
retaining only RD in EX/MEM and MEM/WB could save another 20 bits per pipeline.
Control fields likewise have stage-specific last uses. Audit every observer
before changing the schema; this is not permission to discard fields wholesale.

In particular, topology reports `MEM_WB.Q_INSTRUCTION`, `Q_NEXT_PC`, and
`Q_STORE_DATA` as unconnected to circuit wires. They are read directly by
`observeRetirementPacket`, so they are **not dead in this simulator**.

Keep retirement evidence initially. If a later research mode separates tracing
from modeled hardware, it must preserve instruction age through stalls, flushes,
faults, and memory side effects without executing an oracle to fabricate results.
Do not claim hardware savings merely by moving the same storage into C++.
Selecting a single WB result earlier is another option, but can move a mux onto
an earlier critical path and conflicts with separate memory-trace data needs.

Evidence: [retirement observer](../../include/modules/rv32i/RV32IPipelineObservation.hpp),
[baseline dependencies](../../src/modules/rv32i/RV32IBaselinePipeline.cpp),
[packet storage](../../src/modules/rv32i/RV32IPipelineRegisters.cpp).

### H18. Consolidate alignment responsibility carefully

The single-cycle Memory stage constructs the full lane helper but does not use
its guarded `WORD_ADDR`; it separately formats a native word address and
checks alignment. ExecutionStatus independently checks address/size for precise
traps. Baseline reuses that Memory stage. Some of this is unused lane-address
logic; some is a necessary local write-safety guard versus architectural fault
reporting.

Consider a shared alignment/result helper or a lane configuration without the
unused guarded-address output. Do not simply remove store alignment suppression
or round bad addresses into valid writes. Preserve reserved SIZE=3 and exception
priority. Advanced fetch's fixed word-size alignment can also use a two-low-bit
check, as NextPC already does, instead of a general size-controlled checker.

Evidence: [lane address logic](../../src/modules/rv32i/RV32IMemoryLanes.cpp),
[single-cycle stages](../../src/modules/rv32i/RV32ISingleCycleStages.cpp),
[execution status](../../src/modules/rv32i/RV32IExecutionControlStatusUnit.cpp).

### H19. Specialized adders for constant operands/unused carries

Remaining PC+4 sites use a general Adder32 with B=4 and Cin=0. A structural
constant-four incrementer could be smaller. Many other PC/target adders have
Cin=0 and no consumer of Cout. A no-carry-in or sum-only prefix configuration
can avoid portions used solely for those capabilities.

Do not remove carry logic that still feeds higher sum bits, or the C31/Cout
paths used by AddSub32 overflow. Do not replace the parallel-prefix network
with a long ripple incrementer merely to reduce nodes: area and delay trade off.
Compare both node count and certified period before adopting this.

Evidence: [Adder32](../../src/modules/composite/Adder32.cpp),
[prefix network](../../include/modules/composite/ParallelPrefixAdder32.hpp),
[predecode](../../src/modules/rv32i/RV32IPrediction.cpp).

### H20. Always-ready response specialization is optional

Single-cycle and baseline zero-wait ports tie `RSP_READY` high. Their structural
FF endpoints nevertheless build the general response holding buffer. Under the
fixed always-ready contract, its full state and 32+3 held payload bits cannot
be needed to retain an unconsumed response after reset.

An explicit no-backpressure endpoint configuration could remove those 36 bits
per 32-bit port and associated muxes. Keep the default general endpoint and
the advanced pipeline's buffer. A read-only fetch requester does not imply an
always-ready response consumer: advanced fetch can stall.

Because the default core memory is a scalable behavioral endpoint, this is
principally a structural specialization and modeling choice, not a reason to
restart current benchmarks. It must preserve same-edge request/response,
exactly-once stores, reset, and response-status behavior.

Evidence: [zero-wait port](../../src/modules/rv32i/RV32IZeroWaitMemoryPort.hpp),
[FF controller](../../src/modules/memory/MemoryFFController.hpp),
[response buffer](../../src/modules/memory/MemoryController.cpp).

### H21. Smaller ALU8 reductions can be balanced

ZeroDetect8 has a seven-OR serial chain; three OR levels suffice for eight bits.
ALU8ResultSelector's six-source OR uses five serial levels rather than three.
These retain the same number of two-input OR gates but reduce combinational
depth. They are optional library improvements, not RV32I execution changes.

Keep the documented ALU8 flags and all supported operations. It already uses
one shared AddSub8 through ArithmeticUnit8, and comparison consumes that
arithmetic result rather than constructing a second subtractor.

Evidence: [ZeroDetect8](../../src/modules/composite/ZeroDetect8.cpp),
[result selector](../../src/modules/composite/ALU8ResultSelector.cpp),
[`StructuralBuilder::any`](../../src/modules/composite/Structural8Builder.hpp).

### H22. Generic register-write decoder has repeated prefixes

Decoder5to32 builds a separate five-input match/enable network for each row.
Share partial decode terms or use grouped predecode while retaining visible
row outputs. In the RV32I register file, OUT0 is unused because x0 is already
hardwired to zero; its four match ANDs and enable AND are an additional small
specialization opportunity. Do not remove OUT0 from the generic decoder's
public contract.

The main saving here is shared decode terms, not x0 storage: the register file
already stores only x1–x31. Recheck write timing and unknown-address behavior.

Evidence: [decoder construction](../../src/modules/basic/Decoder.cpp),
[register file](../../src/modules/rv32i/RegisterFile32x32.cpp).

## Storage abstraction follow-up

### What counts as a similar issue?

A 32-bit register needs 32 logical storage bits. Seeing 32 FF cells inside its
**structural implementation** is expected. There are three separate questions:

1. **Is the state needed?** Constant or identical fields may be removed/shared
   under the appropriate contract (H01/H02/H04).
2. **Is construction reused?** A word register should not be rebuilt in each
   consumer when the same clock/reset/enable contract already exists.
3. **Can fidelity be selected at the useful boundary?** Making each cell
   behavioral still leaves 32 simulator component instances. A word-level
   behavioral implementation can represent the same 32 bits in one component.

Only the first changes the logical storage-bit count. Grouping and compact
simulation are not hardware area savings, and neither implies a shorter clock
period. Structural drill-down remains part of the design.

### Inventory of current storage construction

| Location | Current storage | Existing compact boundary | Assessment |
| --- | --- | --- | --- |
| Advanced `FETCH_REQUEST` and `DATA_REQUEST` | Five `MemoryPayloadRegister` children per requester: TAG 32, OP 3, ADDR 32, WRITE_DATA 32, WRITE_MASK 4 | Individual FF cells, or state bookkeeping inside a behavioral whole core; no payload-register or standalone requester behavioral implementation | H23 for surviving payloads; H01/H02 remove fetch-only redundancy first |
| `MemoryResponseBuffer<W>.HELD_DATA` / `HELD_STATUS` | W-bit and 3-bit `MemoryPayloadRegister`s | Individual FF cells, or a behavioral enclosing endpoint; no payload/buffer behavioral implementation | Same H23 gap; retain general response storage under backpressure |
| `RV32IExecutionControlStatusUnit.TRAP_CAUSE_BIT_*` | Four directly assembled FF cells with shared trap enable | Individual FF cells or behavioral whole status unit | H24: reusable four-bit register |
| `RV32IPipelineExecutionStatus.TRAP_CAUSE_*` | Another directly assembled four-FF register | Individual FF cells or behavioral whole status unit | H24: same storage contract, different surrounding event logic |
| `RV32IPredictionEntry.BIT_*` | Two directly assembled FF cells per table entry | Individual FF cells or behavioral whole predictor; no separate entry/table behavioral implementation | Optional narrow-register reuse after H04; do not pad one-/two-bit counters into 32-bit registers |
| PC state, prediction metadata, pipeline payload registers | `families::Register32` | Selectable 32-bit register | Already grouped correctly; packet padding remains H03, not a missing word abstraction |
| `RegisterFile4x32` / `RegisterFile32x32` | Three Register32 children / 31-word FFStorageBank with x0 hardwired | Register32 / FFStorageBank and register-file behavioral implementations | Already reuse storage; byte-mask overhead is H10 |
| `FFMemory<W>` | `FFStorageBank<W>` containing `FFWord<W>` children | Bank, word, and memory families have compact implementations | Correct lower-level hierarchy; do not remove its representative cells |
| `BUSY`, `SENT`, `ABANDONED`, `PENDING`, `OCCUPIED`, valid/halt/trap flags | Individual state bits | Selectable FF cells | Legitimate one-bit state; not 32-bit words in disguise |

Each current generic requester has **103 payload bits**, plus three handshake
bits. Two requesters therefore contain 206 payload bits. H01/H02 remove 71 of
those from fetch, leaving 135 payload bits across fetch and data; choosing a
compact representation for those survivors saves no further logical bits.
These counts describe the expanded requester design, not the object count of
a behavioral whole core.

One structural response buffer stores **W + 3 payload bits plus occupancy**:
12, 20, or 36 bits at W=8, 16, or 32. Both the zero-wait FF controller and the
delayed controllers use this buffer. It adds no cycle when empty, but must
retain a blocked response. H20 is a separate, narrower always-ready proposal.

The `REQUEST_OP/ADDR/DATA/MASK` children of **MemoryTransactionPort** are
rewires, despite their names: the native sequencer captures its request on
START. Do not count them as a second set of payload registers. Likewise,
`MemoryOutputBuffer<W>` is a combinational enabled output driver (DATA or Z/X),
not clocked response storage. DRAM sense columns retain row data and restore
charge cells; their precharge/sense behavior is not a normal FF-word contract.

Evidence: [payloads and transaction forwarding](../../src/modules/memory/MemoryController.cpp),
[requester](../../src/modules/rv32i/RV32IMemoryRequest.cpp),
[FF response path](../../src/modules/memory/MemoryFFController.hpp),
[endpoint catalog](../../src/modules/memory/MemoryEndpointCatalog.cpp),
historical `src/modules/memory/Register.cpp` family (retired),
[FF families](../../src/modules/memory/FFMemory.cpp),
small register file (since retired),
[FFMemory](../../include/modules/memory/FFMemory.hpp),
[output driver](../../src/modules/memory/DigitalMemory.cpp),
[DRAM sense columns](../../src/modules/memory/DRAM.cpp).

### H23. Payload words bypass the reusable word-level fidelity boundary

`MemoryPayloadRegister` already has word-level D/Q pins and shared CLK/RST/WE,
but builds each bit directly through `families::FlipFlopCell`. Both callers
instantiate it with `addNewComponent`, and there is no payload-register catalog
contract or behavioral factory. A profile selecting behavioral FF cells
therefore compacts each bit separately, not the word.

`Register32` already provides structural and behavioral implementations with
the same external pin shape at width 32. `FFWord<W>` also has both variants,
but exposes byte masks and supports only W=8/16/32. Neither is an existing
width-generic replacement for the 3-bit status/operation or 4-bit mask fields.
The payload helper currently handles 1/2/3/4/8/16/32, not arbitrary widths:
in particular its bit-adapter dispatch does not accept width 5 even though
dynamic pins/wires support it. H03's compact register addresses need explicit
five-bit support rather than an assumed template substitution.

Proposed direction, not an implemented component:

- Provide one width-aware whole-register family, provisionally `Register<W>`,
  with D[W], Q[W], CLK, RST, and WE. Start with the supported widths actually
  needed; include 5 if implementing compact packet addresses.
- Keep a structural implementation from FF cells and a tested compact
  word-level implementation. Do not replace structural storage with an opaque
  module or introduce a memory bus/controller for a register.
- Preserve the public `Register32` family/profile contract while sharing its
  implementation machinery. Resolve H10's whole-word versus byte-masked
  distinction here rather than making every narrow register a masked memory
  word. Do not silently widen metadata to 32 bits.
- Migrate the surviving requester and response fields through family/catalog
  selection, not fixed-class construction. Keep response mux/occupancy and
  requester handshake logic visible. A new behavioral whole controller or
  whole requester is not required to fix this gap.
- Reuse existing compact register semantics where appropriate, but verify
  reset, uncertain enable/data, and edge capture. Direct FF fanout and
  Register32's current byte-enable gates have different structural paths;
  replacing one with the other is not automatically timing-equivalent.

The reference-core helper `withBehavioralMemoryParts` currently selects
behavioral Register32, register-file, and FF-cell families. It cannot select a
nonexistent payload family. Update the profile deliberately alongside the new
family and record the resulting fidelity manifest. This matters for comparisons:
fewer host objects must not be presented as a hardware optimization.

Timing is a migration dependency, not an optional follow-up. SafeClockTiming
uses concrete supported storage types, including Register32Direct and
FlipFlopCellDirect, for sequential boundaries and binary-startup evidence.
A new compact class needs its own justified timing/reset model and tests;
do not suppress unsupported-state diagnostics or just assume the old
sufficient-clock certificate covers it.

Evidence: [payload construction](../../src/modules/memory/MemoryController.cpp),
historical `src/modules/memory/Register.cpp` (retired),
[byte-masked FFWord](../../include/modules/memory/FFWord.hpp),
[width dispatch](../../src/modules/memory/DigitalMemory.cpp),
[storage profiles](../../src/modules/rv32i/RV32IBuildProfiles.cpp),
[safe-clock storage models](../../src/analysis/SafeClockTiming.cpp).

### H24. Trap-cause storage repeats a four-bit register design

Both execution-status implementations directly assemble four FF cells between
a splitter and joiner. Each group shares CLK/RST and the trap-event write
enable. Use the narrow-register family from H23 for the cause field, while
retaining the independent halt/trap flags and each unit's event-priority logic.

Both whole status units already have behavioral implementations. This finding
is about reusable storage and a finer fidelity boundary inside an otherwise
structural unit, not missing behavioral functionality for the whole component.
There are still four cause bits after the refactor; no storage saving is claimed.

The predictor's two-bit entry can use the same storage machinery if useful,
but it is already grouped as an expandable entry. Prioritize H04's actual
one-bit-policy redundancy before adding optional entry-level abstractions.

Evidence: [execution control/status](../../src/modules/rv32i/RV32IExecutionControlStatusUnit.cpp),
[pipeline execution status and family](../../src/modules/rv32i/RV32IPipelineStages.cpp),
[status catalog](../../src/components/selection/BuiltinComponentCatalog.cpp),
[prediction entry](../../src/modules/rv32i/RV32IPrediction.cpp).

## Keep: similar-looking hardware with distinct responsibilities

| Hardware | Reason to retain |
| --- | --- |
| Pipeline valid bits and clocked stage boundaries | Distinguish bubbles and instructions; retain state across stalls |
| General requester payload capture | Keeps requests stable while memory refuses acceptance |
| Requester sent/abandoned tracking | Prevents duplicate issue and drains stale fetch responses |
| General endpoint response buffer | Holds data/status when the consumer is not ready |
| DRAM row buffer, restore, refresh state | Different from charge-cell storage and bus-response holding state |
| Flash write-enable/program/erase sequencing | Encodes the device contract; not ordinary RAM write duplication |
| SRAM latch and bitline/peripheral structure | Represents the selected digital SRAM abstraction, not FF-memory redundancy |
| ROM constant cells | Represent the programmed image; replacing all of them loses the representative structural design |
| Branch-target adder beside ALU comparison | Target computation and operand comparison occur concurrently |
| Fetch and Execute arithmetic | Different instructions occupy these stages simultaneously |
| Two branch-recovery comparisons | Intentional parallel comparison of target/fallthrough to shorten the late branch-decision path |
| Hazard/forwarding comparisons at different stages | Compare different in-flight producers; not duplicate checks of the same state |
| Multiple terminal/fault checks | Enforce age and exception priority; ungated pending-terminal differs from actual commit |
| Rewires/splitters/joiners | Usually represent connectivity, not additional storage |
| FFWord's individual FF cells in structural mode | The lower-level implementation of a word; compact simulation should be an alternative, not a deletion |
| MemoryOutputBuffer | A combinational output driver, not a second response register |

The shared Shifter32 already uses one right-shift network with reversal for left
shifts. ALU32 already shares AddSub32 for arithmetic/comparison. These previous
improvements should not be undone. The standalone PipelineControlFlowUnit is
not the current Execute datapath; its three adders must not be counted as three
extra adders in every core.

Technology cell/sequence primitives implemented in C++ are not automatically
misclassified FF registers. This audit did not perform transistor-level
validation of SRAM/DRAM/flash physics or prove their timing parameters realistic.

## Prerequisites and guardrails

1. **Specify the equivalence domain.** Same binary architectural state after
   reset, same settled four-state pin behavior, and identical event history are
   different requirements. Constant folding and replacing gates with wires may
   preserve the first while breaking the latter two.
2. **Treat identity gates carefully.** `Z AND 1` and `Z OR 0` produce X in these
   gates; an identity Rewire returns Z. Repeated identical gates can often be
   shared without changing this normalization.
3. **Do not optimize by changing simulator delays.** BitSplitter currently has
   delay 1; BitJoiner and Rewire have delay 0. Replacing all splitters with
   rewires would change the timing model, not merely remove physical hardware.
   Any such policy change needs a separate design and recalibration.
4. **No automatic deletion from sink counts alone.** Account for public pins,
   state/retirement capabilities, inspector/time-travel data, and tested internal
   paths. The topology findings provide concrete counterexamples.
5. **Keep specializations few and explicit.** Prefer a compact packet schema,
   read-only requester, and small reusable WB bypass over dozens of loosely
   related boolean flags. Keep lower-level structural implementations and test
   any behavioral equivalents against them.
6. **Preserve memory compatibility.** Single-cycle/baseline require zero-wait
   FF endpoints. Existing system/experiment guards enforce this. Removing state
   from those configurations must not make the advanced core unable to stall.
7. **Keep the same research accounting.** State clearly which storage is
   compact behavioral, which logic is structural, and which fields exist only
   for observation. Fewer simulator objects are not by themselves better
   hardware performance.

## Suggested implementation sequence and validation

| Batch | Work | Required evidence before moving on |
| --- | --- | --- |
| A | H01/H02/H04/H05/H06 | Request stability/draining, all predictor policies, stage parity, unchanged architectural results and expected cycle counts |
| A2 | H23, then H24; coordinate H10 | Selectable width-aware register, leaf equivalence, timing/reset support, preserved compact/expanded visualization |
| B | H03; assess H17 without deleting trace data | Versioned/explicit packet schema, reset/hold/flush/unknown tests, full retirement signature parity |
| C | H07/H08 | Four-state settled equivalence, illegal decoding and memory-lane cases, recalculated timing |
| D | H09–H15/H22 | Small structural fixtures first, geometry/boundary tests, equivalent compact models where applicable |
| E | H16/H18–H21 | Adopt only after contract decisions and measured area/depth benefit; avoid simultaneous broad redesign |

After each affected batch, use existing registry-driven component tests and
add targeted cases for the changed specialization. In particular:

- Requester/endpoint cases: ready low for several edges, same-edge completion,
  response held, redirects while unsent and sent, return to an earlier PC,
  reset with an outstanding request, and exactly-once stores.
- Packet/predictor cases: all defined field values, old/new contract separation,
  held and flushed packets, aliasing, one-/two-bit state transitions, and no
  predictor training from killed or terminal instructions.
- Memory helpers: W=8/16/32, singleton and non-power-of-two geometry, maximum
  supported representative sizes, high invalid addresses, byte masks, and X/Z.
- Reusable register: add standalone structural/behavioral tests for each
  supported width, not just endpoint integration. Cover startup, reset without
  a clock edge, reset/enable priority, disabled hold, known rising-edge capture,
  changing D between edges, and the declared X/Z domain. Existing
  `Register32Test`/`FFWordContractTest` are starting evidence, not automatic
  proof for a new family. `RV32IMemoryRequestTest`, `MemoryResponseBufferTest`,
  `MemoryEndpointContractTest`, and both execution-status contracts cover the
  affected integrations. Verify family selection actually produces a compact
  word, rather than W compact cells. Update the layout assertions that currently
  expect `MemoryPayloadRegister.BIT_*` children and keep an intentional expanded
  view. Add timing-certificate regression coverage for any new compact type.
- Core validation: existing functional/fault programs, all 22 fixed programs on
  the affected cores, advanced predictor-policy cases, and delayed-memory
  integration for advanced. Preserve retirement observation and memory effects.
- Performance: recalculate the sufficient-clock certificate at the same fidelity
  and assumptions; distinguish cycle count, period, modeled ticks, event count,
  and host runtime. Do not claim an area-only change improves the clock.
- Visualizer/docs: regenerate affected examples and layouts, preserve handmade
  stage order and memory row/column organization, update schema/profile keys
  where needed, and check native/WASM parity and memory/retirement inspection.

Build and run focused checks serially. Do not start a full-size fully expanded
memory or a second large benchmark campaign as a routine regression.

## Existing experiment results

Keep completed and ongoing frozen campaigns as the pre-update baseline.
Behavioral execution uses structurally derived clock periods; the reference
profile itself uses compact register/memory boundaries, not every FF expanded
to gates. Thus an expanded FF gate saving need not affect that profile's period.

These findings do not invalidate correctness/equivalence results for the tested
snapshot. They do mean those results describe the existing implementation, not
a minimal circuit. Do not modify snapshot binaries or merge pre-/post-update
results into one campaign. Start new focused comparison artifacts only after
implementation and validation; rerun large workloads only if the changed
question warrants it.

Evidence: [experiment profiles](../../src/analysis/RV32IExperiment.cpp),
[frozen campaign runner](../../tools/benchmarks/run-full-embench-background.py),
[previous timing optimization](rv32i-circuit-timing-optimization.md),
[previous logic optimization](rv32i-logic-timing-optimization.md).
