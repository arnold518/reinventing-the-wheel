#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <stdexcept>

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
inline bool validFifoDepth(size_t depth) { return depth >= 1 && depth <= 7; }
inline bool validVideoAperture(size_t bytes) {
    return bytes >= 4 && bytes <= MaxFramebufferBytes && std::has_single_bit(bytes);
}
}
