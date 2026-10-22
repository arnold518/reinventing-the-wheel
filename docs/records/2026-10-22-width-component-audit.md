# Component width generalization

Started: 2026-10-22. Status: closed (implementation accepted). Code state: main `alpha` at `88c0a95`,
with uncommitted width/device/game implementation and documentation.

## Summary

Shared width/configuration recipes and public references are implemented on
main alpha. Width, operation contract and adder topology are independent;
`ALU<W>` defaults to parallel-prefix addition. Mux fan-in, decoder size,
register-file geometry, iterative arithmetic width and transport/pixel widths
are construction parameters. Configured device registers retain their 32-bit
MMIO ABI; surface geometry and queue configuration remain independent.

The owner's complete-replacement decision supersedes the wrapper/ID retention
proposals in the dated evidence below. Fixed public components and aliases are
retired. `RegisterFile32x32` is the approved architectural exception and shares
the generic recipe. The closed [replacement record](2026-10-22-general-component-retirement.md)
owns the final native/API/reference/timing/portable/browser evidence and the
new ALU scenarios/pages. Earlier 555-test/72-timing/808-table results below
belong to their own snapshot. Full game phase 6 and phase 7 remain open in the
[game milestone record](2026-10-21-scripted-games-phase4-6-implementation.md).

## 2026-10-22 — Complete native and new timing checkpoint

All 555 distinct selected native registrations pass. Fifteen redundant game
registrations are reserved for the stronger ten-guest, four-run recording
campaign. All 808 delay entries per profile are recomputed unchanged; all 160
numeric per-part normal comparison rows match the saved baseline. Strict
settling is report-only and retains the X/Z diagnostic limits documented in
the phase-6 record.

The updated standalone `ArithmeticTimingOptimizationTest` passes all 72 new
width/semantics/flags/topology/profile cases. It verifies valid four-state
settling graphs and a real change in prefix-level structure when topology
changes. Unit ripple/prefix bounds are 18/13, 34/16 and 66/19 ticks for widths
8/16/32. These are conservative bounds, not physical Fmax or hold proofs.
Artifacts are under `build-milestone/acceptance-width/`. Current model digest
and guest binary remain unchanged; compiled recording comparisons have begun.

## 2026-10-22 — Scope and initial evidence

- Read the current status, internal index, public ALU pages, public headers
  and current circuit implementations in the dedicated alpha checkout.
  Preserve the primary checkout's ongoing work and existing preview records.
- ALU8 defines 29 operations, a carry/no-borrow input and an invalid-opcode
  output. ALU32 defines 13 operations with a different opcode map and three
  comparison outputs. Their width is only one contract difference.
- Inventory public fixed-width classes and related families across basic,
  composite, device, RV32I register-file and RV32IM arithmetic modules.
  Identify already parameterized components separately. No compile,
  simulation, browser session or service restart is needed for this audit.

Detailed comparisons and recommendations will be appended after checking
the implementations, family registration, timing/profile boundaries and
relevant test definitions.

## 2026-10-22 — Findings and comparison

### ALU8 and ALU32

Compared `include/modules/composite/ALU8.hpp`, `ALU32.hpp`, their `.cpp`
implementations, `ALU8Behavioral.cpp`, and the public references
`site-docs/alu8/2.6.md` and `site-docs/rv32i/4.3/alu32.md`.

| Property | ALU8 | ALU32 |
| --- | --- | --- |
| Data width | 8 | 32 |
| Opcode width | 5 | 5 |
| Defined operations | 29 | 13 |
| Arithmetic | ADD, ADC, SUB, SBC, INC, DEC, NEG | ADD, SUB |
| Logic | AND, OR, XOR, NOT, NAND, NOR, XNOR | AND, OR, XOR |
| Movement | SLL, SRL, SRA, ROL, ROR | SLL, SRL, SRA |
| Comparison/selection | EQ, SLTU, SLT, MINU, MAXU, MINS, MAXS | SLT, SLTU; separate qualified comparison outputs |
| Other operations | PASS_A, PASS_B, CLEAR | PASS_A, PASS_B, ZERO |
| Carry input | C_IN | None |
| Other outputs | ZERO, NEGATIVE, CARRY, OVERFLOW, INVALID_OP | ZERO, NEGATIVE, CARRY_OUT, OVERFLOW, EQ, LT_SIGNED, LT_UNSIGNED |
| Shift distance | B[2:0] | B[4:0] |
| Shift carry | Last displaced bit; C_IN for distance zero | No shift carry output |
| Comparison carry/overflow | Cleared for comparison/select operations | Retained from subtraction for SUB/SLT/SLTU |
| Invalid operation | Zero result and INVALID_OP=1 | Zero result; no invalid-operation pin |
| Default arithmetic circuit | Eight FullAdders in one shared ripple chain | One shared parallel-prefix add/sub unit |

Opcode numbers are incompatible: `0x01` is ADC for ALU8 and SUB for ALU32;
SUB itself is `0x02` versus `0x01`. A width substitution alone changes the
meaning of a program driving OP. ALU32Core uses the ALU32 class with
`full_flags=false`; it omits ZERO/NEGATIVE/CARRY_OUT/OVERFLOW while retaining
result and comparison outputs. It is a family, not an additional class.

Both complete ALUs have structural and behavioral implementations. All 11
chapter-2 families are selectable, including the ALU8 helpers. ALU32's
control/compare/result/flag helper entries are structural-only. The
structural ALU8 asks for its child families; the structural ALU32 directly
builds gate-level arithmetic/logic/shift children. Preserve or deliberately
reconcile that selection behavior during a future refactor.

### Inventory and proposed generalization

Counted class declarations in public `include/modules/` headers: 22 composite,
five bus-mux, nine device, three RV32IM arithmetic and one register-file
class. Excluded ISA-prefixed RV32I/RV32IM components and names whose numbers
only identify a mux's input count or decoder fanout. Private behavioral
counterparts and internal bit slices are implementations of these contracts,
not additional public entries in this count.

| Current public classes/families | Difference to preserve | Proposed shared implementation |
| --- | --- | --- |
| Adder8, Adder32 | Same A/B/Cin to Sum/Cout contract by width; 8-bit ripple versus default 32-bit prefix, with a ripple option | Adder<W> with an explicit topology choice; RippleAdder<W> and ParallelPrefixAdder<W> internals |
| ParallelPrefixAdder32 | Fixed word size and carry-into-sign output named C31 | ParallelPrefixAdder<W>, with a width-independent carry-into-sign role |
| AddSub8, AddSub32 | AddSub8 has independent C_IN; AddSub32 uses SUB as initial carry. Overflow gate equations also differ | AddSub<W> shared circuitry with carry/overflow adapters preserving each old contract |
| ArithmeticUnit8 | Seven arithmetic modes layered over one add/sub unit | ArithmeticUnit<W> |
| ZeroDetect8, ZeroDetect32 | Same zero-reduction function; both use gate trees structurally | ZeroDetect<W> |
| LogicUnit8, Logic32 | Seven selected operations and one output versus three simultaneous AND/OR/XOR outputs | Shared BitwiseLogic<W>; separate operation-selection shell |
| BarrelShifter8, Shifter32 | Shift/rotate OP and carry input/output versus LEFT/ARITHMETIC controls and shifts only | BarrelShifter<W> with explicit rotate/carry features and legacy control adapters |
| CompareSelect8, ALU32CompareUnit | Seven selected comparison/min/max outputs versus enabled EQ/LT flags and SLT/SLTU words | Comparator<W> plus CompareSelect<W> |
| ALU8, ALU32, ALU32Core family | Width, operation map, pins, flag rules and topology differ | ALU<W> with an explicit operation contract, topology and optional result flags; legacy adapters |
| ALU8ControlDecoder, ALU32ControlDecoder | Different five-bit opcode maps; neither has an operand-width data bus | ALUControlDecoder<OperationSet>; width should not identify the opcode map |
| ALU8ResultSelector, ALU32ResultSelector | Six independent result enables combined by OR versus nine decoded result sources | Shared width-aware selection building blocks; ResultSelector<W, SelectionContract> if a combined interface is useful |
| ALU8FlagUnit, ALU32FlagUnit | Shift-carry selection versus arithmetic-only carry; standalone four-state sign handling differs | ALUFlags<W, FlagPolicy> with compatibility adapters |
| Mux2to1_32bit, Mux4to1_32bit, Mux8to1_8bit, Mux8to1_32bit, Mux32to1_32bit | Repeated per-bit mux structures; input count and bus width are independent | Mux<Inputs, W> |
| RegisterFile32x32 | 32 words of 32 bits, two read ports, one write port, x0 hardwired to zero | RegisterFile<W, Count> with explicit port/reset/zero-register policies |
| Multiplier32, Divider32, MulDivUnit32 | Fixed registers, sign positions, iteration counter/completion, result halves and signed edge cases | Multiplier<W>, Divider<W>, MulDivUnit<W>, retaining the RV32IM-facing operation adapter |
| DeviceRegisterPort32, BusErrorTarget32, AddressRouter32, InstructionAccessGuard32 | 32-bit address/data protocol, four byte enables, aligned word commands; fixed map/guard rules | Components parameterized by a BusSpec; separate address-map and access-rule configuration |
| ConsoleDevice32, TimerDevice32, ButtonDevice32 | Bus width is distinct from character width, tick width and keyboard/event format | ConsoleDevice<BusSpec>, TimerDevice<BusSpec>, ButtonDevice<BusSpec> with independent device parameters |
| FramebufferDisplay32 | 32-bit bus; screen width, height and backing capacity already parameters | FramebufferDisplay<BusSpec> plus the existing geometry/storage configuration |
| IndexedPixelSelector32 | Selects an RGB332 byte from a 32-bit word and expands it to 32-bit ARGB | IndexedPixelSelector<WordW, PixelFormat>; input word size and output color format remain separate |

The additional `Mux2to1_4bit` should join the same mux family. Scalar
Mux2to1/Mux4to1/Mux8to1/Mux32to1 are its W=1 forms. Decoder5to32 describes
five address inputs and 32 decoded outputs; Decoder<AddressBits> is a
reasonable separate generalization, not a 32-bit data word. The same
distinction applies to IncrementBy4: four is the increment value, not width.
RV32I/RV32IM names identify an ISA and should retain that meaning.

### Existing parameterized precedents

- Register<W>, Cache<W>, FFWord<W>, FFStorageBank<W>, FFMemory<W>, SRAM<W>,
  DRAM<W>, ROM<W>, NORFlash<W>, TwoPortMemoryArbiter<W>, BitSplitter<W>,
  BitJoiner<W> and ConstantValue<W> already use width parameters in C++.
  This list does not claim that every component instantiates every width.
- ClockedCounter and SynchronousFifo already accept runtime width arguments
  and display parameterized type names. Counter supports 4/32 bits; FIFO
  supports 8/32 bits and configurable depths 1–7.
- MultiplyStep and DivideStep already accept runtime widths, currently
  restricted to 4/32. Their iterative parent units remain fixed at 32 bits.
- The pin/wire instantiation list in `include/basic/SupportedWidths.hpp` is
  1, 2, 3, 4, 5, 8, 16, 32. A generic class must declare its supported subset;
  writing `<W>` does not add arbitrary widths or 64-bit simulator support.

### Compatibility and implementation order

1. Generalize muxes, zero detection and adders first. Preserve ripple/prefix
   defaults per old instantiation. Use existing structural bit cells and
   gates; a generic class must still expose the real circuit hierarchy.
2. Extract shared arithmetic, bitwise, barrel-shift and comparison circuitry.
   Keep operation selection, carry conventions and public pin differences
   in explicit shells. Unify the ALUs after that common hardware exists.
3. Generalize iterative M units and the register file with their actual
   size/port/state parameters. Treat bus/device generalization as a separate
   protocol change: changing a bus to 16 bits needs a register transaction
   policy and does not merely narrow every existing 32-bit register.

Concrete catalog families still need identities for their actual pin widths,
as Register and Cache do today. Templates can share their factories while
preserving current IDs such as `arithmetic.adder.width8`,
`arithmetic.adder.width32`, `alu.width8`, `rv32i.alu32` and
`rv32i.alu32-core`. A new opcode contract must not silently reinterpret an
old ID. `canonicalContractId()` currently aliases memory/storage retirements;
it has no general alias rule for these ALU names.

Aliases for C++ types alone do not preserve saved layouts or observers.
`VisualizerSession.cpp` uses component types/child structure for layout
identity, and `DeviceInspector.cpp` matches exact strings such as
FramebufferDisplay32/ButtonDevice32/ConsoleDevice32/TimerDevice32. A future
migration must preserve those keys or provide explicit mappings and update
observation to use component capabilities/metadata. Update scenario metadata,
bindings, layout/color matching and public documentation with any change.

Preserve four-state behavior and timing, not just binary arithmetic. For
example, standalone ALU8FlagUnit computes its sign through OR with zero,
turning a Z sign input into X; ALU32FlagUnit rewires the sign bit and keeps Z.
The existing ALU8 fidelity test compares 11 structural/behavioral families;
ALUHierarchyTest explicitly checks named children, eight ripple FullAdders
for ALU8, and the shared prefix unit for ALU32. These definitions require
deliberate review if hierarchy/type names change. Characterized delays and
timing-profile tables must be regenerated when structural paths change;
there is no single timing table valid for every W/topology/operation policy.

### Verification and limits

Reviewed current source, public contracts, catalog registration, profile
guidance and existing test definitions; generated the class count directly
from public headers. No tests were added or run, and no compiler, WASM,
browser, benchmark or service process was started. This audit makes no new
performance or equivalence claim. Changes are internal documentation only;
the local preview, public pages, simulator components, primary pending work
and existing preview record remain unchanged. Whitespace validation uses
`git diff --check`; existing preview documentation remains uncommitted.

## 2026-10-22 — Consolidation of game-roadmap analysis

At the owner's explicit request, merged the separate game-roadmap review
into this existing width record. The original audit's dated sections above
and the game review's dated sections below are preserved verbatim. Updated
the header/summary, project status, roadmap and changelog links, and removed
the duplicate record. The initial audit took place in the former dedicated
checkout; the game review and consolidation use the main alpha repository,
with no linked worktrees.

The 12 fixed-width game components are part of the earlier 40-class
inventory, not 12 additional classes. Its six ISA identities and four
parameterized blocks explain the remaining game-roadmap entries. The
protocol finding below refines the earlier BusSpec suggestions: current
data-width generalization can reuse the existing W/32-bit-address contract.
Register ABI, device state, screen geometry and observation remain separate
design choices.

Verification for this consolidation checks preservation of both dated bodies,
updated document links and whitespace. No component, public page, build,
simulation, test suite or service changed. Width refactoring remains proposed;
phase-6 acceptance and phase-7 status are unchanged.

## 2026-10-22 — Scope

- Work directly in the main alpha checkout; no linked worktrees exist.
- Read the current status/index, open game roadmap, public device references
  and current module interfaces/implementations. Count names from the adopted
  inventory, not the superseded early component proposals.
- Inspect the shared memory protocol before proposing a new bus abstraction.
  Distinguish transport word width from the CPU ISA, address width, logical
  register widths, pixel format, framebuffer dimensions and host timestamps.
- Preserve existing components, ABI, profile/layout identities and pending
  documentation. New findings and recommendations will be appended below.

## 2026-10-22 — Inventory and proposed generic components

Parsed the adopted `New hardware component inventory` in the open game
roadmap and checked current public headers and implementations.

| Category | Count | Names |
| --- | ---: | --- |
| Fixed-width suffix | 12 | Multiplier32, Divider32, MulDivUnit32, AddressRouter32, BusErrorTarget32, InstructionAccessGuard32, DeviceRegisterPort32, ConsoleDevice32, TimerDevice32, ButtonDevice32, IndexedPixelSelector32, FramebufferDisplay32 |
| ISA identity | 6 | RV32IMDecodeControlUnit, RV32IMDecodeStage, RV32IMExecuteStage, RV32IMPipelineCoordinator, RV32IMFiveStageCore, RV32IMDeviceSystem |
| Parameterized building blocks | 4 | MultiplyStep<W>, DivideStep<W>, ClockedCounter<W>, SynchronousFifo<W> |

Keep the six ISA identities: they describe specific instruction encoding,
register semantics and CPU assembly. Compose them from generic hardware at
W=32. A generic multiplier or peripheral does not require a new ISA or CPU.

All generic API names below are proposals, not implemented interfaces.

| Current component | Proposed form | Separate parameters/contracts |
| --- | --- | --- |
| Multiplier32 | Multiplier<W> | W-bit operands, two W-bit result halves, signed operation, completion/ownership |
| Divider32 | Divider<W> | Quotient/remainder, signed edge cases, cancel/hold behavior |
| MulDivUnit32 | MulDivUnit<W> | Arithmetic operation selection; RV32IM decoding stays in the ISA adapter |
| AddressRouter32 | AddressRouter<W> | Region table, ports, permissions and local-offset translation |
| BusErrorTarget32 | BusErrorTarget<W> | Error status and held-response semantics |
| InstructionAccessGuard32 | InstructionAccessGuard<W> | Executable regions and fetch-access/alignment policy |
| DeviceRegisterPort32 | DeviceRegisterPort<W> | Lane count/alignment and register-bank transfer policy |
| ConsoleDevice32 | ConsoleDevice<BusW> | Eight-bit characters and independent FIFO depth |
| TimerDevice32 | TimerDevice<BusW, TickW> | Counter width, divisor/rate and multiword-read policy |
| ButtonDevice32 | ButtonDevice<BusW> | Button count, event format and queue depth |
| IndexedPixelSelector32 | IndexedPixelSelector<WordW> | Pixels per source word; retain RGB332 to ARGB32 expansion initially |
| FramebufferDisplay32 | FramebufferDisplay<BusW> | Screen width/height/stride, storage capacity, pixel format and frame-sequence width |

The existing parameterized steps accept only 4/32 bits, counter only 4/32,
and FIFO only 8/32. FIFO depth is 1–7 because COUNT is three bits. The
counter's optional divisor state is internally 32 bits even when its output
counter is four bits. These are documented supported subsets, not arbitrary
width implementations. The simulator's Pin/Wire instantiations are currently
1, 2, 3, 4, 5, 8, 16 and 32 bits.

## 2026-10-22 — Existing protocol is the starting point

`src/modules/memory/MemoryProtocol.cpp:103` defines `protocolPins(..., w, ...)`:

| Data width W | Address bits | Request/response data bits | Byte-mask bits |
| ---: | ---: | ---: | ---: |
| 8 | 32 | 8 | 1 |
| 16 | 32 | 16 | 2 |
| 32 | 32 | 32 | 4 |

Operation/status fields remain three bits; VALID/READY/CLK/RST remain one
bit. MemoryTransactionPort and MemoryResponseBuffer already accept a width.
The immediate device API can follow Cache<W>, with W meaning transport data
width and the current address contract unchanged. This refines the previous
audit's possible BusSpec names; address-width or protocol changes are
independent design axes.

## 2026-10-22 — Dependencies beyond names

### Arithmetic and ownership

`MulDiv.cpp` fixes operand registers at 32 bits, sign/quotient indexes at 31,
iteration state at five bits and last-step detection at all five bits high.
Generic units must derive register/index/counter sizes and compare against
W−1, select steps for W, and retain the exact start/cancel/hold semantics.
Pin-width changes alone leave operation length and state transitions wrong.
High-product correction, signed overflow and reference/compact behavior must
also follow W. Generic arithmetic belongs with shared arithmetic/composite
parts; device logic currently imports private RV32IM structural/catalog
helpers that should become narrow shared utilities during such a refactor.

### Transport, routing and logical registers

`DeviceBus.cpp` fixes protocol calls/data captures at 32, masks at four bits,
alignment checks at two low address bits and the response-buffer family at
width32. Its six-region table is source-level; only video capacity is
configurable. Generalize transport width and expose a region configuration
with validation for supported size/alignment/overlap rules separately.

The existing device ABI uses four-byte register offsets, full-word accesses,
full mask 0xf (zero mask is a no-op), and explicit POP/PRESENT command writes.
On an 8/16-bit endpoint, a 32-bit register spans multiple transfers: define
its layout and read consistency, and execute commands exactly once at a
defined write/commit boundary. A timer can change between partial reads.
Use an explicit narrower register layout or snapshot/assembly adapter before
advertising narrower-bus support. A CPU and endpoint with different data
widths also require a width-conversion bridge. Preserve the current W=32 ABI.

### Device widths are independent

- Console payload is eight bits; its current upper register bits must be
  zero. Payload width, bus width and queue depth are separate choices.
- Timer ticks/divisor/frequency are currently 32 bits. Tick width controls
  wraparound independently of bus width and the already-wide engine time.
- Buttons currently expose 32 levels, a five-bit ID and a 32-bit event with
  DOWN in bit 8. Button count/event ABI need separate declarations.
- Display geometry and storage capacity already accept parameters, but both
  buses are 32 bits, backing depth divides bytes by four, register offsets
  advance by four, and frame sequence is 32 bits.
- Pixel selection extracts one of four bytes from a 32-bit word. RGB332 is
  eight-bit color (3+3+2), while ARGB32 is a separate output format. A 16-bit
  source word could hold two RGB332 pixels without changing the resolution.

### Observation and guest drivers

`DeviceInspector.cpp` uses exact device names, a FramebufferDisplay32 cast,
FFStorageBank<32,...> matching and four-byte pixel-word extraction.
`device_view.js` matches fixed type names and validates/renders 32 keyboard
levels and 32-bit queue heads. Generic device capabilities and metadata need
to drive observations and layout/color matching, retaining old recordings
and saved keys. Renaming types alone can disable visible abstractions.

The guest `platform.h` fixes SCREEN_W=64 and SCREEN_H=48, volatile uint32_t
register accesses and MMIO addresses. `platform.c` uses SCREEN_W for clipping
and row offsets, and packs four pixels per word store. General display
hardware therefore also needs a guest driver that queries geometry/stride
and draws accordingly. Game rules can retain deliberate board sizes. A
larger hardware surface alone does not resize the existing game fixtures.

## 2026-10-22 — Recommendation and verification limits

Proposed order: generic arithmetic/state primitives; protocol/router/error/
guard components and declared maps; register-bank transfer policy; device
state separated from transport; metadata-driven observation and geometry-
aware guest drawing. Keep RV32IMDeviceSystem as the explicit 32-bit assembly
using the generic parts at W=32. Preserve current defaults, opcode/ABI
semantics, catalog IDs, profile/path/layout identities and recording readers.
New structural variants need their own timing characterization and fidelity
evidence before being advertised.

Reviewed source, existing contracts/test definitions and public references;
generated the 12/6/4 count from all 22 adopted inventory rows. No tests were
added or run; no compiler, simulation, browser, benchmark or service restart
ran. Only internal documentation changed; git diff --check validates
whitespace. Work stays on main alpha with no linked worktrees. Phase 6
remains open and phase 7 remains unimplemented. This analysis does not add
new milestone gates or authorize a component refactor.

## 2026-10-22 — ALU width design discussion

Continued the ALU comparison at the owner's request in this consolidated
record. Reopened the topic for design discussion; earlier dated sections
remain evidence of the completed audits. Source and current public ALU pin
contracts still agree with the comparison above. No implementation began.

### Separate width, semantic contract and structure

Use one public C++ template, ALU<W>, with a small construction-time ALUConfig
alongside the existing fidelity profile. W sizes operand/result words,
arithmetic wrapping, sign positions and shift networks. Configuration selects
hardware once during construction; it is not a new runtime instruction mode.

Start with two named semantic contracts rather than independent switches for
every operation and flag:

- Extended: the existing ALU8 operation map, 29 operations, carry input,
  invalid-operation output and arithmetic/shift flag conventions.
- IntegerDatapath: the existing ALU32 map, 13 operations and qualified
  comparison outputs. Its existing full-result-flags/core-only choice is
  explicit, matching ALU32 versus ALU32Core today.

The semantic contract includes pin roles, accepted OP encodings, unsupported
operation results and flag behavior. Within a given contract, ADD/SUB and
every other operation mean the same thing at W=8,16,32. Numeric OP encodings
must not be chosen from W: Extended SUB is 2 at every width; IntegerDatapath
SUB is 1 at every width. Shared named operation semantics can drive the
recipe; each legacy structural decoder still recognizes its original bit
patterns directly. Avoid adding an unconditional translation stage that
changes existing CPU propagation paths.

Adder topology is a separate explicit choice: Ripple or ParallelPrefix.
Structural/behavioral fidelity continues to come from the existing BuildProfile.
Each operation contract builds the gates it needs. The integer CPU does not
acquire extra rotate/min/max circuitry merely because it shares an ALU template.
All configuration combinations advertised by the catalog need a defined
contract and timing model; begin with the named combinations, not an arbitrary
operation-mask API.

| Proposed instance | Semantic contract | Adder | Purpose |
| --- | --- | --- | --- |
| ALU<8> | Extended | Ripple | Existing ALU8 compatibility |
| ALU<16> | Extended | Explicit choice | Establish a genuinely new word width |
| ALU<32> | Extended | Explicit choice | Existing extended operations on larger words |
| ALU<32> | IntegerDatapath, full result flags | ParallelPrefix | Existing ALU32 compatibility |
| ALU<32> | IntegerDatapath, core-only flags | ParallelPrefix | Existing ALU32Core compatibility |

These are proposed construction configurations, not existing API calls.
Default semantics/topology must be documented independently of W; the legacy
factories explicitly select their existing choices.

### Fit the current catalog and preserve contracts

One C++ implementation can back distinct concrete ComponentFamily entries,
as ALU32 already backs ALU32 and ALU32Core with different pin sets. A semantic
contract and width determine the concrete public pin contract. Sharing C++
code does not require merging incompatible catalog IDs or pretending all
pin sets are one contract. Keep old ALU8/ALU32 factory entry points as thin
compatibility construction paths, preserving their OP maps, pins and IDs.
Handle old type/layout keys and child paths explicitly before changing their
visible names. ALU8 and ALU32 should cease to own duplicated datapath logic.

### Proposed implementation sequence

1. Extract generic Mux<Inputs,W>, ZeroDetect<W>, RippleAdder<W> and
   ParallelPrefixAdder<W>, preserving existing structural defaults and paths.
2. Share AddSub<W>, arithmetic, bitwise, barrel-shift and comparison circuitry.
   Keep explicit carry and operation/flag adapters where old interfaces or
   four-state rules differ. A width parameter does not prove those contracts
   equivalent.
3. Assemble ALU<W> from those shared components and the two semantic recipes.
   Keep old families as configured construction paths into the same owner.
4. Bring up W=16 and Extended/W=32 through the same path. Initially use the
   supported word subset 8/16/32; do not infer arbitrary-width or 64-bit support
   from the template spelling.
5. Before declaring equivalence, compare each old contract with its configured
   generic instance, including X/Z, ignored inputs, reserved OP values, flags,
   gate topology and propagation. Characterize the new width/configuration
   timing and cover catalog/profile/layout/transport behavior before exposing
   it as accepted functionality. CPU and game acceptance remain separate.

This gives the same configuration principle as Cache<W>: one width-generic
implementation with explicit behavior/structure settings. Architectural
RV32I/RV32IM names still identify the CPU ISA. Component code, tests, public
pages, builds, simulation and services remain unchanged in this discussion.

## 2026-10-22 — RV32IM changes and shared pipeline structure

The owner asked whether M really needs new implementations of all five
stages. It does not. Separate RV32I/RV32IM family identities preserve their
accepted instructions; their internals should share every unchanged part.
This is independent of word-width generalization.

Read `RV32IMCore.cpp`, `RV32IFiveStageCore.cpp`, the shared
`RV32IFiveStageBlueprint.cpp`/`RV32IExtensionAssembly.hpp`, decode blueprint
and integer execute-stage source. Current structural implementation:

| Part | RV32IM change / actual reuse |
| --- | --- |
| Fetch | Same RV32IFetchStage, fetch requester and prediction path |
| Decode | M control wraps the I decoder; the same decode blueprint, register file and existing decode wiring are used with that control family |
| Execute | Wraps the existing RV32IExecuteStage, adds MulDivUnit32, M operand forwarding, result selection and EXECUTION_WAIT |
| Memory | Same existing RV32I memory-stage family |
| Writeback | Same existing RV32I writeback; M results use OUT_ALU_RESULT and the existing downstream result path |
| Pipeline registers | Same IF/ID, ID/EX, EX/MEM and MEM/WB families |
| Coordinator | Wraps existing I coordinator and qualifies pipeline movement while an M result is pending; core wiring adds cancel/take ownership |
| Core assembly | Both roots call buildFiveStageBlueprint; its configurable families are decode, execute and coordinator, with an extension-wiring hook |

`RV32IMFiveStageCore` names an M-capable assembly of a five-stage pipeline;
it does not mean five new stage implementations. Likewise the existing
`RV32IMemoryStage` name parses as RV32I + MemoryStage, not as a new M-extension
memory stage. Keep that naming ambiguity explicit when discussing reuse.

A replacement ALU alone is insufficient for the iterative M implementation:
decode must accept/classify M, EX must own the multi-cycle request, control
must wait and coordinate result consumption/cancellation, and writeback must
receive the result once. The existing integer ALU remains suitable for I
operations. The proposed ALU<W> and MulDivUnit<W> are separate reusable
execution blocks, with the CPU's extension-specific wiring around them.

One further sharing opportunity is real hardware duplication: the integer
EX child instantiates FORWARDED_OPERANDS, and the M wrapper instantiates
M_OPERANDS via the same helper. That shares source implementation but creates
two forwarding networks. A future composition refactor can route both units
from one network while preserving I-stage contracts, timing and layout
compatibility. This is an opportunity, not a new correctness/performance
claim or an implementation change.

Recommendation: preserve the common pipeline assembly and unchanged stages,
generalize the execution building blocks, and keep M-specific changes in
decode, EX and execution coordination. No test suite, build, simulation,
public page or service change was made for this source review.

## 2026-10-22 — Implementation started

The owner authorized width generalization and phase-6 completion, with only
necessary tests. Work is on main alpha; no linked worktrees exist. Preserve
all legacy IDs, pins, opcodes, child names and default timing recipes.

Implement shared structural word builders with explicit ALU operation contract
and adder topology, generic mux/arithmetic and iterative M execution APIs,
then device routing/configuration and geometry-aware guest drawing. New
width variants begin structural; existing characterized behavioral families
remain available through compatibility construction paths. Generic APIs must
reject unsupported combinations rather than imply multibeat MMIO support.

The host has about 2.8 GiB available RAM and 1.9 GiB free disk. Reuse existing
build directories after retargeting them to main alpha, compile serially,
keep deployment off, and run simulations/browser checks serially with a
headroom guard. Phase 6 remains open until current-source acceptance evidence
is recorded.

### Structural implementation drafted

The common word owners now cover extended and integer ALU assembly, adders,
carry-prefix levels, add/subtract, logic, shifts, comparison, selection, flags,
zero detection and operation decoding. The original eight-/32-bit classes
delegate to those recipes; their default gate names and pin contracts are
preserved. New public APIs are `ALU<W>` and width-parametric slices, muxes,
register files, iterative arithmetic, bus helpers and indexed pixel selectors.

The register-file configuration declares W, Count and hardwired-zero policy;
its two-read/one-write interface remains explicit. M arithmetic declares W
independently of the ISA and iterates W enabled edges. The CPU continues to
use the same shared five-stage blueprint and W=32 extension wiring.

New width variants are structural. Existing behavioral factories and proven
delay tables retain their own contracts; another width's table is not reused.
The shared structural and catalog registration utilities now belong to
component code, with compatibility includes for existing M code. Device ABI
width, framebuffer geometry and observation formats remain distinct.

A 16-bit recorded ALU scenario and focused width, geometry and capability
checks are drafted. Compilation is still in progress; no final validation
claim is made.

### Native verification checkpoint

The all-target native build completed serially with deployment disabled.
The final incremental step took 76.87 s and peaked at 356,500 KiB RSS; an
earlier full compile/link attempt peaked at 538,072 KiB before a mechanical
probe extraction error was corrected. Build logs remain under
`build-milestone/build-width-*.log`. No host OOM occurred.

The focused 10-check panel passes, including all 32 ALU codes under both
operation contracts and topologies at 8/16/32 bits, four-state carries and
flag rules, muxes, iterative 4/8/16-bit M latency/results, all declared
register-file geometries/zero policies, narrower ports/maps/pixels and the
recorded 16-bit ALU. The final rerun also drives every register/mux data bit
high and low and checks the register-state capability. Guest drawing passes
64x48, 320x200, 97x65 and 97x65 with stride 101, including clipping, padding
canaries, last pixel and word-store alignment.

The dependency panel passes 104/105 initially: ALU contracts/fidelity,
register integration, M core/compiled smoke, device/timing, large geometry,
wide time and Python declarations. CacheLayoutTest now passes in 28.20 s;
its native session is released before the portable comparison. The only
failure was Node's virtual-address reservation under the inherited 768 MiB
address-space limit, not host RAM exhaustion. RegisterFileProfileMigrationTest
passes unchanged with `NODE_OPTIONS=--max-old-space-size=256`, AS uncapped and
the same 512 MiB host-headroom stop. This completes that 105-check scope.
The native batch peak was 440,792 KiB RSS.

Old palette values are unchanged; generic prefix colors and the new scalar
result-bit color were added. Receipt:
`build-milestone/acceptance-width/palette-preservation.json`. New layout
entries and full native/WASM/browser/package acceptance remain pending.

### Remaining native resource gates resolved

The large SRAM subarray, clocked subarray and contract checks now pass in
89.04 s at 764,540 KiB peak RSS, under a 1,536 MiB address-space cap and the
unchanged 512 MiB host-headroom stop. The final stronger width panel passes
all ten checks in 11.07 s at 50,036 KiB. All 18 affected legacy timing tables
are recomputed under each of Unit, Embedded and Desktop: 54 comparisons,
zero differences. No timing-table file needs a numeric change.

The API audit preserves all 175 names/signatures/docs in the pre-milestone
snapshot and retains all five later additions; current total 183 includes
`create_alu`, `device_view_kind` and `supports_register_view`. The earlier
180-name checkpoint is a preservation report rather than a full signature
snapshot, so the five later signatures are not claimed independently compared.

The new ALU16 layout contributes 23 entries through an insertion-only merge;
old entry values and original formatting are retained. Full static validation
passes with zero problems in 367 scenarios at 288,088 KiB peak RSS. Current
WASM/site regeneration is underway, using one fresh process per requested
example group to avoid retaining earlier WASM heaps. Game/WASM/browser
acceptance remains pending; phase 6 is still open.

## 2026-10-22 — Public ALU and RV32I reference rewrite

The owner requested current component documentation through RV32I, following
the established component-page template and without an incremental narrative.
Chapter 2 will describe reusable word components and both operation contracts;
the RV32I ALU reference will specify the CPU configuration and integration.
Existing example aliases, recorded rows and page URLs remain usable. Review
the generic builders, concrete compatibility factories and documentation
checks before editing. Keep unrelated implementation, game evidence and the
live simulator package intact. Verification will use the affected document
contracts and a strict, memory-bounded site build; no CPU/game campaign is
needed for these documentation changes.

### Implemented reference organization

- Rewrote §§2.1–2.6 around `Adder<W>`, `AddSub<W>`, word logic/shifts,
  comparison, flags, control/result selection and `ALU<W>`. The references
  separate width, operation contract, adder topology and fidelity. Pins,
  signed ranges, carry rules, shift distances, construction and four-state
  limits describe the implemented supported widths. The concrete eight-bit
  examples retain their test identities and C++ references.
- Moved the eight reusable integer component references to §2.7. The stable
  `rv32i/4.3/alu32.md` URL specifies the CPU's W=32, integer/prefix,
  `full_flags=false` configuration, one `ALU32Core` interface and Execute
  wiring. Its former component fragments resolve to the shared-reference
  links. Complete/core flags and their different structural groups are
  explicit at every supported width.
- Updated the home/navigation, generic API, register-file configuration and
  single-cycle/baseline/advanced integration references. Memory references
  already specify parameterized storage/transport and need no contract
  rewrite for this change. Chapter 2 retains its component template and
  source/test sections; no implementation-history narrative is used there.
- Enabled `mkdocs_timing.py` in the local preview configuration, matching the
  main site hook. The 8893 articles render characterized timing rows as well
  as checkpoint tables. The existing simulator/WASM package is reused.
- Updated documentation-check paths and concrete-example/generic-heading
  mappings. The single-cycle reference compiles the shared integer examples
  before its dependent stage examples. Browser expectation changes concern
  headings/titles only; their scenario aliases and actual circuit types stay
  concrete.

### Verification and limits

- All six focused checks pass serially: `DocumentationExamplesTest`,
  `RV32IBuildingBlocksDocumentationTest`,
  `RV32ISingleCycleDocumentationTest`, `RegisterFileDocumentationTest`,
  `DocumentationDiagramsTest` and `TimingTableDocumentationTest`.
  Total 88.84 s; maximum child RSS 262,704 KiB. The first panel exposed a
  missing FF-storage cross-link and an omitted ALU timing paragraph; both
  were restored before the passing panel. A final two-check integer/timing
  rerun passes in 12.98 s after the full/core interface wording was finalized.
- Main strict MkDocs build passes. Its first link diagnostic identified the
  arithmetic-flags fragment, which is restored. The final strict preview
  build, with the same timing renderer and copied simulator package, passes
  in 69.23 s (69.85 s process time), maximum RSS 192,720 KiB. No warnings or
  errors appear in that final log. Native checks and builds use the existing
  768 MiB address-space cap, 1,000 MiB start headroom and 512 MiB host-available
  stop, with no heavy jobs launched in parallel.
- All 13 extended and nine integer/core C++ snippets are byte-identical to
  their pre-rewrite forms; the 22 relocated/retained example markers have
  identical aliases and registered tests. All 134 public component example
  markers resolve to the existing verified export bundle. No hand-maintained
  example rows or runtime components were added.
- The built-article audit checks 989 local article links/anchors across 18
  affected pages with zero failures. All 18 live articles from port 8893
  match their strict-built article HTML exactly, including rendered timing
  tables. The audit helper's numeric-directory and timing-attribute parsing
  were corrected before this final result. The preview remains active with
  MemoryMax=768 MiB and MemorySwapMax=0.
- Five affected Python check files parse successfully. Full browser/WASM
  suites, game runs and a fresh WASM compile were not run for these reference
  changes; their prior evidence retains its original scope. Runtime component
  implementations, the native live module and captured gameplay assets were
  not changed by this documentation work. The width/game implementation and phase-6 acceptance
  remain open; there is no commit or remote publication.

Evidence: `build-milestone/acceptance-width/alu-docs/` contains pre-edit text
snapshots, preservation and live-link receipts, serial check logs and strict
build logs. Key outcomes are copied above rather than relying on that ignored
directory alone.

## 2026-10-22 — Generic ALU identity and verification audit

The owner questions whether the generic design and its tests/scenarios form
an independent component or mainly combine the former ALU8 and ALU32. This
source audit supports that concern at the contract and organization levels,
while distinguishing the real implemented width generalization.

### Current evidence

- `WordDatapath.hpp` exposes W separately from topology, operation contract
  and full flags. The builders generate real gates at W=8/16/32; the generic
  API is more than a type alias.
- `WordDatapath.cpp` changes the public pin schema for Extended versus
  IntegerDatapath and dispatches to `buildALU` versus `buildIntegerALU`.
  Its `legacy()` function contains explicit concrete-family selections.
  Those two operation choices bundle historical opcode, interface, flag and
  construction policies rather than describing one independently defined
  datapath contract.
- `WordExtended.cpp` instantiates the concrete `ALU8ControlDecoder` family.
  `WordIntegerALU.cpp` instantiates `ALU32ControlDecoder` and selects concrete
  AddSub32/Logic32/Shifter32/comparison/result/flag classes at W=32. These
  classes delegate to shared width-aware recipes, but the generic assembly
  still knows the historical component identities. Child-fidelity policies
  also differ between the two assembly paths.
- `widthComponents()` in `src/tests/RV32IMPhase3Tests.cpp:539` has independent
  mathematical expectations at 8/16/32 bits and both adder topologies. It
  checks all 32 opcode values over four operand pairs and carry choices,
  selected X/Z carry cases, and the two sign-routing policies. This is real
  generic evidence, not only calls to an old ALU as an oracle.
- That function is also the combined owner for mux/register-file/M-unit/bus/
  pixel configuration checks. Its ALU functional matrix constructs
  `full_flags=true`; the timing matrix separately constructs reduced
  configurations. This does not establish comprehensive functional coverage
  for every advertised full/core configuration. Legacy ALU32Core evidence
  still covers the concrete 32-bit reduced interface.
- The dedicated `ALU16ScenarioTest` in `RV32IMPhase3Scenarios.cpp:154` has
  four default extended/ripple checkpoints: add, carry, shift and signed min.
  Most public examples remain the concrete ALU8/ALU32 scenarios, and the
  documentation reference snippets intentionally remain those concrete
  examples. These establish compatibility and sampled behavior, without
  making the generic component the primary owner of the scenario design.

The earlier statement that RV32I uses the same generalized ALU is accurate
at the shared-construction level. It should not be read as evidence that a
single coherent generic datapath contract and assembly have already replaced
the historical split. Passing compatibility/timing checks do not settle that
design question.

### Proposed correction for discussion

Define the reusable ALU contract first: canonical operation meanings, operand
and result widths, carry/overflow/comparison semantics, invalid-operation
behavior and supported optional hardware. Build its structural units from
the existing gates/slices under one composition model. Width and adder
topology remain direct hardware parameters. An enabled operation subset must
remove unused hardware so the CPU configuration need not instantiate every
extended operation.

Put historical opcode maps, public pin names and flag presentation behind
explicit compatibility adapters. Let the RV32I decoder translate instruction
operations into the generic contract. Preserve saved contract IDs, layouts
and recordings through an explicit migration boundary. Any replacement
circuit still needs four-state contract checks and characterized timing;
adapters cannot be assumed to preserve old propagation tables automatically.

Give reusable arithmetic/ALU components dedicated logical test owners and a
configuration-driven scenario generator outside game bring-up files. Check
the generic contract against an independent reference, then separately check
legacy adapter compatibility and RV32I integration. Retain prior tests as
regression evidence. Consolidating §2.7 alone would not address this source
and test-ownership issue.

This turn performs source inspection and records the design finding only.
No component, test, scenario, public page, build, simulation or preview
service was changed or run. The correction is a proposal; no canonical pin
schema, feature configuration or migration plan is claimed finalized.

## 2026-10-22 — Proposed first-class ALU definition

At the owner's request, define the replacement ALU independently of the
historical implementations. This is a proposed contract for discussion,
not the interface currently implemented in `WordDatapath.hpp`.

### Component and parameters

`ALU<W>` is a combinational W-bit component. It selects one specified
arithmetic, bitwise, movement, comparison or data-selection operation on its
operands, and produces a W-bit result plus explicitly defined status.
It has no clock, retained condition codes or instruction decoder.

Its hardware parameters are W, an enabled set of canonical operations, and
the adder topology. Initial production widths remain 8/16/32, matching the
declared word subset of the engine. Ripple and parallel-prefix are
construction choices. Enabled operations select hardware capabilities and
result paths; unused functions are omitted. Shared arithmetic dependencies
remain when required by another enabled operation. No Extended/IntegerDatapath
switch selects a second contract or assembly. Fidelity remains a build-profile
choice with separately established structural/behavioral equivalence and
characterized delays for the concrete configuration.

### Stable interface and semantics

Inputs are A[W], B[W], OP[5] and C_IN[1]. Outputs are OUT[W], ZERO, NEGATIVE,
CARRY_OUT, OVERFLOW and INVALID_OP. These pin meanings and names are stable
across enabled operation sets; there is no full/core interface switch.
OP identifies one canonical semantic operation, independent of width, ISA
or enabled subset. Numeric canonical assignments require an explicit enum
and encoding table before implementation; the historical maps are adapter
contracts and do not define that table by implication.

The initial semantic operation universe consists of:

- ADD, ADC, SUB, SBC, INC, DEC, NEG;
- AND, OR, XOR, NOT, NAND, NOR, XNOR;
- SLL, SRL, SRA, ROL, ROR;
- EQ, SLT, SLTU, MINS, MAXS, MINU, MAXU;
- PASS_A, PASS_B and ZERO.

This list names functions rather than historical opcode groups. One operation
has the same meaning at every W. ADD ignores C_IN; ADC adds it. SUB computes
A−B; SBC computes A−B−(1−C_IN). INC/DEC/NEG share operand preparation and
addition/subtraction hardware. Results wrap modulo 2^W and signed operations
use W-bit two's complement. Shifts/rotates use the low log2(W) bits of B.
Predicates return a zero-extended zero/one; min/max return an original operand.

ZERO and NEGATIVE describe the selected OUT: ZERO is its OR-reduction inverted,
and NEGATIVE directly routes OUT[W−1]. Arithmetic CARRY_OUT means carry or
subtraction no-borrow; OVERFLOW means signed overflow. For shifts/rotates,
CARRY_OUT is the last displaced bit, or C_IN at distance zero, and OVERFLOW
is zero. For logic, comparison, min/max, pass and ZERO operations, both
CARRY_OUT and OVERFLOW are zero. A known reserved or disabled operation
produces OUT=0, INVALID_OP=1, ZERO=1 and other status zero. Known enabled
operations produce INVALID_OP=0 even with unknown data. X/Z propagation for
unknown data/control follows the defined gate structures; an unproven
all-unknown behavioral shortcut is not the semantic reference.

### Composition and integration boundary

One assembly owns canonical operation control, operand preparation, a shared
add/subtract circuit, bitwise paths, barrel movement, subtraction-based
comparison/selection, result selection and status. Existing gates and cells
are reused; generic construction does not select ALU8/ALU32 child classes.

Legacy adapters translate their opcode maps, pin naming and status policies.
The CPU requests its required canonical operations and derives branch
comparisons from subtraction: EQ from zero, unsigned less-than from inverted
no-borrow, and signed less-than from result sign XOR overflow. The existing
ALU32 full-interface flags on SLT/SLTU require an explicit adapter policy;
the canonical comparison result's flags must not be silently reinterpreted.
An adapter may compose subtraction and result/status selection to implement
that policy. Such wiring needs its own four-state and timing validation.
This proposal does not claim a zero-cost or timing-identical migration.

The first-class ALU's pins are fixed. Avoiding unused CPU status work must
be justified through actual connectivity and measured timing, or a separately
defined structural datapath interface; it must not silently change this
component's contract through a full_flags boolean. Existing saved IDs/layouts
and prior recordings remain migration requirements rather than assumed
compatible with a changed hierarchy.

### Evidence ownership

Dedicated generic component specifications define expected operation and
status behavior independently of the circuit implementation. Tests cover
widths, supported operation-set configurations, arithmetic boundaries,
carry/borrow, every effective shift distance, comparison/predicate behavior,
disabled/reserved selections, targeted four-state cases and settling paths.
Representative small slices support exhaustive lower-level tests. Structural
and behavioral fidelities are compared only within their proven domain.

A configuration-driven scenario generator builds the same generic component
from those specifications and produces recorded checkpoints. Legacy adapter
compatibility and RV32I integration have separate test owners. Existing
regressions are preserved. The initial supported operation-set inventory and
its test sampling must be declared; exhaustive coverage of all 2^29 subsets
is not implied.

Only internal design records/status change in this turn. No implementation,
public reference, tests, scenarios, timing tables or live preview is changed;
no build or simulation is run for this definition.

## 2026-10-22 — Optimized generic ALU implementation scope

The owner redirected the preceding draft: retain the implemented opcode/pin
contracts, document ripple-carry and parallel-prefix addition separately, and
use the optimized adder in the generic ALU. The fixed-interface/operation-
subset proposal above is deferred and is not this implementation's contract.
The current task includes generic ALU construction, component-owned scenario
names, width-aware behavioral/reference code, and only focused validation.
Legacy saved contracts and scenarios remain compatibility entry points.

Implementation and checks are in progress. Host headroom at start is about
937 MiB RAM and 939 MiB disk; builds and checks remain serial and bounded.

### Focused ALU implementation evidence

The public `ALU<W>`/family defaults select parallel prefix. Structural ALU
recipes request width-aware child/control families. `ExtendedALUValueModel`
owns width/topology-aware four-state rules; the existing concrete compact
adapter uses its eight-bit ripple specialization. New component-owned
scenarios generate 64 all-opcode rows at each W=8/16/32 and matching 16-bit
ripple/prefix adder recordings. The public binary references in Chapters
2.1–2.7 take width parameters; RV32I explicitly instantiates W=32.

Fourteen focused native checks pass (96.30 s), including generic width,
legacy arithmetic/fidelity/hierarchy, 72 timing cases, catalog ownership and
registry/CTest pairing. Five documentation checks pass (72.99 s), including
compiled reference replay at 8/16/32 bits, integer/RV32I reference integration,
diagrams and timing markup. The first native run exposed a contextless
construction fallback and an old scenario's logical owner; both were fixed
before the passing rerun. Repeated memory guards stopped only our compiler;
a 512-MiB address-space attempt failed allocation in Clang, without host OOM.
The successful native build peaked at 470,108 KiB RSS; the WASM build peaked
at 374,276 KiB RSS. The live preview is temporarily stopped for the bounded
package refresh. Strict packaging/live inspection are pending.

The owner's subsequent instruction explicitly rejects compatibility IDs and
requires retiring every fixed-configuration component, including mux fan-in
variants. That broader replacement is tracked in the
[retirement record](2026-10-22-general-component-retirement.md) and supersedes
this section's compatibility-preservation implementation boundary.


## 2026-10-22 — Width work closed through complete component replacement

The generic ALU/scenario/reference and complete-retirement implementation is
accepted by the linked replacement record. It removes the former width-specific
construction routes and saved-ID aliases; both operation contracts work at
8/16/32 bits with either adder topology. The primary scenario is the same
width-driven 64-row generator at all three widths, with 16-bit prefix as the
default public view. Both ripple and prefix adders are documented; RV32I uses
the Chapter 2 integer configuration. Configurable storage, mux/decoder,
iterative arithmetic and bus/pixel APIs use their shared lower-level recipes.

Current acceptance consists of 22 native registrations, two API registrations,
seven documentation/metadata registrations, four catalog methods, six
portable/browser registrations, all 158 generated groups, three 809-entry
regenerated timing profiles and complete unit-file verification. Fresh Pong's
18 exact-reference frames and every retained pin/wire vector match the prior
capture. No old-ID compatibility path remains; the actual architectural
register-file exception is deliberate. Full native/web suites, the remaining
game campaign and interactive sessions are not marked complete by this record.


### Final current-source larger-display check

The replacement record additionally refreshes `RV32IMLargerDisplayTest`:
128x96 and 97x65 CPU geometry reads/high-address stores/PRESENT pass with zero
setup violations. Current native acceptance therefore totals 23 registrations,
plus two API registrations; the latter include direct 320x200 device tests.
The broader game campaign and interactive milestones retain their open status.
