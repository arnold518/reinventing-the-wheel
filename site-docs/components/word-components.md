# Parameterized word components

Width, operation contract, adder topology and storage geometry are independent
construction parameters. Each supported configuration has its own exact catalog
ID and build-profile selection. `RegisterFile32x32` is the RV32I architectural
exception: the ISA fixes both its word width and register count.

## `ALU<W>`

`ALU<W>` supports **8, 16 and 32** bits. `ALUConfig` selects its operations
and physical adder when the circuit is built. `ALU<W>(name)` and `families::alu(W)` select extended operations, parallel-prefix carry and full flags. An explicit configuration selects the other supported recipes:

```cpp
#include "modules/composite/ALU.hpp"

using namespace circuit::word;
ALUConfig config{Operations::Extended, AdderTopology::ParallelPrefix, true};
auto alu = Component::create<ALU<16>>("ALU", config);
// In a parent, use the selectable family:
builder.add(circuit::families::alu(16, config), "ALU");
```

Python provides the same choice through
`circuit_backend.create_alu("ALU", 16, "extended", "prefix")`. Unsupported
fidelities and configurations are rejected.

Changing `W` preserves operation codes and flag rules. `full_flags` may be
false for `IntegerDatapath`; the extended contract requires its flags.
Fidelity is selected by the existing build profile, independently of this
configuration.

| Contract | Operation codes | Arithmetic and flags |
| --- | --- | --- |
| `Extended` | [extended codes](../alu/2.6.md), at every W | 29 operations; carry/no-borrow input; arithmetic and shift carry; comparison results clear carry/overflow; reserved 7, 15, 31 assert `INVALID_OP` |
| `IntegerDatapath` | [integer codes](../alu/2.7.md), at every W | 13 operations; comparison flags qualified by SUB/SLT/SLTU; subtraction flags retained for SLT/SLTU; reserved codes produce zero |

| Pin | Direction | Width | Availability |
| --- | --- | --- | --- |
| A, B | Input | W each | Both contracts |
| OP | Input | 5 | Both contracts |
| C_IN | Input | 1 | Extended |
| OUT | Output | W | Both contracts |
| ZERO, NEGATIVE, OVERFLOW | Output | 1 each | Extended, or integer with `full_flags` |
| CARRY, INVALID_OP | Output | 1 each | Extended |
| CARRY_OUT | Output | 1 | Integer with `full_flags` |
| EQ, LT_SIGNED, LT_UNSIGNED | Output | 1 each | Integer |

Shifts consume the low `log2(W)` bits of B. Rotation and zero-distance shift
carry follow the extended contract. Both contracts use four-state gates;
unknown inputs are propagated by the selected circuit's gates.

The [extended ALU](../alu/2.6.md) composes arithmetic, selected logic, barrel
shifter, comparison, result-selection and flag recipes. The [integer ALU](../alu/2.7.md)
composes its control decoder, add/subtract, parallel logic, barrel shifter,
comparison, result-selection and flag recipes. Ripple and parallel-prefix
addition are explicit choices at all supported widths. `ALUConfig{}` selects
extended operations, parallel-prefix carry and full flags.

Width/configuration families without a characterized behavioral factory provide **structural** circuits.
Behavioral factories are scoped to their characterized contracts. Delay
tables are specific to width, operation contract and topology.

### Recorded 16-bit example

The shared generator checks all 32 opcodes with two operand/carry sets (64 rows). W = 8 and W = 32 use the same scenario definition. The independently selectable ripple example is `alu-extended-ripple-w16`.

<!-- simulator-examples: alu ALUScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=alu&amp;embed=1" title="ALU&lt;16&gt;" loading="lazy" allowfullscreen></iframe></div>

## Arithmetic and selection units

`WordDatapath.hpp` also exposes `Adder<W>`, `AddSub<W>`, `ZeroDetect<W>`,
`ArithmeticUnit<W>`, `LogicUnit<W>`, `BarrelShifter<W>`, `CompareSelect<W>`,
`ALUFlagUnit<W>` and `ALUResultSelector<W>`. Their W is 8, 16 or 32.
`ALUControlDecoder` takes an operation contract; decoding itself has no
operand width.

| Component | Inputs | Outputs |
| --- | --- | --- |
| `Adder<W>` | A[W], B[W], Cin | Sum[W], Cout |
| `AddSub<W>`, extended | A[W], B[W], SUB, C_IN | OUT[W], CARRY_OUT, OVERFLOW |
| `AddSub<W>`, integer | A[W], B[W], SUB | OUT[W], CARRY_OUT, OVERFLOW |
| `ZeroDetect<W>` | A[W] | ZERO |

The remaining slice pin names follow their [extended](../alu/2.6.md) or
[integer](../alu/2.7.md) contract, with operand/result widths replaced
by W. Extended add/subtract uses the explicit C_IN even when SUB is set;
integer add/subtract uses SUB as the carry input. The sign/overflow gates
retain their contract-specific four-state rules.

`ArithmeticUnit<W>` uses the extended arithmetic contract; its configuration
chooses the adder topology. `ALUControlDecoder` selects an operation contract
without a data-width parameter. `word.control.extended` and
`word.control.integer` identify its two control maps. Each map works at every
supported operand width.

`Mux<Inputs,W>` supports Inputs = 2, 4, 8, 16 or 32 and W = 1, 4, 8, 16 or 32.
Two-input muxes have A and B; larger muxes have IN0…IN(Inputs−1). SEL has
`log2(Inputs)` bits; OUT has W bits. Its balanced tree composes scalar two-input muxes. `circuit::word::mux(Inputs,W)` returns its family.

`ParallelPrefixAdder<W>` exposes A[W], B[W], Cin, Sum[W], Cout and the carry
into the sign bit (`C7`, `C15` or `C31`). Half-adder cells form generate/propagate signals; logarithmically many
carry-combination levels supply every sum bit.

`Decoder<AddressBits>` accepts ADDR[AddressBits] and ENABLE; its 2^AddressBits
single-bit outputs are OUT0…OUT(N−1). AddressBits is 1 through 5. With known
enable/address, exactly the selected output is high; disabling ENABLE clears
all outputs. The `suppress_zero` construction option permanently ties OUT0 low.
Unknown controls follow the minterm gates. Families are
`logic.decoder.<AddressBits>.full` or `.zero-disabled`.

`Incrementer<W,Step>` accepts A[W] and produces Sum[W] = A + Step modulo 2^W.
W is 8, 16 or 32; Step is a nonnegative constant below 2^W (default 1).
A power-of-two Step uses a prefix increment network; other constants use a
constant carry network, and Step=0 forwards every bit. Unknown bits follow
those gates. `Incrementer<32,4>` computes the CPU's PC+4. This helper has a
structural form and a shared settled-value function for compact parent parts;
it is not an independently selectable behavioral catalog family.

## `RegisterFile<W,Count>`

`modules/storage/RegisterFile.hpp` provides a register file with two
asynchronous read ports and one enabled rising-edge write port. W is 8, 16
or 32; Count is a power of two from 2 through 32. `RegisterFileConfig` chooses
whether register zero is hardwired to zero. The default is true.

| Pin | Direction | Width |
| --- | --- | --- |
| CLK, RST, REG_WRITE | Input | 1 each |
| RS1_ADDR, RS2_ADDR, RD_ADDR | Input | log2(Count) each |
| WRITE_DATA | Input | W |
| RS1_DATA, RS2_DATA | Output | W each |

The circuit composes `Decoder<log2(Count)>`, two balanced `Mux<Count,W>` read
trees and a bank of `Register<W>` words.
Reset and unknown control behavior come from those storage cells. A hardwired
zero has no storage and ignores writes. The 32×32 hardwired-zero
configuration is also the `RegisterFile32x32` RV32I component, with contract
`rv32i.register-file`. It uses the same construction recipe. Register inspection
uses `RegisterStateView`.

## Iterative multiplication and division

`modules/composite/IterativeArithmetic.hpp` exposes `Multiplier<W>`,
`Divider<W>` and `MulDivUnit<W>` for W = 4, 8, 16 or 32.

Pins follow the [multiplier](../rv32im/multiplier.md),
[divider](../rv32im/divider.md) and [combined unit](../rv32im/muldiv.md), with
operand/result widths replaced by W. Control pins remain one bit and the
combined OP remains three bits. There are W enabled iteration edges after
launch. Pause, cancellation, held completion and TAKE retain the existing
ownership rules. Signed overflow and division by zero use the same M-unit
rules at every width. These units are reusable hardware; the RV32IM CPU
continues to select W=32.

## Device transport and observation

`DeviceRegisterPort<W>`, `BusErrorTarget<W>`, `AddressRouter<W>` and
`InstructionAccessGuard<W>` support W = 8, 16 and 32 through
`modules/device/DeviceBus.hpp`. Their request/response protocol keeps
32-bit byte addresses, W-bit data and W/8 byte-mask bits. Register-port
alignment is W/8 bytes; a nonzero write mask must cover the whole bus word.
A zero mask performs no write action. The router's six regions have
configurable aligned, non-overlapping power-of-two ranges. Catalog parameters
are `ram_base`, `ram_bytes`, `console_base`, `console_bytes`, and the
corresponding timer/buttons/display/video pairs; `video_bytes` sets the pixel-memory aperture (4,096 bytes by default).
`InstructionAccessGuard<W>` retains the low-RAM fetch range `[0,0x40000)`.
Customizing a standalone router does not relocate the platform's fetch guard,
loader or guest ABI.

The game platform's console, timer, buttons and display control registers
retain their **32-bit MMIO ABI**. Narrow transport helpers do not imply an
implemented bridge or atomic multiregister access to those devices.
Framebuffer width, height, capacity and queue depths remain separate
parameters.

`IndexedPixelSelector<W>` supports 8-, 16- and 32-bit words independently of
its eight-bit RGB332 pixel format and 32-bit ARGB output. WORD has W bits;
OFFSET has `max(1,log2(W/8))` bits. The one-byte form selects its only byte for OFFSET=0 and produces index zero
for OFFSET=1. INDEX has eight bits. Counters support 4, 8, 16 and 32 bits;
FIFO words support 8, 16 and 32 bits with the existing depth limit 1–7.

The viewer receives `deviceViewKind` metadata from the `DeviceView`
capability and display geometry/storage-word metadata from the device. Keyboard snapshots can
supply `buttonCount`; absent metadata retains the original 32-button view.
These are recorded observations; the keyboard image does not inject live
input.

## Checks

`WidthComponentsTest` checks ALU codes/flags, four-state carries, supported
mux selections, iterative arithmetic latency/results, register-file
geometries, narrower ports and pixel selection. Component, fidelity and timing checks validate the corresponding configured
contracts. `ALUScenarioTest` provides the recorded example above. Geometry checks run the actual guest
drawing library against large and odd/padded strides; compiled game evidence
is documented separately in [recorded games](../rv32im/recorded-games.md).
