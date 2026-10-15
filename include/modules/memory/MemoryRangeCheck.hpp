#pragma once
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace circuit::memory_detail {

// Gates computing (address < capacity) for a little-endian bit list. A
// power-of-two capacity needs only an OR of the higher bits; otherwise a
// balanced unsigned comparison against the constant (the same circuit the
// FF request check uses).
inline StructuralWiring::PinPtr addressBelow(StructuralWiring& w, const std::vector<StructuralWiring::PinPtr>& bits,
                                             uint64_t capacity, const std::string& prefix = "RANGE") {
    const size_t n = bits.size();
    if (capacity >= (uint64_t{1} << n)) return w.constant(prefix + "_ALWAYS", true);
    if ((capacity & (capacity - 1)) == 0) {
        std::vector<StructuralWiring::PinPtr> high;
        size_t first = 0; while ((uint64_t{1} << first) < capacity) ++first;
        for (size_t i = first; i < n; ++i) high.push_back(bits[i]);
        return w.invert(prefix + "_IN", w.reduce(prefix + "_HIGH", high, false));
    }
    struct Group { StructuralWiring::PinPtr equal, greater; };
    std::vector<Group> groups;
    const auto never = w.constant(prefix + "_FALSE", false);
    for (size_t i = 0; i < n; ++i) {
        const bool one = (capacity >> i) & 1;
        groups.push_back({one ? bits[i] : w.invert(prefix + "_NOT_" + std::to_string(i), bits[i]),
                          one ? never : bits[i]});
    }
    for (size_t stride = 1; stride < n; stride *= 2)
        for (size_t low = 0; low + stride < n; low += 2 * stride) {
            auto& lo = groups[low];
            auto& hi = groups[low + stride];
            const auto tag = std::to_string(stride) + "_" + std::to_string(low);
            lo.greater = w.either(prefix + "_GREATER_" + tag, hi.greater,
                                  w.both(prefix + "_EQUAL_GREATER_" + tag, hi.equal, lo.greater));
            lo.equal = w.both(prefix + "_EQUAL_" + tag, hi.equal, lo.equal);
        }
    // address >= capacity when greater or equal.
    return w.invert(prefix + "_IN", w.either(prefix + "_AT_LEAST", groups.front().greater, groups.front().equal));
}

} // namespace circuit::memory_detail
