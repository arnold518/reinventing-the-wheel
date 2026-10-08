#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/ParallelPrefixAdder32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include <string>

namespace {
void defineAdder32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin("Cin", PinType::INPUT);
    self->addPin<32>("Sum", PinType::OUTPUT);
    self->addPin("Cout", PinType::OUTPUT);
}

// A + B + Cin, bit by bit with the full-adder gate rules.
class Adder32Behavioral final : public BasicComponent {
public:
    explicit Adder32Behavioral(std::string name)
        : BasicComponent(std::move(name), 1, defineAdder32Pins) {}
    const char* getTypeName() const override { return Adder32::TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override {
        using namespace circuit::logic_values;
        const auto a = getInputPin<32>("A")->getValueAsVector();
        const auto b = getInputPin<32>("B")->getValueAsVector();
        auto carry = getInputPin("Cin")->getValue();
        std::vector<LogicValue> sum;
        sum.reserve(32);
        for (size_t bit = 0; bit < 32; ++bit) {
            const auto half = xorGate(a[bit], b[bit]);
            sum.push_back(xorGate(half, carry));
            carry = orGate(andGate(a[bit], b[bit]), andGate(half, carry));
        }
        _updateOutputWire<32>(simulator, "Sum", sum, current_time);
        _updateOutputWire(simulator, "Cout", carry, current_time);
    }
};
}

namespace circuit::families {
const ComponentFamily Adder32{
    "arithmetic.adder.width32", "Adder32", defineAdder32Pins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Adder32>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<Adder32Behavioral>(context, name);
    }};
}

Adder32::Adder32(std::string name, bool parallel_prefix)
    : IOComponent(std::move(name), defineAdder32Pins), parallel_prefix_(parallel_prefix) {}

void Adder32::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    if (parallel_prefix_) {
        auto adder=builder.addNewComponent<ParallelPrefixAdder32>("PREFIX_ADDER");
        for (auto p:{"A","B","Cin"}) w.connect(getInputPinDynamic(p),adder->getInputPinDynamic(p));
        for (auto p:{"Sum","Cout"}) w.connect(adder->getOutputPinDynamic(p),getOutputPinDynamic(p));
        w.finish(); return;
    }
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("SUM_JOIN");

    w.connect(getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    w.connect(getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 32; ++i) {
        const auto index = std::to_string(i);
        const auto adder_name = "FA" + index;
        builder.addNewComponent<FullAdder>(adder_name);

        w.connect(builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        w.connect(builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + index),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        w.connect(builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<32>>("SUM_JOIN", "IN_" + index)});
    }

    w.connect(getInputPin("Cin"),
        {builder.getInputPin<FullAdder>("FA0", "Carry_in")});

    for (size_t i = 0; i < 31; ++i) {
        w.connect(builder.getOutputPin<FullAdder>("FA" + std::to_string(i), "Carry_out"),
            {builder.getInputPin<FullAdder>("FA" + std::to_string(i + 1), "Carry_in")});
    }

    w.connect(builder.getOutputPin<FullAdder>("FA31", "Carry_out"),
        {getOutputPin("Cout")});

    w.connect(builder.getOutputPin<BitJoiner<32>, 32>("SUM_JOIN", "OUT"),
        {getOutputPin<32>("Sum")});
    w.finish();
}
