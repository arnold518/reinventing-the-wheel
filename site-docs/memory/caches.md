# § 3.7. Caches

A cache keeps copies of recently read words. A requester connects to its
upper [transaction port](transactions.md); the `MEM_` port connects to a separate
lower [memory endpoint](controllers.md) or another cache. The cache does not own the
backing memory or its initial contents.

| Component | Description | Structural | Behavioral |
| --- | --- | :---: | :---: |
| `Cache<W>` | Expanded FF/SRAM tag and data banks | Yes | No |
| `Cache<W>` with scalable banks | The same controller with scalable tag and data endpoints | Yes | No |

`W` is the number of bits in one word: 8, 16 or 32. Both forms have the same
pins and behavior. Their catalog IDs are `memory.cache.<W>` and
`memory.cache.scalable.<W>`. The scalable form changes the size of the tag
and data stores; its cache controller remains structural.

## Configuration

Choose the word width `W` and these four geometry parameters when building
the circuit. They determine where a word can be kept and how much data fits.
Addresses are always 32-bit **byte addresses**, even for a 16- or 32-bit word.

| Parameter | What it means | Default and allowed values |
| --- | --- | --- |
| `W` (C++ template width) | Bits in one word; a 32-bit word occupies four bytes. | 8, 16 or 32 |
| `sets` | Number of groups selected by the address. | Default 4; power of two, up to 4 expanded or 64 scalable |
| `ways` | Places a line can occupy within its selected set. One way is direct mapped; two ways can hold two competing lines. | Default 1; 1 or 2 |
| `block_bytes` | Bytes copied from the lower memory on a miss. This is one cache line. | Default one word (`0` selects it); 1, 2 or 4 whole words per line, with a power-of-two byte count |
| `backing_depth_words` | Size of the lower address range the cache accepts; this is **not** cache capacity. | Default 16; power of two and large enough for every cache line |

The cache holds `sets × ways × block_bytes` data bytes. For example,
`Cache<32>` with two sets, two ways and `block_bytes=8` holds
`2 × 2 × 8 = 32` data bytes: eight 32-bit words. A read miss fetches the
two words of its line through two lower READ transactions.

The remaining choices select behavior and the reusable memory parts:

| Parameter | Meaning | Choices |
| --- | --- | --- |
| `mode` | Whether the upper port may write. | `read_only` (default) or `read_write` |
| `tag_technology`, `data_technology` | Memory technology for each way's tag and data store; the two choices are independent. | `ff` (default) or `sram` |
| `storage_form` | Expanded cells or scalable stores under the same controller. | `expanded` or `scalable`, matching the catalog ID |

Each way uses one 32-bit tag endpoint and one W-bit data endpoint. Expanded
stores also obey the 4,096-cell limit of their memory components. The cache
parameters do not choose the lower memory technology; connect any compatible
lower endpoint to `MEM_`.

## 3.7.1. `Cache<W>`

### IO pins

Directions are relative to the cache. All addresses are **byte** addresses.
Connect each `MEM_` pin to the corresponding unprefixed pin of one
matching-width lower endpoint.

| Pin | Direction | Width | Description |
| --- | --- | ---: | --- |
| `CLK` | Input | 1 | Rising-edge transaction clock |
| `RST` | Input | 1 | Clear transaction and cache metadata state |
| `REQ_VALID` | Input | 1 | Offer an upper request |
| `REQ_READY` | Output | 1 | Cache can accept the upper request |
| `REQ_OP` | Input | 3 | READ or WRITE operation code |
| `REQ_ADDR` | Input | 32 | Upper byte address |
| `REQ_WRITE_DATA` | Input | W | Positioned write payload |
| `REQ_WRITE_MASK` | Input | W/8 | Enabled write byte lanes |
| `RSP_VALID` | Output | 1 | Upper response is available |
| `RSP_READY` | Input | 1 | Accept the upper response |
| `RSP_READ_DATA` | Output | W | Read result; zero for writes and errors |
| `RSP_STATUS` | Output | 3 | Common endpoint status code |
| `INV_VALID` | Input | 1 | Request all-line invalidation |
| `INV_READY` | Output | 1 | Invalidation can be accepted at the next edge |
| `MEM_CLK` | Output | 1 | Clock forwarded to lower endpoint |
| `MEM_RST` | Output | 1 | Reset forwarded to lower endpoint |
| `MEM_REQ_VALID` | Output | 1 | Offer a lower request |
| `MEM_REQ_READY` | Input | 1 | Lower endpoint can accept the request |
| `MEM_REQ_OP` | Output | 3 | Lower READ or WRITE code |
| `MEM_REQ_ADDR` | Output | 32 | Lower byte address |
| `MEM_REQ_WRITE_DATA` | Output | W | Lower write payload |
| `MEM_REQ_WRITE_MASK` | Output | W/8 | Lower write byte lanes |
| `MEM_RSP_VALID` | Input | 1 | Lower response is available |
| `MEM_RSP_READY` | Output | 1 | Accept the lower response |
| `MEM_RSP_READ_DATA` | Input | W | Lower read result |
| `MEM_RSP_STATUS` | Input | 3 | Lower status code |

### Behavior

For a READ, the address selects one set. The cache compares the address tag
with that set's valid ways:

1. **Hit:** return the stored word. The lower port receives no READ.
2. **Miss:** use an empty way if possible. Otherwise, a two-way cache replaces
   the way used least recently; a one-way cache replaces its only way.
3. Fetch every word of the line through `MEM_`, store the new tag and data,
   then return the requested word. A failed fill leaves that way invalid.

Only one upper transaction can be outstanding. A completed response stays
valid until `RSP_READY` accepts it.

In `read_write` mode, every accepted WRITE goes to the lower memory. If its
line is already cached, the selected byte lanes change there too. A write
miss does not fetch or allocate a line: this is **write-through,
no-write-allocate**. A lower write failure leaves the cached copy unchanged;
if the lower write succeeds but the cached update fails, the line is
invalidated. `read_only` mode rejects WRITE; PROGRAM and ERASE are unsupported.

`INV_VALID` clears the cache's valid lines at an idle rising edge when
`INV_READY` is high. Hold the request while busy. Reset clears cache metadata
and in-flight transactions; it does not erase the backing image.

For two levels, connect L1's `MEM_` pins to the upper pins of a matching-width
L2, then connect L2's `MEM_` pins to RAM. An L1 miss can hit in L2 without
accessing RAM. Each cache manages its own lines. The surrounding circuit must
coordinate invalidation, arbitrate multiple requesters and route device
addresses around cacheable RAM.

#### C++ example

This small reference describes the **completed binary transactions** of a
cache. `backing` stands for a separately connected memory endpoint; `Line`
groups the tag, data and valid state that the simulator builds from separate
tag/data endpoints and clocked bits. Template parameters show how word width,
sets, ways and words per line change the same algorithm. The actual cache
implements lookup and control structurally, with clocked REQ/RSP handshakes.

```cpp
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

template<std::size_t W, std::size_t Sets, std::size_t Ways,
         std::size_t WordsPerLine>
class CacheReference {
    static_assert((W == 8 || W == 16 || W == 32) && Sets > 0 &&
                  (Ways == 1 || Ways == 2) && WordsPerLine > 0);
    static constexpr std::size_t BytesPerWord = W / 8;
    struct Line {
        bool valid = false;
        std::size_t tag = 0;
        std::array<std::uint32_t, WordsPerLine> data{};
    };
    struct Address {
        std::size_t word, block_base, set, tag, offset;
    };
    std::array<std::array<Line, Ways>, Sets> lines_{};
    std::array<std::size_t, Sets> most_recent_way_{};
    std::vector<std::uint32_t>& backing_;
    bool writable_;

    Address decode(std::uint32_t byte_address) const {
        if (byte_address % BytesPerWord) throw std::invalid_argument("alignment");
        const auto word = byte_address / BytesPerWord;
        const auto block = word / WordsPerLine;
        const auto base = block * WordsPerLine;
        if (base + WordsPerLine > backing_.size()) throw std::out_of_range("line");
        return {word, base, block % Sets, block / Sets, word % WordsPerLine};
    }

public:
    std::size_t lower_reads = 0, lower_writes = 0;
    CacheReference(std::vector<std::uint32_t>& backing, bool writable)
        : backing_(backing), writable_(writable) {}

    void invalidate() {
        for (auto& set : lines_) for (auto& line : set) line.valid = false;
        most_recent_way_.fill(0); // Backing contents are unchanged.
    }

    std::uint32_t read(std::uint32_t byte_address) {
        const auto a = decode(byte_address);
        auto& set = lines_[a.set];
        for (std::size_t way = 0; way < Ways; ++way) {
            if (set[way].valid && set[way].tag == a.tag) {
                most_recent_way_[a.set] = way;
                return set[way].data[a.offset]; // Hit: no backing READ.
            }
        }
        const std::size_t victim = Ways == 1 ? 0 :
            !set[0].valid ? 0 : !set[1].valid ? 1 :
            1 - most_recent_way_[a.set]; // Invalid first, then two-way LRU.
        auto& line = set[victim];
        line.valid = false;
        for (std::size_t i = 0; i < WordsPerLine; ++i) {
            line.data[i] = backing_[a.block_base + i];
            ++lower_reads; // One lower READ per word of the line.
        }
        line.tag = a.tag;
        line.valid = true; // Publish only after the whole line has arrived.
        most_recent_way_[a.set] = victim;
        return line.data[a.offset];
    }

    bool write(std::uint32_t byte_address, std::uint32_t data,
               std::uint32_t byte_mask) {
        if (!writable_) return false; // A read-only cache rejects WRITE.
        const auto a = decode(byte_address);
        if (byte_mask >= (1u << BytesPerWord))
            throw std::invalid_argument("mask");
        auto merged = backing_[a.word];
        for (std::size_t lane = 0; lane < BytesPerWord; ++lane) {
            if (byte_mask & (1u << lane)) {
                const auto bits = 0xffu << (8 * lane);
                merged = (merged & ~bits) | (data & bits);
            }
        }
        backing_[a.word] = merged;
        ++lower_writes; // Every accepted WRITE goes to backing.
        for (std::size_t way = 0; way < Ways; ++way) {
            auto& line = lines_[a.set][way];
            if (line.valid && line.tag == a.tag) {
                line.data[a.offset] = merged; // Update a cached hit only.
                most_recent_way_[a.set] = way;
                break; // A write miss does not allocate a line.
            }
        }
        return true;
    }
};
```

For example, `CacheReference<32, 2, 2, 2>` has two sets, two ways and two
32-bit words per line. Reading byte address `0` fills words 0 and 1;
reading byte address `4` then hits without another backing READ. Its masked
WRITE sends one backing WRITE and changes a cached word only if that line is
present. The example assumes successful backing and internal transactions;
the simulator also handles held responses and failure statuses.

#### Unknown inputs

Unknown operation or address bits return `InvalidRequest`; an unknown byte
mask on a WRITE does likewise. Read requests ignore write payload and mask.
The cache validates alignment and the configured backing range before a
lower request. Its backing depth must contain complete lines. The tests use
clocked transaction checkpoints; no safe CPU clock or fixed hit latency has
yet been certified. Lower endpoint technology and profile affect the time
spent on fills and writes.

### Implementation

#### Structural implementation

Bit splitters divide the address into block offset, set index and tag. Gates
and a 32-bit comparator check each way; clocked state bits hold validity,
current way and two-way replacement. A visible controller sequences tag and
data endpoint requests and one lower transaction per fill word. FF or SRAM
tag/data endpoints are selected independently by contract ID. Each way has
one `TAG_STORE` endpoint for address tags and one `DATA_STORE` endpoint for
cached words: one way builds two child memories, and two ways build four.
The backing memory connects to `MEM_` outside the cache. The scalable form
selects scalable endpoint storage under this same controller.

#### Behavioral implementation

There is no fused behavioral cache. Scalable child endpoints may themselves
select behavioral storage through the build profile; the cache control and
per-line metadata remain structural. A whole-cache behavioral form would
need its own transaction-equivalence and timing evidence before it could be
registered as another fidelity of this contract.

### Examples

Each example names its geometry and shows the corresponding circuit in the
visualizer. The first three drive known responses into one cache's lower
port, making each hit, fill and eviction easy to follow. The fourth wires two
caches to a real FF RAM endpoint. These are correctness examples, not speed
benchmarks.

| Example | Word | Sets × ways | Line | Data capacity | Tag/data stores |
| --- | ---: | ---: | ---: | ---: | --- |
| Direct mapped | 32 bits | 2 × 1 | 1 word | 8 bytes | Expanded FF/FF |
| Two way with writes | 8 bits | 2 × 2 | 2 words | 8 bytes | Expanded FF/FF |
| Scalable two way | 32 bits | 8 × 2 | 2 words | 128 bytes | Scalable SRAM/SRAM |
| Two levels | 8 bits | L1: 1 × 1; L2: 2 × 2 | L1: 1 word; L2: 2 words | L1: 1 byte; L2: 8 bytes | FF cache stores, FF backing RAM |

#### Direct mapped: one place per set

This `Cache<32>` has `sets=2`, `ways=1`, `block_bytes=4` and
`backing_depth_words=16`. Address `0` fills its only way in set 0; reading
`0` again is a hit. Address `8` maps to the same set and replaces `0`, so a
later read of `0` must refill. The lower port is scripted and shows one READ
per miss. An address beyond the configured backing range is rejected without
a lower request.

<!-- simulator-examples: cache-direct-mapped CacheDirectMappedScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=cache-direct-mapped" title="Direct-mapped cache and conflict miss" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=cache-direct-mapped" class="md-button">Open direct-mapped scenario</a>

#### Two ways and two-word lines

This `Cache<8>` has `sets=2`, `ways=2`, `block_bytes=2` and
`backing_depth_words=16`. Reading `0` fetches bytes `0` and `1`; reading
`1` then hits in the same line. Another line can occupy the second way, and
a third conflicting line replaces the least recently used way. The scripted
lower port also shows a write-through hit, a write miss without allocation,
invalidation and a rejected address.

<!-- simulator-examples: cache-read-hit CacheComponentScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=cache-read-hit" title="Cache lines, replacement and writes" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=cache-read-hit" class="md-button">Open two-way scenario</a>

#### Scalable stores with larger geometry

This `Cache<32>` has `sets=8`, `ways=2`, `block_bytes=8`,
`backing_depth_words=64` and independently selected scalable SRAM tag and
data stores. Reading byte address `4` fetches words at `0` and `4`; the next
read of `0` is a same-line hit. Lines starting at `0`, `64` and `128` all map
to one set, so the later reads show both ways and least-recently-used
replacement. The lower responses are scripted; the visualized cache control
is the same structural design as in the smaller examples.

<!-- simulator-examples: cache-scalable-two-way CacheScalableScenarioTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=cache-scalable-two-way" title="Scalable two-way cache" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=cache-scalable-two-way" class="md-button">Open scalable cache scenario</a>

#### Two levels over a real memory endpoint

The hierarchy scenario connects one requester to a one-word direct-mapped
L1, then to a two-way L2 with two-word lines, then to a 16-byte FF RAM
endpoint. Both levels use `Cache<8>` and the same request/response pins.

`requester → L1 → L2 → FF RAM`

The first read of address `0` misses in both caches, so L2 reads bytes `0`
and `1` from RAM. Repeating `0` hits L1. Reading `1` misses L1 but hits L2:
the verified RAM-read count stays at two. A later write to `0` reaches RAM;
after both idle caches are invalidated, reading `0` refills the changed
value. The ten timeline checkpoints show those boundaries in the actual
connected circuit. This fixture has one requester and broadcasts
invalidation only while both levels are idle.

<div class="simulator-embed"><iframe src="../../simulator/?scenario=cache-two-level" title="Two cache levels over RAM" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=cache-two-level" class="md-button">Open two-level scenario</a>

### Tests

The three single-cache examples are registered component scenarios with
generated checkpoint tables. `CacheTwoLevelScenarioTest` is a precomputed
visualizer timeline over a connected two-cache fixture and checks exact RAM
traffic and final data. The headless `CacheReadTest` uses
standalone cache-plus-endpoint circuits across W=8/16/32, expanded and
scalable storage, one/two-way sets, one/two/four-word lines, FF/SRAM tag and
data banks, masked writes, invalidation, reset during a miss, FF/SRAM/DRAM/ROM
backing reads and a failed second fill word. A deterministic mixed stream is
compared with an independent set/way/byte model. An optional advanced RV32I
test configuration also connects separate instruction and data caches to the
core, with a structural arbiter when they share one RAM. Short store/load and
branch programs pass, but no performance benefit has been established.
External writes require invalidation; no coherence protocol is provided.

A separate 32-bit headless fixture connects an expanded upper cache to a
scalable SRAM-backed L2 and DRAM. It checks that an L1 miss can hit L2 without
another DRAM read, masked writes reach DRAM and idle-time invalidation makes
both levels refill. These checks establish functional composition, not an
inclusive or coherent cache policy.

`CacheDocumentationTest` compiles the exact C++ reference above and checks its
widths, hits, fills, replacement, writes and invalidation.
