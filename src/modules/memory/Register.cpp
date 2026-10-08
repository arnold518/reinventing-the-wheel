#include "modules/memory/Register.hpp"
#include "basic/SupportedWidths.hpp"
#include "modules/memory/RegisterBehavioral.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"

namespace {
void definePins(IOComponent* self, size_t width) {
    self->addPinDynamic("D", PinType::INPUT, width);
    self->addPinDynamic("Q", PinType::OUTPUT, width);
    for (const auto* pin : {"CLK", "RST", "WE"}) self->addPin(pin, PinType::INPUT);
}
}

template<size_t Width>
Register<Width>::Register(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) { definePins(self, Width); }) {
    static_assert(Width == 1 || Width == 2 || Width == 3 || Width == 4 ||
                  Width == 5 || Width == 8 || Width == 16 || Width == 32);
}

template<size_t Width>
void Register<Width>::buildInternals(ComponentBuilder& builder) {
    circuit::memory_detail::StructuralWiring wires(builder);
    std::shared_ptr<IOComponent> split, join;
    if constexpr (Width != 1) {
        split = builder.addNewComponent<BitSplitter<Width>>("INPUT_BITS");
        join = builder.addNewComponent<BitJoiner<Width>>("OUTPUT_BITS");
        wires.connect(this->getInputPinDynamic("D"), split->getInputPinDynamic("IN"));
        wires.connect(join->getOutputPinDynamic("OUT"), this->getOutputPinDynamic("Q"));
    }
    for (size_t bit = 0; bit < Width; ++bit) {
        auto cell = builder.add(circuit::families::FlipFlopCell, "BIT_" + std::to_string(bit));
        for (const auto* pin : {"CLK", "RST", "WE"})
            wires.connect(this->getInputPin(pin), cell->getInputPin(pin));
        wires.connect(split ? split->getOutputPin("OUT_" + std::to_string(bit)) : this->getInputPin("D"),
                      cell->getInputPin("D"));
        wires.connect(cell->getOutputPin("Q"),
                      join ? join->getInputPin("IN_" + std::to_string(bit)) : this->getOutputPin("Q"));
    }
    wires.finish();
}

namespace circuit::families {
template<size_t Width>
const ComponentFamily& registerFamily() {
    static const std::string id = "storage.register." + std::to_string(Width);
    static const std::string type = Register<Width>::TypeName;
    static const ComponentFamily family{
        id, type, [](IOComponent* self) { definePins(self, Width); },
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<::Register<Width>>(context, name);
        },
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<RegisterBehavioral>(context, name, Width);
        }};
    return family;
}

const ComponentFamily& registerWord(size_t width) {
    return dispatchWidth(width,
        []<size_t WIDTH>() -> const ComponentFamily& { return registerFamily<WIDTH>(); },
        []() -> const ComponentFamily& { throw std::invalid_argument("Unsupported register width"); });
}
}

template class Register<1>;
template class Register<2>;
template class Register<3>;
template class Register<4>;
template class Register<5>;
template class Register<8>;
template class Register<16>;
template class Register<32>;
