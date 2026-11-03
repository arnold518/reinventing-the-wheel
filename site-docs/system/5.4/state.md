# § 5.4.2. Counter and Queue

Two small clocked parts hold the state inside the devices: a counter (the
timer, the LED blink) and a first-in, first-out queue (console bytes, button
events).

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `ClockedCounter` | Counts enabled clock edges, optionally every n-th | Yes | Yes (4 and 32 bits) |
| `SynchronousFifo` | A queue of 1–7 words with sticky overflow | Yes | Yes (8 and 32 bits) |

## 5.4.2.1. `ClockedCounter`

Catalog: `device.counter.W` for W = 4, 8, 16, 32; parameter `divisor`
(positive, default 1).

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset clears the count and the divider |
| `ENABLE` | Input | 1 | Low holds the count and the divider |
| `COUNT` | Output | W | Enabled ticks since reset, wrapping |
| `TICK` | Output | 1 | This edge will increment the count |

### Behavior

`COUNT` increments on every `divisor`-th enabled rising edge and wraps at
2^W. `TICK` is a signal, not a second clock: there is one clock in the
machine.

With divisor 1, `TICK = ENABLE && !RST` and can stay high between many
edges; it is an increment enable, not a narrow pulse. With a larger divisor,
the divider phase advances only on enabled edges. Pausing preserves that
phase as well as `COUNT`, so resuming does not restart the interval. Reset
clears both; divisor 0 is rejected at construction.

#### C++ reference

```cpp
#include <cstdint>

template<unsigned W>
struct CounterRef {
    static_assert(W == 4 || W == 8 || W == 16 || W == 32);
    uint32_t count = 0, phase = 0;
    bool tick(bool reset, bool enable, uint32_t divisor = 1) const {
        return !reset && enable && phase == divisor - 1;
    }
    void edge(bool reset, bool enable, uint32_t divisor = 1) {
        if (reset) { count = phase = 0; return; }
        if (!enable) return;
        if (phase == divisor - 1) {
            phase = 0;
            count = (count + 1) & (UINT32_MAX >> (32 - W));
        } else ++phase;
    }
};
```

This models binary edge transitions with a fixed positive divisor. It does
not model reset's propagation between edges or uncertain clocks/enables.

### Implementation

`TICK_CONTROL` makes the enable (with phase state, a terminal comparison and
an advance group when `divisor` > 1); `COUNT_STATE` holds the count and
`INCREMENT` adds one with the adders of § 2.

The compact 4- and 32-bit counters execute their own compiled gates. Other
widths retain their structural assembly even when compact children are
preferred. Parameterized divider logic changes the circuit and is timed for
that configuration.

**Timing.** A count change follows an enabled rising edge after the
register's propagation; `TICK` can also change combinationally with `ENABLE`.
The following stored paths are for the default divisor 1, with reset low;
they do not characterize every positive divisor or certify an enclosing
timer's clock.

<!-- timing-rows
4-bit edge to count | device.counter.4.behavioral | CLK ↑ → COUNT
32-bit edge to count | device.counter.32.behavioral | CLK ↑ → COUNT
4-bit enable to tick | device.counter.4.behavioral | ENABLE → TICK
32-bit enable setup | device.counter.32.behavioral | ENABLE before CLK ↑
-->

### Examples

<!-- simulator-examples: clocked-counter ClockedCounterScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=clocked-counter&amp;embed=1" title="ClockedCounter simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=clocked-counter" class="md-button">Open the full visualizer</a>

## 5.4.2.2. `SynchronousFifo`

Catalog: `device.fifo.W` for W = 8, 16, 32; parameter `depth` 1–7
(default 4).

### IO pins

| Pin | Direction | Width | Meaning |
| --- | --- | ---: | --- |
| `CLK`, `RST` | Input | 1 each | Clock; reset empties the queue and clears overflow |
| `PUSH`, `PUSH_DATA` | Input | 1, W | Append a word |
| `POP` | Input | 1 | Remove the oldest word |
| `CLEAR_OVERFLOW` | Input | 1 | Clear the lost-word flag |
| `DATA` | Output | W | The oldest word (0 when empty) |
| `VALID`, `FULL`, `PUSH_READY` | Output | 1 each | Not empty; full; a push would be kept |
| `COUNT` | Output | 3 | Words held |
| `OVERFLOW` | Output | 1 | A push was dropped since the last clear |

### Behavior

At a rising edge, `POP` with `VALID` removes the head and `PUSH` with
`PUSH_READY` appends. A push to a full queue is dropped and sets
`OVERFLOW`. When full, a simultaneous pop and push pops and drops the push;
when empty, a simultaneous pop and push keeps the new word.

For a partly full queue, simultaneous pop and push preserve its count and
replace the old head with the next word in FIFO order. A new dropped push
wins over `CLEAR_OVERFLOW` on the same edge; otherwise that input clears
the sticky flag. Empty `POP` is ignored. Reset makes `COUNT = 0`, `VALID = 0`,
`DATA = 0` and clears overflow; old slot contents cannot be read as a valid
word. All decisions use occupancy before the edge, which explains why a
full queue cannot accept a simultaneous replacement.

#### C++ reference

```cpp
#include <cstdint>
#include <deque>
#include <stdexcept>

template<unsigned W>
struct FifoRef {
    static_assert(W == 8 || W == 16 || W == 32);
    unsigned depth;
    std::deque<uint32_t> words;
    bool overflow = false;
    explicit FifoRef(unsigned capacity) : depth(capacity) {
        if (depth < 1 || depth > 7) throw std::invalid_argument("depth");
    }
    uint32_t data() const { return words.empty() ? 0 : words.front(); }
    void edge(bool reset, bool push, uint32_t value, bool pop, bool clear) {
        if (reset) { words.clear(); overflow = false; return; }
        const bool full = words.size() == depth, empty = words.empty();
        if (pop && !empty) words.pop_front();
        if (push && !full) words.push_back(value & (UINT32_MAX >> (32 - W)));
        overflow = (push && full) || (overflow && !clear);
    }
};
```

This binary reference uses a host queue only to describe accepted words;
the component uses registers, decoders and muxes. Arbitrary `X`/`Z` queue
controls are outside this integer reference.

### Implementation

`CONTROL` holds the count and overflow; `POINTERS` the head and tail;
`STORAGE` the words, a write decoder and the read mux.

Pointers wrap at the configured depth, including non-powers of two. The
compact forms exist at W = 8 and 32 and compile this same circuit; W = 16
is structural. The read selection is gated by `VALID`, so an empty queue
returns zero rather than a stale slot.

**Timing.** Acceptance is decided from the pre-edge `FULL`/`VALID`; count,
head data and overflow settle after the edge. The selected stored paths are
for depth 4 and reset low. Other depths alter pointer wrap/decode paths and
must use their own derived timing rather than these default rows.

<!-- timing-rows
Byte queue edge to head | device.fifo.8.behavioral | CLK ↑ → DATA
Word queue edge to head | device.fifo.32.behavioral | CLK ↑ → DATA
Push data setup, byte queue | device.fifo.8.behavioral | PUSH_DATA before CLK ↑
Pop setup, word queue | device.fifo.32.behavioral | POP before CLK ↑
-->

### Examples

<!-- simulator-examples: synchronous-fifo SynchronousFifoScenarioTest -->

<div class="simulator-embed">
  <iframe src="../../../simulator/?scenario=synchronous-fifo&amp;embed=1" title="SynchronousFifo simulation" loading="lazy" allowfullscreen></iframe>
</div>

<a href="../../../simulator/?scenario=synchronous-fifo" class="md-button">Open the full visualizer</a>

## Source and tests

[Interfaces](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/include/modules/device/DeviceComponents.hpp),
[counter, FIFO and compact registrations](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/modules/device/DeviceState.cpp),
[functional and queue-model checks](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMPhase3Tests.cpp).

| Test | Coverage |
| --- | --- |
| `ClockedCounterTest` | Increment and pause at 4/8/16/32 bits (20 edges, so wrap is exercised at 4 bits); divisor 3 at 4 bits; structural and available compact forms |
| `SynchronousFifoTest` | Order, count, full, overflow, simultaneous push/pop, reset; depths 1–7 at W = 8/32 against a queue model with deterministic push/pop patterns; both forms |
