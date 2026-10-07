#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace circuit::analysis::detail {
// A set of LOW/HIGH/X/Z values, guaranteed after `at`. `settled` bounds the
// final transition, which need not reveal the value. These are different facts.
inline constexpr size_t unbounded = std::numeric_limits<size_t>::max();
struct TimedBit {
    unsigned values = 15;
    size_t at = 0, settled = unbounded;
};
inline size_t after(size_t time, size_t delay) {
    if (time == unbounded || delay >= unbounded - time)
        throw std::overflow_error("Timing arithmetic overflow");
    return time + delay;
}
inline size_t settledAt(const std::vector<TimedBit>& bits) {
    size_t time = 0;
    for (const auto& b : bits) time = std::max(time, b.settled);
    return time;
}
struct TimedChoices {
    unsigned values = 0;
    size_t at = 0, settled = unbounded;
};
inline TimedChoices operationChoices(const std::vector<TimedBit>& bits) {
    TimedChoices c;
    c.settled = settledAt(bits);
    for (const auto& b : bits) {
        c.at = std::max(c.at, b.at);
        if (b.values & 12) c.values |= 1u << 8; // any nonbinary opcode
    }
    for (unsigned op = 0; op < 8; ++op) {
        bool possible = true;
        for (size_t i = 0; i < 3; ++i) possible &= bool(bits[i].values & (1u << ((op >> i) & 1)));
        if (possible) c.values |= 1u << op;
    }
    return c;
}
inline TimedChoices maskChoices(const std::vector<TimedBit>& bits) {
    TimedChoices c{2, 0, settledAt(bits)}; // choice 0: nonbinary, 1: binary
    for (const auto& b : bits) {
        c.at = std::max(c.at, b.at);
        if (b.values & 12) c.values |= 1;
        if (!(b.values & 3)) c.values &= ~2u;
    }
    return c;
}
inline TimedChoices addressChoices(const std::vector<TimedBit>& bits,
                                   size_t bytes_per_word, size_t capacity) {
    // Choices are validator status codes: 0 OK, 1 alignment, 2 range, 6 X/Z.
    TimedChoices c{0, 0, settledAt(bits)};
    bool binary = true, aligned = true, misaligned = false;
    uint64_t lo = 0, hi = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        const auto& b = bits[i]; c.at = std::max(c.at, b.at);
        if (b.values & 12) c.values |= 1u << 6;
        binary &= bool(b.values & 3);
        if ((uint64_t{1} << i) < bytes_per_word) {
            aligned &= bool(b.values & 1); misaligned |= bool(b.values & 2);
        } else {
            if (!(b.values & 1)) lo |= uint64_t{1} << i;
            if (b.values & 2) hi |= uint64_t{1} << i;
        }
    }
    if (binary) {
        if (misaligned) c.values |= 1u << 1;
        if (aligned) {
            if (lo < capacity) c.values |= 1;
            if (hi >= capacity) c.values |= 1u << 2;
        }
    }
    return c;
}
// Exact FF validateRequest truth table over conservative input categories.
// Enumerating categories, not addresses, keeps the proof independent of size.
inline unsigned ffStatus(unsigned op, unsigned address, unsigned mask) {
    if (op == 8 || address == 6) return 6;
    if (op > 3) return 4;
    if ((op == 1 || op == 2) && !mask) return 6;
    if (address) return address;
    return op > 1 ? 4 : 0;
}
inline std::array<TimedBit, 3> ffStatusBounds(
    const std::vector<TimedBit>& op, const std::vector<TimedBit>& addr,
    const std::vector<TimedBit>& mask, size_t bytes, size_t capacity, size_t delay) {
    const std::array choices{operationChoices(op), addressChoices(addr, bytes, capacity), maskChoices(mask)};
    std::array<TimedBit, 3> result;
    size_t restrictions = 0;
    for (const auto& c : choices) restrictions = std::max(restrictions, c.at);
    for (unsigned bit = 0; bit < 3; ++bit) {
        auto& r = result[bit]; r.values = 0; r.at = after(restrictions, delay);
        std::array<bool, 3> depends{};
        for (unsigned o = 0; o < 9; ++o) if (choices[0].values & (1u << o))
        for (unsigned a = 0; a < 7; ++a) if (choices[1].values & (1u << a))
        for (unsigned m = 0; m < 2; ++m) if (choices[2].values & (1u << m)) {
            const auto value = (ffStatus(o, a, m) >> bit) & 1;
            r.values |= 1u << value;
            const std::array args{o, a, m};
            for (unsigned input = 0; input < 3; ++input)
                for (unsigned v = 0; v < 9; ++v) if (choices[input].values & (1u << v)) {
                    auto other = args; other[input] = v;
                    depends[input] = depends[input] || (((ffStatus(other[0], other[1], other[2]) >> bit) & 1) != value);
                }
        }
        size_t t = restrictions;
        for (unsigned i = 0; i < 3; ++i) if (depends[i]) t = std::max(t, choices[i].settled);
        if (t != unbounded) r.settled = after(t, delay);
    }
    return result;
}
} // namespace circuit::analysis::detail
