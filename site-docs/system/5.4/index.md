# § 5.4. Devices

A device is how the computer reaches the outside world: text to a host, a
clock, keys, a screen, lights. To the processor every device looks the same
— a window of **registers** on the bus, read and written with ordinary loads
and stores. Writing a console's TX register sends a byte; reading the
buttons' COUNT register says how many key events are waiting.

<!-- diagram: device-shape -->

Every device here has the same three layers:

1. **The register port** (`DeviceRegisterPort<32>`), shared by all devices:
   it takes one bus request at a time, holds it, and returns the answer.
2. **The register front end**, generated from the device's **register
   table**: it decodes the offset, rejects wrong accesses with a status,
   picks the value a read returns, and gives the core one strobe per
   writable register.
3. **The device core**: the only part that differs between devices — a queue,
   a counter, button levels, a frame counter, LEDs — and its pins to the
   outside.

| Section | Coverage |
| --- | --- |
| [5.4.1. Registers](registers.md) | Register tables, `DeviceRegisterPort<W>`, the register front end |
| [5.4.2. Counter and Queue](state.md) | `ClockedCounter`, `SynchronousFifo`: the state inside devices |
| [5.4.3. Console](console.md) | `ConsoleDevice`: bytes to a host |
| [5.4.4. Timer](timer.md) | `TimerDevice`: a tick count |
| [5.4.5. Buttons](buttons.md) | `ButtonDevice`: key levels and an event queue |
| [5.4.6. Display](display.md) | `FramebufferDisplay`: pixel memory and frame presentation |
| [5.4.7. LED Panel](leds.md) | `LedPanel`: sixteen lights, added to a machine only through its description |

## A device from the program's side

The games use the header generated from the machine's description
([§ 5.5](../5.5.md#the-guest-header)). Waiting for a key press and
consuming it is three register accesses:

```c
#include "platform.h" /* io_read/io_write and the generated system.h addresses */

uint32_t next_event(void) {
    while (io_read(BUTTONS_COUNT) == 0) {}      /* poll until an event waits */
    uint32_t event = io_read(BUTTONS_HEAD);     /* button in bits 4..0, bit 8 = press */
    io_write(BUTTONS_POP, 1);                   /* remove it from the queue */
    return event;
}
```

`io_read` and `io_write` are a volatile 32-bit load and store. Nothing else
is needed: there are no interrupts, so a program polls.

## Fidelity and the visualizer

Every device has a structural form (gates and registers) and a compact
form that runs compiled versions of the same groups with characterized
storage; contract tests compare them. In the visualizer a compact or
collapsed device is drawn as what it is — a screen, a keyboard, a console, a
row of lights — and an expanded structural device shows its circuit. The
device scenarios open in the `logic` profile, which shows the circuit with
its registers as compact storage; the `visual` profile shows the pictures,
and `gate` builds even the registers from flip-flops.
