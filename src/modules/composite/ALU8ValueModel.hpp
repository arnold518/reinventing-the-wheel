#pragma once

#include "basic/LogicValue.hpp"
#include <array>

namespace circuit::detail::alu8 {
// Settled-value abstraction of the existing Chapter 2 gate networks. Keep
// their Boolean factoring: algebraically equivalent binary expressions can
// differ for X/Z. These functions allocate no circuit objects or events.
using Bit = LogicValue;
template<size_t Width> using Bits = std::array<Bit, Width>;
using Word = Bits<8>;
using Op = Bits<3>;
constexpr Bit L = Bit::LOW, H = Bit::HIGH, X = Bit::UNKNOWN;

inline Bit inv(Bit a) { return a == L ? H : a == H ? L : X; }
inline Bit land(Bit a, Bit b) {
    return a == L || b == L ? L : a == H && b == H ? H : X;
}
inline Bit lor(Bit a, Bit b) {
    return a == H || b == H ? H : a == L && b == L ? L : X;
}
inline Bit lxor(Bit a, Bit b) {
    return (a == L || a == H) && (b == L || b == H) ? (a == b ? L : H) : X;
}
inline Bit mux(Bit a, Bit b, Bit select) {
    // Mux2to1 is NOT + two ANDs + OR, not an optimistic conditional.
    return lor(land(a, inv(select)), land(b, select));
}
inline Bit select8(Bits<8> inputs, const Op& op) {
    for (size_t stage = 0; stage < 3; ++stage)
        for (size_t i = 0; i < (size_t{4} >> stage); ++i)
            inputs[i] = mux(inputs[2*i], inputs[2*i+1], op[stage]);
    return inputs[0];
}
inline Word word(unsigned value) {
    Word result;
    for (size_t i = 0; i < 8; ++i) result[i] = (value >> i) & 1 ? H : L;
    return result;
}
inline Word selectWords(const std::array<Word, 8>& words, const Op& op) {
    Word result;
    for (size_t bit = 0; bit < 8; ++bit) {
        Bits<8> values;
        for (size_t i = 0; i < 8; ++i) values[i] = words[i][bit];
        result[bit] = select8(values, op);
    }
    return result;
}
inline Bit zero(const Word& a) {
    Bit any = a[0];
    for (size_t i = 1; i < 8; ++i) any = lor(any, a[i]);
    return inv(any);
}

struct Arithmetic { Word out; Bit carry, overflow = L, invalid = L; };
inline Arithmetic add(const Word& a, const Word& b, Bit carry) {
    Word sum;
    for (size_t i = 0; i < 8; ++i) {
        const auto half_sum = lxor(a[i], b[i]);
        sum[i] = lxor(half_sum, carry);
        carry = lor(land(a[i], b[i]), land(half_sum, carry));
    }
    return {sum, carry};
}
inline Arithmetic addsub(const Word& a, Word b, Bit sub, Bit carry) {
    for (auto& bit : b) bit = lxor(bit, sub);
    auto result = add(a, b, carry);
    result.overflow = land(inv(lxor(a[7], b[7])), lxor(a[7], result.out[7]));
    return result;
}
inline Arithmetic arithmetic(const Word& a, const Word& b, const Op& op, Bit cin) {
    const auto z = word(0), one = word(1);
    auto result = addsub(
        selectWords({a,a,a,a,a,a,z,z}, op),
        selectWords({b,b,b,b,z,one,a,z}, op),
        select8({L,L,H,H,L,H,H,L}, op),
        select8({L,cin,H,cin,H,H,H,L}, op));
    result.invalid = select8({L,L,L,L,L,L,L,H}, op);
    const auto valid = inv(result.invalid);
    for (auto& bit : result.out) bit = land(bit, valid);
    result.carry = land(result.carry, valid);
    result.overflow = land(result.overflow, valid);
    return result;
}
inline Word logic(const Word& a, const Word& b, const Op& op) {
    Word out;
    for (size_t i = 0; i < 8; ++i) {
        const auto ab_and = land(a[i], b[i]), ab_or = lor(a[i], b[i]), ab_xor = lxor(a[i], b[i]);
        out[i] = select8({ab_and, ab_or, ab_xor, inv(a[i]), inv(ab_and), inv(ab_or), inv(ab_xor), L}, op);
    }
    return out;
}
struct Shift { Word out; Bit carry; };
inline Shift shift(const Word& a, const Word& b, const Op& op, Bit carry) {
    const auto left = select8({H,L,L,H,L,L,L,L}, op);
    const auto rotate = select8({L,L,L,H,H,L,L,L}, op);
    const auto arithmetic = select8({L,L,H,L,L,L,L,L}, op);
    const auto valid = select8({H,H,H,H,H,L,L,L}, op);
    const auto fill = land(a[7], arithmetic);
    Word current;
    for (size_t i = 0; i < 8; ++i) current[i] = mux(a[i], a[7-i], left);
    for (size_t stage = 0; stage < 3; ++stage) {
        const size_t distance = size_t{1} << stage;
        const auto previous = current;
        carry = mux(carry, previous[distance-1], b[stage]);
        for (size_t i = 0; i < 8; ++i) {
            const auto moved = i + distance < 8 ? previous[i+distance]
                : mux(fill, previous[i+distance-8], rotate);
            current[i] = mux(previous[i], moved, b[stage]);
        }
    }
    const auto shifted = current;
    for (size_t i = 0; i < 8; ++i)
        current[i] = land(mux(shifted[i], shifted[7-i], left), valid);
    return {current, land(carry, valid)};
}
inline Word compare(const Word& a, const Word& b, const Word& diff,
                    Bit carry, Bit overflow, const Op& op) {
    const auto equal = zero(diff), ltu = inv(carry), lts = lxor(diff[7], overflow);
    Word out;
    for (size_t i = 0; i < 8; ++i)
        out[i] = select8({i == 0 ? equal : L, i == 0 ? ltu : L, i == 0 ? lts : L,
            mux(b[i], a[i], ltu), mux(a[i], b[i], ltu),
            mux(b[i], a[i], lts), mux(a[i], b[i], lts), L}, op);
    return out;
}
struct Flags { Bit zero, negative, carry, overflow; };
inline Flags flags(const Word& result, Bit arithmetic, Bit shift,
                   Bit arithmetic_carry, Bit arithmetic_overflow, Bit shift_carry) {
    return {zero(result), lor(result[7], L),
        lor(land(arithmetic, arithmetic_carry), land(shift, shift_carry)),
        land(arithmetic, arithmetic_overflow)};
}
struct Control { Op local, arithmetic, compare; Bits<6> enables; Bit invalid; };
inline Control control(const Bits<5>& op) {
    Bits<32> decoded;
    for (size_t i = 0; i < 32; ++i) {
        Bit match = H;
        for (size_t bit = 0; bit < 5; ++bit)
            match = land(match, (i >> bit) & 1 ? op[bit] : inv(op[bit]));
        decoded[i] = match;
    }
    auto group = [&](size_t first, size_t last) {
        Bit any = decoded[first];
        for (size_t i = first + 1; i <= last; ++i) any = lor(any, decoded[i]);
        return any;
    };
    Control result;
    result.enables = {group(0,6), group(8,14), group(16,20), group(21,27), decoded[28], decoded[29]};
    result.invalid = lor(lor(decoded[7], decoded[15]), decoded[31]);
    for (size_t bit = 0; bit < 3; ++bit) {
        // LOCAL_OP is wiring only: unlike a gate output, it preserves Z.
        result.local[bit] = op[bit];
        result.arithmetic[bit] = mux(op[bit], bit == 1 ? H : L, result.enables[3]);
        Bit any = L;
        for (size_t i = 0; i < 7; ++i)
            if ((i >> bit) & 1) any = lor(any, decoded[21+i]);
        result.compare[bit] = any;
    }
    return result;
}
inline Word select(const std::array<Word, 6>& words, const Bits<6>& enables) {
    Word out;
    for (size_t bit = 0; bit < 8; ++bit) {
        Bit any = L;
        for (size_t i = 0; i < 6; ++i) any = lor(any, land(words[i][bit], enables[i]));
        out[bit] = any;
    }
    return out;
}
struct Result { Word out; Flags flags; Bit invalid; };
inline Result alu(const Word& a, const Word& b, const Bits<5>& op, Bit cin) {
    const auto c = control(op);
    const auto arith = arithmetic(a, b, c.arithmetic, cin);
    const auto shifter = shift(a, b, c.local, cin);
    const auto out = select({arith.out, logic(a,b,c.local), shifter.out,
        compare(a,b,arith.out,arith.carry,arith.overflow,c.compare), a, b}, c.enables);
    return {out, flags(out, c.enables[0], c.enables[2], arith.carry, arith.overflow, shifter.carry), c.invalid};
}
} // namespace circuit::detail::alu8
