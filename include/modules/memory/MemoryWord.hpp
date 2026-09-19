#pragma once

#include "components/IOComponent.hpp"
#include "modules/memory/MemoryCellTraits.hpp"
#include "modules/memory/FFWord.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <type_traits>

// One actual storage word. Array address selection stays outside this block;
// byte enables and the selected cell's storage/reset behavior stay inside it.
template<size_t Width, class Cell>
class MemoryWord : public IOComponent {
    static_assert(Width == 8 || Width == 16 || Width == 32);
    static_assert(std::is_base_of_v<IOComponent, Cell>);
    using Traits = circuit::memory_detail::MemoryCellTraits<Cell>;
public:
    using CellType = Cell;
    inline static const std::string TypeName = std::string("MemoryWord<") + Traits::array_name + "," + std::to_string(Width) + ">";
    const char* getTypeName() const override { return TypeName.c_str(); }
    explicit MemoryWord(std::string name) : IOComponent(std::move(name), [](IOComponent* self) {
        if constexpr (!Traits::readonly) {
            self->addPin<Width>("WRITE_DATA", PinType::INPUT);
            self->addPin<Width / 8>("WRITE_MASK", PinType::INPUT);
            for (const auto* pin : {"WE", "CLK", "RST"}) self->addPin(pin, PinType::INPUT);
        }
        self->addPin<Width>("Q", PinType::OUTPUT);
    }) {}
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        StructuralWiring w(b);
        auto output = b.addNewComponent<BitJoiner<Width>>("OUTPUT");
        std::shared_ptr<BitSplitter<Width>> data;
        std::shared_ptr<BitSplitter<Width / 8>> mask;
        StructuralWiring::PinPtr zero;
        if constexpr (!Traits::readonly) {
            // Preserve the original one-tick input splitter paths. Q packing
            // is zero-delay, so introducing the word boundary adds no latency.
            data = b.addNewComponent<BitSplitter<Width>>("DATA");
            mask = b.addNewComponent<BitSplitter<Width / 8>>("MASK");
            w.connect(this->template getInputPin<Width>("WRITE_DATA"), data->template getInputPin<Width>("IN"));
            w.connect(this->template getInputPin<Width / 8>("WRITE_MASK"), mask->template getInputPin<Width / 8>("IN"));
            zero = w.constant("ZERO", false);
        }
        auto connect = [&](auto source, auto sink) { w.connect(source, sink); };
        for (size_t byte = 0; byte < Width / 8; ++byte) {
            StructuralWiring::PinPtr enable;
            if constexpr (!Traits::readonly)
                enable = w.both("BYTE_" + std::to_string(byte), this->getInputPin("WE"),
                                mask->getOutputPin("OUT_" + std::to_string(byte)));
            for (size_t bit = byte * 8; bit < (byte + 1) * 8; ++bit) {
                CellInputs inputs{};
                if constexpr (!Traits::readonly)
                    inputs = {data->getOutputPin("OUT_" + std::to_string(bit)), enable,
                              this->getInputPin("CLK"), this->getInputPin("RST"), zero};
                auto q = Traits::buildCell(b, "CELL_" + std::to_string(bit), inputs, connect);
                w.connect(q, output->getInputPin("IN_" + std::to_string(bit)));
            }
        }
        w.connect(output->template getOutputPin<Width>("OUT"), this->template getOutputPin<Width>("Q"));
        w.finish();
    }
};

// Compatibility entry point. FF storage has its own cell-selection boundary.
template<size_t Width>
class MemoryWord<Width, FlipFlopCell> : public FFWord<Width> {
public:
    using FFWord<Width>::FFWord;
};
