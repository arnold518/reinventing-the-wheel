#pragma once

#include "basic/LogicValue.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

// A four-state value of up to 64 bits in two words. Bit i of `value` and of
// `unknown` together hold LogicValue i: LOW (0,0), HIGH (1,0), UNKNOWN (0,1),
// HIGH_Z (1,1) — the enum's numbers, value bit first. Bits at or above the
// width are always zero, so equal signals compare equal as two words.
struct PackedLogic {
    static constexpr size_t kMaxWidth = 64;

    uint64_t value = 0;
    uint64_t unknown = 0;

    static constexpr uint64_t mask(size_t width) {
        return width >= 64 ? ~uint64_t{0} : (uint64_t{1} << width) - 1;
    }
    static constexpr PackedLogic allUnknown(size_t width) { return {0, mask(width)}; }
    // Every bit `bit`.
    static constexpr PackedLogic filled(LogicValue bit, size_t width) {
        const auto code = static_cast<uint8_t>(bit);
        return {(code & 1U) ? mask(width) : 0, (code & 2U) ? mask(width) : 0};
    }
    // Two-state bits; bits at or above `width` are dropped.
    static constexpr PackedLogic fromUInt64(uint64_t bits, size_t width) { return {bits & mask(width), 0}; }
    // Missing trailing bits are UNKNOWN and extra ones are dropped, as the
    // vector setters always did.
    static PackedLogic fromVector(const std::vector<LogicValue>& bits, size_t width) {
        const size_t count = bits.size() < width ? bits.size() : width;
        PackedLogic packed{0, mask(width) & ~mask(count)};
        for (size_t i = 0; i < count; ++i) {
            const auto code = static_cast<uint64_t>(bits[i]);
            packed.value |= (code & 1U) << i;
            packed.unknown |= ((code >> 1) & 1U) << i;
        }
        return packed;
    }

    constexpr LogicValue get(size_t index) const {
        return static_cast<LogicValue>(((value >> index) & 1U) | (((unknown >> index) & 1U) << 1));
    }
    constexpr void set(size_t index, LogicValue bit) {
        const uint64_t code = static_cast<uint8_t>(bit);
        value = (value & ~(uint64_t{1} << index)) | ((code & 1U) << index);
        unknown = (unknown & ~(uint64_t{1} << index)) | (((code >> 1) & 1U) << index);
    }
    // HIGH bits only, as getValueAsUInt64 always reported.
    constexpr uint64_t highBits() const { return value & ~unknown; }

    // The same signal seen at another width: narrower drops high bits, wider
    // pads with UNKNOWN (the padded-vector rule of cross-width fanout).
    constexpr PackedLogic resized(size_t from, size_t to) const {
        if (to <= from) return {value & mask(to), unknown & mask(to)};
        return {value, unknown | (mask(to) & ~mask(from))};
    }

    std::vector<LogicValue> toVector(size_t width) const {
        std::vector<LogicValue> bits(width);
        for (size_t i = 0; i < width; ++i) bits[i] = get(i);
        return bits;
    }

    friend constexpr bool operator==(const PackedLogic&, const PackedLogic&) = default;
};
