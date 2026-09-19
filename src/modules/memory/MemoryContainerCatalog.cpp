#include "modules/memory/MemoryContainerCatalog.hpp"
#include "modules/memory/MemoryArray.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <charconv>
#include <limits>

MemoryConfig memoryConfigFromParameters(const circuit::ParameterMap& parameters, bool rom) {
    MemoryConfig config;
    if (rom) config.initial_value = LogicValue::LOW;
    for (const auto& [key, value] : parameters) {
        if (key == "initial_value") {
            if (value == "zero") config.initial_value = LogicValue::LOW;
            else if (value == "ones") config.initial_value = LogicValue::HIGH;
            else if (value == "unknown") config.initial_value = LogicValue::UNKNOWN;
            else throw std::invalid_argument("Invalid memory initial_value");
        } else {
            size_t number = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                throw std::invalid_argument("Invalid numeric memory parameter: " + key);
            if (key == "depth_words") config.depth_words = number;
            else if (key == "read_delay_ticks") config.read_delay_ticks = number;
            else if (key == "record_history" && number <= 1) config.record_history = number != 0;
            else throw std::invalid_argument("Unsupported memory parameter: " + key);
        }
    }
    if (!config.depth_words || !config.read_delay_ticks || config.read_delay_ticks > 1'000'000)
        throw std::invalid_argument("Invalid memory depth or delay");
    return config;
}
namespace {
template<size_t Width, class Model, class Cell = void>
void addModel(circuit::ComponentCatalog& catalog, const std::string& model) {
    using namespace circuit;
    const auto id = "memory.container." + model + "." + std::to_string(Width);
    auto schema = Component::create<IOComponent>("SCHEMA", defineMemoryContainerPins<Width, Model>);
    std::vector<PinDescriptor> pins;
    for (const auto& [name, pin] : schema->getAllInputPins()) pins.push_back({name, pin->getWidth(), PinType::INPUT});
    for (const auto& [name, pin] : schema->getAllOutputPins()) pins.push_back({name, pin->getWidth(), PinType::OUTPUT});
    catalog.registerContract({id, 1, LegacyMemoryContainer<Width, Model>::TypeName, pins, {}, "known-binary", "stable-after-settle"});
    const std::string test = model == "sram" ? "SRAMContainerTest" : model == "rom" ? "ROMContainerTest" : "FFMemoryContainerTest";
    auto valid = [](const ParameterMap& p) {
        try {
            auto c = memoryConfigFromParameters(p, Model::readonly);
            return c.depth_words <= std::numeric_limits<uint32_t>::max() / (Width / 8);
        } catch (const std::exception&) { return false; }
    };
    ImplementationDescriptor behavioral;
    behavioral.id = id + ".behavioral"; behavioral.contract_id = id;
    behavioral.fidelity = Fidelity::Behavioral;
    behavioral.concrete_type_name = LegacyMemoryContainer<Width, Model>::TypeName;
    behavioral.capabilities = {"memory-state-view", "memory-image-load"};
    behavioral.supports = valid;
    behavioral.factory = [](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<LegacyMemoryContainer<Width, Model>>(context, name, memoryConfigFromParameters(p, Model::readonly));
    };
    behavioral.evidence = {VerificationStatus::Verified, {"configurable gate/latch/FF structural MemoryArray"},
        {test}, {"MemoryContainerContractTest"}, {"MemoryContainerContractTest"}, "known-binary", "stable-after-settle"};
    catalog.registerImplementation(std::move(behavioral));
    if constexpr (!std::is_void_v<Cell>) {
        using Array = std::conditional_t<std::is_same_v<Cell, FlipFlopCell>, FFMemory<Width>, MemoryArray<Width, Cell>>;
        ImplementationDescriptor structural;
        // Keep the persisted implementation ID stable for existing profiles.
        structural.id = id + ".structural-slice"; structural.contract_id = id;
        structural.fidelity = Fidelity::Structural;
        structural.concrete_type_name = Array::TypeName;
        structural.supports = [valid](const ParameterMap& p) {
            if (!valid(p)) return false;
            return Array::supportsConfiguration(memoryConfigFromParameters(p, Model::readonly));
        };
        structural.factory = [](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<Array>(context, name, memoryConfigFromParameters(p, Model::readonly));
        };
        structural.evidence = {VerificationStatus::Verified, {"gates and representative storage cells"},
            {test}, {"MemoryContainerContractTest"}, {}, "known-binary", "stable-after-settle"};
        catalog.registerImplementation(std::move(structural));
    }
}
template<size_t Width>
void addWidth(circuit::ComponentCatalog& catalog) {
    addModel<Width, FlipFlopArrayModel, FlipFlopCell>(catalog, "ff");
    addModel<Width, SRAMModel, GatedDLatch>(catalog, "sram");
    addModel<Width, ROMModel, ConstantValue<1>>(catalog, "rom");
    addModel<Width, IdealRAMModel>(catalog, "ideal-reference");
}
}
void registerMemoryContainers(circuit::ComponentCatalog& catalog) {
    registerFFMemoryWords(catalog);
    addWidth<8>(catalog); addWidth<16>(catalog); addWidth<32>(catalog);
}
