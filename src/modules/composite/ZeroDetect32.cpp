#include "modules/composite/ZeroDetect32.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "simulator/Simulator.hpp"
#include <string>
#include <utility>

namespace {
void defineZeroDetect32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin("ZERO", PinType::OUTPUT);
}

class ZeroDetect32Direct final : public BasicComponent {
public:
    explicit ZeroDetect32Direct(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::ZeroDetect32.pinInitializer()) {}

    static constexpr const char* TypeName = "ZeroDetect32";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        bool has_unknown = false;
        for (const auto value :
             getInputPin<32>("A")->getValueAsVector()) {
            if (value == LogicValue::HIGH) {
                _updateOutputWire(
                    simulator, "ZERO", LogicValue::LOW, current_time);
                return;
            }
            if (value != LogicValue::LOW) {
                has_unknown = true;
            }
        }
        _updateOutputWire(
            simulator,
            "ZERO",
            has_unknown ? LogicValue::UNKNOWN : LogicValue::HIGH,
            current_time);
    }
};
}

namespace circuit::families {
const ComponentFamily ZeroDetect32{
    "compare.zero.width32",
    "ZeroDetect32",
    defineZeroDetect32Pins,
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ZeroDetect32>(context, name);
    },
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<ZeroDetect32Direct>(context, name);
    }};
}

ZeroDetect32::ZeroDetect32(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::ZeroDetect32.pinInitializer()) {}

void ZeroDetect32::buildInternals(ComponentBuilder& builder) {
    circuit::memory_detail::StructuralWiring w(builder);
    auto bits=builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    w.connect(getInputPin<32>("A"),bits->getInputPin<32>("IN"));
    std::vector<circuit::memory_detail::StructuralWiring::PinPtr> terms;
    for(size_t i=0;i<32;++i) terms.push_back(bits->getOutputPin("OUT_"+std::to_string(i)));
    w.connect(w.invert("NOT_ANY",w.reduce("ANY_BIT",terms,false)),getOutputPin("ZERO"));
    w.finish();
}
