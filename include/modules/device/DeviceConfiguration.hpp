#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <stdexcept>
#include <array>
#include <cstdint>
#include <string>

namespace circuit::device {
// Capacity limits are host budgets, independent of the chosen visible shape.
inline constexpr size_t MaxFramebufferBytes = 256 * 1024;
inline constexpr size_t MaxFramebufferDimension = 4096;
inline constexpr size_t MaxCapturedPixelBytes = 4 * 1024 * 1024;

struct FramebufferConfig {
    size_t width = 4, height = 4, storage_bytes = 0;
    void validate() const {
        if (!width || !height || width > MaxFramebufferDimension || height > MaxFramebufferDimension
            || height > MaxFramebufferBytes / width)
            throw std::invalid_argument("Framebuffer geometry exceeds the 256 KiB pixel budget");
        if (storage_bytes && (storage_bytes < width * height || storage_bytes < 4
            || storage_bytes > MaxFramebufferBytes || !std::has_single_bit(storage_bytes)))
            throw std::invalid_argument("Framebuffer storage must cover the surface and be a power of two in 4..262144 bytes");
    }
    size_t capacityBytes() const {
        validate();
        return storage_bytes ? storage_bytes : std::bit_ceil(std::max(size_t{4}, width * height));
    }
};
struct AddressRegion {
    std::string prefix;
    uint32_t base;
    uint64_t bytes;
};
struct AddressMap {
    std::array<AddressRegion,6> regions{{
        {"RAM_",0,0x40000},{"CONSOLE_",0x10000000,0x100},
        {"TIMER_",0x10001000,0x100},{"BUTTONS_",0x10002000,0x100},
        {"DISPLAY_",0x10003000,0x100},{"VIDEO_",0x20000000,0x1000}}};
    void validate() const {
        const std::array<std::string,6> names{"RAM_","CONSOLE_","TIMER_","BUTTONS_","DISPLAY_","VIDEO_"};
        for(size_t i=0;i<regions.size();++i){const auto& a=regions[i];
            if(a.prefix!=names[i] || a.bytes<4 || a.bytes>(uint64_t{1}<<32) || !std::has_single_bit(a.bytes)
                || uint64_t(a.base)%a.bytes || uint64_t(a.base)+a.bytes>(uint64_t{1}<<32))
                throw std::invalid_argument("Invalid device address region");
            for(size_t j=0;j<i;++j){const auto& b=regions[j];
                if(uint64_t(a.base)<uint64_t(b.base)+b.bytes && uint64_t(b.base)<uint64_t(a.base)+a.bytes)
                    throw std::invalid_argument("Overlapping device address regions");}
        }
    }
};
inline bool validFifoDepth(size_t depth) { return depth >= 1 && depth <= 7; }
inline bool validVideoAperture(size_t bytes) {
    return bytes >= 4 && bytes <= MaxFramebufferBytes && std::has_single_bit(bytes);
}
}
