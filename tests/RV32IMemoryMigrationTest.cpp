#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "tests/RV32IProgramRoot.hpp"
#include "simulator/SimulationTest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/selection/BuildManifest.hpp"
#include <iostream>

using namespace circuit;
using namespace circuit::memory;
namespace {
void require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
template<size_t W> void drive(Simulator& sim, size_t t, const std::shared_ptr<Wire<W>>& wire, uint64_t value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(t, wire, value));
}
class RequesterRig {
public:
    Simulator sim;
    std::shared_ptr<IOComponent> root, requester, lanes, memory;
    std::map<std::string,std::shared_ptr<WireBase>> inputs;
    size_t time = 0, requests = 0, writes = 0;
    bool read_enabled=false, write_enabled=false;
    RequesterRig(Technology technology, bool expanded, bool mock=false) {
        root = Component::create<IOComponent>("TEST");
        ComponentBuilder b(root);
        requester = b.addNewComponent<RV32IMemoryRequest>("REQUEST");
        lanes = b.addNewComponent<RV32IMemoryLanes>("LANES");
        EndpointConfig config; config.depth_words = 4;
        if (technology == Technology::ROM) config.image_hex = "80ff341278563412";
        if (!mock) {
            if (expanded) memory = b.addNewComponent<MemoryEndpoint>("MEMORY",32,technology,config);
            else memory = b.addNewComponent<BehavioralMemoryContainer>("MEMORY",32,technology,config);
        }
        memory_detail::StructuralWiring w(b);
        for(auto name:{"WORD_ADDR","WORD_WRITE_DATA","WRITE_MASK"})
            w.connect(lanes->getOutputPinDynamic(name),requester->getInputPinDynamic(
                std::string(name)=="WORD_ADDR"?"ADDR":std::string(name)=="WORD_WRITE_DATA"?"WRITE_DATA":"WRITE_MASK"));
        w.connect(requester->getOutputPin<32>("READ_DATA"),lanes->getInputPin<32>("WORD_DATA"));
        if(memory) {
            for(auto& [name,pin]:memory->getAllInputPins()) if(name!="CLK" && name!="RST")
                w.connect(requester->getOutputPinDynamic("BUS_"+name),pin);
            for(auto& [name,pin]:memory->getAllOutputPins())
                w.connect(pin,requester->getInputPinDynamic("BUS_"+name));
        }
        w.finish();
        for(auto name:{"CLK","RST","ISSUE","TAKE","TAG","OP"}) {
            auto pin=requester->getInputPinDynamic(name);
            std::vector<std::shared_ptr<PinBase>> sinks{pin};
            if(memory && (std::string(name)=="CLK"||std::string(name)=="RST")) sinks.push_back(memory->getInputPinDynamic(name));
            if(std::string(name)=="TAG") sinks.push_back(lanes->getInputPin<32>("ADDR"));
            inputs[std::string(name)=="TAG"?"ADDR":name]=b.addNewWireDynamic("INPUT_"+std::string(name),pin->getWidth(),nullptr,sinks);
        }
        for(auto name:{"SIZE","SIGN_EXTEND","WRITE_DATA"}) {
            auto pin=lanes->getInputPinDynamic(name);
            inputs[name]=b.addNewWireDynamic("INPUT_"+std::string(name),pin->getWidth(),nullptr,{pin});
        }
        if(mock) for(auto name:{"REQ_READY","RSP_VALID","RSP_READ_DATA","RSP_STATUS"}) {
            auto pin=requester->getInputPinDynamic("BUS_"+std::string(name));
            inputs[name]=b.addNewWireDynamic("INPUT_"+std::string(name),pin->getWidth(),nullptr,{pin});
        }
        sim.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root,sim);
        for (auto& [name,wire] : inputs) set(name,0);
        set("RST",1); sim.advanceAndRecord(time=1000);
        set("RST",0); sim.advanceAndRecord(time+=1000);
    }
    void set(const std::string& name, uint64_t value) {
        if(name=="READ_EN") { read_enabled=value; set("ISSUE",read_enabled||write_enabled); return; }
        if(name=="WRITE_EN") { write_enabled=value; set("OP",value?1:0); set("ISSUE",read_enabled||write_enabled); return; }
        if(name=="CONSUME") { set("TAKE",value); return; }
        const auto wire = inputs.at(name);
        if (wire->getWidth()==1) drive(sim,time,std::dynamic_pointer_cast<Wire<1>>(wire),value);
        else if(wire->getWidth()==2) drive(sim,time,std::dynamic_pointer_cast<Wire<2>>(wire),value);
        else if(wire->getWidth()==3) drive(sim,time,std::dynamic_pointer_cast<Wire<3>>(wire),value);
        else if(wire->getWidth()==4) drive(sim,time,std::dynamic_pointer_cast<Wire<4>>(wire),value);
        else drive(sim,time,std::dynamic_pointer_cast<Wire<32>>(wire),value);
    }
    uint64_t out(const char* name) const {
        if(std::string(name)=="READ_DATA") return lanes->getOutputPin<32>("READ_DATA")->getValueAsUInt64();
        return requester->getOutputPinDynamic(std::string(name)=="READY"?"DONE":name)->getValueAsUInt64();
    }
    void cycle() {
        set("CLK",0); sim.advanceAndRecord(time+=1000);
        if (requester->getOutputPin("BUS_REQ_VALID")->getValue()==LogicValue::HIGH && requester->getInputPin("BUS_REQ_READY")->getValue()==LogicValue::HIGH) {
            ++requests;
            writes += requester->getOutputPin<3>("BUS_REQ_OP")->getValueAsUInt64()==1;
        }
        set("CLK",1); sim.advanceAndRecord(time+=1000);
        set("CLK",0); sim.advanceAndRecord(time+=1000);
    }
    uint32_t access(bool write, uint32_t address, unsigned size, uint32_t value=0, bool sign=false, bool fault=false) {
        set("READ_EN",!write); set("WRITE_EN",write); set("ADDR",address); set("SIZE",size);
        set("WRITE_DATA",value); set("SIGN_EXTEND",sign); set("CONSUME",0);
        sim.advanceAndRecord(time+=1000);
        unsigned cycles=0;
        while (!out("READY") && ++cycles<80) cycle();
        require(cycles<80,"requester response timed out");
        require(bool(out("FAULT"))==fault,"requester fault mismatch");
        const auto result=uint32_t(out("READ_DATA"));
        // A held response must not launch another transaction or replay a store.
        auto count=requests;
        cycle(); cycle();
        require(out("READY") && requests==count,"response backpressure/replay");
        set("CONSUME",1); cycle();
        set("READ_EN",0);set("WRITE_EN",0);set("CONSUME",0);cycle();
        return result;
    }
};
void checkRequester(Technology technology, bool expanded) {
    RequesterRig rig(technology,expanded);
    if(technology==Technology::ROM) {
        require(rig.access(false,0,0,0,true)==0xffffff80,"ROM signed byte");
        require(rig.access(false,2,1)==0x1234,"ROM high halfword");
        rig.access(true,0,2,0,false,true);
    } else if(technology==Technology::NORFlash) {
        require(rig.access(false,0,2)==0xffffffff,"flash erased word");
        rig.access(true,0,2,0,false,true);
        require(rig.access(false,0,2)==0xffffffff,"ordinary store must not program flash");
    } else {
        rig.access(true,0,2,0x12345678);
        for (unsigned lane=0;lane<4;++lane) {
            rig.access(true,lane,0,0x80+lane);
            require(rig.access(false,lane,0)==0x80+lane,"unsigned byte lane");
            require(rig.access(false,lane,0,0,true)==0xffffff80+lane,"signed byte lane");
        }
        require(rig.access(false,0,2)==0x83828180,"byte masks preserve adjacent lanes");
        rig.access(true,2,1,0xdeadff80);
        require(rig.access(false,2,1)==0xff80,"unsigned upper halfword");
        require(rig.access(false,2,1,0,true)==0xffffff80,"signed upper halfword");
        require(rig.access(false,0,2)==0xff808180,"halfword mask");
        auto stored=rig.access(false,0,2);
        rig.access(true,1,1,0,false,true);
        rig.access(true,2,2,0,false,true);
        require(rig.access(false,0,2)==stored,"misaligned store changed memory");
        rig.access(true,16,2,0,false,true);
        // Reset while a response is held clears the transaction, not storage.
        rig.set("RST",1);rig.cycle();rig.set("RST",0);rig.cycle();
        require(rig.access(false,0,2)==0xff808180,"reset erased memory");
        if (technology==Technology::SRAM) {
            rig.access(true,4,2,0x87654321);
            auto before=rig.requests;
            rig.set("READ_EN",1);rig.set("ADDR",0);rig.set("SIZE",2);
            // Change the fetch address after the old request has been latched.
            rig.cycle();rig.cycle();rig.set("ADDR",4);
            for(unsigned i=0;i<80 && !rig.out("READY");++i) rig.cycle();
            require(rig.out("READY") && rig.out("READ_DATA")==0x87654321,"obsolete response used after redirect");
            require(rig.requests==before+2,"redirect did not drain exactly one old request");
            rig.set("CONSUME",1);rig.cycle();rig.set("READ_EN",0);rig.set("CONSUME",0);rig.cycle();
            // Reset after the endpoint committed a store and holds its response.
            auto stores=rig.writes;
            rig.set("WRITE_EN",1);rig.set("ADDR",4);rig.set("SIZE",2);rig.set("WRITE_DATA",0x12345678);
            for(unsigned i=0;i<80 && !rig.out("READY");++i) rig.cycle();
            require(rig.out("READY"),"store did not complete before reset");
            rig.set("RST",1);rig.set("WRITE_EN",0);rig.cycle();
            rig.set("RST",0);rig.cycle();
            require(rig.writes==stores+1,"reset replayed completed store");
            require(rig.access(false,4,2)==0x12345678,"reset did not preserve completed store");
        }
    }
    rig.access(false,16,2,0,false,true);
    std::cout << "requester " << technologyName(technology) << " " << (expanded?"endpoint":"container") << " passed\n";
}
void checkRequestBackpressure() {
    RequesterRig rig(Technology::SRAM,false,true);
    rig.set("ADDR",0);rig.set("SIZE",2);rig.set("READ_EN",1);
    rig.cycle();
    require(rig.out("BUS_REQ_VALID")==1 && rig.out("BUS_REQ_ADDR")==0,"request was not captured");
    // A redirect must not withdraw or mutate an unaccepted request.
    rig.set("ADDR",4);
    for(unsigned i=0;i<3;++i) {
        rig.cycle();
        require(rig.out("BUS_REQ_VALID")==1 && rig.out("BUS_REQ_ADDR")==0,"request changed under backpressure");
    }
    rig.set("REQ_READY",1);rig.cycle();
    require(!rig.out("BUS_REQ_VALID") && !rig.out("READY"),"acceptance was confused with completion");
    rig.set("ADDR",0); // ABA: returning to the old tag must not revive it.
    rig.set("RSP_READ_DATA",0xdeadbeef);rig.set("RSP_VALID",1);
    rig.sim.advanceAndRecord(rig.time+=1000);
    require(!rig.out("READY") && rig.out("BUS_RSP_READY"),"obsolete response was exposed");
    rig.cycle();rig.set("RSP_VALID",0);rig.cycle();rig.cycle();
    rig.set("RSP_READ_DATA",0x12345678);rig.set("RSP_VALID",1);
    rig.sim.advanceAndRecord(rig.time+=1000);
    require(rig.out("READY") && !rig.out("BUS_RSP_READY"),"response was not held");
    auto requests=rig.requests;
    rig.cycle();rig.cycle();
    require(rig.out("READ_DATA")==0x12345678 && rig.requests==requests,"response replay");
    rig.set("CONSUME",1);rig.set("READ_EN",0);rig.cycle();
    require(!rig.out("READY"),"response did not clear");
    rig.set("RST",1);rig.cycle();
    require(!rig.out("BUS_REQ_VALID") && !rig.out("BUS_RSP_READY"),"reset did not quiesce port");
    std::cout<<"request acceptance, redirect, ABA, response backpressure, and reset passed\n";
}
void checkProgram(bool pipeline, bool expanded, bool behavioral) {
    rv32i::MemoryConfiguration instruction, data;
    instruction.technology=Technology::ROM;
    instruction.expanded_endpoint=expanded;
    instruction.device.depth_words=8;
    // addi x1,x0,8; addi x2,x0,-128; sb x2,0(x1); lb x3,0(x1); ebreak
    const auto program=rv32i::RV32IProgram::fromWords({0x00800093,0xf8000113,0x00208023,0x00008183,0x00100073});
    const char* hex="0123456789abcdef";
    for(auto byte:program.bytes()) { instruction.device.image_hex+=hex[byte>>4];instruction.device.image_hex+=hex[byte&15]; }
    data.device.depth_words=4;data.expanded_endpoint=expanded;
    auto profile=std::make_shared<const BuildProfile>(withExactFidelity(canonicalDefaultProfile(),
        "CPU.CORE",behavioral?Fidelity::Behavioral:Fidelity::Structural));
    auto manifest=std::make_shared<BuildManifest>(profile->name(),profile->fingerprint());
    auto context=BuildContext::rootScope(builtinComponentCatalog(),profile,manifest)->child("CPU",std::nullopt);
    auto root=Component::createWithContext<RV32IProgramRoot>(context,"CPU",
        pipeline?families::RV32IFiveStageCore:families::RV32ISingleCycleCore,2000,instruction,data);
    require(root->getChildren().size()==4,"external memory adapter remains");
    std::shared_ptr<IOComponent> core;
    for(auto child:root->getChildren()) if(child->getName()=="CORE") core=std::dynamic_pointer_cast<IOComponent>(child);
    require(core!=nullptr,"missing CPU core");
    for(auto prefix:{"IMEM_","DMEM_"}) {
        for(auto pin:{"REQ_VALID","REQ_OP","REQ_ADDR","REQ_WRITE_DATA","REQ_WRITE_MASK","RSP_READY"})
            require(core->getOutputPinDynamic(std::string(prefix)+pin)!=nullptr,"missing requester output");
        for(auto pin:{"REQ_READY","RSP_VALID","RSP_READ_DATA","RSP_STATUS"})
            require(core->getInputPinDynamic(std::string(prefix)+pin)!=nullptr,"missing requester input");
    }
    ComponentBuilder b(root);
    auto rst=b.addNewWire("RESET",nullptr,{root->getInputPin("RST")});
    auto enable=b.addNewWire("ENABLE",nullptr,{root->getInputPin("ENABLE")});
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    SimulationTest::scheduleInitialEventsForTree(root,sim);
    drive(sim,0,rst,1);drive(sim,0,enable,1);root->startClock(sim,1000);
    sim.advanceAndRecord(3000);drive(sim,3000,rst,0);
    size_t t=4000, cycles=0;
    for(;cycles<500;++cycles) {
        const bool paused=cycles%7<2;
        const auto before=root->snapshotArchitecturalState().toKnownState();
        drive(sim,t,enable,!paused);
        sim.advanceAndRecord(t+=4000);
        if(paused) {
            const auto after=root->snapshotArchitecturalState().toKnownState();
            require(after.pc==before.pc && after.instruction_count==before.instruction_count,
                    "paused core advanced architectural state");
        }
        if(root->getOutputPin("HALTED")->getValue()==LogicValue::HIGH) break;
        require(root->getOutputPin("TRAPPED")->getValue()!=LogicValue::HIGH,"CPU endpoint program trapped");
    }
    require(cycles<500,"CPU endpoint program timed out");
    const auto state=root->snapshotArchitecturalState().toKnownState();
    require(state.x[3]==0xffffff80,"CPU endpoint program result");
    std::cout << "program " << (pipeline?"pipeline":"single-cycle") << " " << (expanded?"endpoint":"container") << "/" << (behavioral?"behavioral":"structural") << " passed, cycles=" << cycles+1 << "\n";
}
}
int main() {
    try {
        {
            auto memory=Component::create<MemoryContainer<32,SRAMModel>>("IMAGE");
            memory->clearContents();memory->initializeBytes(1,{0x80,0xff});
            require(memory->readBytes(0,4)==std::vector<uint8_t>({0,0x80,0xff,0}),"pre-simulation image provisioning");
            Simulator simulator;memory->evaluate(0,simulator);
            bool rejected=false;
            try { memory->initializeBytes(0,{1}); } catch(const std::logic_error&) { rejected=true; }
            require(rejected,"image loader mutated a running memory");
        }
        checkRequestBackpressure();
        for(bool expanded:{false,true}) {
            for(auto t:{Technology::FF,Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash}) checkRequester(t,expanded);
            for(bool pipeline:{false,true}) for(bool behavioral:{false,true}) checkProgram(pipeline,expanded,behavioral);
        }
        std::cout << "RV32I universal memory integration passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
}
