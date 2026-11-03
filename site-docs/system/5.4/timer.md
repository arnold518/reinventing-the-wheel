# § 5.4.4. Timer

Time, as the machine knows it: a count of its own clock ticks. Games use it
to pace themselves; it has no connection to the host's clock.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `TimerDevice` | A 32-bit counter behind three registers | Yes | Yes |

Catalog: `device.timer`; parameters `divisor` (clock edges per tick,
default 1) and `frequency_hz` (a declared clock frequency, default 0 =
undeclared).

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset of the device's state |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` | Input | 1, 3, 32, 32, 4, 1 | Register requests (offsets in the device's window) |
| `REQ_READY`, `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Output | 1, 1, 32, 3 | Register answers |

## Registers

| Offset | Register | Access | Bits | Meaning |
| --- | --- | --- | ---: | --- |
| `0x0` | `COUNT` | read-only | 32 | Ticks since reset |
| `0x4` | `DIVISOR` | setting | 32 | Clock edges per tick |
| `0x8` | `FREQUENCY_HZ` | setting | 32 | Declared frequency; 0 when undeclared |

## Behavior

`COUNT` advances by one every `DIVISOR` rising edges. A read samples it at
the register port's execute edge and keeps that value while the answer
waits. All three registers are read-only: a write answers ReadOnly. A zero
`FREQUENCY_HZ` means the machine was built without declaring its clock; it
is never guessed from the host's speed.

The counter wraps modulo 2^32; reset clears both count and divider phase.
It runs independently of CPU `ENABLE`: pausing the processor does not pause
this device. `FREQUENCY_HZ` describes the input clock, so when it is declared,
the intended count rate is `frequency_hz / divisor`, not `frequency_hz`
unconditionally. The setting neither creates a clock nor changes gate
delays. Divisor 0 is rejected when constructing the device.

A program can measure a bounded interval across wrap with unsigned
subtraction, `uint32_t elapsed = now - before`; it must sample often enough
that it does not miss a whole counter wrap. There is no compare register,
alarm, interrupt output or host wall-clock synchronization.

## Implementation

The structural form shows the grouped circuit. The compact form runs
compiled versions of the same groups with characterized storage, register and
queue models; control delays come from the structural groups. Contract tests
compare both forms; the `visual` preset selects the compact form.

| Part | Contents |
| --- | --- |
| `PORT` | `DeviceRegisterPort<32>` |
| `REGISTERS` | The front end; `DIVISOR` and `FREQUENCY_HZ` are constants inside it |
| `COUNTER` | `ClockedCounter<32>` with the divisor, always enabled |

**Timing.** COUNT is sampled at the register execution edge; a stalled
response holds that sample even while the counter continues. The default
counter table describes divisor 1. A different divisor adds phase, terminal
comparison and increment logic, whose delays must be derived for that
configuration. The timer is a composition of timed parts, with no separate
whole-timer table. Read-only values can carry structural unknown bits; the
numeric timer checks use binary clocks and requests after reset.

## Examples

The timer runs with divisor 4 and a declared 222 MHz clock (the 4.5 ns
machine): `DIVISOR` reads 4 and `FREQUENCY_HZ` 222000000, writes to either
are refused (ReadOnly), and `COUNT` advances once every four clock edges, so
two reads a few cycles apart can return the same value.

<!-- simulator-examples: timer-device TimerDeviceScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=timer-device&amp;embed=1" title="TimerDevice simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=timer-device" class="md-button">Open the full visualizer</a>

## Source and tests

[Counter and timer interface](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/device/DeviceComponents.hpp),
[timer wiring](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceState.cpp),
[register definitions](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceRegisters.cpp),
[verification](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp).

| Test | Coverage |
| --- | --- |
| `TimerDeviceTest` | Advance, a read held under backpressure, the settings, a write refused as read-only (compact form) |
| `TimerDeviceScenarioTest` | The recorded example, structural and compact forms compared |
