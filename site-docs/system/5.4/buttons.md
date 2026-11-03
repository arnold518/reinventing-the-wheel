# § 5.4.5. Buttons

Input from a player: 32 logical buttons. The device keeps every button's
current level and a queue of press and release events, so a short tap
between two polls is not lost.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `ButtonDevice` | Button levels, an event queue, six registers | Yes | Yes |

Catalog: `device.buttons`; parameter `fifo_depth` 1–7 (default 4).

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset of the device's state |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` | Input | 1, 3, 32, 32, 4, 1 | Register requests (offsets in the device's window) |
| `REQ_READY`, `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Output | 1, 1, 32, 3 | Register answers |
| `INPUT_VALID` | Input | 1 | An input event at this edge |
| `INPUT_BUTTON`, `INPUT_DOWN` | Input | 5, 1 | Which button; pressed (1) or released (0) |

## Registers

| Offset | Register | Access | Bits | Meaning |
| --- | --- | --- | ---: | --- |
| `0x00` | `LEVELS` | read-only | 32 | Bit n: button n is held |
| `0x04` | `HEAD` | read-only | 32 | Oldest event: `BUTTON` in bits 4..0, `DOWN` in bit 8 |
| `0x08` | `COUNT` | read-only | 3 | Events waiting |
| `0x0C` | `STATUS` | read-only | 2 | Bit 0 `VALID`: an event waits; bit 1 `OVERFLOW`: an event was lost |
| `0x10` | `POP` | command | 1 | Remove the oldest event |
| `0x14` | `CLEAR_OVERFLOW` | command | 1 | Clear `OVERFLOW` |

## Behavior

At an edge with `INPUT_VALID`, the button's level becomes `INPUT_DOWN` and
the event is queued. A full queue drops the event (setting `OVERFLOW`) but
still updates the level. Reading `HEAD` never removes it; `POP` does.

Each edge with `INPUT_VALID = 1` is a new event, even if the button's level
has not changed. There is no debouncing or edge detector. Deassert VALID
between events unless repeated events are intended. Reset clears all levels,
empties the event queue and clears overflow.

An empty HEAD reads zero; use `STATUS.VALID` or COUNT to distinguish that
from a button-0 release. POP on an empty queue and either command written
with value 0 do nothing. Commands accept only 0 or 1; reading one is
Unsupported. A full queue's simultaneous POP and new input drops the new
event and raises overflow, following the queue's pre-edge occupancy rule;
the level still changes. A new overflow wins over CLEAR_OVERFLOW that edge.

### Example

Button 2 is pressed and released between two polls of the program:

| Step | `LEVELS` | `COUNT` | `HEAD` |
| --- | ---: | ---: | ---: |
| After the press | `0x4` | 1 | `0x102` |
| After the release | `0x0` | 2 | `0x102` |
| After one `POP` | `0x0` | 1 | `0x002` |

## Implementation

The structural form shows the grouped circuit. The compact form runs
compiled versions of the same groups with characterized storage, register and
queue models; control delays come from the structural groups. Contract tests
compare both forms; the `visual` preset selects the compact form.

| Part | Contents |
| --- | --- |
| `PORT` | `DeviceRegisterPort<32>` |
| `REGISTERS` | The front end; `POP_WRITE` and `CLEAR_OVERFLOW_WRITE` drive the queue |
| `LEVELS` | A `Register<32>` written on every input event |
| `INPUT_UPDATE` | One `Decoder<5>` selects the button; `NEXT = (LEVELS & ~SELECT) \| (SELECT & DOWN)` with three word gates |
| `EVENT_ENCODING` | Packs the button and the press bit |
| `EVENTS` | `SynchronousFifo` of 32-bit events |
| `STATUS_BITS` | `VALID` and `OVERFLOW` side by side |

**Timing.** Input pins must settle before the edge that captures the event
and updates LEVELS. Register reads sample once and hold their response, so
later input events do not rewrite a waiting HEAD or LEVELS answer. The queue
and register port have their own timing; the level-update and register-table
groups derive parameter-specific compiled delays. There is no whole-button
delay table. Unknown external button/handshake fields follow their structural
decoder and state paths; the event-order tests use known binary input events.

## Visual abstraction

A compact or collapsed button device is drawn as 32 keys — held, released
or unknown — at the selected recorded time, with the queue's occupancy,
head event and overflow in the inspector. The game platform names the
logical buttons (0 up, 1 right, 2 down, 3 left, 4 action, 5 restart, 6 update,
7–8 player two, 9 pause); the device itself knows only numbers.

## Examples

A press and a release go into the queue while `LEVELS` follows the button;
the program-side reads see the head event, the count and the status, and pop
both events. Then five presses arrive on five edges into the four-place
queue: the fifth is lost, `STATUS` shows `VALID` and `OVERFLOW` (3), the
queue keeps the four oldest, and `LEVELS` still records all five buttons.
`CLEAR_OVERFLOW` clears the flag.

<!-- simulator-examples: button-device ButtonDeviceScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=button-device&amp;embed=1" title="ButtonDevice simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=button-device" class="md-button">Open the full visualizer</a>

## Source and tests

[Interface](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/device/DeviceComponents.hpp),
[input decoder, levels and queue](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceState.cpp),
[register definitions](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceRegisters.cpp),
[verification](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp).

| Test | Coverage |
| --- | --- |
| `ButtonDeviceTest` | Levels, a tap between polls, head reads without popping, POP, overflow keeping levels, overflow clear, depths 1, 3 and 7 (compact form) |
| `ButtonDeviceScenarioTest` | The recorded example, structural and compact forms compared |
