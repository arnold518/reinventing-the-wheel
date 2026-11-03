# Behavioral form gap review

Started: 2026-10-28. Status: open. Code state: `alpha` at `5549ee0` plus the
uncommitted working tree (word grouping review applied).

## Summary

The owner asked for every component that has no behavioral (compact) form to
be analysed and the gaps filled, because in the explorer only a part with a
compact form can be collapsed. A census of every scenario at gate level found
256 composite types without a compact form, in five kinds (below). Work
proceeds tier by tier; each new compact form gets an equivalence test against
its gates and proven delay tables. Tier 1 (width variants) is in progress.

## 2026-10-28 — Census

`VisualizerSessionProbe preview` at gate level (fallback `logic`, then the
scenario default) for every registered test, one program per program family:
444 component types. 145 offer both forms, 79 only structural, 27 structural
plus unspecified, 183 only `unspecified` (built directly with
`addNewComponent`, not through the catalog). Composite types without a
compact form: 256.

### A. Width or parameter variants of a family that has a compact form elsewhere (27)

`ALU<16>`, `ALU<32>`, `ALUFlagUnit<16>`, `ALUFlagUnit<32>`, `ALUResultSelector<16>`, `ALUResultSelector<32>`, `AddSub<16>`, `AddSub<32>`, `ArithmeticUnit<16>`, `ArithmeticUnit<32>`, `BarrelShifter<16>`, `BarrelShifter<32>`, `CompareSelect<16>`, `CompareSelect<32>`, `Decoder<5,zero-disabled>`, `LogicUnit<16>`, `LogicUnit<32>`, `Mux<2,16>`, `Mux<2,1>`, `Mux<2,4>`, `Mux<2,8>`, `Mux<32,1>`, `Mux<4,1>`, `Mux<8,16>`, `Mux<8,1>`, `Mux<8,8>`, `ZeroDetect<16>`

### B. Catalog families with only a gate-level form (48)

`Cache<32>`, `Cache<8>`, `DRAMCommandControl<8>`, `DivideStep<32>`, `DivideStep<4>`, `Divider<32>`, `MatrixColumnMux<8,1>`, `MatrixColumnMux<8,2>`, `MatrixRowDecoder<2>`, `MatrixRowDecoder<3>`, `MatrixRowDecoder<4>`, `MatrixWriteDistributor<8,1>`, `MatrixWriteDistributor<8,2>`, `MemoryResponseBuffer<32>`, `MemoryResponseBuffer<8>`, `MemoryTransactionPort<8>`, `Multiplier<32>`, `MultiplyStep<32>`, `MultiplyStep<4>`, `NORFlashCommandControl<8>`, `ROMAddressControl`, `RV32IALUAddSub`, `RV32IALUCompare`, `RV32IALUControl`, `RV32IALULogic`, `RV32IALUResultSelect`, `RV32IALUShifter`, `RV32IBitPatternMatcher`, `RV32IMDecodeControlUnit`, `RV32IMDecodeStage`, `RV32IMExecuteStage`, `RV32IMPipelineCoordinator`, `SRAMAccessControl<32>`, `SRAMAccessControl<8>`, `SRAMColumnSelector<32,2>`, `SRAMColumnSelector<8,1>`, `SRAMColumnSelector<8,2>`, `SRAMPrecharge<16>`, `SRAMPrecharge<2>`, `SRAMPrecharge<64>`, `SRAMPrecharge<8>`, `SRAMRowDecoder<16,2>`, `SRAMRowDecoder<4,1>`, `SRAMRowDecoder<4,2>`, `SRAMRowDecoder<6,2>`, `SRAMRowDecoder<8,2>`, `SRAMWriteDriver`, `TwoPortMemoryArbiter<8>`

### C. Internal groups with no contract (built directly, not through the catalog) (117)

`ALUResultBit`, `ArithmeticIterationControl<32>`, `CacheAddressPath<32>`, `CacheAddressPath<8>`, `CacheLineState<1>`, `CacheLineState<2>`, `CacheLineState<4>`, `CacheLineState<8>`, `CacheLookupPhases`, `CacheReplacementState`, `CacheRequestCheck<32>`, `CacheRequestCheck<8>`, `CacheTagMatch<32>`, `CarryCombine`, `CarryPrefixLevel<16,1>`, `CarryPrefixLevel<16,2>`, `CarryPrefixLevel<16,4>`, `CarryPrefixLevel<16,8>`, `CarryPrefixLevel<16>`, `CarryPrefixLevel<1>`, `CarryPrefixLevel<2>`, `CarryPrefixLevel<4>`, `CarryPrefixLevel<8,1>`, `CarryPrefixLevel<8,2>`, `CarryPrefixLevel<8,4>`, `CarryPrefixLevel<8>`, `CommandMemorySequencer<dram,8>`, `CommandMemorySequencer<nor-flash,8>`, `DivideDatapath<32>`, `DivideOperands<32>`, `DivideResultCorrection<32>`, `FetchAddressDecode<32,1>`, `FetchRequestValidation<32>`, `Incrementer<32,4>`, `MatrixRowDecoder<1>`, `MemoryAccessControl<1,RW>`, `MemoryAccessControl<2,RW>`, `MemoryAccessControl<4,RW>`, `MemoryAddressDecoder<1,RW>`, `MemoryAddressDecoder<2,RW>`, `MemoryAddressDecoder<4,RW>`, `MemoryArbiterGrant`, `MemoryArbiterState`, `MemoryFFRequestCheck<32,1024>`, `MemoryFFRequestCheck<32,256>`, `MemoryFFRequestCheck<32,4>`, `MemoryFFRequestCheck<32,65536>`, `MemoryFFRequestCheck<8,4>`, `MemoryPayloadMux<1>`, `MemoryPayloadMux<32>`, `MemoryPayloadMux<3>`, `MemoryPayloadMux<4>`, `MemoryPayloadMux<8>`, `MemoryReadSelector<32,1>`, `MemoryReadSelector<32,2>`, `MemoryReadSelector<32,4>`, `MemoryReadSelector<8,1>`, `MemoryReadSelector<8,4>`, `MemoryRequestCheck<dram,8>`, `MemoryRequestCheck<nor-flash,8>`, `MemoryRequestCheck<rom,8>`, `MemoryRequestCheck<sram,8>`, `MemoryTimer<33,3>`, `MemoryTimer<43,4>`, `MemoryTimer<70,4,1>`, `MemoryTransactionPort<32>`, `MulDivResultSelection<32>`, `MultiplyDatapath<32>`, `MultiplyOperands<32>`, `MultiplyResultCorrection<32>`, `RV32IBaselinePacketStatus`, `RV32IByteWriteMask`, `RV32IDecodeControlField<ALU_A_SEL>`, `RV32IDecodeControlField<ALU_B_SEL>`, `RV32IDecodeControlField<ALU_OP>`, `RV32IDecodeControlField<BRANCH_TYPE>`, `RV32IDecodeControlField<DECODE_TRAP_CAUSE>`, `RV32IDecodeControlField<HALT_REQUEST>`, `RV32IDecodeControlField<IMMEDIATE_SELECT>`, `RV32IDecodeControlField<JUMP_TYPE>`, `RV32IDecodeControlField<LEGAL>`, `RV32IDecodeControlField<LOAD_SIGN_EXTEND>`, `RV32IDecodeControlField<MEM_READ>`, `RV32IDecodeControlField<MEM_SIZE>`, `RV32IDecodeControlField<MEM_WRITE>`, `RV32IDecodeControlField<REG_WRITE>`, `RV32IDecodeControlField<TRAP_REQUEST>`, `RV32IDecodeControlField<USES_RS1>`, `RV32IDecodeControlField<USES_RS2>`, `RV32IDecodeControlField<WRITEBACK_SEL>`, `RV32IDecodeControlSignals`, `RV32IExecutePacketStatus`, `RV32IFetchRequest`, `RV32IForwardedOperands`, `RV32IInstructionRecognition`, `RV32ILoadByte<0>`, `RV32ILoadByte<1>`, `RV32ILoadByte<2>`, `RV32ILoadByte<3>`, `RV32ILoadExtender`, `RV32IMDecodeOverrides`, `RV32IMExecutionControl`, `RV32IMPipelineHoldControl`, `RV32IMemoryAddress`, `RV32IMemoryAddressDataOnly`, `RV32IMemoryLanesDataOnly`, `RV32IPredictionEntry`, `RV32IPredictionTable<16>`, `RV32IPredictionTable<4>`, `RV32IRequestTagMatch`, `RV32IStatusPriority`, `RV32IWritebackBypass`, `RV32IZeroWaitMemoryPort`, `RegisterFileStorageBank<32,31>`, `StaticMemorySequencer<rom,8>`, `StaticMemorySequencer<sram,8>`, `TwoPortMemoryArbiter<32>`

### D. Device I/O (deferred by the owner) (55)

`BusErrorResponse<32>`, `BusErrorTarget<32>`, `ButtonCommands`, `ButtonEventEncoding`, `ButtonInputUpdate`, `ButtonLevelUpdate`, `ButtonReadData`, `ButtonRegisterDecode`, `ClockTickEnable`, `ConsoleCommand`, `ConsoleReadData`, `ConsoleRegisterDecode`, `CounterIncrement<32>`, `CounterIncrement<4>`, `DeviceAddressByteMatch<0>`, `DeviceAddressByteMatch<2>`, `DeviceAddressByteMatch<4>`, `DeviceAddressDecode<32,6>`, `DeviceAddressWindow<12>`, `DeviceAddressWindow<18>`, `DeviceAddressWindow<8>`, `DeviceRegisterAddressMatch`, `DeviceRegisterControl`, `DeviceRegisterExecution<32>`, `DeviceRegisterRequest<32>`, `DeviceRegisterValidation<32>`, `DeviceRouteChannel<32,12>`, `DeviceRouteChannel<32,18>`, `DeviceRouteChannel<32,8>`, `DeviceRouterControl<1>`, `DeviceRouterControl<6>`, `DeviceRouterRequest<32,1>`, `DeviceRouterRequest<32,6>`, `DeviceRouterResponse<32,1>`, `DeviceRouterResponse<32,6>`, `DeviceStatusMux`, `DisplayFrameControl`, `DisplayReadData`, `DisplayRegisterDecode`, `FifoOccupancy`, `FifoPointers<4>`, `FifoReadSelect<32,4>`, `FifoReadSelect<8,4>`, `FifoStorage<32,4>`, `FifoStorage<8,4>`, `FifoWriteDecode<4>`, `IndexedPixelSelector<32>`, `MemoryArbiterResponseRoute`, `PixelByteSelect<32>`, `RV32IMDeviceSystem`, `Rgb332Expansion`, `RouterRequestValidation<32>`, `TimerAccess`, `TimerReadData`, `TimerRegisterDecode`

### E. Primitives and fixtures (gate-level by definition, or test roots) (9)

`CacheFixture`, `DFlipFlop`, `FullAdder`, `GatedDLatch`, `HalfAdder`, `IOComponent`, `RV32IMProgramRoot`, `RV32IProgramRoot`, `SRLatch`

Notes:

- A: the compact classes are already width-generic (`WordBehavioral<W>`
  on `ALUValueModel<W>`, `BusMuxBehavioral<N,S,W>`); only the factories were
  limited to the characterized widths. `Decoder<5,zero-disabled>` already has
  a compact form, but the register file builds its `RD_DECODER` directly
  (`addNewComponent<logic::WordDecoder>`), not through the catalog, so no
  profile can select it.
- B: includes the six RV32I ALU parts. The structural `RV32IALU` also
  requires its parts to be gate level (`addStructural` in `RV32IALU.cpp`),
  so a compact form for them is only useful once that requirement is lifted,
  which needs the parts' compact forms to reproduce their gates' four-state
  rules exactly (not only the known-binary domain of the whole ALU).
- C: making these collapsible needs each to become a catalog family first
  (contract ID, test owner, verification evidence) and then a compact form.
- E: `DFlipFlop`, the latches, `FullAdder` and `HalfAdder` are the
  gate-level primitives the lower-level-first rule builds everything from.

## Plan

1. Tier 1 (A): open the existing compact classes to every width; one new
   `CompactFormEquivalenceTest`; regenerate the delay tables.
2. Tier 2 (B): compact forms for the catalog families, starting with the six
   RV32I ALU parts (four-state exact), then lift the ALU's gate-level
   requirement.
3. Tier 3 (C): promote internal groups to catalog families with compact
   forms, by subsystem.
4. D waits for device phase P1; E stays gate level.

## 2026-10-28 — Tier 1 and the six RV32I ALU parts

- The Chapter 2 compact class (`WordBehavioral<W>`, the gates' four-state
  value model) is offered at 8, 16 and 32 bits for every unit, and the word
  mux compact class (`BusMuxBehavioral`) at every geometry. 37 families.
- The six RV32I ALU parts got compact forms that follow their gates bit by
  bit with the gate rules (wiring passes Z), and the structural `RV32IALU`
  no longer requires gate-level parts, so the explorer can collapse them.
- New `CompactFormEquivalenceTest`: all 43 families beside their gates on
  binary, dense four-state and single-unknown vectors (8,256 vectors). The
  RV32I profile audit gained the six part contracts and `logic.and.8` /
  `logic.or.8` (the load-lane word gates of the earlier batch).

## 2026-10-28 — Generic gate-list compact form (tiers 2 and 3, combinational parts)

Owner: "decide yourself … complete the non-device jobs". Rather than write a
compact class for every remaining part, a combinational part of plain gates
gets one generic compact form that evaluates its own gate list:

- `buildDelayTwin` marks a twin `exact_values` when it has no flip-flops and
  no leaf (a memory primitive may hold state the list cannot see), and records
  output bits wired straight to an input bit through zero-delay wiring
  (`wired`), which pass Z unchanged.
- `circuit::GateListCompact` (`components/selection/GateListCompact.*`): one
  leaf that settles the twin once per change and publishes its values; Z on a
  wired bit is copied. Its delays are the stored proven table, or, for a
  parameter set without one, proven on the same gates when first built with
  the generator's options; the twin is attached for data-dependent timing.
- Selection: an implementation flagged `gate_list` is chosen only by an
  exact-path rule (`ProfileDecision::exact_path`, the explorer's choices and
  tests) or a parent's requirement. A general "prefer behavioral" rule keeps
  the gates, so no existing profile, preset or recording changes.
- `part_delay_tables --gate-list-compacts FILE` writes
  `GateListCompacts.generated.inc`: contracts with a gate-level form, no
  compact form of their own, not in the no-gate-list file, and an exact gate
  list (Device I/O contracts excluded: deferred). `--verify` also compares
  this list. 33 contracts: the multiply/divide steps, `HalfAdder`,
  `FullAdder`, the 1- to 4-bit decoders, the shared row decoder, column mux
  and write distributor, the SRAM row decoder, precharge, write driver and
  column selector, the RV32IM decode control and pipeline coordinator.
- `GateListCompactEquivalenceTest`: each of the 33 beside its gates on
  binary, dense four-state and single-unknown vectors (3,696 vectors); the
  RV32I toggle audit leaves these explicit-only forms to it.

Not covered by the generic form: parts with flip-flops (controllers, stages,
caches) and parts containing memory primitives. They keep needing
hand-written compact forms. The internal groups without a contract (C) are
still not collapsible.
