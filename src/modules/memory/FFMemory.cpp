#include "modules/memory/FFMemory.hpp"
#include "modules/memory/FFWordBehavioral.hpp"
#include "modules/memory/FFMemoryBehavioral.hpp"
#include "modules/memory/FFStorageBankBehavioral.hpp"
#include "modules/memory/FFMemoryCatalog.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <charconv>

size_t ffStorageBankDepthFromParameters(const circuit::ParameterMap& parameters) {
    size_t depth = 4;
    for (const auto& [key, value] : parameters) {
        if (key != "depth_words") throw std::invalid_argument("Unsupported FF bank parameter: " + key);
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), depth);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !depth)
            throw std::invalid_argument("Invalid FF bank depth_words");
    }
    return depth;
}

namespace circuit::families {
template<size_t Width>
const ComponentFamily& FFStorageBank() {
    static const std::string id = "memory.ff-storage-bank." + std::to_string(Width);
    static const std::string type = "FFStorageBank<" + std::to_string(Width) + ">";
    static const ComponentFamily family{
        id, type, [](IOComponent* self) { defineFFStorageBankPins<Width>(self, 4); },
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            const auto depth = context ? ffStorageBankDepthFromParameters(context->parameters()) : 4;
            return Component::createWithContext<::FFStorageBank<Width>>(context, name, depth);
        },
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            const auto depth = context ? ffStorageBankDepthFromParameters(context->parameters()) : 4;
            return Component::createWithContext<FFStorageBankBehavioral<Width>>(context, name, depth);
        }};
    return family;
}

template<size_t Width>
const ComponentFamily& FFWord() {
    static const std::string id = "memory.ff-word." + std::to_string(Width);
    static const ComponentFamily family{
        id, ::FFWord<Width>::TypeName, defineFFWordPins<Width>,
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<::FFWord<Width>>(context, name);
        },
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<FFWordBehavioral<Width>>(context, name);
        }};
    return family;
}

template<size_t Width>
const ComponentFamily& FFMemory() {
    // Persisted profiles and callers keep the original device contract ID.
    static const std::string id = "memory.container.ff." + std::to_string(Width);
    static const ComponentFamily family{
        id, ::FFMemory<Width>::TypeName, defineMemoryArrayPins<Width>,
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            const auto config = context ? ffMemoryConfigFromParameters(context->parameters()) : MemoryConfig{};
            return Component::createWithContext<::FFMemory<Width>>(context, name, config);
        },
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            const auto config = context ? ffMemoryConfigFromParameters(context->parameters()) : MemoryConfig{};
            return Component::createWithContext<FFMemoryBehavioral<Width>>(context, name, config);
        }};
    return family;
}

template const ComponentFamily& FFWord<8>();
template const ComponentFamily& FFWord<16>();
template const ComponentFamily& FFWord<32>();
template const ComponentFamily& FFStorageBank<8>();
template const ComponentFamily& FFStorageBank<16>();
template const ComponentFamily& FFStorageBank<32>();
template const ComponentFamily& FFMemory<8>();
template const ComponentFamily& FFMemory<16>();
template const ComponentFamily& FFMemory<32>();
}

namespace {
template<size_t Width>
void registerBank(circuit::ComponentCatalog& catalog) {
    using namespace circuit;
    const auto& family = families::FFStorageBank<Width>();
    const auto id = std::string(family.id());
    const auto schema = [](const ParameterMap& p) {
        const auto depth = validateFFStorageBankDepth<Width>(ffStorageBankDepthFromParameters(p));
        std::vector<PinDescriptor> pins{{"WRITE_DATA", Width, PinType::INPUT},
            {"WRITE_MASK", Width / 8, PinType::INPUT}, {"CLK", 1, PinType::INPUT}, {"RST", 1, PinType::INPUT}};
        for (size_t n = 0; n < depth; ++n) {
            pins.push_back({"WRITE_" + std::to_string(n), 1, PinType::INPUT});
            pins.push_back({"WORD_" + std::to_string(n), Width, PinType::OUTPUT});
        }
        return pins;
    };
    catalog.registerContract({id, 1, std::string(family.typeName()), schema({}), {},
        "known-binary", "stable-after-settle", schema});
    for (const auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        ImplementationDescriptor impl;
        impl.id = id + (fidelity == Fidelity::Structural ? ".structural.words" : ".behavioral.direct");
        impl.contract_id = id;
        impl.fidelity = fidelity;
        impl.concrete_type_name = std::string(family.typeName());
        impl.supports = [](const ParameterMap& p) {
            try { validateFFStorageBankDepth<Width>(ffStorageBankDepthFromParameters(p)); return true; }
            catch (const std::exception&) { return false; }
        };
        impl.factory = [fidelity](const std::string& name, const ParameterMap&, const std::shared_ptr<BuildContext>& context) {
            return families::FFStorageBank<Width>().create(fidelity, name, context);
        };
        impl.evidence = {VerificationStatus::Verified, {"parallel structural FFWords with independent enables"},
            {"FFStorageBankTest"}, {"FFStorageBankContractTest", "FFMemoryFidelityTest"}, {},
            "known-binary", "stable-after-settle"};
        catalog.registerImplementation(std::move(impl));
    }
}

template<size_t Width>
void registerWord(circuit::ComponentCatalog& catalog) {
    using namespace circuit;
    const auto& family = families::FFWord<Width>();
    const auto id = std::string(family.id());
    auto schema = Component::create<IOComponent>("SCHEMA", family.pinInitializer());
    std::vector<PinDescriptor> pins;
    for (const auto& [name, pin] : schema->getAllInputPins()) pins.push_back({name, pin->getWidth(), PinType::INPUT});
    for (const auto& [name, pin] : schema->getAllOutputPins()) pins.push_back({name, pin->getWidth(), PinType::OUTPUT});
    catalog.registerContract({id, 1, ::FFWord<Width>::TypeName, pins, {}, "known-binary", "stable-after-settle"});
    for (const auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        ImplementationDescriptor impl;
        impl.id = id + (fidelity == Fidelity::Structural ? ".structural.cells" : ".behavioral.direct");
        impl.contract_id = id;
        impl.fidelity = fidelity;
        impl.concrete_type_name = ::FFWord<Width>::TypeName;
        impl.supports = [](const ParameterMap& p) { return p.empty(); };
        impl.factory = [fidelity](const std::string& name, const ParameterMap&, const std::shared_ptr<BuildContext>& context) {
            return families::FFWord<Width>().create(fidelity, name, context);
        };
        impl.evidence = {VerificationStatus::Verified, {"byte enables and FlipFlopCell structural words"},
            {"FFWordTest"}, {"FFWordContractTest"}, {}, "known-binary", "stable-after-settle"};
        catalog.registerImplementation(std::move(impl));
    }
}
}

void registerFFMemoryWords(circuit::ComponentCatalog& catalog) {
    registerWord<8>(catalog);
    registerWord<16>(catalog);
    registerWord<32>(catalog);
    registerBank<8>(catalog);
    registerBank<16>(catalog);
    registerBank<32>(catalog);
}
