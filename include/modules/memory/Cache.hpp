#pragma once

#include "modules/memory/MemoryProtocol.hpp"
#include <cstddef>
#include <string>

namespace circuit::memory {

// Which way of a full set a miss replaces (an invalid way always goes first).
// lru: the least recently used way, exact (one or two ways). plru: a tree of
// ways-1 bits per set, each pointing to the half that holds the victim; every
// access points the bits on its path away from its way. fifo: a pointer per
// set to the oldest filled way. random: the low bits of a 16-bit LFSR shared
// by all sets, stepped by every fill.
enum class CacheReplacement { LRU, PLRU, FIFO, Random };
const char* cacheReplacementName(CacheReplacement);

struct CacheConfig {
    size_t sets = 4;
    size_t ways = 1;  // 1, 2, 4 or 8
    size_t block_bytes = 0; // 0 selects one word at the chosen endpoint width.
    size_t backing_depth_words = 16;
    bool read_write = false;
    Technology tag_technology = Technology::FF;
    Technology data_technology = Technology::FF;
    bool scalable_storage = false;
    // A miss fills the line with one burst read from the requested word
    // (the backing memory must answer bursts of a line) instead of one read
    // per word.
    bool burst_fill = false;
    // Entries of the write buffer (0, 1, 2 or 4; read-write caches only). A
    // buffered write is answered in its lookup cycle and reaches the backing
    // memory later, oldest first, whenever no fill uses the lower port; a
    // read miss waits until the buffer is empty. 0: write-through, answered
    // when the backing memory completes the write.
    size_t write_buffer = 0;
    // Write-back (read-write caches, no write buffer): a write hit only marks
    // its line dirty; a dirty victim is written back before its set is
    // refilled; invalidation writes back every dirty line first.
    bool write_back = false;
    // Write-allocate (read-write caches): a write miss fills its line first.
    bool write_allocate = false;
    // Where the valid, dirty and replacement bits are kept: flip-flops in the
    // controller (false; up to 4 expanded or 64 scalable sets), or a line-state
    // store, one word per set in a memory bank beside the tags (true; up to
    // 128 expanded or 4,096 scalable sets). The store is cleared set by set
    // after reset and on invalidation.
    bool line_store = false;
    // Exact LRU is built for one or two ways; four and eight ways use plru,
    // fifo or random.
    CacheReplacement replacement = CacheReplacement::LRU;
    // The controller as gates (false), or as one compact part that runs the
    // same gates' list (true): the same cycles, many times faster.
    bool compact_controller = false;

    void validate(size_t width) const;
};

CacheConfig cacheConfig(const ParameterMap&);
ParameterMap cacheParameters(const CacheConfig&);
void cachePins(IOComponent*, size_t width);
void registerCaches(ComponentCatalog&);

template<size_t Width>
class Cache : public IOComponent {
public:
    Cache(std::string name, CacheConfig config = {});
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;

private:
    CacheConfig config_;
    std::string type_;
};

} // namespace circuit::memory
