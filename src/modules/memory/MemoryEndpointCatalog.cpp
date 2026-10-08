#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"

namespace circuit::memory {
namespace {
void add(ComponentCatalog& c, const std::string& id, const std::string& type,
         memory_device::Schema schema, ComponentFactory structural, ComponentFactory behavioral,
         const std::string& test, ParameterPredicate support = {}) {
    ContractDescriptor contract{id, 2, type, schema({}), {}, "four-state", "transaction-completion-v2", schema};
    if (id.find(".scalable-endpoint.") != std::string::npos) contract.preferred_fidelity = Fidelity::Behavioral;
    c.registerContract(std::move(contract));
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
        // A controller owns no storage, so it has no memory state to show.
        if (f == Fidelity::Behavioral && id.find(".controller.") == std::string::npos)
            impl.capabilities = {"memory-state-view"};
        impl.evidence = {VerificationStatus::Verified, {"FF transaction registers plus native structural device"},
            {test}, {"MemoryEndpointContractTest"}, {"MemoryEndpointTimingTest"}, "four-state", "transaction-completion-v2"};
        if (id.find(".controller.") != std::string::npos) {
            // One logical component test owns the controller contract. Device
            // endpoint scenarios remain integration evidence, not a second owner.
            impl.evidence.contract_tests = {"MemoryControllerTest"};
            impl.evidence.representative_tests.push_back(test);
        }
        c.registerImplementation(std::move(impl));
    }
}

// Handshake state and payload registers used by the transaction components.
void registerClockedStateBit(ComponentCatalog& c) {
    const auto& state_bit = families::ClockedStateBit;
    const auto state_id = std::string(state_bit.id());
    c.registerContract({state_id, 1, "Clocked state bit", memory_device::describe(
        state_bit.pinInitializer()), {}, "four-state", "clock-boundary"});
    for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        ImplementationDescriptor implementation;
        implementation.id = state_id + (fidelity == Fidelity::Structural ? ".structural.dff" : ".behavioral.direct");
        implementation.contract_id = state_id;
        implementation.fidelity = fidelity;
        implementation.concrete_type_name = "ClockedStateBit";
        implementation.factory = [fidelity](const std::string& name, const ParameterMap&,
                                 const std::shared_ptr<BuildContext>& context) {
            return families::ClockedStateBit.create(fidelity, name, context);
        };
        implementation.evidence = {VerificationStatus::Characterized,
            {"DFlipFlop master/slave storage"}, {"ClockedStateBitTest"},
            {"ClockedStateBitTest"}, {}, "four-state", "clock-boundary"};
        c.registerImplementation(std::move(implementation));
    }
}

void registerRegisterWords(ComponentCatalog& c) {
    for (size_t width : {1,2,3,4,5,8,16,32}) {
        const auto& family = families::registerWord(width);
        const auto id = std::string(family.id());
        c.registerContract({id, 1, std::string(family.typeName()),
            memory_device::describe(family.pinInitializer()), {}, "four-state", "clock-boundary"});
        for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
            ImplementationDescriptor implementation;
            implementation.id = id + (fidelity == Fidelity::Structural ? ".structural.bits" : ".behavioral.word");
            implementation.contract_id = id;
            implementation.fidelity = fidelity;
            implementation.concrete_type_name = std::string(family.typeName());
            implementation.factory = [width, fidelity](const std::string& name, const ParameterMap&,
                                    const std::shared_ptr<BuildContext>& context) {
                return families::registerWord(width).create(fidelity, name, context);
            };
            implementation.evidence = {VerificationStatus::Characterized,
                {"FlipFlopCell structural lanes with one shared D/Q bus"},
                {"RegisterWidthsTest"},
                {"RegisterWidthsTest"}, {}, "four-state", "clock-boundary"};
            c.registerImplementation(std::move(implementation));
        }
    }
}
}
void registerMemoryEndpoints(ComponentCatalog& c) {
    registerClockedStateBit(c);
    registerRegisterWords(c);
    for (size_t width : {8,16,32}) {
        const auto suffix = std::to_string(width);
        add(c,"memory.shared.response-buffer."+suffix,"MemoryResponseBuffer<"+suffix+">",
            [width](const ParameterMap& p){memory_device::keys(p,{});return memory_device::describe(
                [width](IOComponent* s){defineMemoryResponseBufferPins(s,width);});},
            [width](const std::string& n,const ParameterMap&,const std::shared_ptr<BuildContext>& ctx){
                return Component::createWithContext<MemoryResponseBuffer>(ctx,n,width);
            },{},"MemoryResponseBufferTest");
        add(c, "memory.shared.transaction-port." + suffix, "MemoryTransactionPort<" + suffix + ">",
            [width](const ParameterMap& p) {
                memory_device::keys(p, {});
                return memory_device::describe([width](IOComponent* s) { defineMemoryTransactionPortPins(s,width); });
            }, [width](const std::string& n, const ParameterMap&, const std::shared_ptr<BuildContext>& ctx) {
                return Component::createWithContext<MemoryTransactionPort>(ctx,n,width);
            }, {}, "MemoryTransactionPortTest");
        for (auto t : {Technology::FF, Technology::SRAM, Technology::DRAM, Technology::ROM, Technology::NORFlash}) {
            const std::string name = technologyName(t), dims = "<" + name + "," + suffix + ">";
            const std::string test = t == Technology::FF ? "MemoryEndpointFFTest" : t == Technology::SRAM ? "MemoryEndpointSRAMTest" :
                t == Technology::DRAM ? "MemoryEndpointDRAMTest" : t == Technology::ROM ? "MemoryEndpointROMTest" :
                "MemoryEndpointFlashTest";
            auto schema = [=](const ParameterMap& p) {
                endpointConfig(p).validate(width,t);
                return memory_device::describe([=](IOComponent* s) { protocolPins(s,width); });
            };
            ComponentFactory compact = [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                return Component::createWithContext<BehavioralMemoryEndpoint>(ctx,n,width,t,endpointConfig(p));
            };
            add(c, "memory." + name + ".controller." + suffix, "MemoryController" + dims,
                [=](const ParameterMap& p) {
                    endpointConfig(p).validate(width,t);
                    return memory_device::describe([=](IOComponent* s) { protocolPins(s,width); nativePins(s,width,t,true); });
                }, [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                    return Component::createWithContext<MemoryController>(ctx,n,width,t,endpointConfig(p));
                }, [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                    return createBehavioralMemoryController(n,width,t,endpointConfig(p),ctx);
                }, test);
            add(c, "memory." + name + ".endpoint." + suffix, "MemoryEndpoint" + dims, schema,
                [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                    return Component::createWithContext<MemoryEndpoint>(ctx,n,width,t,endpointConfig(p));
                }, compact, test,
                [=](const ParameterMap& p) { endpointConfig(p).validate(width,t,true); return true; });
            add(c, "memory." + name + ".scalable-endpoint." + suffix, "MemoryEndpoint" + dims, schema,
                    [=](const std::string& n, const ParameterMap& p, const std::shared_ptr<BuildContext>& ctx) {
                        // controller view, never millions of cells
                        return Component::createWithContext<MemoryEndpoint>(ctx,n,width,t,endpointConfig(p),true);
                    }, compact, "ScalableMemoryEndpointTest");
            {
                auto nativeSchema = [=](const ParameterMap& p) {
                    auto config = endpointConfig(p);
                    config.refresh_interval_ticks = 0;
                    config.validate(width,t);
                    return memory_device::describe([=](IOComponent* s) {
                        nativePins(s,width,t,false);
                        if (t == Technology::DRAM) s->addPin("ROW_OPEN",PinType::OUTPUT);
                        if (t == Technology::NORFlash) s->addPin("WRITE_ENABLED",PinType::OUTPUT);
                    });
                };
                memory_device::registerBlock(c,"memory." + name + ".scalable-device." + suffix,
                    "MemoryContainer" + dims,nativeSchema,{},
                    [=](const std::string& n,const ParameterMap& p,const std::shared_ptr<BuildContext>& ctx) {
                        return createMemoryContainer(n,width,t,endpointConfig(p),ctx);
                    },"NativeMemoryContainerTest");
            }
        }
    }
}
}
