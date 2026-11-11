#include "tests/RV32IMRecordedTests.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/GuestScenarioTests.hpp"
#include "RV32IMScenarioProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "tests/ComponentTestModel.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/BasicComponent.hpp"
#include <algorithm>
#include <functional>
#include <tuple>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::vector<uint8_t> tinyElf(){
    const auto lui=[](unsigned rd,uint32_t upper){return (upper<<12)|(rd<<7)|0x37u;};
    const auto addi=[](unsigned rd,unsigned rs,uint32_t imm){return ((imm&4095)<<20)|(rs<<15)|(rd<<7)|0x13u;};
    const auto sw=[](unsigned rs2,unsigned rs1,uint32_t offset){return ((offset>>5)<<25)|(rs2<<20)|(rs1<<15)|(2u<<12)|((offset&31)<<7)|0x23u;};
    const std::vector<uint32_t> words{lui(1,0x10000),addi(2,0,66),0x00000463,sw(2,1,0),addi(2,0,65),sw(2,1,0),
        lui(3,0x20000),lui(4,0x031ce),addi(4,4,255),sw(4,3,0),
        lui(5,0x10003),addi(6,0,1),sw(6,5,20),lui(7,0x10001),0x0003a403,
        lui(9,0x3f),sw(8,9,0),addi(10,0,7),addi(11,0,3),0x02b50633,sw(12,9,4),addi(13,0,4095),sw(13,9,8)&~(7u<<12),0x0084a703,0x00100073};
    std::vector<uint8_t> elf(0x100+4*words.size());
    auto put=[&](size_t offset,uint32_t value,unsigned bytes=4){for(unsigned i=0;i<bytes;++i)elf.at(offset+i)=value>>(8*i);};
    elf[0]=127;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=elf[5]=elf[6]=1;
    put(16,2,2);put(18,243,2);put(20,1);put(28,52);put(40,52,2);put(42,32,2);put(44,1,2);
    put(52,1);put(56,0x100);put(68,4*words.size());put(72,4*words.size());put(76,7);put(80,4);
    for(size_t i=0;i<words.size();++i)put(0x100+4*i,words[i]);return elf;
}
uint32_t word(const std::vector<uint8_t>& bytes,size_t index){uint32_t value=0;for(unsigned b=0;b<4;++b)value|=uint32_t(bytes.at(4*index+b))<<(8*b);return value;}
void checkSystem(unsigned configuration){
    const auto elf=tinyElf();
    {
        circuit::runtime::RecordedGuestOptions o;o.max_cycles=16000;o.max_instructions=1000;o.max_frames=2;
        o.machine.caches=configuration==1||configuration==2;if(configuration==2)o.machine.memory=circuit::system::MainMemory::DRAM;
        if(configuration==5)o.system="games-machine-two-bus";
        if(configuration==6)o.system="games-machine-registered-bridge";
        if(configuration==7)o.system="games-machine-checked";
        if(configuration>=3){o.start_time=(SimulationTime{1}<<(configuration==3?54:63))+123;o.reset_release=checkedTimeAdd(o.start_time,2000);o.origin=checkedTimeAdd(o.start_time,3000);}
        std::cout<<"Computer system configuration "<<configuration<<" ("<<o.system<<")"<<std::endl;
        const auto result=circuit::runtime::runRecordedGuest(elf,{},o);
        require(result.console=="A"&&result.frames.size()==1,"System console/presentation mismatch");
        const auto& pixels=result.frames[0].pixels;
        require(pixels[0]==255&&pixels[1]==224&&pixels[2]==28&&pixels[3]==3,"Pixel order or PRESENT visibility mismatch");
        require(std::all_of(pixels.begin()+4,pixels.end(),[](auto value){return value==0;}),"Pixel zero initialization mismatch");
        require(word(result.report,0)>0&&word(result.report,1)==21&&word(result.report,2)==255&&result.state.x[14]==255,"Timer or integrated M instruction mismatch");
        require(!result.setup_violations,"System setup violation");
        std::cout<<result.cycles<<" cycles, "<<result.instructions<<" retired, "
                 <<result.execution_seconds<<" execution seconds, "<<result.events.processed_events<<" events"<<std::endl;
    }
}
void largerDisplay(){
    using namespace circuit::runtime;
    for(const auto& shape:std::vector<std::pair<size_t,size_t>>{{128,96},{97,65}}) {
        const auto [width,height]=shape;RecordedGuestOptions options;
        options.machine.width=width;options.machine.height=height;options.machine.console_depth=2;options.machine.button_depth=7;
        options.max_cycles=3000;options.max_instructions=100;options.max_frames=1;
        auto elf=tinyElf();
        const uint32_t address=0x20000000u+uint32_t((width*height-1)&~size_t{3});
        const uint32_t upper=(address+0x800)>>12,low=address&4095;
        const std::vector<uint32_t> words{(upper<<12)|(1u<<7)|0x37u,(low<<20)|(1u<<15)|(1u<<7)|0x13u,
            0x0e000113,0x0020a023,0x100031b7,0x0001a283,0x0041a303,0x00100213,0x0041aa23,0x00100073};
        for(size_t i=0;i<words.size();++i)for(unsigned byte=0;byte<4;++byte)elf.at(0x100+4*i+byte)=words[i]>>(8*byte);
        const auto result=runRecordedGuest(elf,{},options);
        require(result.frames.size()==1&&!result.setup_violations,"Larger-screen CPU presentation failed");
        require(result.state.x[5]==width&&result.state.x[6]==height,"CPU geometry reads differ from configured display");
        const auto& frame=result.frames[0];require(frame.width==width&&frame.height==height&&frame.stride==width,"Larger-screen capture geometry");
        require(frame.pixels.size()==width*height&&frame.pixels.at(address-0x20000000u)==224,"Larger-screen high video address did not reach capture");
        require(std::all_of(frame.known.begin(),frame.known.end(),[](auto mask){return mask==255;}),"Larger-screen capture has unknown pixels");
        std::cout<<width<<"x"<<height<<": "<<result.cycles<<" cycles, "<<result.execution_seconds<<"s, "<<result.video_allocated_bytes<<" resident video bytes"<<std::endl;
    }
}
void lockstep(){
    using namespace circuit::runtime;
    RecordedGuestOptions options;options.max_cycles=16000;options.max_instructions=1000;options.max_frames=2;
    const auto elf=tinyElf();const auto baseline=runRecordedGuest(elf,{},options);
    const auto compare=[&](const RecordedGuestResult& other,bool same_events){
        require(baseline.cycles==other.cycles&&baseline.instructions==other.instructions&&
            baseline.console==other.console&&baseline.report==other.report&&baseline.report_known==other.report_known&&
            baseline.state.x==other.state.x&&baseline.state.pc==other.state.pc&&
            baseline.state.halted==other.state.halted&&baseline.state.trapped==other.state.trapped&&baseline.state.trap_cause==other.state.trap_cause&&
            baseline.presentations==other.presentations&&baseline.changed_frames==other.changed_frames,
            "Repeated/fidelity/quantum device-system state mismatch");
        require(baseline.frames.size()==other.frames.size()&&baseline.device_reads.size()==other.device_reads.size(),
            "Repeated recording sizes differ");
        for(size_t i=0;i<baseline.frames.size();++i){const auto&a=baseline.frames[i];const auto&b=other.frames[i];
            require(a.time==b.time&&a.sequence==b.sequence&&a.width==b.width&&a.height==b.height&&
                a.stride==b.stride&&a.pixels==b.pixels&&a.known==b.known,"Repeated frame differs");}
        for(size_t i=0;i<baseline.device_reads.size();++i){const auto&a=baseline.device_reads[i];const auto&b=other.device_reads[i];
            require(a.tick==b.tick&&a.address==b.address&&a.value==b.value,"Repeated device sample differs");}
        if(same_events)require(baseline.events.processed_events==other.events.processed_events&&
            baseline.events.scheduled_events==other.events.scheduled_events&&
            baseline.events.maximum_event_queue_depth==other.events.maximum_event_queue_depth,
            "Event accounting depends on repetition or quantum");
    };
    compare(runRecordedGuest(elf,{},options),true);
    options.quantum_events=97;compare(runRecordedGuest(elf,{},options),true);
    options.expanded_core=true;compare(runRecordedGuest(elf,{},options),false);
    options.compact_children=false;compare(runRecordedGuest(elf,{},options),false);
    options.fidelity_profile="visual";
    auto visual=runRecordedGuest(elf,{},options);compare(visual,false);
    require(visual.profile_name=="fidelity-visual-v1" && visual.fidelity_profile=="visual" &&
        visual.build_manifest.find("device.framebuffer")!=std::string::npos,
        "Visual run did not record its named profile and actual selections");
}
void validation(){
    const auto elf=tinyElf();
    const auto rejected=[&](const std::vector<circuit::runtime::ButtonInput>& inputs,circuit::runtime::RecordedGuestOptions o={}){
        bool failed=false;try{(void)circuit::runtime::runRecordedGuest(elf,inputs,o);}catch(const std::exception&){failed=true;}require(failed,"Invalid runtime input accepted");
    };
    rejected({{3000,1,0,true}});rejected({{4500,1,0,true}});rejected({{4501,1,32,true}});
    rejected({{4501,1,0,true},{4501,2,1,true}});rejected({{4501,2,0,true},{5501,1,1,true}});
    circuit::runtime::RecordedGuestOptions paced;
    paced.presentation_inputs={{{1,true}}};
    rejected({{4501,1,1,true}},paced); // Absolute and paced input cannot mix.
    paced.presentation_inputs={{{32,true}}};rejected({},paced);
    paced.presentation_inputs={std::vector<circuit::runtime::ButtonTransition>(8,{1,true})};rejected({},paced);
    circuit::runtime::RecordedGuestOptions o;o.quantum_events=0;rejected({},o);
    o={};o.machine.timer_divisor=0;rejected({},o);
    o={};o.machine.width=1;o.machine.height=1;o.machine.video_bytes=4;rejected({},o);
    o={};o.machine.button_depth=8;rejected({},o);
    o={};o.machine.console_depth=0;rejected({},o);
    o={};o.machine.width=4096;o.machine.height=65;rejected({},o);
    o={};o.max_console_bytes=0;rejected({},o);o={};o.max_device_reads=65537;rejected({},o);
    using namespace circuit::runtime;
    const uint64_t sequence=(uint64_t{1}<<54)+1;
    const std::vector<ButtonInput> transitions{{4501,sequence,1,true},{5501,sequence+1,1,false},{6501,sequence+2,31,true}};
    RecordedGuestOptions good;good.max_cycles=16000;good.max_instructions=1000;good.max_frames=2;
    const auto result=runRecordedGuest(elf,transitions,good);
    require(result.applied_inputs.size()==transitions.size(),"Recorded sampled-input count differs");
    for(size_t i=0;i<transitions.size();++i){const auto&a=transitions[i];const auto&b=result.applied_inputs[i];
        require(a.tick==b.tick&&a.sequence==b.sequence&&a.button==b.button&&a.down==b.down,"Consecutive input log shifted to the next transition");}
    require(recordedGuestJson(result).find("\"sequence\":\""+std::to_string(sequence)+"\"")!=std::string::npos,"Wide input sequence lost exact JSON transport");
    const auto final_boundary=checkedTimeAdd(good.origin,checkedTimeAdd(checkedTimeMultiply(result.cycles-1,good.period),good.period/2+1));
    std::string failure;
    try{(void)runRecordedGuest(elf,{{final_boundary,1,1,true}},good);}catch(const std::exception& error){failure=error.what();}
    require(failure=="Guest stopped before scripted input was sampled","A future scheduled event was counted as sampled after halt");

}
void evidenceBudgets(){
    using namespace circuit::runtime;
    for(bool console:{false,true}){
        auto fixture=recordedGuestFixture("bringup");
        if(console)fixture.options.max_console_bytes=1;else fixture.options.max_device_reads=1;
        bool exhausted=false;std::string failure;
        try{(void)runRecordedGuest(fixture.elf,fixture.input,fixture.options);}
        catch(const std::exception& e){failure=e.what();exhausted=failure==(console?"Console byte budget exhausted":"Device sample budget exhausted");}
        if(!exhausted)throw std::runtime_error("Evidence budget did not fail explicitly at its bound: "+failure);
    }
    RecordedGuestOptions options;options.machine.width=options.machine.height=512;
    options.max_cycles=3000;options.max_instructions=100;options.max_frames=64;
    auto elf=tinyElf();std::vector<uint32_t> words{0x100030b7,0x00100113};
    for(unsigned i=0;i<17;++i)words.push_back(0x0020aa23);words.push_back(0x00100073);
    for(size_t i=0;i<words.size();++i)for(unsigned byte=0;byte<4;++byte)elf.at(0x100+4*i+byte)=words[i]>>(8*byte);
    std::string failure;try{(void)runRecordedGuest(elf,{},options);}catch(const std::exception& error){failure=error.what();}
    require(failure=="Captured pixel byte budget exhausted","Larger frames did not enforce aggregate capture budget");
    options.capture_frames=false;const auto result=runRecordedGuest(elf,{},options);
    require(result.presentations==17&&result.frames.empty()&&!result.setup_violations,"Capture-disabled run retained frames or lost presentations");

}
void faults(){
    using namespace circuit::runtime;
    const auto lui=[](unsigned rd,uint32_t upper){return (upper<<12)|(rd<<7)|0x37u;};
    for(const auto& words:std::vector<std::vector<uint32_t>>{
        {0x04000033,0x00100073}, // reserved encoding
        {lui(1,0x10000),0x00008067,0x00100073}, // attempted MMIO instruction fetch
        {0x00202083,0x00100073}, // misaligned RAM word load, no bus request
        {lui(1,0x10000),0x1000a103,0x00100073}, // gap between register windows
        {lui(1,0x30000),0x0000a103,0x00100073}, // unmapped data address
        {lui(1,0x20001),0x0000a023,0x00100073}}){ // beyond video backing
        auto elf=tinyElf();
        for(size_t i=0;i<words.size();++i)for(unsigned byte=0;byte<4;++byte)
            elf.at(0x100+4*i+byte)=words[i]>>(8*byte);
        RecordedGuestOptions options;options.max_cycles=2000;options.max_instructions=100;
        std::string failure;
        try{(void)runRecordedGuest(elf,{},options);}catch(const std::exception& error){failure=error.what();}
        if(failure!="Guest trapped")throw std::runtime_error("Fault did not reach the checked architectural trap: "+failure);
        require(BasicComponent::setupViolations()==0,"Fault run observed setup violations");
    }
}
// The games machine's structure without a program: reset, disabled, one
// queued input. Quick to open in any profile (the machine scenarios run a
// program and are best in Compact).
circuit::test::ComponentTestSpec structureSpec(){
    using namespace circuit::test;
    ComponentTestSpec spec{"ComputerSystemStructureScenarioTest","system.computer","COMPUTER_SYSTEM_ROOT",
                           {{"machine","games-machine"},{"width","4"},{"height","4"}},{{"contract",{},{},{}}}};
    NamedValues inputs,expected;
    for(const auto&p:circuit::builtinComponentCatalog().contract(spec.contract_id).pinsFor(spec.parameters))
        if(p.direction==PinType::INPUT)inputs[p.name]=logicBits(p.width,0);
    inputs["TX_READY"]=logicBits(1,1);inputs["RST"]=logicBits(1,1);
    for(auto p:{"HALTED","TRAPPED","TX_VALID","PRESENT"})expected[p]=logicBits(1,0);
    expected["FRAME_SEQUENCE"]=logicBits(32,0);expected["TRAP_CAUSE"]=logicBits(4,0);
    auto action=[&](const char* name){spec.scenarios[0].actions.push_back({name,CheckpointKind::Settled,inputs,expected,true,{}});};
    action("reset");inputs["RST"]=logicBits(1,0);action("disabled");
    inputs["INPUT_VALID"]=logicBits(1,1);inputs["INPUT_BUTTON"]=logicBits(5,1);inputs["INPUT_DOWN"]=logicBits(1,1);
    inputs["CLK"]=logicBits(1,1);action("queued-input");inputs["CLK"]=logicBits(1,0);inputs["INPUT_VALID"]=logicBits(1,0);action("held-disabled");
    return spec;
}
class Check : public StandaloneVerificationTest {
    std::string name_;std::function<void()> check_;
public:Check(std::string name,std::function<void()> check):name_(std::move(name)),check_(std::move(check)){}
    std::string getTestName()const override{return name_;}void verifyResults()override{check_();}
};
}
void appendRV32IMRecordedTests(std::vector<TestRegistryEntry>& entries){
    auto add=[&](const std::string& name,std::function<void()> check){entries.push_back({name,name,RegisteredTestKind::Integration,name=="ComputerSystemTest"?std::vector<std::string>{"system.computer"}:std::vector<std::string>{},{},{"simulation","rv32im","device","phase4"},[name,check]{return std::make_unique<Check>(name,check);},false});};
    add("ComputerSystemTest",[]{checkSystem(0);});
    add("ComputerSystemCachedFFTest",[]{checkSystem(1);});
    add("ComputerSystemCachedDRAMTest",[]{checkSystem(2);});
    add("ComputerSystemWideTimeTest",[]{checkSystem(3);});
    add("ComputerSystemUnsignedTimeTest",[]{checkSystem(4);});
    add("ComputerSystemTwoBusTest",[]{checkSystem(5);});
    add("ComputerSystemRegisteredBridgeTest",[]{checkSystem(6);});
    add("ComputerSystemPortCheckTest",[]{checkSystem(7);});
    add("ComputerSystemLockstepTest",lockstep);
    add("RV32IMLargerDisplayTest",largerDisplay);
    add("RV32IMRecordedInputValidationTest",validation);
    add("RV32IMRecordedEvidenceBudgetTest",evidenceBudgets);
    add("ComputerSystemFaultTest",faults);
    // The bring-up program on each machine and the short games run as
    // visualizer scenarios with every check (GuestScenarioTests).
    appendGuestScenarioTests(entries);
    const auto profile=circuit::test::rv32imInspectionProfile();
    entries.push_back({"ComputerSystemStructureScenarioTest","ComputerSystemTest",RegisteredTestKind::Sequential,{"system.computer"},"contract",
        {"simulation","rv32im","phase4-scenario"},
        [profile]{return std::make_unique<circuit::test::ComponentScenarioTest>(structureSpec(),"contract",profile);},true});
}
