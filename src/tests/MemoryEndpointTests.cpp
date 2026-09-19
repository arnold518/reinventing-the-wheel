#include "tests/MemoryEndpointTests.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include <random>
using namespace circuit;
using namespace circuit::test;
using namespace circuit::memory;
namespace {
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } require(failed,"Expected rejection"); }
struct Scenario {
    ComponentTestSpec spec;
    NamedValues state;
    size_t width;
    Scenario(std::string name, std::string contract, size_t w, ParameterMap config = {})
        : spec{std::move(name),std::move(contract),"MEMORY_ENDPOINT_ROOT",std::move(config),{{"transactions",{},{},{}}}},
          state{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"REQ_VALID",logicBit(false)},
                {"REQ_OP",logicBits(3,0)},{"REQ_ADDR",logicBits(32,0)},
                {"REQ_WRITE_DATA",logicBits(w,0)},{"REQ_WRITE_MASK",logicBits(w/8,0)},
                {"RSP_READY",logicBit(false)}}, width(w) {}
    void step(std::string label, NamedValues changes = {}, NamedValues expected = {}) {
        for (auto& [name,value] : changes) state[name] = value;
        spec.scenarios[0].actions.push_back({std::move(label),CheckpointKind::Settled,state,std::move(expected),true,{}});
    }
    void init() {
        step("reset", {}, {{"REQ_READY",logicBit(false)},{"RSP_VALID",logicBit(false)}});
        step("release-reset", {{"RST",logicBit(false)}}, {{"REQ_READY",logicBit(true)}});
    }
    void transaction(std::string label, unsigned op, uint32_t address, uint32_t data, unsigned mask,
                     uint32_t result, Status status = Status::Ok) {
        step(label+"-setup", {{"CLK",logicBit(false)},{"REQ_VALID",logicBit(true)},{"REQ_OP",logicBits(3,op)},
            {"REQ_ADDR",logicBits(32,address)},{"REQ_WRITE_DATA",logicBits(width,data)},
            {"REQ_WRITE_MASK",logicBits(width/8,mask)},{"RSP_READY",logicBit(false)}}, {{"REQ_READY",logicBit(true)}});
        step(label+"-accept", {{"CLK",logicBit(true)}}, {{"REQ_READY",logicBit(false)},{"RSP_VALID",logicBit(false)}});
        step(label+"-latched", {{"CLK",logicBit(false)},{"REQ_VALID",logicBit(false)},
            {"REQ_ADDR",logicBits(32,0xffffffff)},{"REQ_WRITE_DATA",logicBits(width,0)}});
        step(label+"-execute", {{"CLK",logicBit(true)}}, {{"RSP_VALID",logicBit(false)}});
        // A relay has another transaction frontend behind it. Supply enough
        // clocks for either path; no response is consumed until RSP_READY.
        for (size_t cycle = 0; cycle < 5; ++cycle) {
            step(label+"-wait-low-"+std::to_string(cycle), {{"CLK",logicBit(false)}});
            step(label+"-wait-high-"+std::to_string(cycle), {{"CLK",logicBit(true)}});
        }
        NamedValues response{{"RSP_VALID",logicBit(true)},{"RSP_STATUS",logicBits(3,unsigned(status))},
                             {"RSP_READ_DATA",logicBits(width,result)},{"REQ_READY",logicBit(false)}};
        step(label+"-response", {{"CLK",logicBit(false)}}, response);
        step(label+"-hold", {{"CLK",logicBit(true)}}, response);
        step(label+"-ready", {{"CLK",logicBit(false)},{"RSP_READY",logicBit(true)}}, response);
        step(label+"-consumed", {{"CLK",logicBit(true)}}, {{"RSP_VALID",logicBit(false)},{"REQ_READY",logicBit(true)}});
    }
};
std::string testName(Technology t) {
    switch(t) {
    case Technology::FF: return "MemoryEndpointFFTest";
    case Technology::SRAM: return "MemoryEndpointSRAMTest";
    case Technology::DRAM: return "MemoryEndpointDRAMTest";
    case Technology::ROM: return "MemoryEndpointROMTest";
    case Technology::NORFlash: return "MemoryEndpointFlashTest";
    default: return "MemoryEndpointRelayTest";
    }
}
ComponentTestSpec endpointSpec(Technology t, size_t width = 8, bool container = false, size_t depth = 4, size_t row_words = 1) {
    ParameterMap config{{"depth_words",std::to_string(depth)},{"words_per_row",std::to_string(row_words)},{"block_words",std::to_string(row_words)}};
    if (t == Technology::ROM) config["image_hex"] = "a53c81ff";
    Scenario s(container ? "ScalableMemoryContainerTest" : testName(t),
        std::string(container ? "memory.scalable." : "memory.endpoint.") + technologyName(t) + "." + std::to_string(width), width, config);
    s.init(); const unsigned mask = (1u << (width/8))-1;
    if (t == Technology::ROM) {
        const uint32_t expected = width == 8 ? 0xa5 : width == 16 ? 0x3ca5 : 0xff813ca5;
        s.transaction("read-image",0,0,0,0,expected);
        s.transaction("reject-write",1,0,0,mask,0,Status::ReadOnly);
    } else {
        const unsigned write = t == Technology::NORFlash ? 2 : 1;
        const uint32_t first = uint32_t(0xa5a53c81ull & ((uint64_t(1)<<width)-1));
        s.transaction("write-first",write,0,first,mask,0);
        s.transaction("read-first",0,0,0,0,first);
        s.transaction("write-last",write,(depth-1)*(width/8),0x3c,mask,0);
        s.transaction("read-last",0,(depth-1)*(width/8),0,0,0x3c);
        s.transaction("zero-mask",write,0,0,0,0);
        s.transaction("unchanged",0,0,0,0,first);
        if (t != Technology::NORFlash) {
            s.transaction("masked-low-byte",1,0,0x5a,1,0);
            s.transaction("read-masked-byte",0,0,0,0,(first & ~0xffu)|0x5a);
        }
        if (t == Technology::NORFlash) {
            s.transaction("cannot-program-ones",2,0,0xffffffff,mask,0);
            s.transaction("still-programmed",0,0,0,0,first);
            s.transaction("reject-ram-write",1,0,0,mask,0,Status::Unsupported);
            s.transaction("erase-first-block",3,0,0,0,0);
            s.transaction("read-erased",0,0,0,0,width==32 ? 0xffffffff : (1u<<width)-1);
            s.transaction("other-block-retained",0,(depth-1)*(width/8),0,0,0x3c);
        } else s.transaction("unsupported-program",2,0,0,mask,0,Status::Unsupported);
    }
    s.transaction("range-error",0,depth*(width/8),0,0,0,Status::Range);
    s.transaction("reserved-opcode",7,0,0,0,0,Status::Unsupported);
    if (width > 8) s.transaction("unaligned",0,1,0,0,0,Status::Alignment);
    if (t != Technology::ROM) {
        const auto begin = s.spec.scenarios[0].actions.size();
        s.transaction("unknown-mask",t==Technology::NORFlash?2:1,0,0,0,0,Status::InvalidRequest);
        for (size_t i : {begin,begin+1})
            s.spec.scenarios[0].actions[i].inputs["REQ_WRITE_MASK"]=LogicVector(width/8,LogicValue::UNKNOWN);
        // Use a fresh word (erased ones for NOR); unknown selected data must
        // remain unknown instead of silently becoming a binary zero.
        const auto write = s.spec.scenarios[0].actions.size();
        s.transaction("unknown-data",t==Technology::NORFlash?2:1,width/8,0,mask,0);
        for (size_t i : {write,write+1})
            s.spec.scenarios[0].actions[i].inputs["REQ_WRITE_DATA"]=LogicVector(width,LogicValue::UNKNOWN);
        const auto read=s.spec.scenarios[0].actions.size();
        s.transaction("read-unknown",0,width/8,0,0,0);
        for (size_t i=read;i<s.spec.scenarios[0].actions.size();++i) {
            auto& expected=s.spec.scenarios[0].actions[i].expected_outputs;
            if (expected.count("RSP_READ_DATA")) expected["RSP_READ_DATA"]=LogicVector(width,LogicValue::UNKNOWN);
            if (i==read || i==read+1) {
                auto& inputs=s.spec.scenarios[0].actions[i].inputs;
                inputs["REQ_WRITE_DATA"]=LogicVector(width,LogicValue::UNKNOWN);
                inputs["REQ_WRITE_MASK"]=LogicVector(width/8,LogicValue::UNKNOWN);
            }
        }
    }
    return s.spec;
}
ComponentTestSpec frontendSpec() {
    Scenario s("MemoryTransactionPortTest","memory.transaction-port.8",8);
    s.state["BACK_READY"] = logicBit(true); s.state["DONE"] = logicBit(false);
    s.state["RESULT"] = logicBits(8,0xa5); s.state["STATUS"] = logicBits(3,0);
    s.init(); s.step("request",{{"REQ_VALID",logicBit(true)},{"REQ_ADDR",logicBits(32,12)}});
    s.step("accept",{{"CLK",logicBit(true)}},{{"REQ_READY",logicBit(false)},{"ADDR",logicBits(32,12)}});
    s.step("payload-held",{{"CLK",logicBit(false)},{"REQ_ADDR",logicBits(32,0)},{"REQ_VALID",logicBit(false)}},{{"ADDR",logicBits(32,12)}});
    s.step("start",{{"CLK",logicBit(true)}},{{"START",logicBit(true)},{"RSP_VALID",logicBit(false)}});
    s.step("done",{{"CLK",logicBit(false)},{"DONE",logicBit(true)}});
    s.step("capture-response",{{"CLK",logicBit(true)}},{{"RSP_VALID",logicBit(true)},{"RSP_READ_DATA",logicBits(8,0xa5)}});
    s.step("hold",{{"CLK",logicBit(false)},{"RESULT",logicBits(8,0)}},{{"RSP_READ_DATA",logicBits(8,0xa5)}});
    s.step("reset-response",{{"RST",logicBit(true)}},{{"RSP_VALID",logicBit(false)},{"REQ_READY",logicBit(false)}});
    return s.spec;
}
ComponentTestSpec controllerSpec() {
    Scenario s("MemoryControllerTest","memory.controller.ff.8",8);
    s.state["MEM_READ_DATA"]=logicBits(8,0xa5);s.state["MEM_FAULT"]=logicBit(false);
    s.init();s.transaction("read-attached-port",0,0,0,0,0xa5);
    s.transaction("write-attached-port",1,0,0x3c,1,0);
    return s.spec;
}
}
MemoryTransactionPortTest::MemoryTransactionPortTest() : ComponentScenarioTest(frontendSpec()) {}
MemoryControllerTest::MemoryControllerTest() : ComponentScenarioTest(controllerSpec()) {}
MemoryEndpointFFTest::MemoryEndpointFFTest() : ComponentScenarioTest(endpointSpec(Technology::FF)) {}
MemoryEndpointSRAMTest::MemoryEndpointSRAMTest() : ComponentScenarioTest(endpointSpec(Technology::SRAM)) {}
MemoryEndpointDRAMTest::MemoryEndpointDRAMTest() : ComponentScenarioTest(endpointSpec(Technology::DRAM)) {}
MemoryEndpointROMTest::MemoryEndpointROMTest() : ComponentScenarioTest(endpointSpec(Technology::ROM)) {}
MemoryEndpointFlashTest::MemoryEndpointFlashTest() : ComponentScenarioTest(endpointSpec(Technology::NORFlash)) {}
MemoryEndpointRelayTest::MemoryEndpointRelayTest() : ComponentScenarioTest(endpointSpec(Technology::Container)) {}
ScalableMemoryContainerTest::ScalableMemoryContainerTest() : ComponentScenarioTest(endpointSpec(Technology::SRAM,8,true)) {}
void MemoryEndpointContractTest::verifyResults() {
    ComponentTestRunner runner;
    for (auto t : {Technology::FF,Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash})
        for (size_t width : {8,16,32}) {
            auto spec = endpointSpec(t,width);
            auto result = runner.runAll(spec,spec.scenarios[0]);
            require(result.targets.size()==2,"Missing endpoint fidelity");
            runner.compare(result.targets[0],result.targets[1]);
        }
    for (auto t : {Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash}) {
        auto spec=endpointSpec(t,16,false,6,2);
        auto result=runner.runAll(spec,spec.scenarios[0]);
        runner.compare(result.targets[0],result.targets[1]);
    }
    MemoryStorage dense(8192,8,LogicValue::UNKNOWN,false);
    MemoryStorage sparse(8192,8,LogicValue::UNKNOWN,false,{true,65536,65536});
    std::mt19937 rng(123);
    for (size_t i=0;i<256;++i) {
        auto address=rng()%8192; auto value=logicBits(8,rng());
        dense.writeLogic(i,address,value); sparse.writeLogic(i,address,value);
        require(dense.readLogic(address,1)==sparse.readLogic(address,1),"Paged contents mismatch");
    }
    sparse.fill(300,LogicValue::HIGH); require(sparse.allocatedDataBytes()==0,"Sparse fill allocated pages");
    require(sparse.readBytes(8000,1)[0]==255,"Sparse fill incorrect");
    MemoryStorage huge(1u<<28,8,LogicValue::UNKNOWN,false,{true,16384,0});
    require(huge.allocatedDataBytes()==0,"Sparse construction scales with capacity");
    huge.writeLogic(1,(1u<<28)-1,logicBits(8,42));
    require(huge.allocatedDataBytes()<16384 && huge.readBytes((1u<<28)-1,1)[0]==42,"Sparse last address failed");
    rejects([&]{huge.writeLogic(2,0,logicBits(8,1));});
    rejects([]{MemoryStorage(1u<<28,8,LogicValue::LOW,false,{false,1024,0});});
    MemoryStorage bounded(16,8,LogicValue::LOW,true,{true,16384,1});
    rejects([&]{bounded.writeLogic(1,0,logicBits(8,1));});
    EndpointConfig config; config.depth_words=1u<<28; config.record_history=false;
    auto large=Component::create<MemoryContainer<8,circuit::memory::SRAMModel>>("LARGE",config);
    require(large->memoryStorage().capacityBytes()==1u<<28 && large->getChildren().empty(),"Container not compact");
    config.retention_ticks=1024;
    rejects([&]{config.validate(8,Technology::DRAM);});
}
void MemoryEndpointTimingTest::verifyResults() {
    struct Rig {
        std::shared_ptr<IOComponent> root;
        Simulator sim;
        size_t now = 100;
        std::map<std::string,std::shared_ptr<WireBase>> wires;
        std::string context;
        explicit Rig(Technology t, bool compact, ParameterMap config = {}) {
            context=std::string(technologyName(t))+(compact?" behavioral":" structural");
            config["words_per_row"]="1"; config["block_words"]="1";
            auto profile=withExactFidelity(canonicalDefaultProfile(),"DUT",compact?Fidelity::Behavioral:Fidelity::Structural);
            root=std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(
                {"memory.endpoint."+std::string(technologyName(t))+".8","DUT",config,{},{},{}},profile).root);
            ComponentBuilder b(root);
            for (auto& [name,pin]:root->getAllInputPins()) wires[name]=b.addNewWireDynamic("IN_"+name,pin->getWidth(),nullptr,{pin});
            for (auto& [name,pin]:root->getAllOutputPins()) b.addNewWireDynamic("OUT_"+name,pin->getWidth(),pin,{});
            for (auto& [name,wire]:wires) sim.scheduleEvent(makeTestWireUpdate(0,wire,TestValue(uint64_t(name=="RST"))));
            SimulationTest::scheduleInitialEventsForTree(root,sim,0); sim.advanceAndRecord(100);
            drive({{"RST",0}});
        }
        void drive(std::map<std::string,uint64_t> values, size_t wait=100) {
            const auto at=now+1;
            for (auto& [name,value]:values) sim.scheduleEvent(makeTestWireUpdate(at,wires.at(name),TestValue(value)));
            now=at+wait;sim.advanceAndRecord(now);
        }
        void unknown(const char* name,size_t width) {
            sim.scheduleEvent(makeTestWireUpdate(now+1,wires.at(name),TestValue(LogicVector(width,LogicValue::UNKNOWN))));
            now+=101;sim.advanceAndRecord(now);
        }
        void cycle() {drive({{"CLK",0}});drive({{"CLK",1}});}
        LogicVector out(const char* name) {return root->getOutputPinDynamic(name)->getValueAsVector();}
        void start(unsigned op,unsigned addr,unsigned data=0,unsigned mask=1) {
            drive({{"CLK",0},{"REQ_OP",op},{"REQ_ADDR",addr},{"REQ_WRITE_DATA",data},{"REQ_WRITE_MASK",mask},{"REQ_VALID",0},{"RSP_READY",0}});
            for (unsigned i=0;out("REQ_READY")!=logicBit(true)&&i<100;++i) cycle();
            require(out("REQ_READY")==logicBit(true),context+" request never ready at "+std::to_string(sim.getCurrentTime())+
                " op="+std::to_string(op)+" address="+std::to_string(addr)+" response="+formatLogicVector(out("RSP_VALID")));
            drive({{"CLK",0},{"REQ_VALID",1}});drive({{"CLK",1}});
            drive({{"CLK",0},{"REQ_VALID",0},{"REQ_ADDR",0xffffffff},{"REQ_WRITE_DATA",0}});
        }
        void response() {
            for (unsigned i=0;out("RSP_VALID")!=logicBit(true)&&i<100;++i) cycle();
            require(out("RSP_VALID")==logicBit(true),"Response deadline exceeded");
        }
        void consume() {drive({{"CLK",0},{"RSP_READY",1}});drive({{"CLK",1}});drive({{"CLK",0},{"RSP_READY",0}});}
        void transaction(unsigned op,unsigned address,unsigned value,unsigned expected) {
            start(op,address,value);response();
            require(out("RSP_STATUS")==logicBits(3,0),"Unexpected transaction error");
            require(out("RSP_READ_DATA")==logicBits(8,expected),"Timed response data mismatch");consume();
        }
    };
    for (bool compact:{false,true}) {
        Rig ff(Technology::FF,compact);
        ff.transaction(1,0,0xa5,0);ff.transaction(0,0,0,0xa5);
        ff.drive({{"RST",1}});ff.drive({{"RST",0}});
        ff.transaction(0,0,0,0xa5); // Protocol reset is not FF storage reset.
        ff.drive({{"CLK",0},{"REQ_OP",0},{"REQ_VALID",1},{"REQ_ADDR",0}});
        ff.unknown("REQ_ADDR",32);ff.drive({{"CLK",1}});ff.drive({{"CLK",0},{"REQ_VALID",0}});ff.response();
        require(ff.out("RSP_STATUS")==logicBits(3,6),"Unknown address was coerced to zero");ff.consume();
        ff.start(0,0);ff.response();
        for (int i=0;i<5;++i) {ff.cycle();require(ff.out("RSP_READ_DATA")==logicBits(8,0xa5),"Response changed under backpressure");}
        ff.consume();
        Rig flash(Technology::NORFlash,compact,{{"program_ticks","4000"},{"erase_ticks","5000"}});
        flash.start(2,0,0x81);flash.cycle();
        require(flash.out("RSP_VALID")==logicBit(false),"Flash completed before its program interval");
        flash.drive({{"RST",1}});flash.drive({{"RST",0}});
        for(int i=0;i<35;++i) {flash.cycle();require(flash.out("RSP_VALID")==logicBit(false),"Reset resurrected a response");}
        flash.transaction(0,0,0,0x81); // Issued program drained, never rolled back.
        Rig dram(Technology::DRAM,compact,{{"retention_ticks","20000"},{"refresh_interval_ticks","1024"}});
        dram.transaction(1,0,0x5a,0);dram.start(0,0);dram.response();
        for(int i=0;i<220;++i) {dram.cycle();require(dram.out("RSP_READ_DATA")==logicBits(8,0x5a),"Refresh changed a held response");}
        dram.consume();dram.transaction(0,0,0,0x5a);
        // Close row zero, then stop the clock beyond retention. Expiry is not
        // repaired by a later refresh; both representations must return X.
        dram.transaction(1,3,0x3c,0);dram.drive({{"CLK",0}},30000);
        dram.start(0,0);dram.response();
        require(dram.out("RSP_READ_DATA")==LogicVector(8,LogicValue::UNKNOWN),"Expired DRAM data survived stopped-clock retention");
    }
}
