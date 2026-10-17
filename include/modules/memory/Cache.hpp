#pragma once

#include "modules/memory/MemoryProtocol.hpp"
#include <cstddef>
#include <string>

namespace circuit::memory {

struct CacheConfig {
    size_t sets = 4;
    size_t ways = 1;
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
