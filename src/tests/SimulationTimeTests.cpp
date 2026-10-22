#include "tests/SimulationTimeTests.hpp"
#include "tests/TestHelpers.hpp"
#include "tests/TestRegistry.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/memory/MemoryStorage.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/storage/RegisterFile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <algorithm>
#include <limits>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Verification equipment with an explicitly declared scalar transport delay.
class VerificationTimeProbe : public BasicComponent {
public:
    explicit VerificationTimeProbe(std::string name)
        : BasicComponent(std::move(name), 1, [](IOComponent* pins) {
            pins->addPin("IN", PinType::INPUT);
            pins->addPin("OUT", PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override { return "VerificationTimeProbe"; }
    void evaluate(SimulationTime time, Simulator& simulator) override {
        _updateOutputWire(simulator, "OUT", getInputPin("IN")->getValue(), time);
    }
};

struct ProbeRig {
    std::shared_ptr<VerificationTimeProbe> root = Component::create<VerificationTimeProbe>("TIME_PROBE");
    std::shared_ptr<WireBase> input, output;
    Simulator simulator;
    explicit ProbeRig(bool table = false, std::shared_ptr<const circuit::DelayTwin> twin = {}) {
        ComponentBuilder builder(root);
        input = builder.addNewWireDynamic("IN_WIRE", 1, nullptr, {root->getInputPin("IN")});
        output = builder.addNewWireDynamic("OUT_WIRE", 1, root->getOutputPin("OUT"), {});
        if (table) {
            circuit::PartDelays delays;
            delays.evidence = "verification-only publication boundary";
            delays.arcs = {{"IN", "OUT", {1, 3}}};
            delays.twin = std::move(twin);
            root->setDelays(std::move(delays));
        }
        SimulationTest::scheduleInitialEventsForTree(root, simulator);
        set(0, LogicValue::LOW);
        simulator.advanceAndRecord(10);
    }
    void set(SimulationTime time, LogicValue value) {
        simulator.scheduleEvent(makeTestWireUpdate(time, input, TestValue(value)));
    }
    LogicValue value() const { return output->getValueVector().at(0); }
};

class VerificationTwin : public circuit::DelayTwin {
public:
    mutable size_t calls = 0;
    VerificationTwin() { inputs = {{"IN", 1}}; outputs = {{"OUT", 1}}; }
    bool settle(State*, long long now, const std::vector<std::vector<LogicValue>>& values,
                const std::vector<std::vector<long long>>&, std::vector<long long>& settled,
                std::vector<std::vector<uint8_t>>& result) const override {
        ++calls;
        settled = {now + 1};
        const auto input = values.at(0).at(0);
        result = {{uint8_t(input == LogicValue::LOW ? 0 : input == LogicValue::HIGH ? 1 : 2)}};
        return true;
    }
};

class SimulationTimeTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "SimulationTimeTest"; }
    void verifyResults() override {
        static_assert(sizeof(SimulationTime) == 8);
        static_assert(sizeof(decltype(Event::time)) == 8);
        for (const SimulationTime base : {(SimulationTime{1} << 32) + 3,
                                         (SimulationTime{1} << 54) + 3,
                                         (SimulationTime{1} << 63) + 3}) {
            ProbeRig rig;
            rig.set(base, LogicValue::HIGH);
            require(rig.simulator.nextEventTime() == base, "Wide queued time truncated");
            rig.simulator.advanceAndRecord(base);
            require(rig.value() == LogicValue::LOW, "Delayed output published early");
            rig.simulator.advanceAndRecord(checkedTimeAdd(base, 1));
            require(rig.value() == LogicValue::HIGH, "Wide delayed output missing");
            require(rig.simulator.getCurrentTime() == base + 1, "Wide current time truncated");
            const auto times = rig.simulator.getUniqueTimestamps();
            require(std::binary_search(times.begin(), times.end(), base + 1), "Wide history time missing");
            rig.simulator.setCircuitStateAtTime(base);
            require(rig.value() == LogicValue::LOW, "Wide signal rewind failed");
            rig.simulator.setCircuitStateAtTime(base + 1);
            require(rig.value() == LogicValue::HIGH, "Wide signal forward seek failed");

            MemoryStorage memory(2, 32, LogicValue::LOW, true);
            std::vector<LogicValue> byte(8, LogicValue::LOW);
            for (size_t bit = 0; bit < 8; ++bit)
                if (0x5a & (1u << bit)) byte[bit] = LogicValue::HIGH;
            memory.writeLogic(base, 0, byte, true);
            const auto before = memory.getWordsAtTime(base - 1, 0, 1).at(0).second;
            const auto after = memory.getWordsAtTime(base, 0, 1).at(0).second;
            require(before != after, "Wide memory history collapsed");
            require(memory.getTouchedWordCountAtTime(base - 1) == 0, "Future memory touch visible early");
            require(memory.getTouchedWordCountAtTime(base) == 1, "Wide memory touch missing");
            require(memory.getByteWritesInTimeRange(base - 1, base).at(0) == 0x5a, "Wide bus-write range truncated");
        }
        for (const bool beyond_signed : {false, true}) {
            auto twin = std::make_shared<VerificationTwin>();
            ProbeRig rig(true, twin);
            const auto calls = twin->calls;
            const SimulationTime base = (SimulationTime{1} << (beyond_signed ? 63 : 54)) + 7;
            const SimulationTime delay = beyond_signed ? 3 : 1;
            rig.set(base, LogicValue::HIGH);
            rig.simulator.advanceAndRecord(base + delay - 1);
            require(rig.value() == LogicValue::LOW, "Publication crossed its timing bound");
            rig.simulator.advanceAndRecord(base + delay);
            require(rig.value() == LogicValue::HIGH, "Wide twin/table publication failed");
            require(beyond_signed ? twin->calls == calls : twin->calls > calls,
                    "Signed twin range guard failed");
        }
        for (const SimulationTime base : {(SimulationTime{1}<<54)+11,(SimulationTime{1}<<63)+11}) {
            const auto profile=circuit::BuildProfileBuilder("wide-register-history")
                .addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::any())).build();
            auto built=circuit::builtinComponentCatalog().createRoot(circuit::storage::registerFile(32,32).request("WIDE_RF"),profile);
            auto root=std::dynamic_pointer_cast<IOComponent>(built.root);
            auto view=std::dynamic_pointer_cast<RegisterStateView>(built.root);
            ComponentBuilder builder(root);Simulator sim;
            std::map<std::string,std::shared_ptr<WireBase>> inputs;
            for(const auto&[name,pin]:root->getAllInputPins())inputs[name]=builder.addNewWireDynamic(name,pin->getWidth(),nullptr,{pin});
            for(const auto&[name,pin]:root->getAllOutputPins())builder.addNewWireDynamic(name,pin->getWidth(),pin,{});
            auto drive=[&](SimulationTime time,const std::string& name,uint64_t value){sim.scheduleEvent(makeTestWireUpdate(time,inputs.at(name),TestValue(value)));};
            SimulationTest::scheduleInitialEventsForTree(root,sim,base);
            for(const auto&[name,pin]:inputs)drive(base,name,name=="RST");
            drive(base+1001,"RST",0);drive(base+1001,"RD_ADDR",5);drive(base+1001,"REG_WRITE",1);drive(base+1001,"WRITE_DATA",0x80000001);
            drive(base+1500,"CLK",1);drive(base+2000,"CLK",0);
            drive(base+2001,"WRITE_DATA",0x12345678);drive(base+2500,"CLK",1);drive(base+3000,"CLK",0);
            sim.advanceAndRecord(base+3500);
            const auto isWord=[](const std::vector<LogicValue>& word,uint32_t expected){
                for(size_t bit=0;bit<word.size();++bit)if(word[bit]!=((expected>>bit)&1?LogicValue::HIGH:LogicValue::LOW))return false;
                return word.size()==32;
            };
            require(isWord(view->getRegisterStateAtTime(base+1499).at(5),0),"Wide register history exposed a future write");
            require(isWord(view->getRegisterStateAtTime(base+2000).at(5),0x80000001),"Wide register history lost first write");
            require(isWord(view->getRegisterStateAtTime(MAX_SIMULATION_TIME).at(5),0x12345678),"Wide register latest-state query narrowed");
        }
        // Real memory timing primitives carry both a wide absolute time and a
        // delay beyond wasm32 size_t. No billions-of-edges loop is required.
        {
            const SimulationTime delay=(SimulationTime{1}<<32)+17,base=(SimulationTime{1}<<54)+23;
            auto line=Component::create<circuit::memory::MemoryDelayLine>("WIDE_LINE",delay);
            ComponentBuilder builder(line);Simulator sim;
            auto input=builder.addNewWireDynamic("IN",1,nullptr,{line->getInputPin("IN")});
            auto output=builder.addNewWireDynamic("OUT",1,line->getOutputPin("OUT"),{});
            SimulationTest::scheduleInitialEventsForTree(line,sim);
            sim.scheduleEvent(makeTestWireUpdate(0,input,TestValue(LogicValue::LOW)));
            sim.advanceAndRecord(delay+1);
            sim.scheduleEvent(makeTestWireUpdate(base,input,TestValue(LogicValue::HIGH)));
            sim.advanceAndRecord(base+delay-1);require(output->getSingleValue()==LogicValue::LOW,"Wide delay line published early");
            sim.advanceAndRecord(base+delay);require(output->getSingleValue()==LogicValue::HIGH,"Wide delay line time narrowed");
            auto reset=Component::create<circuit::memory::PowerOnReset>("WIDE_RESET",delay);
            ComponentBuilder reset_builder(reset);Simulator reset_sim;
            auto reset_output=reset_builder.addNewWireDynamic("RESET",1,reset->getOutputPin("OUT"),{});
            SimulationTest::scheduleInitialEventsForTree(reset,reset_sim,base);
            reset_sim.advanceAndRecord(base+delay-1);require(reset_output->getSingleValue()==LogicValue::HIGH,"Wide reset pulse ended early");
            reset_sim.advanceAndRecord(base+delay);require(reset_output->getSingleValue()==LogicValue::LOW,"Wide reset pulse missing end");
        }
        bool add_overflow = false, multiply_overflow = false, scheduling_overflow = false;
        try { (void)checkedTimeAdd(MAX_SIMULATION_TIME, 1); }
        catch (const std::overflow_error&) { add_overflow = true; }
        try { (void)checkedTimeMultiply(MAX_SIMULATION_TIME, 2); }
        catch (const std::overflow_error&) { multiply_overflow = true; }
        try {
            ProbeRig rig;
            rig.set(MAX_SIMULATION_TIME, LogicValue::HIGH);
            rig.simulator.advanceAndRecord(MAX_SIMULATION_TIME);
        } catch (const std::overflow_error&) { scheduling_overflow = true; }
        require(add_overflow && multiply_overflow && scheduling_overflow, "Time overflow wrapped silently");
    }
};

class SimulationTimePlaybackTest : public SimulationTest {
    std::shared_ptr<WireBase> input_, output_;
public:
    static constexpr SimulationTime base = (SimulationTime{1} << 54) + 3;
    std::string getTestName() const override { return "SimulationTimePlaybackTest"; }
    SimulationTime getRunDuration() const override { return base + 16; }
    std::vector<SimulationCheckpoint> getCheckpoints() const override {
        return {{base + 2, "wide-first", "exact decimal timestamp"},
                {base + 10, "wide-second", "exact decimal timestamp"}};
    }
    void buildCircuit() override {
        auto probe = Component::create<VerificationTimeProbe>("TIME_PROBE");
        root = probe;
        builder = std::make_unique<ComponentBuilder>(root);
        input_ = builder->addNewWireDynamic("IN_WIRE", 1, nullptr, {probe->getInputPin("IN")});
        output_ = builder->addNewWireDynamic("OUT_WIRE", 1, probe->getOutputPin("OUT"), {});
    }
    void setInitialState() override {
        sim->scheduleEvent(makeTestWireUpdate(base, input_, TestValue(LogicValue::LOW)));
        sim->scheduleEvent(makeTestWireUpdate(base + 8, input_, TestValue(LogicValue::HIGH)));
    }
    void verifyResults() override {
        require(output_->getValueVector().at(0) == LogicValue::HIGH, "Wide playback final value");
        sim->setCircuitStateAtTime(base + 2);
        require(output_->getValueVector().at(0) == LogicValue::LOW, "Wide playback first checkpoint");
        sim->setCircuitStateAtTime(base + 10);
        require(output_->getValueVector().at(0) == LogicValue::HIGH, "Wide playback second checkpoint");
    }
};
}

void appendSimulationTimeTests(std::vector<TestRegistryEntry>& entries) {
    entries.push_back({"SimulationTimeTest", "SimulationTimeTest", RegisteredTestKind::Integration,
        {}, {}, {"simulation", "time", "phase4"}, [] { return std::make_unique<SimulationTimeTest>(); }, false});
    entries.push_back({"SimulationTimePlaybackTest", "SimulationTimePlaybackTest", RegisteredTestKind::Integration,
        {}, {}, {"simulation", "time", "phase4"}, [] { return std::make_unique<SimulationTimePlaybackTest>(); }, false});
}
