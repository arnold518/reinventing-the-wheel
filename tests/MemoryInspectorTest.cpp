#include "visualizer/MemoryInspector.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "simulator/Simulator.hpp"
#include <iostream>
#include <stdexcept>

// Trace fixture, not a product memory model. It supplies exact bus histories to
// test the observer independently of either controller implementation.
class TraceMemory : public IOComponent, public MemoryStateView {
    MemoryStorage storage_{512, 32, LogicValue::LOW, true};
public:
    explicit TraceMemory(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
        circuit::memory::protocolPins(s, 32);
    }) {
        storage_.loadBytes(0, std::vector<uint8_t>(2048, 0x13));
    }
    const char* getTypeName() const override { return "MemoryEndpoint<ff,32>"; }
    const MemoryStorage& memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t a, const std::vector<uint8_t>& b) override { storage_.loadBytes(a,b); }
};

void require(bool condition, const std::string& why) {
    if (!condition) throw std::runtime_error(why);
}
void contains(const std::string& json, const std::string& text) { require(json.find(text) != std::string::npos, text + " absent from " + json); }

int main() {
    try {
        auto memory = Component::create<TraceMemory>("INSTRUCTION_MEMORY");
        Simulator sim;
        auto put = [&](size_t time, const std::string& name, uint64_t value) {
            auto p = memory->getAllInputPins().contains(name) ? memory->getInputPinDynamic(name) : memory->getOutputPinDynamic(name);
            std::vector<LogicValue> bits(p->getWidth());
            for (size_t bit=0; bit<bits.size(); ++bit) bits[bit] = (value >> bit & 1) ? LogicValue::HIGH : LogicValue::LOW;
            sim.recordPinChange(time, p, bits);
        };
        for (auto name : {"CLK", "RST", "REQ_VALID", "REQ_READY", "RSP_VALID", "RSP_READY", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_STATUS"}) put(0,name,0);
        put(1,"REQ_ADDR",4); put(1,"REQ_WRITE_MASK",15); put(1,"REQ_VALID",1); put(1,"REQ_READY",1);
        put(10,"CLK",1); put(10,"REQ_READY",0); // post-edge READY must not hide the acceptance
        put(11,"REQ_ADDR",1600); put(11,"REQ_OP",1); put(11,"REQ_WRITE_DATA",0xa5); put(11,"REQ_WRITE_MASK",1);
        put(15,"CLK",0); put(20,"CLK",1); put(20,"RSP_VALID",1);
        put(25,"CLK",0); put(29,"REQ_READY",1); put(29,"RSP_READY",1); put(30,"CLK",1);
        put(31,"RSP_VALID",0); put(31,"REQ_VALID",0); put(31,"REQ_READY",0); put(31,"REQ_ADDR",0);
        put(35,"CLK",0); put(39,"RSP_VALID",1); put(40,"CLK",1); put(41,"RSP_VALID",0);
        circuit::visualizer::MemoryInspector inspector(memory, sim);
        const auto work = sim.getPerformanceCounters().effective_pin_changes;
        auto pending = inspector.stateJson(0,12);
        contains(pending,"\"activeAddress\":4"); contains(pending,"\"operation\":\"READ\"");
        contains(pending,"\"acceptedTime\":10"); contains(pending,"\"phase\":\"pending\"");
        contains(inspector.stateJson(0,21),"\"phase\":\"response-held\"");
        auto replaced = inspector.stateJson(0,32);
        contains(replaced,"\"activeAddress\":1600"); contains(replaced,"\"operation\":\"WRITE\"");
        contains(replaced,"\"writeMask\":\"0001\""); contains(replaced,"\"acceptedTime\":30");
        contains(inspector.stateJson(0,41),"\"phase\":\"idle\"");
        require(inspector.stateJson(0,12) == pending, "rewind changed accepted context");
        require(sim.getPerformanceCounters().effective_pin_changes == work && sim.getCurrentTime() == 0,
                "inspection changed the simulator");
        auto page = inspector.stateJson(0,12,"",1600,8);
        contains(page,"\"pageAddress\":1600"); contains(page,"\"pageWords\":8");
        contains(page,"\"touchedWordCount\":512"); contains(page,"\"touchedWordsTruncated\":true");
        contains(page,"\"memoryRole\":\"instruction\""); contains(page,"\"instructionText\":");
        for (auto address : {-2LL,2048LL,4294967296LL}) {
            bool rejected=false;
            try { inspector.stateJson(0,0,"",address,8); } catch(const std::invalid_argument&) { rejected=true; }
            require(rejected,"invalid page accepted");
        }
        // Incremental recording invalidates the cached transaction history.
        put(42,"RST",1);
        contains(inspector.stateJson(0,42),"\"phase\":\"reset\"");
        contains(inspector.stateJson(0,42),"\"activeAddress\":null");

        // Zero-wait response is consumed on the same edge; no fake pending state.
        Simulator bypass;
        auto p = [&](const std::string& name) { return memory->getAllInputPins().contains(name) ? memory->getInputPinDynamic(name) : memory->getOutputPinDynamic(name); };
        for (auto name : {"CLK", "RST", "REQ_VALID", "REQ_READY", "RSP_VALID", "RSP_READY"})
            bypass.recordPinChange(0,p(name),{name==std::string("CLK")||name==std::string("RST") ? LogicValue::LOW : LogicValue::HIGH});
        bypass.recordPinChange(10,p("CLK"),{LogicValue::HIGH});
        bypass.recordPinChange(11,p("REQ_VALID"),{LogicValue::LOW});
        bypass.recordPinChange(11,p("RSP_VALID"),{LogicValue::LOW});
        circuit::visualizer::MemoryInspector fast(memory,bypass);
        contains(fast.stateJson(0,12),"\"phase\":\"idle\"");
        contains(fast.stateJson(0,12),"\"lastCompletedRequestTime\":10");
        std::cout << "Memory observer: pre-edge acceptance, bypass, held payload, replacement, reset, rewind and paging passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
