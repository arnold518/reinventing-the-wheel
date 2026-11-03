# § 5.4.3. Console

Text out of the machine. The program writes bytes; a host (the recording,
the visualizer's console panel) takes them from the device's output pins.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `ConsoleDevice` | A byte queue behind three registers | Yes | Yes |

Catalog: `device.console`; parameter `fifo_depth` 1–7 (default 4).

## IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset of the device's state |
| `REQ_VALID`, `REQ_OP`, `REQ_ADDR`, `REQ_WRITE_DATA`, `REQ_WRITE_MASK`, `RSP_READY` | Input | 1, 3, 32, 32, 4, 1 | Register requests (offsets in the device's window) |
| `REQ_READY`, `RSP_VALID`, `RSP_READ_DATA`, `RSP_STATUS` | Output | 1, 1, 32, 3 | Register answers |
| `TX_VALID`, `TX_DATA` | Output | 1, 8 | The oldest queued byte, offered to the host |
| `TX_READY` | Input | 1 | The host takes the byte at this edge |

## Registers

| Offset | Register | Access | Bits | Meaning |
| --- | --- | --- | ---: | --- |
| `0x0` | `TX` | write-only data | 8 | Queue one byte |
| `0x4` | `STATUS` | read-only | 1 | Bit 0 `ROOM`: the queue has room |
| `0x8` | `COUNT` | read-only | 3 | Bytes waiting |

## Behavior

A write to `TX` queues the low byte. When the queue is full, the write
waits (the register port's `BACK_READY` is low) until the host takes a
byte; a program that must not block reads `STATUS` first. Reads have no side
effect.

The accepted write stays pending while full; the byte is pushed exactly
once when space is available. Holding `TX_READY = 1` consumes one offered
byte at every rising edge with `TX_VALID = 1`. A full queue that is popped
on an edge becomes writable afterwards, so the waiting write need not
complete on that same pop edge. Reset discards queued bytes and any pending
register transaction, making `COUNT = 0` and `TX_VALID = 0`.

`TX` accepts a full-word or zero-mask store through the register port;
it is not a byte-addressed register. Values above `0xff`, or nonzero-mask
payloads containing `X`/`Z`, are Invalid and do not enqueue a byte. Reading
TX is Unsupported; writing STATUS or COUNT is ReadOnly. Empty `TX_DATA` is
zero, but a host should use `TX_VALID` to decide whether it is a byte.

```c
void console_write(const char *s) {
    while (*s) {
        while (!io_read(CONSOLE_STATUS)) {}   /* wait for room */
        io_write(CONSOLE_TX, (uint8_t)*s++);
    }
}
```

## Implementation

The structural form shows the grouped circuit. The compact form runs
compiled versions of the same groups with characterized storage, register and
queue models; control delays come from the structural groups. Contract tests
compare both forms; the `visual` preset selects the compact form.

| Part | Contents |
| --- | --- |
| `PORT` | `DeviceRegisterPort<32>` |
| `REGISTERS` | The front end of the table above |
| `TX_FIFO` | `SynchronousFifo` of bytes; `TX_WRITE` pushes, `TX_READY` pops |
| `TX_BYTE` | The low byte of the write data |

**Timing.** The port captures a request, then the FIFO accepts its qualified
write on an execution edge. Its response is held independently of later
host consumption. The port and byte FIFO have stored default delay tables
([register port](registers.md#implementation), [queue](state.md#implementation_1));
the parameterized register/decode groups derive their own delays at build
time. There is no extra scalar whole-console delay. Tested protocol traffic
uses binary handshakes after reset; the payload rejection above does not
specify behavior for unknown clock or `TX_READY` edges.

## Examples

<!-- simulator-examples: console-device ConsoleDeviceScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=console-device&amp;embed=1" title="ConsoleDevice simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=console-device" class="md-button">Open the full visualizer</a>

## Source and tests

[Device interface](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/device/DeviceComponents.hpp),
[console/FIFO wiring](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceState.cpp),
[register definitions](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceRegisters.cpp),
[verification](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp).

| Test | Coverage |
| --- | --- |
| `ConsoleDeviceTest` | Queue order, full queue and room, configured depths 1, 3 and 7, a value wider than a byte refused (compact form) |
| `ConsoleDeviceScenarioTest` | The recorded example, structural and compact forms compared |
