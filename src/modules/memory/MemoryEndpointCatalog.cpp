#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryContainer.hpp"

namespace circuit::memory {
namespace {
void add(ComponentCatalog& c, const std::string& id, const std::string& type,
         memory_device::Schema schema, ComponentFactory structural, ComponentFactory behavioral,
         const std::string& test, ParameterPredicate support = {}) {
    c.registerContract({id, 1, type, schema({}), {}, "four-state", "transaction-completion", schema});
    for (auto f : {Fidelity::Structural, Fidelity::Behavioral}) {
        auto factory = f == Fidelity::Structural ? structural : behavioral;
        if (!factory) continue;
        ImplementationDescriptor impl;
        impl.id = id + (f == Fidelity::Structural ? ".structural" : ".behavioral");
        impl.contract_id = id; impl.fidelity = f; impl.concrete_type_name = type;
        impl.factory = factory;
        impl.supports = [=](const ParameterMap& p) {
            try { schema(p); return f != Fidelity::Structural || !support || support(p); }
            catch (const std::exception&) { return false; }
        };
        if (f == Fidelity::Behavioral) impl.capabilities = {"memory-state-view"};
        impl.evidence = {VerificationStatus::Verified, {"FF transaction registers plus native structural device"},
            {test}, {"MemoryEndpointContractTest"}, {"MemoryEndpointTimingTest"}, "four-state", "transaction-completion"};
        if (id.starts_with("memory.controller.")) impl.evidence.contract_tests.push_back("MemoryControllerTest");
        c.registerImplementation(std::move(impl));
    }
}
}
void registerMemoryEndpoints(ComponentCatalog& c) {
    for (size_t width : {8,16,32}) {
        const auto suffix = std::to_string(width);
        add(c, "memory.transaction-port." + suffix, "MemoryTransactionPort<" + suffix + ">",
            [width](const ParameterMap& p) {
                memory_device::keys(p, {});
                return memory_device::describe([width](IOComponent* s) { defineMemoryTransactionPortPins(s,width); });
            }, [width](const std::string& n, const ParameterMap&, const std::shared_ptr<BuildContext>& ctx) {
                return Component::createWithContext<MemoryTransactionPort>(ctx,n,width);
            }, {}, "MemoryTransactionPortTest");
        for (auto t : {Technology::FF, Technology::SRAM, Technology::DRAM, Technology::ROM, Technology::NORFlash, Technology::Container}) {
            const std::string name = technologyName(t), dims = "<" + name + "," + suffix + ">";
            const std::string test = t == Technology::FF ? "MemoryEndpointFFTest" : t == Technology::SRAM ? "MemoryEndpointSRAMTest" :
                t == Technology::DRAM ? "MemoryEndpointDRAMTest" : t == Technology::ROM ? "MemoryEndpointROMTest" :
                t == Technology::NORFlash ? "MemoryEndpointFlashTest" : "MemoryEndpointRelayTest";
            auto schema = [=](const ParameterMap& p) {
                endpointConfig(p).validate(width,t);
                return memory_device::describe([=](IOComponent* s) { protocolPins(s,width); });
            };
            ComponentFactory compact = [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                return Component::createWithContext<BehavioralMemoryContainer>(ctx,n,width,t,endpointConfig(p));
            };
            add(c, "memory.controller." + name + "." + suffix, "MemoryController" + dims,
                [=](const ParameterMap& p) {
                    endpointConfig(p).validate(width,t);
                    return memory_device::describe([=](IOComponent* s) { protocolPins(s,width); nativePins(s,width,t,true); });
                }, [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                    return Component::createWithContext<MemoryController>(ctx,n,width,t,endpointConfig(p));
                }, {}, test);
            add(c, "memory.endpoint." + name + "." + suffix, "MemoryEndpoint" + dims, schema,
                [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                    return Component::createWithContext<MemoryEndpoint>(ctx,n,width,t,endpointConfig(p));
                }, t == Technology::Container ? ComponentFactory{} : compact, test,
                [=](const ParameterMap& p) { endpointConfig(p).validate(width,t,t != Technology::Container); return true; });
            if (t != Technology::Container)
                add(c, "memory.scalable." + name + "." + suffix, "MemoryContainer" + dims, schema,
                    {}, compact, "ScalableMemoryContainerTest");
        }
    }
}
}
