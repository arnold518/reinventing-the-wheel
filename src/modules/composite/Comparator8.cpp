#include "modules/composite/Comparator8.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/utility/BitAdapter.hpp"

EqualityChecker8::EqualityChecker8(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("EQ", PinType::OUTPUT);
    }) {}

void EqualityChecker8::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<XOR8>("XOR");
    builder.addNewComponent<ZeroDetect8>("ZERO");

    builder.addNewWire<8>(
        "A_bus_internal",
        getInputPin<8>("A"),
        {builder.getInputPin<XOR8, 8>("XOR", "A")});
    builder.addNewWire<8>(
        "B_bus_internal",
        getInputPin<8>("B"),
        {builder.getInputPin<XOR8, 8>("XOR", "B")});
    builder.addNewWire<8>(
        "XOR_to_ZERO",
        builder.getOutputPin<XOR8, 8>("XOR", "OUT"),
        {builder.getInputPin<ZeroDetect8, 8>("ZERO", "A")});
    builder.addNewWire(
        "ZERO_to_EQ",
        builder.getOutputPin<ZeroDetect8>("ZERO", "ZERO"),
        {getOutputPin("EQ")});
}
