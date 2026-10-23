# Device I/O components

Transport components support 8-, 16- and 32-bit data through the common
memory request/response protocol. The game platform and peripheral register
banks use its 32-bit MMIO ABI.
They support predetermined external pin events and retain responses under
backpressure. The [RV32IM device system](../rv32im/device-system.md) connects
them to a CPU and separate memories; the [recorded runner](../rv32im/recorded-games.md)
supplies scripted input and captures presentations. Live input transport is
a later mode.

Recorded component scenarios use descriptive roots and show the structural
assembly by default. The explorer triangle selects another available
implementation; **Apply & Run** rebuilds the circuit and its recorded timeline.
Use **Device views** to switch between the display/input previews and circuit
inspection.

## Address map

The [data router](address-router.md) has these default windows. Each region is an aligned, non-overlapping power-of-two range. Standalone
router parameters declare the base and size; the platform/guest ABI uses the
map below. Child addresses are local offsets. RAM is the only destination intended to lead to
caches; device/video traffic uses its separate destination ports. Place the
[instruction guard](instruction-guard.md) before instruction-side memory so
speculative fetches cannot reach a device.

| Destination | Base | Bytes |
| --- | --- | --- |
| RAM | `0x00000000` | `0x40000` |
| Console | `0x10000000` | `0x100` |
| Timer | `0x10001000` | `0x100` |
| Buttons | `0x10002000` | `0x100` |
| Display control | `0x10003000` | `0x100` |
| Video storage | `0x20000000` | Configured capacity; default `0x1000` |

These windows share the CPU's address space, not one storage array. RAM holds
program data; each peripheral owns its counters, registers or queues. Video
storage is a separate memory endpoint. The router turns a global video address
such as `0x20000000` into local offset `0`; the high base does not allocate
512 MB of RAM. Gaps between windows require no backing storage.

## Components

- Bus: [`AddressRouter<W>`](address-router.md), [`BusErrorTarget<W>`](error-target.md),
  [`InstructionAccessGuard<W>`](instruction-guard.md),
  [`DeviceRegisterPort<W>`](register-port.md).
- State: [ClockedCounter](counter.md), [SynchronousFifo](fifo.md).
- Devices: [ConsoleDevice](console.md), [TimerDevice](timer.md),
  [ButtonDevice](buttons.md).
- Display: [`IndexedPixelSelector<W>`](pixel-selector.md),
  [FramebufferDisplay](framebuffer.md).

## Shared protocol

REQ_VALID/REQ_READY and RSP_VALID/RSP_READY transfer at rising CLK edges.
Operations are READ=0 and WRITE=1. A response carries RSP_READ_DATA and
RSP_STATUS: OK=0, Alignment=1, Range=2, ReadOnly=3, Unsupported=4, Device=5,
InvalidRequest=6. Bursts and flash operations are rejected before devices;
they must not enter an arbiter that expects several response beats.

Register-bank reads are aligned full words with no pop side effect. Commands
use full mask `0xf`; zero mask is a no-op, and other nonzero masks are invalid.
Pixel storage uses the existing byte-mask memory contract instead. Backend
reads sample once at execution, and commands execute once per accepted
request. Software uses explicit POP/PRESENT writes for side effects.

RST is a coordinated whole-bus reset that discards pending state. Clock,
reset and handshake controls must obey their setup/hold contracts. Unknown
payload fields are rejected where the frontend validates them; arbitrary
unknown control signals retain four-state gate behavior. All devices share
the global simulation clock. Timer ticks and scripted input times have no
relationship to host wall time.

## Integration boundary

The router has no cache-facing device route. The integrated system puts
caches solely behind RAM_ and ties device clocks/resets to the system clock.
PRESENT increments an observable sequence; the runner captures settled pixels
after the accepted write. The component visualizer can show display pixels
and keyboard levels at the selected recorded time, with a toggle back to
their internal circuits. Standalone component checks and integrated
native/WASM game parity have separate verification scopes.

## Declaring device parameters in Python

The native binding uses the same catalog and parameter validation as C++:

```python
screen = circuit_backend.create_device_component(
    "device.framebuffer", "DISPLAY", {"width": "128", "height": "96"})
keys = circuit_backend.create_device_component(
    "device.buttons", "KEYBOARD", {"fifo_depth": "7"})
```

Parameter values are decimal strings, as in cache declarations. `fidelity`
selects an available structural or behavioral implementation explicitly;
unsupported combinations fail. The parameter limits and pins belong to the
individual component references. Non-default visual configurations have
separate floorplan keys while inheriting the type's colors.
