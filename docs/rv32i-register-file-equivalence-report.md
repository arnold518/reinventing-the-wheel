# RV32I Register-File Equivalence Report

## Outcome

Block 3 has two implementations of the same register-file contract:

- `RegisterFile32x32`: composed register-file datapath.
- the behavioral fidelity of `RegisterFile32x32`: compact reference model.

The independent equivalence proof is `RV32IRegisterFileEquivalenceTest`, and it passes.

## Contract

Inputs:

- `RS1_ADDR[4:0]` and `RS2_ADDR[4:0]`: two independent read addresses.
- `RD_ADDR[4:0]`: write address.
- `WRITE_DATA[31:0]`: value presented to the write port.
- `REG_WRITE`: enables a write.
- `CLK`: a write occurs on a rising edge.
- `RST`: clears x1-x31; x0 is always zero.

Outputs:

- `RS1_DATA[31:0]`
- `RS2_DATA[31:0]`

Reads are combinational. Storage updates only at the clock edge when `REG_WRITE` is active. A write to x0 is ignored.

## Lower-level-first status

`RegisterFile32x32` structurally exposes the important register-file organization:

- a 5-to-32 write decoder;
- separate 32-to-1, 32-bit read muxes for rs1 and rs2;
- a hardwired zero source for x0;
- explicit fanout for clock, reset, write data, and individual write enables.

To keep the full 32x32 component usable, x1-x31 are compact the behavioral fidelity of `Register32` word cells. This is an allowed scale abstraction because the repository already contains and tests the lower-level `Register32` implementation. The compact whole-file model remains a separate sibling and is not used to replace the composed decoder/mux datapath.

## Verification

`RV32IRegisterFileEquivalenceTest` supplies the same signal sequence to two isolated simulations and checks each trace against hard-coded expected values. It covers:

- reset values on both read ports;
- attempted write to x0;
- a distinct data pattern written to every x1-x31 register;
- selection of every entry through the rs1 read mux;
- simultaneous selection of the preceding entry through the rs2 read mux;
- a disabled write that must preserve x5;
- reading x31 at the opposite port;
- reset recovery after the file contains data.

Command and result:

```text
ctest --test-dir build -R '^RV32IRegisterFileEquivalenceTest$' --output-on-failure
RV32IRegisterFileEquivalenceTest ... Passed
```

## Boundary

This equivalence test proves the normal fully known binary contract. `RegisterFile32x32BehavioralUnknownPolicyTest` separately exercises ambiguous addresses, unknown enables, unknown reset, and bitwise merging behavior. Partial-unknown equivalence is not claimed.
