# § 4.3.3. RV32I ALU Configuration

RV32I Execute uses the 32-bit **integer datapath contract**, parallel-prefix addition and `full_flags=false`. The catalog family is `word.alu.32.integer.prefix.core`, with concrete type `ALU<32,integer,prefix,core>`. It is the CPU configuration of [ALU&lt;W&gt;](../../alu/2.7.md), built by the shared structural recipes.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `ALU<32,integer,prefix,core>` | Integer result and branch comparisons for the RV32I datapath | Yes | Yes |

## Datapath selection

The corresponding generic configuration is `ALUConfig{Operations::IntegerDatapath, AdderTopology::ParallelPrefix, false}` at W=32. A parent requests `circuit::families::alu(32, config)` through its build profile. Full-flag mode selects `word.alu.32.integer.prefix`; the CPU uses the reduced interface because it does not read ZERO, NEGATIVE, CARRY_OUT or OVERFLOW.

[Execute](../4.4/execute.md) contains one `ALU` child for integer arithmetic, address calculation, comparisons and JAL/JALR targets. Conditional branches select SUB to obtain EQ/LT_SIGNED/LT_UNSIGNED while a separate adder computes PC+immediate. PC+4 uses its own incrementer. [Baseline](../4.5/execute.md) and [advanced](../4.6/forwarding.md) pipelines compose the same execution datapath.

The reusable component definitions are in Chapter 2:

| Function | Reference |
| --- | --- |
| Addition and subtraction | [AddSub&lt;W&gt;](../../alu/2.7.md#271-addsubw) and [adder topologies](../../alu/2.1.md#213-adderw) |
| Bitwise operations | [LogicUnit&lt;W&gt;](../../alu/2.7.md#272-logicunitw) |
| Shift network | [BarrelShifter&lt;W&gt;](../../alu/2.7.md#273-barrelshifterw) |
| Opcode controls | [ALUControlDecoder](../../alu/2.7.md#274-alucontroldecoder) |
| Comparisons | [CompareSelect&lt;W&gt;](../../alu/2.7.md#275-compareselectw) |
| Result selection | [ALUResultSelector&lt;W&gt;](../../alu/2.7.md#276-aluresultselectorw) |
| Optional result flags | [ALUFlagUnit&lt;W&gt;](../../alu/2.7.md#277-aluflagunitw) |
| Opcode map and complete ALU | [Integer ALU&lt;W&gt;](../../alu/2.7.md#278-aluw) |

## 4.3.3.1. `ALU<32,integer,prefix,core>`

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `A`, `B` | Input | 32 each | Operands |
| `OP` | Input | 5 | Operation |
| `OUT` | Output | 32 | Selected result |
| `EQ`, `LT_SIGNED`, `LT_UNSIGNED` | Output | 1 each | Comparisons, valid for subtraction operations |

### Behavior

`ALU<32,integer,prefix,core>` retains inputs `A[32]`, `B[32]`, `OP[5]` and outputs `OUT[32]`, `EQ`, `LT_SIGNED`, and `LT_UNSIGNED`. It omits `ZERO`, `NEGATIVE`, `CARRY_OUT`, and `OVERFLOW`, so its structural hierarchy has no `FLAGS` child. The arithmetic unit still computes carry and overflow internally because signed comparison needs them. The [complete integer ALU](../../alu/2.7.md#278-aluw) exposes all flags. Both variants use the same operation codes and settled binary result/compare rules. `ALUIntegerCoreWidth32Test` checks the reduced interface in structural and behavioral fidelities against the same arithmetic vectors as `ALUIntegerWidth32Test`.

#### C++ example

This binary interface reference calls `integer_alu_example::alu<32>` from [the integer ALU reference](../../alu/2.7.md#278-aluw).

```cpp
namespace integer_alu_example {
struct CoreResult {
    uint32_t OUT;
    bool EQ, LT_SIGNED, LT_UNSIGNED;
};
CoreResult core(uint32_t a, uint32_t b, unsigned op) {
    const auto r = alu<32>(a, b, op);
    return {r.OUT, r.EQ, r.LT_SIGNED, r.LT_UNSIGNED};
}
}
```

#### Unknown inputs

The structural form can mask unknown values in unused logic; the behavioral form returns unknown outputs if any bit of `A`, `B`, or `OP` is nonbinary. Their equivalence claim covers settled binary inputs. The unit is combinational and has no clock.

### Implementation

#### Structural implementation

The `CONTROL`, `ARITHMETIC`, `LOGIC`, `SHIFT`, `COMPARE`, and `RESULT_SELECT` groups share the [integer ALU construction](../../alu/2.7.md). There is no `FLAGS` group or externally connected flag storage.

#### Behavioral implementation

The `word.alu.32.integer.prefix.core` family uses `WordBehavioral<W, Topology>` in its reduced-interface mode. It computes the same result and comparisons as the general behavioral ALU but does not expose the four omitted flags.

### Examples

<!-- simulator-examples: alu-integer-prefix-core-w32 ALUIntegerCoreWidth32Test -->

#### Embedded simulation

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=alu-integer-prefix-core-w32&amp;embed=1" title="ALU<32,integer,prefix> CPU datapath" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=alu-integer-prefix-core-w32" class="md-button">Open ALU<32,integer,prefix,core></a>

### Tests

`ALUIntegerCoreWidth32Test` checks structural and behavioral results against the same arithmetic vectors. `RV32IBuildingBlocksDocumentationTest` replays the binary examples against the C++ reference above.
