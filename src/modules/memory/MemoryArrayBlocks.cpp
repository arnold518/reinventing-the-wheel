#include "modules/memory/MemoryArrayBlocks.hpp"

MemoryAddressDecoder::MemoryAddressDecoder(std::string name, size_t depth, bool readonly)
    : IOComponent(std::move(name), [depth, readonly](IOComponent* self) {
        self->addPin<32>("WORD_ADDR", PinType::INPUT);
        self->addPin("IN_RANGE", PinType::OUTPUT);
        for (size_t bit = 0; bit < circuit::memory_detail::arrayAddressBits(depth); ++bit)
            self->addPin("SELECT_" + std::to_string(bit), PinType::OUTPUT);
        if (!readonly) for (size_t word = 0; word < depth; ++word)
            self->addPin("VALID_" + std::to_string(word), PinType::OUTPUT);
    }), depth_(depth), readonly_(readonly),
      type_("MemoryAddressDecoder<" + std::to_string(depth) + (readonly ? ",RO>" : ",RW>")) {
    circuit::memory_detail::arrayAddressBits(depth);
}

void MemoryAddressDecoder::buildInternals(ComponentBuilder& b) {
    using namespace circuit::memory_detail;
    StructuralWiring w(b);
    auto address = b.addNewComponent<BitSplitter<32>>("ADDRESS");
    w.connect(getInputPin<32>("WORD_ADDR"), address->getInputPin<32>("IN"));
    const auto bits = arrayAddressBits(depth_);
    std::vector<StructuralWiring::PinPtr> positive, negative, high, selected;
    for (size_t bit = 0; bit < 32; ++bit) {
        auto pin = address->getOutputPin("OUT_" + std::to_string(bit));
        if (bit < bits) {
            positive.push_back(pin);
            negative.push_back(w.invert("NOT_A" + std::to_string(bit), pin));
            w.connect(pin, getOutputPin("SELECT_" + std::to_string(bit)));
        } else high.push_back(pin);
    }
    const auto highValid = w.invert("HIGH_VALID", w.reduce("HIGH", high, false));
    // A single word needs full-address validation but no word decoder.
    if (depth_ > 1) for (size_t word = 0; word < depth_; ++word) {
        selected.push_back(decodeMatch(w, "WORD_" + std::to_string(word) + "_SELECT",
                                       positive, negative, word));
    }
    const auto inRange = (size_t{1} << bits) == depth_ ? highValid :
        w.both("IN_RANGE", highValid, w.reduce("LOW_VALID", selected, false));
    w.connect(inRange, getOutputPin("IN_RANGE"));
    if (!readonly_) for (size_t word = 0; word < depth_; ++word) {
        auto valid = depth_ == 1 ? inRange : w.both("WORD_" + std::to_string(word) + "_VALID", selected[word], highValid);
        w.connect(valid, getOutputPin("VALID_" + std::to_string(word)));
    }
    w.finish();
}

MemoryAccessControl::MemoryAccessControl(std::string name, size_t depth, bool readonly)
    : IOComponent(std::move(name), [depth, readonly](IOComponent* self) {
        for (const auto* pin : {"IN_RANGE", "READ_EN", "WE"}) self->addPin(pin, PinType::INPUT);
        for (const auto* pin : {"READ_VALID", "FAULT"}) self->addPin(pin, PinType::OUTPUT);
        if (!readonly) for (size_t word = 0; word < depth; ++word) {
            self->addPin("VALID_" + std::to_string(word), PinType::INPUT);
            self->addPin("WRITE_" + std::to_string(word), PinType::OUTPUT);
        }
    }), depth_(depth), readonly_(readonly),
      type_("MemoryAccessControl<" + std::to_string(depth) + (readonly ? ",RO>" : ",RW>")) {
    circuit::memory_detail::arrayAddressBits(depth);
}

void MemoryAccessControl::buildInternals(ComponentBuilder& b) {
    circuit::memory_detail::StructuralWiring w(b);
    w.connect(w.both("READ_VALID", getInputPin("READ_EN"), getInputPin("IN_RANGE")), getOutputPin("READ_VALID"));
    auto active = w.either("ACTIVE", getInputPin("READ_EN"), getInputPin("WE"));
    auto rangeFault = w.both("RANGE_FAULT", active, w.invert("OUT_OF_RANGE", getInputPin("IN_RANGE")));
    w.connect(readonly_ ? w.either("FAULT_OUT", rangeFault, getInputPin("WE")) : rangeFault, getOutputPin("FAULT"));
    if (!readonly_) for (size_t word = 0; word < depth_; ++word) {
        const auto suffix = std::to_string(word);
        w.connect(w.both("WORD_" + suffix + "_WRITE", getInputPin("VALID_" + suffix), getInputPin("WE")),
                  getOutputPin("WRITE_" + suffix));
    }
    w.finish();
}
