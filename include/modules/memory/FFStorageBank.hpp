#pragma once

#include "modules/memory/FFWord.hpp"

namespace circuit::families {
template<size_t Width> const ComponentFamily& FFStorageBank();
}

size_t ffStorageBankDepthFromParameters(const circuit::ParameterMap& parameters);

template<size_t Width>
size_t validateFFStorageBankDepth(size_t depth) {
    static_assert(Width == 8 || Width == 16 || Width == 32);
    if (!depth || depth > 4096 / Width)
        throw std::invalid_argument("FFStorageBank requires 1..4096/Width words");
    return depth;
}

template<size_t Width>
void defineFFStorageBankPins(IOComponent* self, size_t depth) {
    validateFFStorageBankDepth<Width>(depth);
    self->addPin<Width>("WRITE_DATA", PinType::INPUT);
    self->addPin<Width / 8>("WRITE_MASK", PinType::INPUT);
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    for (size_t word = 0; word < depth; ++word) {
        self->addPin("WRITE_" + std::to_string(word), PinType::INPUT);
        self->addPin<Width>("WORD_" + std::to_string(word), PinType::OUTPUT);
    }
}

template<size_t Width>
std::string ffStorageBankTypeName(size_t depth) {
    return "FFStorageBank<" + std::to_string(Width) + "," + std::to_string(depth) + ">";
}

// Selectable storage grouping. Device address decoding stays outside the bank.
template<size_t Width>
class FFStorageBank : public IOComponent {
public:
    FFStorageBank(std::string name, size_t depth)
        : IOComponent(std::move(name), [depth](IOComponent* self) {
            defineFFStorageBankPins<Width>(self, depth);
        }), depth_(validateFFStorageBankDepth<Width>(depth)), type_(ffStorageBankTypeName<Width>(depth)) {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        circuit::memory_detail::StructuralWiring w(b);
        for (size_t word = 0; word < depth_; ++word) {
            const auto suffix = std::to_string(word);
            auto storage = b.add(circuit::families::FFWord<Width>(), "WORD_" + suffix);
            for (const auto* pin : {"WRITE_DATA", "WRITE_MASK", "CLK", "RST"})
                w.connect(this->getInputPinDynamic(pin), storage->getInputPinDynamic(pin));
            w.connect(this->getInputPin("WRITE_" + suffix), storage->getInputPin("WE"));
            w.connect(storage->template getOutputPin<Width>("Q"), this->template getOutputPin<Width>("WORD_" + suffix));
        }
        w.finish();
    }
private:
    size_t depth_;
    std::string type_;
};
