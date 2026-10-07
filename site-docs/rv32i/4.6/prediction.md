# § 4.6.3. Branch Prediction

Fetch predicts a next PC while Execute remains responsible for the actual outcome. Prediction changes throughput, never the architectural meaning of an instruction.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `RV32IPredictionCounter` | Stateless counter-update circuit | Yes | Yes |
| `RV32IBranchPredecode` | B/J instruction recognition and target calculation | Yes | Yes |
| `RV32IBranchPredictor` | Static policy or indexed FF table with next-PC selection | Yes | Yes |
| `RV32IPredictionEntry` | Internal row: one FF in one-bit mode, two in two-bit mode | Yes | Via predictor |
| `RV32IPredictionTable<N>` | Internal update decoder and read mux tree | Yes | Via predictor |

## Policies and configuration

| `prediction` | Conditional branch prediction | Reset / training |
| --- | --- | --- |
| `not-taken` | PC+4 | Default; no table |
| `taken` | Encoded target | No table |
| `backward-taken` | Target when the signed immediate is negative; otherwise PC+4 | No table |
| `one-bit` | Most recent outcome at this index | Reset not-taken; taken writes `11`, untaken writes `00` |
| `two-bit` | Taken when counter is `10` or `11` | Reset `00`; saturating increment/decrement |

`prediction_entries` is a power of two from 1 to 256, default 16. The table index is `(PC >> 2) & (N - 1)`. It has no address tags: different branches can intentionally alias. Static policies ignore table contents but still validate the supplied size.

All policies predict a direct JAL's encoded target. JALR uses PC+4 until Execute resolves its register-dependent target. There is no target cache, BTB, return-address stack or instruction-history predictor. Switching policy means constructing/reloading a configured core, not changing a pin while a program runs. Fidelity is an independent structural/behavioral choice.

## PredictionCounter

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CURRENT` | Input | 2 | Current state |
| `TAKEN`, `ONE_BIT` | Input | 1 each | Observed outcome and update mode |
| `NEXT` | Output | 2 | Next state; no clock or storage in this block |

### Behavior and C++ example

Two-bit states are strongly not-taken, weakly not-taken, weakly taken, strongly taken in numeric order. Saturation prevents wraparound. One-bit mode reports `00` or `11` through this two-bit interface, but each table row stores only one outcome bit.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:counter"
```

### Implementation

The structural implementation uses increment/decrement Boolean equations and direction/mode muxes. The behavioral implementation computes the same next-state function. Storage belongs to `RV32IPredictionEntry`, not this component.

### Examples

<!-- simulator-examples: rv32i-prediction-counter RV32IPredictionCounterTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-prediction-counter&amp;embed=1" title="Prediction counter transitions" loading="lazy" allowfullscreen></iframe></div>

## BranchPredecode

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `PC`, `INSTRUCTION` | Input | 32 each | Current fetch address and instruction bits |
| `CONDITIONAL`, `DIRECT_JUMP`, `BACKWARD` | Output | 1 each | Recognized branch, JAL, negative selected immediate |
| `TARGET`, `PC_PLUS_4` | Output | 32 each | Candidate target and sequential address |

### Behavior and C++ example

Recognize branch opcode `0x63` with funct3 other than reserved `010`/`011`, and JAL opcode `0x6f`. Reassemble and sign-extend the B/J immediate, then add it to PC modulo 2³². For other instructions, the selected immediate is zero, TARGET=PC, and BACKWARD=0. Full legality and target-alignment checks still happen later; predecode never raises a trap or authorizes a side effect.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:predecode"
```

### Implementation

Structural opcode matchers, field/sign-extension rewiring, muxes and adders implement the calculation. Behavioral predecode performs the same bit extraction and unsigned arithmetic. These are **Fetch** adders: prediction must run concurrently with Execute, so they cannot use Execute's busy ALU.

### Examples

<!-- simulator-examples: rv32i-branch-predecode RV32IBranchPredecodeTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-branch-predecode&amp;embed=1" title="Branch target predecode" loading="lazy" allowfullscreen></iframe></div>

## BranchPredictor

### IO pins

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Rising-edge training; reset clears table immediately |
| `PC`, `TARGET`, `PC_PLUS_4` | Input | 32 each | Lookup PC and predecoded candidate addresses |
| `CONDITIONAL`, `DIRECT_JUMP`, `BACKWARD` | Input | 1 each | Predecode results |
| `UPDATE`, `UPDATE_TAKEN` | Input | 1 each | Qualified conditional-branch training and actual outcome |
| `UPDATE_PC` | Input | 32 | Branch's own PC, not current Fetch PC |
| `NEXT_PC` | Output | 32 | Predicted target or PC+4 |
| `TAKEN` | Output | 1 | Selected target; always true for DIRECT_JUMP |
| `COUNTER` | Output | 2 | Current indexed state; zero for static policies |

### Behavior and C++ example

Lookup is combinational. Training updates one row on the rising edge when UPDATE=1 and reset is inactive. Reset clears every row to zero regardless of UPDATE. Low PC bits are ignored for indexing. Static policies never modify state; ordinary instructions always select PC+4 even if the indexed counter predicts taken.

In the code, call `evaluate` for state updates, then `lookup` for settled outputs. Capturing a prediction at the same edge as a colliding update uses the **old** table value; [PredictionPath](recovery.md#predictionpath) makes that ordering explicit.

```cpp
--8<-- "site-docs/rv32i/4.6/examples.hpp:predictor"
```

### Implementation

Structural dynamic policies instantiate `TABLE`, with numbered `ROW_n` entries and update-address decoding. A one-bit row stores one FF and uses one read-selection tree; the public two-bit counter output duplicates that bit as 00 or 11. A two-bit row stores two FFs and includes the saturating-counter update network and two read trees. Static policies omit the table entirely. The behavioral implementation replaces the selected bank with a bounded array and follows the same indexing, edge/reset, saturation, and aliasing rules.

### Examples

The embedded preview starts with a four-entry two-bit predictor. The registered contract test additionally covers all five policies at 1, 4 and 16 entries. Every preview checkpoint, including its time and number, appears below.

<!-- simulator-examples: rv32i-branch-predictor RV32IBranchPredictorTest -->
<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32i-branch-predictor&amp;embed=1" title="Indexed branch predictor" loading="lazy" allowfullscreen></iframe></div>

## Tests

`RV32IPredictionCounterTest` exhausts all 16 mode/state/outcome combinations. `RV32IBranchPredecodeTest` covers targets, wraparound, reserved encodings and JALR fallback. `RV32IBranchPredictorTest` checks saturation, reset, disabled training, independent rows, aliasing and all policies in both fidelities. `RV32IAdvancedExamplesTest` replays their actions through these exact C++ examples and rejects malformed configuration values.
