#include "tests/MemoryContainerTests.hpp"
#include "modules/memory/MemoryArray.hpp"
#include "modules/memory/LegacyMemoryContainer.hpp"
#include "modules/memory/MemoryBusAdapter32.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"

namespace {
using namespace circuit::test;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
ComponentTestSpec adapterSpec() {
    ComponentTestSpec spec;
    spec.id = "MemoryBusAdapter32Test"; spec.contract_id = "memory.bus-adapter.width32";
    spec.instance_name = "MEMORY_BUS_ADAPTER_ROOT";
    ActionScenario scenario; scenario.id = "transactions";
    NamedValues state{{"CLK", logicBit(false)}, {"RST", logicBit(true)}, {"REQ_VALID", logicBit(false)},
        {"REQ_WRITE", logicBit(false)}, {"RSP_READY", logicBit(false)}, {"REQ_ADDR", logicBits(32,0)},
        {"REQ_WRITE_DATA", logicBits(32,0)}, {"REQ_WRITE_MASK", logicBits(4,15)},
        {"MEMORY_READ_DATA", logicBits(32,0x12345678)}, {"MEMORY_FAULT", logicBit(false)}};
    auto step = [&](const std::string& id, NamedValues changes, NamedValues outputs) {
        for (auto& [key, value] : changes) state[key] = value;
        scenario.actions.push_back({id, CheckpointKind::Settled, state, outputs, true, {}});
    };
    step("reset", {}, {{"REQ_READY",logicBit(false)}, {"RSP_VALID",logicBit(false)}});
    step("idle", {{"RST",logicBit(false)}}, {{"REQ_READY",logicBit(true)}});
    for (size_t n = 0; n < 4; ++n) {
        const bool writing = n == 1, misaligned = n == 2, fault = n == 3;
        const auto prefix = "transaction-" + std::to_string(n) + "-";
        step(prefix+"request", {{"CLK",logicBit(false)}, {"REQ_VALID",logicBit(true)}, {"RSP_READY",logicBit(false)},
            {"REQ_WRITE",logicBit(writing)}, {"REQ_ADDR",logicBits(32, misaligned ? 5 : 4)},
            {"REQ_WRITE_DATA",logicBits(32,0xabcdef01)}, {"MEMORY_READ_DATA",logicBits(32,0x12345678)},
            {"MEMORY_FAULT",logicBit(fault)}}, {{"REQ_READY",logicBit(true)}});
        step(prefix+"accept", {{"CLK",logicBit(true)}}, {{"REQ_READY",logicBit(false)}, {"RSP_VALID",logicBit(false)}, {"WE",logicBit(false)}, {"READ_EN",logicBit(false)}});
        step(prefix+"latched", {{"CLK",logicBit(false)}, {"REQ_VALID",logicBit(false)}, {"REQ_ADDR",logicBits(32,0)},
            {"REQ_WRITE_DATA",logicBits(32,0)}}, {{"WORD_ADDR",logicBits(32,1)}, {"WRITE_DATA",logicBits(32,0xabcdef01)}});
        step(prefix+"access", {{"CLK",logicBit(true)}}, {{"WE",logicBit(writing && !misaligned)}, {"READ_EN",logicBit(!writing && !misaligned)}});
        step(prefix+"access-hold", {{"CLK",logicBit(false)}}, {{"RSP_VALID",logicBit(false)}});
        const NamedValues response{{"RSP_VALID",logicBit(true)}, {"RSP_FAULT",logicBit(misaligned || fault)},
            {"RSP_READ_DATA",logicBits(32, writing || misaligned ? 0 : 0x12345678)}, {"WE",logicBit(false)}, {"READ_EN",logicBit(false)}};
        step(prefix+"response", {{"CLK",logicBit(true)}}, response);
        step(prefix+"backpressure-low", {{"CLK",logicBit(false)}, {"MEMORY_READ_DATA",logicBits(32,0xffffffff)}, {"MEMORY_FAULT",logicBit(!fault)}}, response);
        step(prefix+"backpressure-high", {{"CLK",logicBit(true)}}, response);
        step(prefix+"ready", {{"CLK",logicBit(false)}, {"RSP_READY",logicBit(true)}}, response);
        step(prefix+"consumed", {{"CLK",logicBit(true)}}, {{"REQ_READY",logicBit(true)}, {"RSP_VALID",logicBit(false)}});
    }
    spec.scenarios.push_back(std::move(scenario)); return spec;
}

template<class Model, class Cell, bool Structural>
class ConnectedMemory : public IOComponent {
public:
    explicit ConnectedMemory(std::string name) : IOComponent(std::move(name), defineMemoryBusClientPins) {}
    void buildInternals(ComponentBuilder& b) override {
        auto adapter = b.addNewComponent<MemoryBusAdapter32>("ADAPTER");
        std::shared_ptr<IOComponent> memory;
        if constexpr (std::is_same_v<Cell, FlipFlopCell>) memory = b.add(circuit::families::FFMemory<32>(), "MEMORY");
        else if constexpr (Structural) memory = b.addNewComponent<MemoryArray<32,Cell>>("MEMORY");
        else memory = b.addNewComponent<LegacyMemoryContainer<32,Model>>("MEMORY");
        for (const auto& [name, pin] : getAllInputPins()) {
            std::vector<std::shared_ptr<PinBase>> sinks{adapter->getInputPinDynamic(name)};
            if (name == "CLK" || name == "RST") sinks.push_back(memory->getInputPinDynamic(name));
            b.addNewWireDynamic(name, pin->getWidth(), pin, sinks);
        }
        for (const auto& [name, pin] : getAllOutputPins()) b.addNewWireDynamic(name, pin->getWidth(), adapter->getOutputPinDynamic(name), {pin});
        for (auto name : {"WORD_ADDR", "WRITE_DATA", "WRITE_MASK", "WE", "READ_EN"}) {
            auto pin = adapter->getOutputPinDynamic(name);
            b.addNewWireDynamic(name, pin->getWidth(), pin, {memory->getInputPinDynamic(name)});
        }
        b.addNewWireDynamic("READ_DATA", 32, memory->getOutputPin<32>("READ_DATA"), {adapter->getInputPin<32>("MEMORY_READ_DATA")});
        b.addNewWire("FAULT", memory->getOutputPin("FAULT"), {adapter->getInputPin("MEMORY_FAULT")});
    }
};
template<class Model, class Cell, bool Structural>
void integration(const std::string& mode = Structural ? "structural" : "behavioral") {
    auto profile = std::make_shared<const circuit::BuildProfile>(
        ffMemoryProfile(mode, 32, "BUS_MEMORY.MEMORY"));
    auto manifest = std::make_shared<circuit::BuildManifest>(profile->name(), profile->fingerprint());
    auto scope = circuit::BuildContext::rootScope(circuit::builtinComponentCatalog(), profile, manifest);
    auto root = Component::createWithContext<ConnectedMemory<Model,Cell,Structural>>(
        scope->child("BUS_MEMORY", std::nullopt), "BUS_MEMORY");
    ComponentBuilder b(root);
    Simulator sim;
    std::map<std::string, std::shared_ptr<WireBase>> wires;
    for (const auto& [name,pin] : root->getAllInputPins()) wires[name] = b.addNewWireDynamic("IN_"+name, pin->getWidth(), nullptr, {pin});
    for (const auto& [name,pin] : root->getAllOutputPins()) b.addNewWireDynamic("OUT_"+name, pin->getWidth(), pin, {});
    auto drive = [&](const std::map<std::string,uint64_t>& values) {
        const auto time = sim.getCurrentTime()+100;
        for (const auto& [name,value] : values) {
            auto event = makeTestWireUpdate(time, wires.at(name), TestValue(value));
            sim.scheduleEvent(event);
        }
        require(sim.drainUntilIdle(time+10000, 1000000).idle(), "Bus/array did not settle");
    };
    auto check = [&](const char* pin, uint64_t value) {
        const auto bits = root->getOutputPinDynamic(pin)->getValueAsVector();
        require(bits == logicBits(bits.size(),value), std::string(Model::name)+ (Structural ? " structural " : " behavioral ")+pin+" mismatch");
    };
    for (const auto& [name,wire] : wires) sim.scheduleEvent(makeTestWireUpdate(0,wire,TestValue(uint64_t(name == "RST"))));
    SimulationTest::scheduleInitialEventsForTree(root,sim,0);
    require(sim.drainUntilIdle(10000,1000000).idle(),"Reset did not settle");
    drive({{"RST",0}}); check("REQ_READY",1);
    auto transaction = [&](uint32_t address, bool write, uint32_t data, uint32_t mask, uint32_t expected, bool error) {
        drive({{"CLK",0}, {"REQ_VALID",1}, {"REQ_ADDR",address}, {"REQ_WRITE",write},
               {"REQ_WRITE_DATA",data}, {"REQ_WRITE_MASK",mask}, {"RSP_READY",0}});
        check("REQ_READY",1);
        drive({{"CLK",1}}); check("REQ_READY",0); check("RSP_VALID",0);
        drive({{"CLK",0}, {"REQ_VALID",0}, {"REQ_ADDR",0}, {"REQ_WRITE_DATA",0}});
        drive({{"CLK",1}}); check("RSP_VALID",0);
        drive({{"CLK",0}}); drive({{"CLK",1}});
        check("RSP_VALID",1); check("RSP_READ_DATA",expected); check("RSP_FAULT",error);
        for (size_t hold = 0; hold < 2; ++hold) {
            drive({{"CLK",0}}); drive({{"CLK",1}});
            check("RSP_VALID",1); check("RSP_READ_DATA",expected); check("REQ_READY",0);
        }
        drive({{"CLK",0},{"RSP_READY",1}}); drive({{"CLK",1}});
        check("RSP_VALID",0); check("REQ_READY",1);
    };
    transaction(4,true,0x12345678,15,0,Model::readonly);
    transaction(4,false,0,0,Model::readonly ? 0 : 0x12345678,false);
    transaction(4,true,0x0000abcd,1,0,Model::readonly);
    transaction(4,false,0,0,Model::readonly ? 0 : 0x123456cd,false);
    transaction(5,true,0xffffffff,15,0,true);
    transaction(4,false,0,0,Model::readonly ? 0 : 0x123456cd,false);
    transaction(16,false,0,0,0,true);
    // Reset before access cancels a captured request. No late response appears.
    drive({{"CLK",0},{"REQ_VALID",1},{"REQ_ADDR",4},{"REQ_WRITE",1},{"REQ_WRITE_DATA",0xffffffff}});
    drive({{"CLK",1}}); drive({{"CLK",0},{"RST",1},{"REQ_VALID",0}});
    check("RSP_VALID",0); check("REQ_READY",0);
    drive({{"RST",0}}); drive({{"CLK",1}}); check("RSP_VALID",0); check("REQ_READY",1);
    transaction(4,false,0,0,Model::level_write ? 0x123456cd : 0,false);
}
}
MemoryBusAdapter32Test::MemoryBusAdapter32Test() : ComponentScenarioTest(adapterSpec()) {}
void MemoryBusAdapterIntegrationTest::verifyResults() {
    integration<FlipFlopArrayModel,FlipFlopCell,false>(); integration<FlipFlopArrayModel,FlipFlopCell,true>();
    integration<FlipFlopArrayModel,FlipFlopCell,true>("cells");
    integration<FlipFlopArrayModel,FlipFlopCell,true>("words");
    integration<FlipFlopArrayModel,FlipFlopCell,true>("bank");
    integration<SRAMModel,GatedDLatch,false>(); integration<SRAMModel,GatedDLatch,true>();
    integration<ROMModel,ConstantValue<1>,false>(); integration<ROMModel,ConstantValue<1>,true>();
}
