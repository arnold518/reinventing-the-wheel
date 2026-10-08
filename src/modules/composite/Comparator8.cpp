#include "components/StructuralNetBuilder.hpp"
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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<XOR8>("XOR");
    builder.addNewComponent<ZeroDetect8>("ZERO");

    w.connect(getInputPin<8>("A"),
        {builder.getInputPin<XOR8, 8>("XOR", "A")});
    w.connect(getInputPin<8>("B"),
        {builder.getInputPin<XOR8, 8>("XOR", "B")});
    w.connect(builder.getOutputPin<XOR8, 8>("XOR", "OUT"),
        {builder.getInputPin<ZeroDetect8, 8>("ZERO", "A")});
    w.connect(builder.getOutputPin<ZeroDetect8>("ZERO", "ZERO"),
        {getOutputPin("EQ")});
    w.finish();
}
