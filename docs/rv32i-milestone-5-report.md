# RV32I Milestone 5 Report: Program Loader And Memory Readback

Last updated: 2026-06-02

## Status

Milestone 5 is implemented.

This milestone is not a new CPU hardware block. It is bring-up infrastructure for the RV32I runner:

```text
raw RV32I program words or bytes
  -> RV32IProgram
  -> preload into BehavioralMemory64Kx32 before simulation
  -> read memory back in tests after simulation
```

The CPU still needs a program counter, instruction oracle, datapath components, and a top-level system. Milestone 5 only makes it practical to load and inspect programs without simulating thousands of setup writes through memory pins.

Visualizer note:

The `rv32i-program-loader` scenario exposes a preloaded `BehavioralMemory64Kx32` root so the new test is available from the visualizer. The visualizer shows the memory component boundary and pins; byte contents are still inspected through C++ tests/debug helpers.

## Implemented Files

Program loader:

- `include/rv32i/RV32IProgram.hpp`
- `src/rv32i/RV32IProgram.cpp`

Memory preload/readback API:

- `include/modules/memory/BehavioralMemory64Kx32.hpp`
- `src/modules/memory/BehavioralMemory64Kx32.cpp`

Tests:

- `include/tests/RV32IProgramTests.hpp`
- `src/tests/RV32IProgramTests.cpp`

Build and registry:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `src/tests/TestRegistry.cpp`
- `src/bindings/TestsBinding.cpp`
- `visualizer-v2/server.py`

## Why This Milestone Exists

The first RV32I CPU will fetch instructions from `BehavioralMemory64Kx32`.

Without preload helpers, a test would need to drive memory pins and clocks once per byte, halfword, or word before it can run the actual CPU. That would make program tests slow, noisy, and hard to read.

Milestone 5 gives tests a direct setup path:

```cpp
auto memory = Component::create<BehavioralMemory64Kx32>("IMEM");
auto program = rv32i::RV32IProgram::fromWords({
    0x00100093U, // addi x1, x0, 1
    0x00208113U, // addi x2, x1, 2
    0x00100073U, // ebreak
});

program.loadInto(*memory, 0);
```

After execution, tests can inspect memory directly:

```cpp
auto word = memory->readWord(0x100);
auto bytes = memory->readBytes(0x200, 16);
```

This keeps setup and assertions clear while the CPU datapath remains the thing being simulated.

## `RV32IProgram`

`RV32IProgram` is a raw byte container for instruction/data fixtures.

It supports two construction paths:

```cpp
auto from_bytes = rv32i::RV32IProgram::fromBytes({0x93, 0x00, 0x10, 0x00});
auto from_words = rv32i::RV32IProgram::fromWords({0x00100093U});
```

RV32I is little-endian, so `fromWords({0x00100093})` stores these bytes:

```text
address + 0: 0x93
address + 1: 0x00
address + 2: 0x10
address + 3: 0x00
```

Public behavior:

| API | Behavior |
| --- | --- |
| `fromBytes(bytes)` | Stores exactly the given bytes. |
| `fromWords(words)` | Converts each 32-bit word to little-endian bytes. |
| `bytes()` | Returns the raw stored byte vector. |
| `sizeBytes()` | Returns byte count. |
| `empty()` | Reports whether the program has no bytes. |
| `byteAt(index)` | Returns one byte, or throws `std::out_of_range`. |
| `wordAt(word_index)` | Reads one complete little-endian 32-bit word, or throws `std::out_of_range`. |
| `loadInto(memory, base_address)` | Calls `BehavioralMemory64Kx32::loadBytes`. |

Important rule:

`wordAt()` only accepts complete words. If a program has five bytes, `wordAt(0)` is valid and `wordAt(1)` is rejected because only one byte remains.

## `BehavioralMemory64Kx32` Setup API

The existing memory component now has direct setup/readback helpers:

```cpp
static constexpr size_t capacityBytes();
static constexpr size_t capacityWords();

void clearContents();
void loadBytes(uint32_t base_address, const std::vector<uint8_t>& data);
void loadWords(uint32_t base_address, const std::vector<uint32_t>& words);
std::vector<uint8_t> readBytes(uint32_t base_address, size_t count) const;
uint8_t readByte(uint32_t address) const;
uint32_t readWord(uint32_t address) const;
```

Capacity:

| Property | Value |
| --- | --- |
| Words | 64K 32-bit words |
| Bytes | 256 KiB |
| Valid byte range | `0x00000000` through `0x0003ffff` |

### Loading Bytes

`loadBytes()` accepts any byte address.

Example:

```cpp
memory->loadBytes(3, {0xaa, 0xbb, 0xcc});
```

This writes:

```text
address 3 = 0xaa
address 4 = 0xbb
address 5 = 0xcc
```

It throws `std::out_of_range` if the byte range crosses the memory end.

### Loading Words

`loadWords()` requires a 4-byte-aligned base address.

Example:

```cpp
memory->loadWords(16, {0x11223344U});
```

This writes bytes:

```text
address 16 = 0x44
address 17 = 0x33
address 18 = 0x22
address 19 = 0x11
```

It throws:

- `std::invalid_argument` for unaligned base addresses.
- `std::out_of_range` for ranges that cross the memory end.

### Reading Back

`readByte()` reads one byte.

`readBytes()` reads a byte range.

`readWord()` requires a 4-byte-aligned address and assembles bytes as little-endian:

```text
bytes [0x44, 0x33, 0x22, 0x11] -> word 0x11223344
```

These helpers are intended for tests, fixture setup, and debug inspection. Normal CPU execution should still use the memory pins and simulator timing.

## Test Coverage

New test:

- `RV32IProgramLoaderTest`

Covered behavior:

- `RV32IProgram::fromWords` little-endian conversion.
- Program byte count and empty state.
- `byteAt()` success and out-of-range rejection.
- `wordAt()` success and trailing partial-word rejection.
- Memory capacity constants.
- Zero-initialized memory readback.
- Byte-granular `loadBytes()` at an unaligned base.
- Little-endian `loadWords()` and `readWord()`.
- Program preload into memory at a nonzero base address.
- Final valid byte-range write at the end of memory.
- Out-of-range load/read rejection.
- Unaligned word load/read rejection.
- `clearContents()` clearing previously loaded program bytes.

Focused verification command:

```bash
ctest --test-dir build --output-on-failure -R "RV32IProgramLoaderTest|RV32IControlTest|RV32IDecoderTest|BehavioralMemory64Kx32Test"
```

Result:

- Passed: 4/4
- Time: 0.39 seconds

Full regression command:

```bash
ctest --test-dir build --output-on-failure
```

Result:

- Passed: 87/87
- Time: 558.75 seconds

## What This Does Not Implement

Milestone 5 does not implement:

- ELF loading.
- Assembly compilation.
- A functional RV32I instruction oracle.
- CPU fetch/decode/execute.
- Program counter behavior.
- Instruction memory timing beyond the existing memory component.
- Data-memory checks after real CPU execution.

Those belong to later milestones.

## Next Step

The next practical milestone is a functional RV32I instruction oracle.

That instruction oracle should use `RV32IProgram`, `RV32IDecoder`, `RV32IControl`, and byte-addressed memory rules to run small programs without components. It will become the correctness oracle for the later component CPU.
