#pragma once
// Construction helper for the array multiplier (§ 6.1): partial products,
// a Dadda tree of full and half adders, and prefix adders for the last two
// rows. It builds gates only; it does not evaluate anything.
#include "components/ComponentBuilder.tpp"
#include "components/StructuralLogic.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace circuit::arithmetic::detail {
using circuit::structural::Logic;
using circuit::structural::P;
using circuit::structural::V;

// Dadda's column-height targets: 2, 3, 4, 6, 9, 13, 19, 28, 42, ...
inline std::vector<size_t> daddaTargets(size_t tallest) {
    std::vector<size_t> d{2};
    while (d.back() < tallest) d.push_back(d.back() * 3 / 2);
    d.pop_back();                       // the first target not below the tallest column
    return {d.rbegin(), d.rend()};      // largest first
}

// Adds two equal-width rows with prefix adders of 32, 16 or 8 bits, the
// carry chained from one adder to the next. The final carry is dropped.
inline V addRows(ComponentBuilder& b, Logic& l, const V& x, const V& y, const std::string& prefix) {
    if (x.size() != y.size() || x.size() % 8) throw std::invalid_argument("Final adder rows must be a multiple of 8 bits");
    V sum;
    P carry = l.bit(false);
    for (size_t first = 0, k = 0; first < x.size(); ++k) {
        const size_t left = x.size() - first;
        const size_t width = left >= 32 ? 32 : left >= 16 ? 16 : 8;
        auto adder = b.add(circuit::families::parallelPrefixAdder(width), prefix + std::to_string(k));
        l.wire(l.join(V(x.begin() + first, x.begin() + first + width)), adder->getInputPinDynamic("A"));
        l.wire(l.join(V(y.begin() + first, y.begin() + first + width)), adder->getInputPinDynamic("B"));
        l.wire(carry, adder->getInputPin("C_IN"));
        const auto bits = l.split(adder->getOutputPinDynamic("OUT"));
        sum.insert(sum.end(), bits.begin(), bits.end());
        carry = adder->getOutputPin("C_OUT");
        first += width;
    }
    return sum;
}

// The low a.size() + b.size() bits of a × b. `sign_a` / `sign_b` are the
// operands' sign bits when they are read as two's complement, or null when
// the operand is unsigned (its sign weight is then never built).
//
// Modified Baugh-Wooley: each operand gets one more bit (its sign bit s with
// weight -2^n). Products of one sign bit with an ordinary bit are negative;
// they are built inverted (NAND) and corrected by constant ones at columns
// n_a and n_b. The sign-times-sign term and the corrections above the result
// width vanish modulo 2^(n_a + n_b).
inline V multiplyBits(ComponentBuilder& b, Logic& l, const V& a, const V& bb, const P& sign_a, const P& sign_b) {
    const size_t na = a.size(), nb = bb.size(), out = na + nb;
    std::vector<V> columns(out);
    // Ordinary partial products: one word AND gate per bit of B (A masked by it).
    for (size_t j = 0; j < nb; ++j) {
        const auto row = l.mask(a, bb[j]);
        for (size_t i = 0; i < na; ++i) columns[i + j].push_back(row[i]);
    }
    // Sign of A times each bit of B, inverted, at columns na + j.
    if (sign_a) {
        const auto row = l.split(l.invertWord(l.join(l.mask(bb, sign_a))));
        for (size_t j = 0; j < nb; ++j) columns[na + j].push_back(row[j]);
        columns[na].push_back(l.bit(true));
    }
    // Sign of B times each bit of A, inverted, at columns nb + i.
    if (sign_b) {
        const auto row = l.split(l.invertWord(l.join(l.mask(a, sign_b))));
        for (size_t i = 0; i < na; ++i) columns[nb + i].push_back(row[i]);
        columns[nb].push_back(l.bit(true));
    }
    // Dadda reduction to two rows.
    size_t tallest = 0;
    for (const auto& c : columns) tallest = std::max(tallest, c.size());
    size_t stage = 0;
    for (const size_t target : daddaTargets(tallest)) {
        std::vector<V> next(out);
        V carries;
        for (size_t c = 0; c < out; ++c) {
            const auto& pool = columns[c];
            size_t height = pool.size() + carries.size(), used = 0, cell = 0;
            V produced, carry_out;
            while (height > target) {
                const auto name = std::to_string(stage) + "_" + std::to_string(c) + "_" + std::to_string(cell++);
                if (height - target >= 2 && pool.size() - used >= 3) {
                    auto fa = b.addNewComponent<FullAdder>("FA_" + name);
                    l.wire(pool[used], fa->getInputPin("A"));
                    l.wire(pool[used + 1], fa->getInputPin("B"));
                    l.wire(pool[used + 2], fa->getInputPin("Carry_in"));
                    produced.push_back(fa->getOutputPin("Sum"));
                    carry_out.push_back(fa->getOutputPin("Carry_out"));
                    used += 3;
                    height -= 2;
                } else if (pool.size() - used >= 2) {
                    auto ha = b.addNewComponent<HalfAdder>("HA_" + name);
                    l.wire(pool[used], ha->getInputPin("A"));
                    l.wire(pool[used + 1], ha->getInputPin("B"));
                    produced.push_back(ha->getOutputPin("Sum"));
                    carry_out.push_back(ha->getOutputPin("Carry"));
                    used += 2;
                    height -= 1;
                } else {
                    throw std::logic_error("Dadda reduction ran out of bits");
                }
            }
            next[c].assign(pool.begin() + used, pool.end());
            next[c].insert(next[c].end(), produced.begin(), produced.end());
            next[c].insert(next[c].end(), carries.begin(), carries.end());
            carries = std::move(carry_out);   // a carry out of the top column is dropped
        }
        columns = std::move(next);
        ++stage;
    }
    V x, y;
    for (const auto& c : columns) {
        x.push_back(c.size() > 0 ? c[0] : l.bit(false));
        y.push_back(c.size() > 1 ? c[1] : l.bit(false));
    }
    return addRows(b, l, x, y, "FINAL_ADD_");
}
} // namespace circuit::arithmetic::detail
