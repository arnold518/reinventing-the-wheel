#pragma once

#include "modules/memory/MemoryArrayBlocks.hpp"

template<size_t Width, class Cell>
void MemoryArray<Width, Cell>::buildInternals(ComponentBuilder& b) {
    using namespace circuit::memory_detail;
    StructuralWiring w(b);
    const auto depth = config_.depth_words;
    auto decoder = b.addNewComponent<MemoryAddressDecoder>("ADDRESS_DECODE", depth, Traits::readonly);
    auto control = b.addNewComponent<MemoryAccessControl>("ACCESS_CONTROL", depth, Traits::readonly);
    auto storage = b.addNewComponent<MemoryStorageBank<Width, Cell>>("STORAGE", depth);
    auto read = b.addNewComponent<MemoryReadSelector<Width>>("READ_SELECT", depth);

    w.connect(this->template getInputPin<32>("WORD_ADDR"), decoder->template getInputPin<32>("WORD_ADDR"));
    w.connect(decoder->getOutputPin("IN_RANGE"), control->getInputPin("IN_RANGE"));
    for (const auto* pin : {"WE", "READ_EN"}) w.connect(this->getInputPin(pin), control->getInputPin(pin));
    w.connect(control->getOutputPin("FAULT"), this->getOutputPin("FAULT"));
    w.connect(control->getOutputPin("READ_VALID"), read->getInputPin("READ_VALID"));
    for (size_t bit = 0; bit < arrayAddressBits(depth); ++bit) {
        const auto pin = "SELECT_" + std::to_string(bit);
        w.connect(decoder->getOutputPin(pin), read->getInputPin(pin));
    }
    if constexpr (!Traits::readonly) {
        for (const auto* pin : {"WRITE_DATA", "WRITE_MASK", "CLK", "RST"})
            w.connect(this->getInputPinDynamic(pin), storage->getInputPinDynamic(pin));
    }
    for (size_t word = 0; word < depth; ++word) {
        const auto suffix = std::to_string(word);
        if constexpr (!Traits::readonly) {
            w.connect(decoder->getOutputPin("VALID_" + suffix), control->getInputPin("VALID_" + suffix));
            w.connect(control->getOutputPin("WRITE_" + suffix), storage->getInputPin("WRITE_" + suffix));
        }
        w.connect(storage->template getOutputPin<Width>("WORD_" + suffix), read->template getInputPin<Width>("WORD_" + suffix));
    }
    w.connect(read->template getOutputPin<Width>("READ_DATA"), this->template getOutputPin<Width>("READ_DATA"));
    w.finish();
}
