# § 5.4.7. LED Panel

Sixteen lights. The LED panel is the example of a device that is added to
a machine without changing any other part: its register table is two rows,
its core is a counter, and it joins a machine through four lines of the
machine's description ([§ 5.5](../5.5.md#adding-a-device)).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `LedPanel` | Two read-write registers, a blink counter, 16 LED pins | Yes | Yes |

Catalog: `device.leds`; no parameters.

<!-- diagram: led-panel -->

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset of the device's state |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` | Input | 1, 3, 32, 32, 4, 1 | Register requests (offsets in the device's window) |
| `REQ_READY`, `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Output | 1, 1, 32, 3 | Register answers |
| `LEDS` | Output | 16 | Bit n: LED n is lit |

## Registers

| Offset | Register | Access | Bits | Meaning |
| --- | --- | --- | ---: | --- |
| `0x0` | `LEDS` | read-write | 16 | Bit n: LED n is on |
| `0x4` | `BLINK` | read-write | 5 | 0: steady; k: lit LEDs blink, on and off for 2^k cycles each |

## Behavior

`LEDS` shows the `LEDS` register, gated by the blink phase: with `BLINK`
= k > 0, the lit LEDs show while bit k of a free-running cycle counter is 0.
Both registers reset to 0 (dark, steady). A value wider than its register is
refused (Invalid) and changes nothing.

Reads of the LEDS register return the programmed mask even during the dark
half of a blink; the output pins are the phase-gated view. BLINK accepts
0–31. The counter runs continuously, and writing LEDS or BLINK does not
restart its phase, so a newly enabled blink can start partway through its
on/off interval. Reset clears the two registers and counter. There is no
interrupt, brightness control or independent clock per light.

### Example

| Program | `LEDS` pins |
| --- | --- |
| `io_write(LEDS_LEDS, 0x0f0f)` | `0x0f0f` |
| `io_write(LEDS_BLINK, 2)` | `0x0f0f` for 4 cycles, `0x0000` for 4 cycles, … |
| `io_write(LEDS_BLINK, 0)` | `0x0f0f` |
| `io_write(LEDS_LEDS, 0x10000)` | unchanged; the store answers Invalid and the core traps |

## Implementation

| Part | Contents |
| --- | --- |
| `PORT` | `DeviceRegisterPort<32>` |
| `REGISTERS` | The front end, storing `LEDS` (16 bits) and `BLINK` (5 bits) |
| `BLINK_COUNTER` | `ClockedCounter<32>`, always enabled |
| `LIGHTS` | `PHASE`, a `Mux<32,1>` picking counter bit `BLINK`; one `ANDGate<16>` masking `LEDS` |

The compact form compiles the same groups, as for the other devices.

**Timing.** An accepted register write takes effect at the execution edge;
the visible pins then settle through the phase selection and output mask.
With nonzero BLINK they can also change on later counter edges without a
new bus access. The front end and LIGHTS group derive delays when built,
and the counter/port use their characterized forms. There is no stored
whole-panel table. Invalid or unknown register payloads follow the shared
front-end rules; equivalence tests exercise binary controls after reset.

## Visual abstraction

A compact or collapsed panel is drawn as a row of sixteen lights at the
selected time.

## Examples

The scenario lights `0x0F0F`, reads it back, is refused a value wider than
16 bits, and sets `BLINK` to 2. The next eight cycles show the pins: four
cycles lit, four dark (the counter keeps running from reset, so the first
interval can be shorter). Writing `BLINK` = 0 makes the pins steady again.

<!-- simulator-examples: led-panel LedPanelScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=led-panel&amp;embed=1" title="LedPanel simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=led-panel" class="md-button">Open the full visualizer</a>

## Source and tests

[Panel circuit and compact composition](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/LedPanel.cpp),
[register definitions](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceRegisters.cpp),
[panel tests and scenario](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/SystemTests.cpp).

| Test | Coverage |
| --- | --- |
| `LedPanelTest` | Write and read back, refused wide value, blink duty, steady, Range and Alignment, both forms |
| `LedPanelScenarioTest` | The recorded example |
| `GameSnakeLedsScenarioTest`, `ComputerSystemLedsScenarioTest` | Programs that drive the panel, its pins checked at every frame ([§ 5.6](../5.6.md), [§ 5.5](../5.5.md#led-panel)) |
