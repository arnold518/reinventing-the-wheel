#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"

namespace circuit::families {
template<size_t Width> const ComponentFamily& FFWord();
}

template<size_t Width>
void defineFFWordPins(IOComponent* self) {
    static_assert(Width == 8 || Width == 16 || Width == 32);
    self->addPin<Width>("WRITE_DATA", PinType::INPUT);
    self->addPin<Width / 8>("WRITE_MASK", PinType::INPUT);
    for (const auto* pin : {"WE", "CLK", "RST"}) self->addPin(pin, PinType::INPUT);
    self->addPin<Width>("Q", PinType::OUTPUT);
}

// A byte-masked word of flip-flops, not a universal physical memory row.
template<size_t Width>
class FFWord : public IOComponent {
public:
    using CellType = FlipFlopCell;
    inline static const std::string TypeName = "FFWord<" + std::to_string(Width) + ">";
    const char* getTypeName() const override { return TypeName.c_str(); }
    explicit FFWord(std::string name)
        : IOComponent(std::move(name), defineFFWordPins<Width>) {}
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        StructuralWiring w(b);
        auto output = b.addNewComponent<BitJoiner<Width>>("OUTPUT");
        auto data = b.addNewComponent<BitSplitter<Width>>("DATA");
        auto mask = b.addNewComponent<BitSplitter<Width / 8>>("MASK");
        w.connect(this->template getInputPin<Width>("WRITE_DATA"), data->template getInputPin<Width>("IN"));
        w.connect(this->template getInputPin<Width / 8>("WRITE_MASK"), mask->template getInputPin<Width / 8>("IN"));
        for (size_t byte = 0; byte < Width / 8; ++byte) {
            auto enable = w.both("BYTE_" + std::to_string(byte), this->getInputPin("WE"),
                                mask->getOutputPin("OUT_" + std::to_string(byte)));
            for (size_t bit = byte * 8; bit < (byte + 1) * 8; ++bit) {
                auto cell = b.add(circuit::families::FlipFlopCell, "CELL_" + std::to_string(bit));
                w.connect(data->getOutputPin("OUT_" + std::to_string(bit)), cell->getInputPin("D"));
                w.connect(enable, cell->getInputPin("WE"));
                w.connect(this->getInputPin("CLK"), cell->getInputPin("CLK"));
                w.connect(this->getInputPin("RST"), cell->getInputPin("RST"));
                w.connect(cell->getOutputPin("Q"), output->getInputPin("IN_" + std::to_string(bit)));
            }
        }
        w.connect(output->template getOutputPin<Width>("OUT"), this->template getOutputPin<Width>("Q"));
        w.finish();
    }
};
