# Structural D Flip-Flop Report

Last updated: 2026-05-17

## Status

`DFlipFlop` has been replaced with a structural implementation.

The old behavioral C++ state machine is no longer used by `DFlipFlop`. The public `DFlipFlop` class is now an `IOComponent` composite built from gates and latch components, so the visualizer can expand and inspect the storage path.

## Implemented Components

### `SRLatch`

Files:

- `include/modules/basic/Latch.hpp`
- `src/modules/basic/Latch.cpp`

Pins:

| Pin | Direction | Meaning |
| --- | --- | --- |
| `S_BAR` | input | Active-low set. |
| `R_BAR` | input | Active-low reset. |
| `Q` | output | Stored state. |
| `Q_BAR` | output | Complement state. |

Internals:

- `NAND_Q`
- `NAND_Q_BAR`
- Cross-coupled feedback wires.

Behavior:

| `S_BAR` | `R_BAR` | Result |
| --- | --- | --- |
| 1 | 1 | Hold previous state. |
| 0 | 1 | Set `Q=1`, `Q_BAR=0`. |
| 1 | 0 | Reset `Q=0`, `Q_BAR=1`. |
| 0 | 0 | Invalid active-low latch state; both outputs go high. |

### `GatedDLatch`

Files:

- `include/modules/basic/Latch.hpp`
- `src/modules/basic/Latch.cpp`

Pins:

| Pin | Direction | Meaning |
| --- | --- | --- |
| `D` | input | Data input. |
| `EN` | input | Level-sensitive enable. |
| `RST` | input | Active-high reset. |
| `Q` | output | Stored state. |
| `Q_BAR` | output | Complement state. |

Internals:

- `NOT_D`
- `NOT_RST`
- `S_DATA_BAR`
- `R_DATA_BAR`
- `S_RESET_GATE`
- `R_RESET_GATE`
- `STATE` (`SRLatch`)

Behavior:

```text
if RST = 1:
    Q = 0
else if EN = 1:
    Q follows D after gate delay
else:
    Q holds previous state
```

### `DFlipFlop`

Files:

- `include/modules/basic/DFlipFlop.hpp`
- `src/modules/basic/DFlipFlop.cpp`

Pins are unchanged:

| Pin | Direction | Meaning |
| --- | --- | --- |
| `D` | input | Data input. |
| `CLK` | input | Rising-edge clock. |
| `RST` | input | Active-high reset. |
| `Q` | output | Stored state. |
| `Q_BAR` | output | Complement state. |

Internals:

- `NOT_CLK`
- `MASTER` (`GatedDLatch`)
- `SLAVE` (`GatedDLatch`)

Clocking structure:

```text
D -> MASTER.D
CLK -> NOT_CLK -> MASTER.EN
CLK -----------> SLAVE.EN
MASTER.Q ------> SLAVE.D
SLAVE.Q -------> Q
SLAVE.Q_BAR ---> Q_BAR
RST -----------> MASTER.RST and SLAVE.RST
```

Behavior:

- When `CLK=0`, the master latch is transparent and samples `D`; the slave latch holds.
- When `CLK=1`, the master latch holds; the slave latch is transparent and publishes the master's stored value.
- The low-to-high `CLK` transition therefore transfers the last low-phase `D` value to `Q`.
- `RST=1` clears both master and slave latch state to low.

## Important Implementation Detail

Input fanout must use one wire per external input pin.

For example, `CLK` must be connected to both `NOT_CLK.IN` and `SLAVE.EN` using a single builder fanout:

```cpp
builder.wire("CLK_internal")
    .fromInput("CLK")
    .to<NOTGate>("NOT_CLK", "IN")
    .to<GatedDLatch>("SLAVE", "EN");
```

Creating two separate `fromInput("CLK")` wires overwrites the input pin's internal connection in the current pin model. The same rule applies to `D` and `RST` fanout inside `GatedDLatch`.

## Test Coverage

Added/updated structural sequential tests:

- `SRLatchTest`
- `GatedDLatchTest`
- `DFlipFlopTest`
- `MemoryBitStructuralContractTest`
- `FullCircuitTest`

Coverage includes:

- SR latch set, reset, hold, and invalid active-low input behavior.
- D latch reset, disabled hold, enabled transparent high/low, unknown propagation, and reset clearing unknown.
- DFF reset, pre-edge hold, rising-edge capture, no high-level capture, no falling-edge capture, unknown capture, and reset clearing unknown.
- MemoryBit write-enable behavior after switching its internal `STATE` DFF to the structural implementation.
- Full integration circuit with reset initialization and a clock low phase before meaningful rising-edge capture.

## Visualizer Exposure

Registered scenarios:

- `sr-latch` -> `SRLatchTest`
- `gated-d-latch` -> `GatedDLatchTest`
- `dff` -> `DFlipFlopTest`
- `memory-bit` -> `MemoryBitStructuralContractTest`

Hosted visualizer verification after restart:

| Scenario | Root type | Components | Pins | Wires | Timestamps |
| --- | --- | ---: | ---: | ---: | ---: |
| `sr-latch` | `SRLatch` | 3 | 10 | 8 | 18 |
| `gated-d-latch` | `GatedDLatch` | 10 | 31 | 20 | 50 |
| `dff` | `DFlipFlop` | 22 | 69 | 42 | 61 |
| `memory-bit` | `MemoryBit` | 28 | 89 | 55 | 129 |

## Verification Results

Commands run:

```bash
cmake --build build -j 4
ctest --test-dir build --output-on-failure -R "SRLatchTest|GatedDLatchTest|DFlipFlopTest|MemoryBitStructuralContractTest|FullCircuitTest|ClockGeneratorTest"
ctest --test-dir build --output-on-failure
```

Results:

- Build passed.
- Focused sequential subset passed: 6/6 in 0.08 seconds.
- Full CTest passed: 72/72 in 202.44 seconds.
- Hosted visualizer restarted and scenario discovery includes `sr-latch`, `gated-d-latch`, `dff`, and `memory-bit`.

## Known Limits

- This is still digital gate-level behavior, not transistor-level behavior.
- Analog metastability is not modeled.
- Setup/hold timing violations are represented only through ordinary digital `UNKNOWN` propagation.
- The structural DFF is larger than the old behavioral primitive, so components built from many DFFs will grow quickly in the visualizer.
