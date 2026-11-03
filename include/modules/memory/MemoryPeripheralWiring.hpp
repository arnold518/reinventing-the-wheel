#pragma once

#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/basic/Mux.hpp"

namespace circuit::memory_detail {

// Construction routines, not simulated components or behavioral shortcuts.
// Callers retain address interpretation, public pins, gate names and hierarchy.
inline StructuralWiring::PinPtr decodeMatch(
    StructuralWiring& wiring, const std::string& name,
    const std::vector<StructuralWiring::PinPtr>& positive,
    const std::vector<StructuralWiring::PinPtr>& negative, size_t value,
    StructuralWiring::PinPtr enable = {}) {
    if (positive.size() != negative.size() || positive.size() > 32)
        throw std::invalid_argument("Invalid memory decode bit vectors");
    std::vector<StructuralWiring::PinPtr> terms;
    if (enable) terms.push_back(std::move(enable));
    for (size_t bit = 0; bit < positive.size(); ++bit)
        terms.push_back((value & (size_t{1} << bit)) ? positive[bit] : negative[bit]);
    return wiring.reduce(name, std::move(terms), true);
}

// One word line per row, decoded from address bits [first_bit, first_bit +
// log2(rows)). A line rises only when `enable` is high and every address bit
// above the row field is zero. The caller splits the address and says which
// bit the row field starts at: SRAM skips its column-select bits, while the
// DRAM/ROM/flash controls already supply a row number.
template<class AddressBit>
std::vector<StructuralWiring::PinPtr> rowDecodeLines(
    StructuralWiring& wiring, AddressBit address_bit, size_t first_bit, size_t rows,
    StructuralWiring::PinPtr enable) {
    size_t row_bits = 0;
    while ((size_t{1} << row_bits) < rows) ++row_bits;
    // Bits first_bit..31 are inverted by one word NOT (NOT_ADDRESS).
    std::vector<StructuralWiring::PinPtr> positive, all;
    for (size_t bit = first_bit; bit < 32; ++bit) all.push_back(address_bit(bit));
    const auto inverted = wiring.invertBits("NOT_ADDRESS", all);
    positive.assign(all.begin(), all.begin() + row_bits);
    std::vector<StructuralWiring::PinPtr> negative(inverted.begin(), inverted.begin() + row_bits);
    std::vector<StructuralWiring::PinPtr> high_zero(inverted.begin() + row_bits, inverted.end());
    auto valid_high = wiring.reduce("HIGH_ADDRESS_VALID", std::move(high_zero), true);
    auto enabled = wiring.both("VALID_AND_ENABLED", valid_high, std::move(enable));
    std::vector<StructuralWiring::PinPtr> lines;
    for (size_t row = 0; row < rows; ++row)
        lines.push_back(decodeMatch(wiring, "ROW_" + std::to_string(row), positive, negative, row, enabled));
    return lines;
}

// Binary mux tree: preserve four-state Mux<2,1> behavior and per-level delay.
// FF banks supply a zero leaf for non-power-of-two depths; SRAM slots are a
// power of two. A singleton passes through with no invented mux stage.
inline StructuralWiring::PinPtr selectMuxTree(
    StructuralWiring& wiring, ComponentBuilder& builder, const std::string& name,
    std::vector<StructuralWiring::PinPtr> inputs,
    const std::vector<StructuralWiring::PinPtr>& selects,
    StructuralWiring::PinPtr missing = {}) {
    if (inputs.empty()) throw std::invalid_argument("Empty memory selector");
    for (size_t level = 0; inputs.size() > 1; ++level) {
        if (level >= selects.size()) throw std::invalid_argument("Missing memory select bit");
        std::vector<StructuralWiring::PinPtr> next;
        for (size_t i = 0; i < inputs.size(); i += 2) {
            auto right = i + 1 < inputs.size() ? inputs[i + 1] : missing;
            if (!right) throw std::invalid_argument("Missing memory selector padding");
            auto mux = builder.addNewComponent<Mux<2,1>>(
                name + "_" + std::to_string(level) + "_" + std::to_string(i / 2));
            wiring.connect(inputs[i], mux->getInputPin("A"));
            wiring.connect(right, mux->getInputPin("B"));
            wiring.connect(selects[level], mux->getInputPin("SEL"));
            next.push_back(mux->getOutputPin("OUT"));
        }
        inputs = std::move(next);
    }
    return inputs.front();
}

} // namespace circuit::memory_detail
