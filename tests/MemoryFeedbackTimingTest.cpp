#include "analysis/SafeClockTiming.hpp"
#include "../src/analysis/TimedLogicDomain.hpp"
#include "../src/modules/memory/MemoryEndpointEngine.hpp"
#include "../src/modules/rv32i/RV32IZeroWaitMemoryPort.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "simulator/SimulationTest.hpp"
#include <iostream>
#include <set>
#include <stdexcept>

using namespace circuit::analysis;
using namespace circuit::memory;
using namespace circuit::memory_device;
namespace {
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
void validator() {
    // Exhaust every opcode in four-state logic and every binary/unknown mask
    // category against the production validator, at each address error class.
    size_t count=0;
    for(size_t width:{8,16,32})for(unsigned op=0;op<64;++op)
    for(unsigned address:{0u,1u,3u,4u,15u,16u,0xffffffffu})for(unsigned mask=0;mask<4;++mask) {
        Request r{bits(3,0),bits(32,address),Bits(width,L),Bits(width/8,LogicValue(mask))};
        std::vector<detail::TimedBit> o(3),a(32),m(width/8);
        for(size_t i=0;i<3;++i){r.op[i]=LogicValue((op>>(i*2))&3);o[i]={1u<<unsigned(r.op[i]),7,7};}
        for(size_t i=0;i<32;++i)a[i]={1u<<unsigned(r.address[i]),11,11};
        for(auto& b:m)b={1u<<mask,13,13};
        EndpointConfig config;
        for(bool unknown_address:{false,true}) {
            if(unknown_address){r.address[3]=X;a[3]={4,11,11};}
            const auto proof=detail::ffStatusBounds(o,a,m,width/8,config.depth_words*(width/8),1);
            const unsigned expected=unsigned(validateRequest(r,width,Technology::FF,config));
            for(unsigned i=0;i<3;++i) {
                check(proof[i].values==(1u<<((expected>>i)&1)),"Validator abstract rule mismatch");
                check(proof[i].settled<=14,"Concrete validator query must settle");
            }
            ++count;
        }
    }
    std::vector<detail::TimedBit> op{{3,9,detail::unbounded},{1,0,0},{1,0,0}};
    std::vector<detail::TimedBit> address(32,{3,0,30}),mask(4,{3,12,detail::unbounded});
    auto proof=detail::ffStatusBounds(op,address,mask,4,16,1);
    for(const auto& b:proof)check(b.settled<=31,"Known read/write with binary masks must be independent of toggling opcode/mask");
    mask[0]={4,12,12};
    proof=detail::ffStatusBounds(op,address,mask,4,16,1);
    check(proof[2].settled==detail::unbounded,"Unknown mask cannot be ignored");
    std::cout<<count<<" concrete four-state validator checks passed\n";
}
struct Fixture {
    std::shared_ptr<IOComponent> root=Component::create<IOComponent>("ROOT",[](IOComponent* c){
        c->addPin("CLK",PinType::INPUT);c->addPin("RST",PinType::INPUT);
        c->addPin<32>("ADDR",PinType::INPUT);c->addPin<2>("SIZE",PinType::INPUT);
        c->addPin("FAULT",PinType::OUTPUT);c->addPin("WE",PinType::OUTPUT);
    });
    std::shared_ptr<BehavioralMemoryEndpoint> endpoint;
    std::map<std::string,std::shared_ptr<WireBase>> inputs,outputs;
    Simulator sim;
    ClockSpecification clock;
    Fixture() {
        ComponentBuilder b(root);circuit::detail::StructuralWiring w(b);
        auto stage=b.addNewComponent<RV32ISingleCycleMemoryStage>("MEMORY");
        auto port=b.addNewComponent<rv32i::ZeroWaitMemoryPort>("PORT");
        endpoint=b.addNewComponent<BehavioralMemoryEndpoint>("MEM",32,Technology::FF);
        for(const auto& [n,p]:stage->getAllOutputPins())if(n.starts_with("DMEM_"))w.connect(p,port->getInputPinDynamic(n.substr(5)));
        for(const auto& [n,p]:stage->getAllInputPins())if(n.starts_with("DMEM_"))w.connect(port->getOutputPinDynamic(n.substr(5)),p);
        for(const auto& [n,p]:endpoint->getAllInputPins())if(n!="CLK"&&n!="RST")w.connect(port->getOutputPinDynamic(n),p);
        for(const auto& [n,p]:endpoint->getAllOutputPins())w.connect(p,port->getInputPinDynamic(n));
        for(auto n:{"CLK","RST"})w.connect(root->getInputPin(n),endpoint->getInputPin(n));
        for(auto n:{"ADDR","SIZE"})w.connect(root->getInputPinDynamic(n),stage->getInputPinDynamic(n));
        auto high=w.constant("ONE",true),low=w.constant("ZERO",false);
        for(auto n:{"ISSUE","MEM_WRITE"})w.connect(high,stage->getInputPin(n));
        w.connect(low,stage->getInputPin("SIGN_EXTEND"));
        w.connect(b.addNewComponent<ConstantValue<32>>("DATA",0)->getOutputPin<32>("OUT"),stage->getInputPin<32>("WRITE_DATA"));
        w.connect(w.invert("NO_FAULT",stage->getOutputPin("FAULT")),stage->getInputPin("COMMIT"));
        w.connect(stage->getOutputPin("FAULT"),root->getOutputPin("FAULT"));
        w.connect(stage->getOutputPin("DMEM_WE"),root->getOutputPin("WE"));w.finish();
        for(const auto& [n,p]:root->getAllInputPins())inputs[n]=b.addNewWireDynamic("drive_"+n,p->getWidth(),nullptr,{p});
        for(const auto& [n,p]:root->getAllOutputPins())outputs[n]=b.addNewWireDynamic("observe_"+n,p->getWidth(),p,{});
        clock.clock_pin="ROOT.CLK";
        clock.inputs={{"ROOT.RST",std::nullopt,{},0},{"ROOT.SIZE",std::nullopt,{},0},{"ROOT.ADDR",std::nullopt,{},0}};
        for(auto n:{"FAULT","WE"})clock.outputs.push_back({std::string("ROOT.")+n,ClockEdge::Rising,0,0});
        sim.setHistoryRecordingEnabled(false);endpoint->setHistoryRecordingEnabled(false);
    }
    void drive(const std::string& name,size_t t,const Bits& v) {
        const auto& wire=inputs.at(name);
        auto send=[&]<size_t W>(){sim.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(t,std::dynamic_pointer_cast<Wire<W>>(wire),v));};
        switch(v.size()){case 1:send.template operator()<1>();break;case 2:send.template operator()<2>();break;
            case 32:send.template operator()<32>();break;default:throw std::runtime_error("Bad width");}
    }
    void initialize() {
        for(const auto& [n,w]:inputs)drive(n,0,bits(w->getWidth(),n=="RST"?1:0));
        SimulationTest::scheduleInitialEventsForTree(root,sim,0);
        sim.runAndRecord(100);drive("RST",101,{L});sim.runAndRecord(200);
        check(!sim.hasPendingEvents(),"Known-address byte store must settle");
    }
};
void feedback() {
    Fixture f;
    const auto safe=computeSafeClockPeriod(f.root,f.clock);
    for(const auto& d:safe.diagnostics)std::cerr<<d<<'\n';
    check(safe.status==ClockTimingStatus::SafeUnderContracts,"Known byte lane feedback should now be provable");
    TimingContracts override_model{[](const IOComponent& c)->std::optional<ClockComponentContract>{
        if(typeid(c)!=typeid(BitSplitter<32>))return std::nullopt;
        ClockComponentContract m;m.evidence="deliberately conservative replacement splitter contract";
        for(const auto& [name,p]:c.getAllOutputPins())m.arcs.push_back({"IN",name,{1,100}});
        return m;
    }};
    const auto replaced=computeSafeClockPeriod(f.root,f.clock,override_model);
    check(replaced.status==ClockTimingStatus::Unknown&&!replaced.safe_period_ticks,
        "Native bit-delay rules must not override an explicit replacement contract");
    f.initialize();
    Fixture reference;reference.initialize();
    const auto period=*safe.safe_period_ticks;
    for(size_t cycle=0;cycle<32;++cycle) {
        const size_t fast=1000+cycle*period,slow=1000+cycle*2000;
        f.drive("CLK",fast,{H});f.drive("CLK",fast+period/2,{L});f.sim.runAndRecord(fast+period-1);
        reference.drive("CLK",slow,{H});reference.drive("CLK",slow+1000,{L});reference.sim.runAndRecord(slow+1999);
        check(!f.sim.hasPendingEvents(),"Known byte-store circuit must drain at computed period");
        for(const auto& [n,w]:f.outputs)check(w->getValueVector()==reference.outputs.at(n)->getValueVector(),"Fast/slow feedback outputs differ");
        check(f.endpoint->memoryStorage().readLogic(0,16)==reference.endpoint->memoryStorage().readLogic(0,16),"Fast/slow feedback storage differs");
    }
    auto changed=f.clock;changed.inputs[2].edge=ClockEdge::Rising;changed.inputs[2].delay={1,1};
    const auto unknown=computeSafeClockPeriod(f.root,changed);
    check(unknown.status==ClockTimingStatus::Unknown&&!unknown.safe_period_ticks,"Unrestricted four-state address must not be certified");
    const size_t start=1000+32*period;
    auto address=bits(32,0);address[0]=X;f.drive("ADDR",start+1,address);
    // Frozen clock, fixed address: observe persistent fault/permission toggles,
    // not a sequence of deliberately changing external requests.
    size_t transitions=0;auto prior=f.outputs.at("FAULT")->getValueVector();
    for(size_t t=start+2;t<start+300;++t) {
        f.sim.runAndRecord(t);auto value=f.outputs.at("FAULT")->getValueVector();
        transitions+=value!=prior;prior=value;
    }
    check(transitions>20&&f.sim.hasPendingEvents(),"Unknown byte lane should reproduce actual store feedback oscillation");
    f.drive("ADDR",start+301,bits(32,0));f.sim.runAndRecord(start+400);
    check(!f.sim.hasPendingEvents(),"Binary address must recover to settled behavior");
    std::cout<<"Known byte-store feedback proven at "<<*safe.safe_period_ticks
        <<" ticks; unknown address caused "<<transitions<<" fault transitions with a frozen clock\n";
}
void coreCounterexample(const std::string& family) {
    auto root=Component::create<IOComponent>("ROOT",[](IOComponent* c){
        c->addPin("CLK",PinType::INPUT);c->addPin("RST",PinType::INPUT);
    });
    auto built=circuit::builtinComponentCatalog().createRoot({family,"CORE",{},{},{},{}},
        rv32i::withBehavioralMemoryParts(circuit::maximallyStructural()));
    auto core=std::dynamic_pointer_cast<IOComponent>(built.root);root->addChild(core);
    ComponentBuilder b(root);EndpointConfig config;config.depth_words=64;
    auto imem=b.addNewComponent<BehavioralMemoryEndpoint>("IMEM",32,Technology::FF,config);
    auto dmem=b.addNewComponent<BehavioralMemoryEndpoint>("DMEM",32,Technology::FF,config);
    rv32i::wireMemorySystem(b,core,imem,dmem,root->getInputPin("CLK"),root->getInputPin("RST"));
    const auto clk=b.addNewWire("clock",nullptr,{root->getInputPin("CLK")});
    const auto rst=b.addNewWire("reset",nullptr,{root->getInputPin("RST")});
    const auto enable=b.addNewWire("enable",nullptr,{core->getInputPin("ENABLE")});
    // lw x1,0(x0); andi x1,x1,1; sb x0,0(x1); ebreak.
    // Uninitialized FF data memory is X by its existing contract. The ANDI
    // leaves only address bit zero unknown, without debug mutation/injection.
    imem->clearContents();
    imem->loadWords(0,{0x00002083,0x0010f093,0x00008023,0x00100073});
    ClockSpecification binary;binary.clock_pin="ROOT.CLK";binary.binary_startup=true;
    binary.inputs={{"ROOT.RST",std::nullopt,{},0},{core->getID()+".ENABLE",std::nullopt,{},1}};
    const auto rejected=computeSafeClockPeriod(root,binary);
    bool initial_unknown=false;
    for(const auto& d:rejected.diagnostics)initial_unknown|=d.find("initial memory image contains")!=std::string::npos;
    check(rejected.status==ClockTimingStatus::Unknown&&!rejected.binary_startup_verified&&initial_unknown,
        "Binary-startup mode must reject this actual uninitialized-memory counterexample");
    imem->setHistoryRecordingEnabled(false);dmem->setHistoryRecordingEnabled(false);
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    auto drive=[&](size_t t,const std::shared_ptr<Wire<1>>& w,LogicValue v){
        sim.scheduleEvent(std::make_shared<WireUpdateEvent<1>>(t,w,v));
    };
    auto run=[&](size_t t){
        auto r=sim.drainUntilIdle(t,1000000);
        check(r.status!=DrainStatus::EventLimitReached,"Counterexample event budget exceeded");
    };
    SimulationTest::scheduleInitialEventsForTree(root,sim,0);
    drive(0,clk,L);drive(0,rst,H);drive(0,enable,H);run(1000);
    drive(1001,rst,L);run(4000);
    size_t stopped=0;
    for(size_t i=0;i<30;++i) {
        const size_t t=5000+i*2000;
        drive(t,clk,H);run(t+999);drive(t+1000,clk,L);run(t+1999);
        check(!sim.hasPendingEvents(),"Core memory path must settle with unknown byte lanes");
        const auto mask=dmem->getInputPin<4>("REQ_WRITE_MASK")->getValueAsVector();
        if(mask==Bits({X,X,L,L})&&dmem->getInputPin("REQ_VALID")->getValue()==H) {
            stopped=t+1999;break;
        }
    }
    check(stopped!=0,"Program must reach its unknown-lane store: "+family);
    auto wire=dmem->getInputPin<3>("REQ_OP")->getExternalWireBase();
    check(wire->getValueVector()==bits(3,1),"Rejected store must retain WRITE opcode");
    const auto status=dmem->getOutputPin<3>("RSP_STATUS")->getExternalWireBase()->getValueVector();
    check(status==bits(3,6),"Unknown write mask must report InvalidRequest");
    run(stopped+500);
    check(!sim.hasPendingEvents()&&wire->getValueVector()==bits(3,1),"Frozen clock must not oscillate");
    for(size_t i=0;i<12;++i) {
        const size_t t=stopped+1000+i*2000;
        drive(t,clk,H);run(t+999);drive(t+1000,clk,L);run(t+1999);
    }
    check(core->getOutputPin("TRAPPED")->getValue()==H,"Invalid store must retire a precise trap");
    check(dmem->memoryStorage().readLogic(0,4)==Bits(32,X),"Rejected store must not change memory");
    std::cout<<family<<": unknown-lane store settled and trapped without a write or feedback loop\n";
}
}
int main(){try{validator();feedback();
    coreCounterexample("rv32i.core.educational-single-cycle");coreCounterexample("rv32i.core.baseline-pipeline");
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
