#include "modules/memory/FFMemoryCatalog.hpp"
#include "modules/memory/FFMemory.hpp"
#include "modules/memory/FFMemoryBehavioral.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <charconv>
#include <limits>

MemoryConfig ffMemoryConfigFromParameters(const circuit::ParameterMap& parameters) {
    MemoryConfig config;
    config.storage_limits = circuit::memory_device::storageLimits(parameters);
    for (const auto& [key, value] : parameters) {
        if (key == "paged" || key == "max_resident_bytes" || key == "max_history_bytes") continue;
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
template<size_t Width>
void addFFMemory(circuit::ComponentCatalog& catalog) {
    using namespace circuit;
    const auto id = std::string("memory.ff.device.") + std::to_string(Width);
    auto schema = Component::create<IOComponent>("SCHEMA", defineMemoryArrayPins<Width>);
    std::vector<PinDescriptor> pins;
    for (const auto& [name, pin] : schema->getAllInputPins()) pins.push_back({name, pin->getWidth(), PinType::INPUT});
    for (const auto& [name, pin] : schema->getAllOutputPins()) pins.push_back({name, pin->getWidth(), PinType::OUTPUT});
    catalog.registerContract({id, 1, FFMemoryBehavioral<Width>::TypeName, pins, {}, "known-binary", "stable-after-settle"});
    const std::string test = "FFMemoryTest";
    auto valid = [](const ParameterMap& p) {
        try {
            auto c = ffMemoryConfigFromParameters(p);
            return c.depth_words <= std::numeric_limits<uint32_t>::max() / (Width / 8);
        } catch (const std::exception&) { return false; }
    };
    ImplementationDescriptor behavioral;
    behavioral.id = id + ".behavioral"; behavioral.contract_id = id;
    behavioral.fidelity = Fidelity::Behavioral;
    behavioral.concrete_type_name = FFMemoryBehavioral<Width>::TypeName;
    behavioral.capabilities = {"memory-state-view", "memory-image-load"};
    behavioral.supports = valid;
    behavioral.factory = [](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<FFMemoryBehavioral<Width>>(context, name, ffMemoryConfigFromParameters(p));
    };
    behavioral.evidence = {VerificationStatus::Verified, {"configurable structural FFMemory"},
        {test}, {"FFMemoryContractTest"}, {"FFMemoryContractTest"}, "known-binary", "stable-after-settle"};
    catalog.registerImplementation(std::move(behavioral));
    {
        using Array = FFMemory<Width>;
        ImplementationDescriptor structural;
        // Keep the persisted implementation ID stable for existing profiles.
        structural.id = id + ".structural-slice"; structural.contract_id = id;
        structural.fidelity = Fidelity::Structural;
        structural.concrete_type_name = Array::TypeName;
        structural.supports = [valid](const ParameterMap& p) {
            if (!valid(p)) return false;
            return Array::supportsConfiguration(ffMemoryConfigFromParameters(p));
        };
        structural.factory = [](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<Array>(context, name, ffMemoryConfigFromParameters(p));
        };
        structural.evidence = {VerificationStatus::Verified, {"gates and representative storage cells"},
            {test}, {"FFMemoryContractTest"}, {}, "known-binary", "stable-after-settle"};
        catalog.registerImplementation(std::move(structural));
    }
}

}
void registerFFMemories(circuit::ComponentCatalog& catalog) {
    registerFFMemoryWords(catalog);
    addFFMemory<8>(catalog); addFFMemory<16>(catalog); addFFMemory<32>(catalog);
}
