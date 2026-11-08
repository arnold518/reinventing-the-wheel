#include "runtime/RecordedGuest.hpp"
#include "modules/device/DeviceConfiguration.hpp"
#include "simulator/SignalTrace.hpp"
#include "components/selection/FidelityPresets.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/BuildManifest.hpp"
#include "rv32im/RV32IMInstruction.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/MachineState.hpp"
#include "simulator/StateArchive.hpp"
#include <bit>
#include <cstring>
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "simulator/Event.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace circuit::runtime {
namespace {
using Clock = std::chrono::steady_clock;
void demand(bool condition,const std::string& why) { if(!condition)throw std::runtime_error(why); }
uint32_t signal(const std::shared_ptr<PinBase>& p) {
    auto wire=p->getExternalWireBase();if(!wire)wire=p->getInternalWireBase();
    const auto bits=wire?wire->getValueVector():p->getValueAsVector();uint32_t value=0;
    for(size_t i=0;i<bits.size();++i){demand(bits[i]==LogicValue::LOW||bits[i]==LogicValue::HIGH,"Unknown signal "+p->getID());if(bits[i]==LogicValue::HIGH)value|=uint32_t{1}<<i;}
    return value;
}
uint32_t out(const std::shared_ptr<IOComponent>& p,const std::string& n){return signal(p->getOutputPinDynamic(n));}
uint32_t in(const std::shared_ptr<IOComponent>& p,const std::string& n){return signal(p->getInputPinDynamic(n));}
std::shared_ptr<IOComponent> child(const std::shared_ptr<IOComponent>& p,const std::string& name) {
    for(const auto& c:p->getChildren())if(c->getName()==name)return std::dynamic_pointer_cast<IOComponent>(c);
    throw std::logic_error("Missing child "+name);
}
std::vector<LogicValue> bits(size_t width,uint64_t value){std::vector<LogicValue> result(width,LogicValue::LOW);for(size_t i=0;i<width;++i)if((value>>i)&1)result[i]=LogicValue::HIGH;return result;}
struct Transaction {
    uint32_t address=0,op=0,data=0,mask=0,value=0,status=0;
    bool sampled=false;
};
struct Devices {
    uint32_t timer=0,divider=0,levels=0,sequence=0,leds=0,blink=0,cycles=0;
    bool overflow=false;
    std::deque<uint32_t> buttons;
    std::deque<uint8_t> console;
    std::vector<uint8_t> video, known;
    explicit Devices(size_t bytes):video(bytes),known(bytes,255) {}
    uint32_t read(uint32_t address,const circuit::system::GameMachineOptions& c) const {
        switch(address){
        case 0x10000004:return console.size()<c.console_depth;case 0x10000008:return console.size();
        case 0x10001000:return timer;case 0x10001004:return c.timer_divisor;case 0x10001008:return c.timer_frequency_hz;
        case 0x10002000:return levels;case 0x10002004:return buttons.empty()?0:buttons.front();
        case 0x10002008:return buttons.size();case 0x1000200c:return (!buttons.empty())|(uint32_t(overflow)<<1);
        case 0x10003000:return c.width;case 0x10003004:return c.height;case 0x10003008:return c.width;
        case 0x1000300c:return 1;case 0x10003010:return sequence;
        case 0x10004000:return leds;case 0x10004004:return blink;
        default:throw std::runtime_error("Unmapped device read in recorded guest");
        }
    }
};
bool registers(uint32_t a,bool leds) {
    for(uint32_t base:{0x10000000u,0x10001000u,0x10002000u,0x10003000u})
        if(a>=base&&a<base+0x100u)return true;
    return leds&&a>=0x10004000u&&a<0x10004100u&&(a==0x10004000u||a==0x10004004u);
}
// The LED panel's pins: lit LEDs, blinking from its free-running counter.
uint32_t shownLeds(uint32_t leds,uint32_t blink,uint32_t cycles){return blink==0||!((cycles>>blink)&1)?leds:0;}
bool video(uint32_t a,size_t bytes) {return a>=0x20000000&&uint64_t(a)<0x20000000ull+bytes;}
uint32_t laneValue(uint32_t word,uint32_t address,uint32_t funct3) {
    const unsigned bytes=1u<<(funct3&3);const unsigned shift=8*(address&3);
    uint32_t value=word>>shift;
    if(bytes<4){const auto mask=(1u<<(8*bytes))-1;value&=mask;if(!(funct3&4)&&(value&(1u<<(8*bytes-1))))value|=~mask;}
    return value;
}
// MMIO loads/stores are the only extra ISA reference boundary. Ordinary I/M
// instructions continue through the independent existing instruction oracle.
rv32i::RV32IInstructionTrace step(rv32i::RV32IState& state,rv32i::RV32IFunctionalMemory& ram,
                                std::deque<Transaction>& completed) {
    if(state.halted||state.trapped||(state.pc&3u)||!ram.canAccess(state.pc,4))
        return rv32im::step(state,ram,ram).base;
    const auto raw=ram.readU32(state.pc);const auto decoded=rv32im::decode(raw);
    const auto opcode=raw&127u;const bool load=opcode==3,store=opcode==35;
    uint32_t address=0;Transaction transaction;
    if(decoded.legal()&&(load||store)) {
        const auto rs1=(raw>>15)&31,rs2=(raw>>20)&31;
        address=state.x[rs1]+uint32_t(load?rv32i::RV32IDecoder::immediateI(raw):rv32i::RV32IDecoder::immediateS(raw));
        const auto size=1u<<((raw>>12)&3);
        if(address&(size-1))return rv32im::step(state,ram,ram).base;
        demand(!completed.empty(),"Memory instruction retired without a completed bus transaction");
        transaction=completed.front();completed.pop_front();
        const uint32_t expected_mask=((1u<<size)-1)<<(address&3);
        demand(transaction.address==(address&~3u)&&transaction.op==(store?1u:0u),"ISA/bus address or operation mismatch");
        if(store){
            uint32_t byte_mask=0;for(unsigned byte=0;byte<4;++byte)if(expected_mask&(1u<<byte))byte_mask|=0xffu<<(8*byte);
            demand(transaction.mask==expected_mask&&(transaction.data&byte_mask)==((state.x[rs2]<<(8*(address&3)))&byte_mask),"ISA/bus store lanes mismatch");
        }
    }
    if(!decoded.legal()||(!load&&!store)||address<0x40000) return rv32im::step(state,ram,ram).base;
    rv32i::RV32IInstructionTrace trace;trace.pc_before=state.pc;trace.raw_instruction=raw;trace.decoded=decoded.base;
    trace.instruction_index=state.instruction_count;trace.rs1=(raw>>15)&31;trace.rs2=(raw>>20)&31;trace.rd=(raw>>7)&31;
    trace.rs1_value=state.x[trace.rs1];trace.rs2_value=state.x[trace.rs2];
    const uint32_t f=(raw>>12)&7,size=1u<<(f&3);
    trace.memory.kind=store?rv32i::RV32IMemoryAccessKind::Write:rv32i::RV32IMemoryAccessKind::Read;
    trace.memory.address=address;trace.memory.size=size==1?rv32i::RV32IMemorySize::Byte:size==2?rv32i::RV32IMemorySize::Halfword:rv32i::RV32IMemorySize::Word;
    // Only LB/LH extend a narrow sign; LW has no extension in RV32.
    trace.memory.sign_extend=load&&f<2;trace.memory.write_data=store?trace.rs2_value:0;
    if(transaction.status){state.trapped=true;state.trap_cause=store?rv32i::RV32IExecutionTrapCause::StoreAccessFault:rv32i::RV32IExecutionTrapCause::LoadAccessFault;trace.memory.fault=true;}
    else {
        if(load){trace.memory.read_data=laneValue(transaction.value,address,f);state.writeRegister(trace.rd,trace.memory.read_data);trace.writeback={true,trace.rd,trace.memory.read_data,trace.rd==0};}
        state.pc+=4;
    }
    ++state.instruction_count;state.forceX0();trace.pc_after=state.pc;trace.trapped=state.trapped;trace.trap_cause=state.trap_cause;return trace;
}
std::string hex(const std::vector<uint8_t>& data){static constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(2*data.size());for(auto b:data){out+=digits[b>>4];out+=digits[b&15];}return out;}
std::string quoted(const std::string& s){std::string r="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){r+='\\';r+=c;}else if(c<32||c>=127){r+="\\u00";r+=hex({c});}else r+=c;}return r+'"';}
void tick(std::ostream& s,SimulationTime t){if(t>MAX_EXACT_JSON_TIME)s<<'"'<<t<<'"';else s<<t;}
}
circuit::ParameterMap recordedMachineParameters(const RecordedGuestOptions& o) {
    circuit::ParameterMap parameters{{"machine",o.system},{"caches",o.machine.caches?"1":"0"},{"dram",o.machine.dram?"1":"0"},{"width",std::to_string(o.machine.width)},{"height",std::to_string(o.machine.height)},
      {"timer_divisor",std::to_string(o.machine.timer_divisor)},{"timer_frequency_hz",std::to_string(o.machine.timer_frequency_hz)}};
    if(o.machine.video_bytes)parameters["video_bytes"]=std::to_string(o.machine.video_bytes);
    if(o.machine.console_depth!=4)parameters["console_depth"]=std::to_string(o.machine.console_depth);
    if(o.machine.button_depth!=4)parameters["button_depth"]=std::to_string(o.machine.button_depth);
    return parameters;
}
namespace {
// Saves of a checked run: the machine (circuit::state) and the runner's
// independent models, so a resumed run keeps every check.
constexpr uint32_t RunTag=0x52554e21;      // "RUN!"
constexpr uint32_t ResultTag=0x52534c54;   // "RSLT"
constexpr uint32_t RunEndTag=0x52454e44;   // "REND"
constexpr uint32_t RunVersion=2;
uint64_t mixHash(uint64_t h,const void* data,size_t n){const auto* p=static_cast<const unsigned char*>(data);for(size_t i=0;i<n;++i){h^=p[i];h*=1099511628211ull;}return h;}
// Every number as 64 bits, so native and WebAssembly (32-bit size_t) agree.
template<class T> uint64_t mixValue(uint64_t h,T v){const uint64_t x=static_cast<uint64_t>(v);return mixHash(h,&x,sizeof x);}
uint64_t mixText(uint64_t h,const std::string& t){return mixValue(mixHash(h,t.data(),t.size()),uint64_t(t.size()));}
// What a save must have been made from: program, input and run options.
uint64_t runIdentity(const rv32i::RV32IElfImage& elf,const std::vector<ButtonInput>& input,const RecordedGuestOptions& o){
    uint64_t h=1469598103934665603ull;
    for(const auto& s:elf.segments()){h=mixValue(h,s.address);h=mixValue(h,s.bytes.size());h=mixHash(h,s.bytes.data(),s.bytes.size());}
    for(const auto& e:input){h=mixValue(h,e.tick);h=mixValue(h,e.sequence);h=mixValue(h,e.button);h=mixValue(h,e.down);}
    for(const auto& batch:o.presentation_inputs){h=mixValue(h,batch.size());for(const auto& t:batch){h=mixValue(h,t.button);h=mixValue(h,t.down);}}
    h=mixText(h,o.system);h=mixText(h,o.fidelity_profile);
    for(auto v:{o.start_time,o.period,o.reset_release,o.origin})h=mixValue(h,v);
    for(auto v:{o.machine.width,o.machine.height,o.machine.video_bytes,o.machine.console_depth,o.machine.button_depth})h=mixValue(h,v);
    for(auto v:{o.machine.timer_divisor,o.machine.timer_frequency_hz})h=mixValue(h,v);
    for(bool v:{o.machine.caches,o.machine.dram,o.compact_children,o.expanded_core})h=mixValue(h,v);
    return h;
}
void putTransaction(circuit::state::Writer& w,const Transaction& t){for(auto v:{t.address,t.op,t.data,t.mask,t.value,t.status})w.u32(v);w.boolean(t.sampled);}
Transaction getTransaction(circuit::state::Reader& r){Transaction t;for(auto* v:{&t.address,&t.op,&t.data,&t.mask,&t.value,&t.status})*v=r.u32();t.sampled=r.boolean();return t;}
// The machine's RAM as plain bytes (an unknown byte reads 0, as an unwritten
// byte of a functional memory does).
std::vector<uint8_t> ramImage(const MemoryStorage& ram,size_t bytes){
    const auto bits=ram.readLogic(0,bytes);std::vector<uint8_t> image(bytes,0);
    for(size_t i=0;i<bytes;++i){uint8_t v=0;bool known=true;
        for(unsigned b=0;b<8;++b){const auto x=bits[8*i+b];if(x==LogicValue::HIGH)v|=uint8_t(1u<<b);else if(x!=LogicValue::LOW)known=false;}
        image[i]=known?v:0;}
    return image;
}
// A functional memory as the bytes where it differs from the machine's RAM
// (normally none: both see the same writes, a cycle apart at most).
void putFunctional(circuit::state::Writer& w,const rv32i::RV32IFunctionalMemory& m,const std::vector<uint8_t>& ram){
    const auto bytes=m.readBytes(0,m.sizeBytes());
    std::vector<uint32_t> differ;for(uint32_t a=0;a<bytes.size();++a)if(bytes[a]!=ram[a])differ.push_back(a);
    w.size(m.sizeBytes());w.size(differ.size());for(auto a:differ){w.u32(a);w.u8(bytes[a]);}
}
void getFunctional(circuit::state::Reader& r,rv32i::RV32IFunctionalMemory& m,const std::vector<uint8_t>& ram){
    if(r.u64()!=m.sizeBytes()||ram.size()!=m.sizeBytes())r.fail("a reference memory of another size");
    m.clear();m.loadBytes(0,ram);
    for(size_t i=0,n=r.size();i<n;++i){const auto a=r.u32();const auto v=r.u8();if(a>=m.sizeBytes())r.fail("a reference byte outside memory");m.writeU8(a,v);}
}
void putSeconds(circuit::state::Writer& w,double v){w.u64(std::bit_cast<uint64_t>(v));}
double getSeconds(circuit::state::Reader& r){return std::bit_cast<double>(r.u64());}
void validateRun(const std::vector<ButtonInput>& input,const RecordedGuestOptions& o) {
    o.machine.validate();
    if(const auto restrictions=circuit::fidelityPresetRestrictions("system.computer");
       restrictions.contains(o.fidelity_profile))throw std::invalid_argument(restrictions.at(o.fidelity_profile));
    demand(input.size()<=65536,"Script input budget exhausted");
    demand(o.period>=100&&!(o.period&1)&&o.reset_release>=checkedTimeAdd(o.start_time,checkedTimeMultiply(o.period,2))&&o.origin>=checkedTimeAdd(o.reset_release,o.period),"Invalid recorded clock boundary");
    const bool paced=!o.presentation_inputs.empty();
    const size_t execution_limit=paced?100000000:10000000;
    demand(o.max_cycles&&o.max_cycles<=execution_limit&&o.max_instructions&&o.max_instructions<=execution_limit&&o.max_frames<=1024&&o.quantum_events&&o.quantum_events<=1000000,"Invalid recorded guest limits");
    demand(!paced||input.empty(),"Choose absolute or presentation-paced input");
    demand(!paced||o.presentation_inputs.size()<o.max_frames,"Too many presentation input batches");
    size_t transitions=0;
    for(const auto& batch:o.presentation_inputs){
        demand(!batch.empty()&&batch.size()<=o.machine.button_depth,"Invalid presentation input batch");
        for(const auto& event:batch)demand(event.button<32,"Invalid presentation button");
        transitions+=batch.size();
    }
    demand(transitions<=65536,"Script input budget exhausted");
    demand(o.max_console_bytes&&o.max_console_bytes<=65536&&o.max_device_reads&&o.max_device_reads<=65536,"Invalid recording evidence limits");
    for(size_t i=0;i<input.size();++i){const auto&e=input[i];
        demand(e.button<32&&e.tick>o.origin&&e.sequence>0,"Invalid scripted input");
        demand((e.tick-o.origin)%o.period==o.period/2+1,"Input must follow a falling edge by one tick");
        if(i)demand(e.tick>input[i-1].tick&&e.sequence>input[i-1].sequence,"Input ticks/sequences must increase");
    }
}
rv32i::RV32IElfImage guestImage(const std::vector<uint8_t>& elf_bytes) {
    auto elf=rv32i::RV32IElfImage::fromBytes(elf_bytes);elf.requireFitsMemory(0x40000);demand(elf.entryPoint()==0,"Guest entry must be zero");
    return elf;
}
RecordedGuestResult runChecked(const std::shared_ptr<circuit::system::ComputerSystem>& machine,Simulator& sim,
                               const rv32i::RV32IElfImage& elf,const std::vector<ButtonInput>& input,
                               const RecordedGuestOptions& o,Clock::time_point setup_start);
}
RecordedGuestResult runRecordedGuest(const std::vector<uint8_t>& elf_bytes,const std::vector<ButtonInput>& input,const RecordedGuestOptions& o) {
    const auto setup_start=Clock::now();
    validateRun(input,o);
    const auto elf=guestImage(elf_bytes);
    auto profile=o.compact_children?circuit::BuildProfileBuilder("recorded-device-compact-children").addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::any())).unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build():circuit::strictAllStructural();
    if(o.compact_children&&o.expanded_core)profile=circuit::withProfileOverrides(std::move(profile),{
        circuit::preferFidelity(circuit::Fidelity::Structural,
            circuit::ProfileSelector::contract("rv32im.core.educational-five-stage"),
            "expand CPU assembly for recorded comparison")},"recorded-device-expanded-core");
    if(!o.fidelity_profile.empty())profile=circuit::fidelityPreset(o.fidelity_profile,circuit::builtinComponentCatalog());
    auto built=circuit::builtinComponentCatalog().createRoot({"system.computer","MACHINE",recordedMachineParameters(o)},profile);
    if(!o.fidelity_profile.empty())circuit::validateFidelityPreset(built.root,o.fidelity_profile);
    auto machine=std::dynamic_pointer_cast<circuit::system::ComputerSystem>(built.root);demand(machine!=nullptr,"system.computer did not build a computer system");machine->setMemoryHistoryRecordingEnabled(false);
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    auto result=runChecked(machine,sim,elf,input,o,setup_start);
    result.profile_name=profile.name();result.profile_fingerprint=built.manifest->profileFingerprint();
    result.build_manifest=built.manifest->serialize();
    return result;
}
RecordedGuestResult runRecordedGuestOn(const std::shared_ptr<circuit::system::ComputerSystem>& machine,Simulator& sim,
    const std::vector<uint8_t>& elf_bytes,const std::vector<ButtonInput>& input,const RecordedGuestOptions& o) {
    const auto setup_start=Clock::now();
    demand(machine!=nullptr,"No computer system to run");
    validateRun(input,o);
    return runChecked(machine,sim,guestImage(elf_bytes),input,o,setup_start);
}
namespace {
RecordedGuestResult runChecked(const std::shared_ptr<circuit::system::ComputerSystem>& machine,Simulator& sim,
                               const rv32i::RV32IElfImage& elf,const std::vector<ButtonInput>& input,
                               const RecordedGuestOptions& o,Clock::time_point setup_start) {
    // A resumed run takes memory from the save; a running machine refuses images.
    if(!o.resume)machine->loadElf(elf);
    ComponentBuilder b(machine);
    // The host's wires on the machine's pins: made by the first run, reused
    // when a session continues on the same machine.
    std::map<std::string,std::shared_ptr<WireBase>> existing;
    for(const auto& wire:machine->getAllWires())if(wire&&wire->getName().rfind("HOST_",0)==0)existing[wire->getName()]=wire;
    std::map<std::string,std::shared_ptr<WireBase>> inputs;
    for(const auto&[n,p]:machine->getAllInputPins()){
        const auto found=existing.find("HOST_"+n);
        inputs[n]=found!=existing.end()?found->second:b.addNewWireDynamic("HOST_"+n,p->getWidth(),nullptr,{p});
    }
    for(const auto&[n,p]:machine->getAllOutputPins())if(!existing.contains("HOST_"+n))b.addNewWireDynamic("HOST_"+n,p->getWidth(),p,{});
    std::unique_ptr<SignalTraceSubscription> trace_subscription;
    if(o.signal_trace) trace_subscription=std::make_unique<SignalTraceSubscription>(sim,o.signal_trace(machine,sim));
    auto drive=[&](SimulationTime t,const std::string& n,uint64_t value){sim.scheduleEvent(makeWireUpdateEvent(t,inputs.at(n),bits(inputs.at(n)->getWidth(),value)));};
    auto advance = [&](SimulationTime target) {
        for (;;) {
            const auto processed=sim.getPerformanceCounters().processed_events;
            demand(processed<o.max_events,"Simulation event budget exhausted");
            const auto count=static_cast<size_t>(std::min<uint64_t>(o.quantum_events,o.max_events-processed));
            const auto result=sim.drainUntilIdle(target,count);
            if(result.status!=DrainStatus::EventLimitReached)break;
        }
    };
    demand(!o.resume||!o.signal_trace,"A resumed run cannot stream a signal trace");
    demand(!o.save_at==!o.on_save,"save_at and on_save go together");
    BasicComponent::resetSetupViolations();
    // A resumed run takes its queue, pins and wires from the save instead.
    if(!o.resume){SimulationTest::scheduleInitialEventsForTree(machine,sim,o.start_time);
        for(const auto&[n,p]:inputs)drive(o.start_time,n,n=="RST"||n=="ENABLE"||n=="TX_READY");
        drive(o.reset_release,"RST",0);}
    rv32i::RV32IFunctionalMemory reference(0x40000),bus_ram(0x40000);
    if(!o.resume)for(const auto&s:elf.segments()){reference.loadBytes(s.address,s.bytes);bus_ram.loadBytes(s.address,s.bytes);}
    rv32i::RV32IState state;Devices device(o.machine.videoCapacityBytes());RecordedGuestResult result;
    std::optional<Transaction> pending;std::deque<Transaction> completed;
    std::map<uint32_t,uint8_t> ram_writes;
    std::map<std::string,std::shared_ptr<IOComponent>> ports;
    const bool has_leds=machine->description().findPart("LEDS")!=nullptr;
    const bool has_checkers=machine->description().check_ports;
    for(auto n:{"console","timer","buttons","display","leds"}){if(std::string(n)=="leds"&&!has_leds)continue;std::string part=n;for(auto& c:part)c=char(std::toupper(static_cast<unsigned char>(c)));ports[n]=child(machine->part(part),"PORT");}
    auto core=machine->core();size_t event_index=0,sampled_event_index=0,batch_index=0;
    auto scheduled_inputs=input;
    const auto start=Clock::now();
    result.setup_seconds=std::chrono::duration<double>(start-setup_start).count();
    result.configuration=o.machine;result.system=o.system;result.fidelity_profile=o.fidelity_profile;
    result.period=o.period;result.start_time=o.start_time;result.quantum_events=o.quantum_events;
    std::vector<uint8_t> previous_pixels,previous_known;
    // Timeline events for observers, emitted once their cycle has settled.
    std::vector<GuestRunEvent> events;bool released=false;size_t line_start=0;
    auto note=[&](GuestRunEvent::Kind kind,uint64_t cycle,uint32_t value=0,std::string text={}){
        if(o.on_event)events.push_back({kind,0,cycle,state.instruction_count,value,std::move(text)});};
    auto emit=[&](SimulationTime time){
        for(auto& e:events){e.time=time;e.instructions=state.instruction_count;o.on_event(e);}events.clear();};
    // Seconds of run time, including the run before a resumed save.
    double elapsed_before=0;
    auto elapsed=[&]{return elapsed_before+std::chrono::duration<double>(Clock::now()-start).count();};
    const uint64_t identity=runIdentity(elf,input,o);
    // Saved at the top of a cycle: the previous cycle ended with advance(end),
    // so the circuit has settled and no timeline event is waiting.
    auto save_run=[&](uint64_t cycle,RecordedGuestOptions::SaveContent content){
        const bool with_results=content==RecordedGuestOptions::SaveContent::WithResults;
        circuit::state::Writer w;
        w.tag(RunTag);w.u32(RunVersion);w.u64(identity);w.u64(cycle);
        // The machine section carries its length, so a session that continues
        // on the same machine can take only the runner's part (resume_in_place).
        {circuit::state::Writer machine_part;circuit::state::saveMachine(machine,sim,machine_part);w.bytes(machine_part.data());}
        w.boolean(released);w.size(line_start);w.size(event_index);w.size(sampled_event_index);w.size(batch_index);
        w.size(scheduled_inputs.size());
        for(const auto& e:scheduled_inputs){w.time(e.tick);w.u64(e.sequence);w.u8(e.button);w.boolean(e.down);}
        w.u32(state.pc);for(auto x:state.x)w.u32(x);w.boolean(state.halted);w.boolean(state.trapped);
        w.u8(static_cast<uint8_t>(state.trap_cause));w.u64(state.instruction_count);
        {const auto ram=ramImage(machine->ramStorage(),reference.sizeBytes());putFunctional(w,reference,ram);putFunctional(w,bus_ram,ram);}
        for(auto v:{device.timer,device.divider,device.levels,device.sequence,device.leds,device.blink,device.cycles})w.u32(v);
        w.boolean(device.overflow);
        w.size(device.buttons.size());for(auto b:device.buttons)w.u32(b);
        w.bytes(std::vector<uint8_t>(device.console.begin(),device.console.end()));
        w.bytes(device.video);w.bytes(device.known);
        w.boolean(pending.has_value());if(pending)putTransaction(w,*pending);
        w.size(completed.size());for(const auto& t:completed)putTransaction(w,t);
        w.size(ram_writes.size());for(const auto&[a,v]:ram_writes){w.u32(a);w.u8(v);}
        w.bytes(previous_pixels);w.bytes(previous_known);
        w.size(BasicComponent::setupViolations());
        w.text(result.console);w.u64(result.presentations);w.u64(result.changed_frames);
        w.boolean(with_results);
        if(with_results){
            w.tag(ResultTag);
            w.size(result.frames.size());
            for(const auto& f:result.frames){w.time(f.time);w.u32(f.sequence);w.size(f.width);w.size(f.height);w.size(f.stride);w.bytes(f.pixels);w.bytes(f.known);}
            w.size(result.frame_cycles.size());for(auto v:result.frame_cycles)w.u64(v);
            w.size(result.frame_instructions.size());for(auto v:result.frame_instructions)w.u64(v);
            w.size(result.frame_seconds.size());for(auto v:result.frame_seconds)putSeconds(w,v);
            w.size(result.frame_inputs.size());for(const auto& i:result.frame_inputs){w.u32(i.levels);w.u32(i.count);w.u32(i.head);w.boolean(i.overflow);}
            w.size(result.frame_leds.size());for(auto v:result.frame_leds)w.u32(v);
            w.size(result.applied_inputs.size());for(const auto& e:result.applied_inputs){w.time(e.tick);w.u64(e.sequence);w.u8(e.button);w.boolean(e.down);}
            w.size(result.device_reads.size());for(const auto& d:result.device_reads){w.time(d.tick);w.u32(d.address);w.u32(d.value);}
            putSeconds(w,result.capture_seconds);putSeconds(w,elapsed());
        }
        w.tag(RunEndTag);
        o.on_save(cycle,checkedTimeAdd(o.origin,checkedTimeMultiply(o.period,cycle))-1,content,w.take());
    };
    size_t first_cycle=0;
    if(o.resume){
        circuit::state::Reader r(*o.resume);
        r.tag(RunTag,"run");const auto version=r.u32();if(version!=1&&version!=RunVersion)r.fail("an unsupported run save version");
        if(r.u64()!=identity)r.fail("a run of another program, input or options");
        first_cycle=static_cast<size_t>(r.u64());
        if(!first_cycle||first_cycle>=o.max_cycles)r.fail("a save outside the cycle budget");
        // Version 1 (recordings made before 2026-11-06 13:00) keeps the machine
        // inline, without its length: it can only be restored in full.
        if(version==1){if(o.resume_in_place)r.fail("a version 1 save cannot continue in place");circuit::state::restoreMachine(machine,sim,r);}
        else{const auto machine_part=r.bytes();
         if(!o.resume_in_place){circuit::state::Reader m(machine_part);circuit::state::restoreMachine(machine,sim,m);if(!m.done())m.fail("bytes after the machine");}}
        released=r.boolean();line_start=static_cast<size_t>(r.u64());event_index=static_cast<size_t>(r.u64());
        sampled_event_index=static_cast<size_t>(r.u64());batch_index=static_cast<size_t>(r.u64());
        scheduled_inputs.assign(r.size(),{});
        for(auto& e:scheduled_inputs){e.tick=r.time();e.sequence=r.u64();e.button=r.u8();e.down=r.boolean();}
        if(event_index>scheduled_inputs.size()||sampled_event_index>event_index||batch_index>o.presentation_inputs.size())r.fail("input cursors outside the input");
        state.pc=r.u32();for(auto& x:state.x)x=r.u32();state.halted=r.boolean();state.trapped=r.boolean();
        state.trap_cause=static_cast<rv32i::RV32IExecutionTrapCause>(r.u8());state.instruction_count=r.u64();
        {const auto ram=ramImage(machine->ramStorage(),reference.sizeBytes());getFunctional(r,reference,ram);getFunctional(r,bus_ram,ram);}
        for(auto* v:{&device.timer,&device.divider,&device.levels,&device.sequence,&device.leds,&device.blink,&device.cycles})*v=r.u32();
        device.overflow=r.boolean();
        device.buttons.assign(r.size(),0);for(auto& b:device.buttons)b=r.u32();
        {const auto bytes=r.bytes();device.console.assign(bytes.begin(),bytes.end());}
        device.video=r.bytes();device.known=r.bytes();
        if(device.video.size()!=o.machine.videoCapacityBytes()||device.known.size()!=device.video.size())r.fail("video memory of another size");
        if(r.boolean())pending=getTransaction(r);
        for(size_t i=0,n=r.size();i<n;++i)completed.push_back(getTransaction(r));
        for(size_t i=0,n=r.size();i<n;++i){const auto a=r.u32();ram_writes[a]=r.u8();}
        previous_pixels=r.bytes();previous_known=r.bytes();
        BasicComponent::setSetupViolations(static_cast<size_t>(r.u64()));
        result.console=r.text();result.presentations=r.u64();result.changed_frames=r.u64();
        if(r.boolean()){
            r.tag(ResultTag,"results");
            result.frames.assign(r.size(),{});
            for(auto& f:result.frames){f.time=r.time();f.sequence=r.u32();f.width=static_cast<size_t>(r.u64());f.height=static_cast<size_t>(r.u64());f.stride=static_cast<size_t>(r.u64());f.pixels=r.bytes();f.known=r.bytes();}
            result.frame_cycles.assign(r.size(),0);for(auto& v:result.frame_cycles)v=r.u64();
            result.frame_instructions.assign(r.size(),0);for(auto& v:result.frame_instructions)v=r.u64();
            result.frame_seconds.assign(r.size(),0);for(auto& v:result.frame_seconds)v=getSeconds(r);
            result.frame_inputs.assign(r.size(),{});for(auto& i:result.frame_inputs){i.levels=r.u32();i.count=r.u32();i.head=r.u32();i.overflow=r.boolean();}
            result.frame_leds.assign(r.size(),0);for(auto& v:result.frame_leds)v=r.u32();
            result.applied_inputs.assign(r.size(),{});for(auto& e:result.applied_inputs){e.tick=r.time();e.sequence=r.u64();e.button=r.u8();e.down=r.boolean();}
            result.device_reads.assign(r.size(),{});for(auto& d:result.device_reads){d.tick=r.time();d.address=r.u32();d.value=r.u32();}
            result.capture_seconds=getSeconds(r);elapsed_before=getSeconds(r);
        }
        r.tag(RunEndTag,"run end");
        if(!r.done())r.fail("bytes after the end");
        result.resumed_cycle=first_cycle;result.cycles=first_cycle;
    }
    // A session opened at a save without running: stop where it starts.
    if(o.stop_after_cycles&&first_cycle&&o.stop_after_cycles<=first_cycle){
        if(o.save_at)if(const auto content=o.save_at(first_cycle,result.presentations);content!=RecordedGuestOptions::SaveContent::None)save_run(first_cycle,content);
        result.stopped=true;result.state=state;result.instructions=state.instruction_count;result.events=sim.getPerformanceCounters();
        result.setup_violations=BasicComponent::setupViolations();result.execution_seconds=elapsed();
        return result;
    }
    for(size_t cycle=first_cycle;cycle<o.max_cycles;++cycle){
        if(o.save_at&&cycle&&!(o.resume&&cycle==first_cycle))
            if(const auto content=o.save_at(cycle,result.presentations);content!=RecordedGuestOptions::SaveContent::None)save_run(cycle,content);
        const auto edge=checkedTimeAdd(o.origin,checkedTimeMultiply(o.period,cycle));
        // Reset gets real clock edges too; the origin marks the first loop edge.
        if(!cycle){for(SimulationTime e=checkedTimeAdd(o.start_time,o.period/2);e<o.reset_release;e=checkedTimeAdd(e,o.period)){drive(e,"CLK",1);drive(checkedTimeAdd(e,o.period/2),"CLK",0);} }
        const auto fall=checkedTimeAdd(edge,o.period/2),end=checkedTimeAdd(edge,o.period-1);
        drive(edge,"CLK",1);drive(fall,"CLK",0);
        // Explicitly define the upcoming safe input boundary (including no event).
        const auto boundary=checkedTimeAdd(fall,1);bool inject=event_index<scheduled_inputs.size()&&scheduled_inputs[event_index].tick==boundary;
        drive(boundary,"INPUT_VALID",inject);
        if(inject){drive(boundary,"INPUT_BUTTON",scheduled_inputs[event_index].button);drive(boundary,"INPUT_DOWN",scheduled_inputs[event_index].down);++event_index;}
        advance(edge-1);
        const bool reset=in(machine,"RST");
        if(!reset&&!released){released=true;note(GuestRunEvent::Kind::ResetReleased,cycle);}
        const bool response=out(core,"DMEM_RSP_READY")&&in(core,"DMEM_RSP_VALID");
        const bool request=out(core,"DMEM_REQ_VALID")&&in(core,"DMEM_REQ_READY");
        if(request){demand(!pending,"More than one data request outstanding");pending=Transaction{out(core,"DMEM_REQ_ADDR"),out(core,"DMEM_REQ_OP"),out(core,"DMEM_REQ_WRITE_DATA"),out(core,"DMEM_REQ_WRITE_MASK")};
            auto&t=*pending;if(t.address<0x40000){t.value=t.op?0:bus_ram.readU32(t.address);t.sampled=true;}
            else if(video(t.address,o.machine.videoCapacityBytes())){t.value=0;for(unsigned i=0;i<4;++i){if(!t.op)demand(device.known[t.address-0x20000000+i]==255,"Guest read an unwritten pixel byte");t.value|=uint32_t(device.video[t.address-0x20000000+i])<<(8*i);}if(t.op)t.value=0;t.sampled=true;}
            else if(!registers(t.address,has_leds)){t.status=2;t.sampled=true;}
        }
        bool pop=false,clear=false,push_console=false,presentation=false;uint8_t tx=0;std::optional<std::pair<uint32_t,uint32_t>> led_write;
        for(const auto&[name,port]:ports){
            const bool fire=out(port,"READ_STROBE")||out(port,"WRITE_STROBE");if(!fire)continue;
            demand(pending.has_value(),"Device executed without CPU ownership");auto&t=*pending;
            const uint32_t base=name=="console"?0x10000000:name=="timer"?0x10001000:name=="buttons"?0x10002000:name=="leds"?0x10004000:0x10003000;
            demand(t.address>=base&&t.address<base+0x100&&out(port,"ADDRESS")==t.address-base,"Device route/address mismatch");
            t.value=t.op?0:device.read(t.address,o.machine);t.sampled=true;
            demand(in(port,"BACK_STATUS")==0,"Valid guest device request rejected");
            if(!t.op){demand(in(port,"BACK_READ_DATA")==t.value,"Device sample mismatch at "+std::to_string(t.address));demand(result.device_reads.size()<std::min(o.max_instructions,o.max_device_reads),"Device sample budget exhausted");result.device_reads.push_back({edge,t.address,t.value});}
            else if(t.mask){demand(t.mask==15,"Partial command from guest");
                if(name=="console"){push_console=true;tx=t.data;demand(t.data<=255,"Invalid console byte");}
                else if(name=="leds"){demand(t.data<(t.address==0x10004000?0x10000u:32u),"LED value wider than its register");led_write={{t.address,t.data}};}
                else {demand(t.data==1,"Invalid device command");if(name=="buttons"){pop=t.address==0x10002010;clear=t.address==0x10002014;demand(pop||clear,"Invalid button write");}
                    else if(name=="display"){demand(t.address==0x10003014,"Invalid display write");presentation=true;}
                    else throw std::runtime_error("Guest wrote read-only timer");}
            }
        }
        if(response){demand(pending&&pending->sampled,"Data response before modeled execution");auto&t=*pending;
            demand(in(core,"DMEM_RSP_STATUS")==t.status&&in(core,"DMEM_RSP_READ_DATA")==t.value,"CPU response differs from independent device/RAM model");
            if(t.op==1&&!t.status)for(unsigned i=0;i<4;++i)if(t.mask&(1u<<i)){
                const uint8_t value=t.data>>(8*i);
                if(t.address<0x40000){bus_ram.writeU8(t.address+i,value);ram_writes[t.address+i]=value;}
                else if(video(t.address,o.machine.videoCapacityBytes())){device.video[t.address-0x20000000+i]=value;device.known[t.address-0x20000000+i]=255;}
            }
            completed.push_back(t);demand(completed.size()<8,"Retirement fell behind data transactions");pending.reset();
        }
        const bool commit=out(machine,"COMMIT_VALID");
        if(commit){const auto actual=machine->pendingRetirement();auto expected=step(state,reference,completed);
            demand(actual.pc==expected.pc_before&&actual.instruction==expected.raw_instruction&&actual.terminal==(expected.halted||expected.trapped)&&(actual.terminal||actual.next_pc==expected.pc_after),"Guest ISA retirement mismatch at "+std::to_string(actual.pc));
            demand(actual.memory.kind==expected.memory.kind,"Guest memory retirement kind mismatch");
            if(expected.memory.kind!=rv32i::RV32IMemoryAccessKind::None)
                demand(actual.memory.address==expected.memory.address&&actual.memory.size==expected.memory.size&&
                    actual.memory.sign_extend==expected.memory.sign_extend&&actual.memory.write_data==expected.memory.write_data&&
                    actual.memory.read_data==expected.memory.read_data&&actual.memory.fault==expected.memory.fault,
                    "Guest memory retirement mismatch at PC "+std::to_string(actual.pc)+
                    " address "+std::to_string(actual.memory.address)+"/"+std::to_string(expected.memory.address)+
                    " size "+std::to_string(unsigned(actual.memory.size))+"/"+std::to_string(unsigned(expected.memory.size))+
                    " sign "+std::to_string(actual.memory.sign_extend)+"/"+std::to_string(expected.memory.sign_extend)+
                    " write "+std::to_string(actual.memory.write_data)+"/"+std::to_string(expected.memory.write_data)+
                    " read "+std::to_string(actual.memory.read_data)+"/"+std::to_string(expected.memory.read_data)+
                    " fault "+std::to_string(actual.memory.fault)+"/"+std::to_string(expected.memory.fault));
        }
        if(reset){device=Devices(o.machine.videoCapacityBytes());}
        else {
            demand(out(machine,"TX_VALID")==!device.console.empty(),"Console valid mismatch");
            if(!device.console.empty()){demand(out(machine,"TX_DATA")==device.console.front(),"Console byte mismatch");demand(result.console.size()<o.max_console_bytes,"Console byte budget exhausted");result.console+=char(device.console.front());device.console.pop_front();
                if(result.console.back()=='\n'){note(GuestRunEvent::Kind::ConsoleLine,cycle,0,result.console.substr(line_start,result.console.size()-line_start-1));line_start=result.console.size();}}
            if(push_console)device.console.push_back(tx);
            const bool was_full=device.buttons.size()==o.machine.button_depth,was_empty=device.buttons.empty();
            const bool push=in(machine,"INPUT_VALID");const uint32_t id=in(machine,"INPUT_BUTTON"),down=in(machine,"INPUT_DOWN");
            if(clear)device.overflow=false;if(pop&&!was_empty)device.buttons.pop_front();
            if(push){
                demand(sampled_event_index<scheduled_inputs.size(),"Device sampled input without a scripted event");
                const auto& applied=scheduled_inputs[sampled_event_index++];
                demand(applied.tick==checkedTimeAdd(edge-o.period/2,1)&&applied.button==id&&applied.down==bool(down),"Sampled input differs from its scheduled transition");
                result.applied_inputs.push_back(applied);note(GuestRunEvent::Kind::InputSampled,cycle,id|(down<<8));device.levels=(device.levels&~(1u<<id))|(down<<id);if(was_full)device.overflow=true;else device.buttons.push_back(id|(down<<8));}
            if(++device.divider==o.machine.timer_divisor){device.divider=0;++device.timer;}
            if(presentation)++device.sequence;
            if(led_write){if(led_write->first==0x10004000)device.leds=led_write->second;else device.blink=led_write->second;}
            ++device.cycles;
        }
        advance(fall-1);
        if(!events.empty())emit(fall-1);
        if(commit){const auto actual=machine->snapshotPipelineState(state.pc).toKnownState();demand(actual.x==state.x&&actual.halted==state.halted&&actual.trapped==state.trapped&&actual.trap_cause==state.trap_cause,"Guest register/terminal state differs from ISA reference");}
        if(presentation){demand(out(machine,"FRAME_SEQUENCE")==device.sequence,"Presentation sequence mismatch");const auto capture_start=Clock::now();auto frame=machine->captureFrame(fall-1);
            for(size_t i=0;i<frame.pixels.size();++i)
                demand(frame.pixels[i]==device.video[i]&&frame.known[i]==device.known[i],
                    "Frame differs from ordered guest pixel writes at byte "+std::to_string(i)+
                    ": actual="+std::to_string(frame.pixels[i])+"/"+std::to_string(frame.known[i])+
                    " expected="+std::to_string(device.video[i])+"/"+std::to_string(device.known[i]));
            demand(result.presentations<o.max_frames,"Presentation budget exhausted");
            ++result.presentations;
            if(frame.pixels!=previous_pixels||frame.known!=previous_known)++result.changed_frames;
            previous_pixels=frame.pixels;previous_known=frame.known;
            const auto buttons=machine->part("BUTTONS"),fifo=child(buttons,"EVENTS");
            const auto levels=out(child(buttons,"LEVELS"),"Q"),count=out(fifo,"COUNT"),head=out(fifo,"DATA"),overflow=out(fifo,"OVERFLOW");
            demand(levels==device.levels&&count==device.buttons.size()&&head==(device.buttons.empty()?0:device.buttons.front())&&overflow==device.overflow,"Presented input state differs from device model");
            result.frame_inputs.push_back({levels,count,head,bool(overflow)});
            if(has_leds){const auto shown=shownLeds(device.leds,device.blink,device.cycles);
                demand(out(machine,"LEDS")==shown,"LED pins differ from the LED register model");result.frame_leds.push_back(shown);}
            result.frame_cycles.push_back(cycle+1);result.frame_instructions.push_back(state.instruction_count);
            result.frame_seconds.push_back(elapsed());
            if(o.presentation_observer)o.presentation_observer(machine,sim,fall-1);
            if(o.on_event){note(GuestRunEvent::Kind::Presented,cycle,uint32_t(device.sequence),has_leds?std::to_string(result.frame_leds.back()):std::string{});emit(fall-1);}
            if(batch_index<o.presentation_inputs.size()){
                demand(device.buttons.empty(),"Presentation-paced input queue not empty");
                const auto& batch=o.presentation_inputs[batch_index++];
                for(size_t i=0;i<batch.size();++i){
                    const auto when=checkedTimeAdd(boundary,checkedTimeMultiply(o.period,i+1));
                    scheduled_inputs.push_back({when,uint64_t(scheduled_inputs.size()+1),batch[i].button,batch[i].down});
                }
            }
            if(o.capture_frames){demand(result.frames.size()<o.max_frames,"Frame capture budget exhausted");
                demand(frame.pixels.size()<=circuit::device::MaxCapturedPixelBytes/(result.frames.size()+1),"Captured pixel byte budget exhausted");result.frames.push_back(std::move(frame));}
            result.capture_seconds+=std::chrono::duration<double>(Clock::now()-capture_start).count();
        }
        advance(end);result.cycles=cycle+1;
        if(has_checkers)demand(!out(machine,"PORT_VIOLATION"),"A bus link broke the port rules (see the CHECK_* parts)");
        if(o.progress&&result.cycles%4096==0)o.progress(result.cycles,state.instruction_count,result.presentations);
        demand(sim.getPerformanceCounters().processed_events<=o.max_events,"Simulation event budget exhausted");
        demand(state.instruction_count<=o.max_instructions,"Guest instruction budget exhausted");
        if(state.halted||state.trapped){demand(!pending&&completed.empty(),"Guest stopped with unfinished transactions");demand(state.halted&&!state.trapped,"Guest trapped");
            demand(device.console.empty(),"Guest stopped before console output drained");
            demand(event_index==scheduled_inputs.size()&&sampled_event_index==scheduled_inputs.size()&&batch_index==o.presentation_inputs.size(),"Guest stopped before scripted input was sampled");
            for(const auto&[address,value]:ram_writes)demand(machine->ramStorage().readBytes(address,1)[0]==value&&reference.readU8(address)==value,"Final RAM state mismatch");
            const auto report=machine->ramStorage().readLogic(0x3f000,36);result.report.resize(36);result.report_known.resize(36);
            for(size_t i=0;i<report.size();++i){if(report[i]==LogicValue::HIGH)result.report[i/8]|=1u<<(i%8);if(report[i]==LogicValue::HIGH||report[i]==LogicValue::LOW)result.report_known[i/8]|=1u<<(i%8);}
            result.state=state;result.instructions=state.instruction_count;result.events=sim.getPerformanceCounters();
            result.ram_allocated_bytes=machine->ramStorage().allocatedDataBytes();result.video_allocated_bytes=machine->videoStorage().allocatedDataBytes();
            result.setup_violations=BasicComponent::setupViolations();demand(!result.setup_violations,"Guest run observed setup violations");
            result.execution_seconds=elapsed();
            if(o.on_event){note(GuestRunEvent::Kind::Halted,cycle);emit(end);}
            return result;
        }
        if(o.stop_after_cycles&&result.cycles>=o.stop_after_cycles){
            // The state here is the top of the next cycle: a save made now
            // continues the run (a replay's next window).
            if(o.save_at)if(const auto content=o.save_at(result.cycles,result.presentations);content!=RecordedGuestOptions::SaveContent::None)save_run(result.cycles,content);
            result.stopped=true;result.state=state;result.instructions=state.instruction_count;result.events=sim.getPerformanceCounters();
            result.setup_violations=BasicComponent::setupViolations();demand(!result.setup_violations,"Guest run observed setup violations");
            result.execution_seconds=elapsed();
            return result;
        }
    }
    throw std::runtime_error("Guest cycle budget exhausted");
}
}
uint32_t savedRunVersion(const std::vector<uint8_t>& save){
    circuit::state::Reader in(save);in.tag(RunTag,"run");return in.u32();
}
uint64_t savedRunCycle(const std::vector<uint8_t>& save){
    circuit::state::Reader in(save);in.tag(RunTag,"run");const auto version=in.u32();
    if(version!=1&&version!=RunVersion)in.fail("an unsupported run save version");
    in.u64();return in.u64();
}
std::vector<uint8_t> savedMachine(const std::vector<uint8_t>& save){
    circuit::state::Reader in(save);in.tag(RunTag,"run");const auto version=in.u32();
    if(version!=1&&version!=RunVersion)in.fail("an unsupported run save version");
    in.u64();in.u64();
    if(version!=1)return in.bytes();
    // Inline: the machine section and the runner's part after it; readers of
    // the machine stop at its end.
    return {save.begin()+static_cast<std::ptrdiff_t>(in.offset()),save.end()};
}
std::string recordedGuestJson(const RecordedGuestResult& r){
    std::ostringstream s;s<<std::setprecision(10);s<<"{\"version\":1,\"cycles\":"<<r.cycles<<",\"instructions\":"<<r.instructions<<",\"setupViolations\":"<<r.setup_violations
      <<",\"setupSeconds\":"<<r.setup_seconds<<",\"executionSeconds\":"<<r.execution_seconds<<",\"captureSeconds\":"<<r.capture_seconds<<",\"ramAllocatedBytes\":"<<r.ram_allocated_bytes
      <<",\"videoAllocatedBytes\":"<<r.video_allocated_bytes<<",\"presentations\":"<<r.presentations<<",\"changedFrames\":"<<r.changed_frames<<",\"profileName\":"<<quoted(r.profile_name)<<",\"profileFingerprint\":"<<quoted(r.profile_fingerprint)<<",\"fidelityPreset\":"<<quoted(r.fidelity_profile)<<",\"buildManifest\":"<<quoted(r.build_manifest)<<",\"console\":"<<quoted(r.console)<<",\"report\":"<<quoted(hex(r.report))<<",\"reportKnown\":"<<quoted(hex(r.report_known))<<",\"frames\":[";
    const auto binary=[](uint32_t value,size_t width){std::string text(width,'0');for(size_t bit=0;bit<width;++bit)if((value>>bit)&1)text[width-bit-1]='1';return text;};
    for(size_t i=0;i<r.frames.size();++i){if(i)s<<',';const auto&f=r.frames[i];s<<"{\"time\":";tick(s,f.time);s<<",\"epoch\":0,\"cycle\":"<<r.frame_cycles.at(i)<<",\"instructions\":"<<r.frame_instructions.at(i)<<",\"hostSeconds\":"<<r.frame_seconds.at(i)<<",\"sequence\":"<<f.sequence<<",\"width\":"<<f.width<<",\"height\":"<<f.height<<",\"stride\":"<<f.stride<<",\"format\":\"rgb332\",\"pixels\":"<<quoted(hex(f.pixels))<<",\"known\":"<<quoted(hex(f.known));const auto& input=r.frame_inputs.at(i);
        s<<",\"input\":{\"supported\":true,\"kind\":\"keyboard\",\"levels\":"<<quoted(binary(input.levels,32))<<",\"count\":"<<quoted(binary(input.count,3))<<",\"head\":"<<quoted(binary(input.head,32))<<",\"overflow\":"<<quoted(input.overflow?"1":"0")<<"}";
        if(i<r.frame_leds.size())s<<",\"leds\":"<<quoted(binary(r.frame_leds[i],16));s<<"}";}
    s<<"],\"inputs\":[";for(size_t i=0;i<r.applied_inputs.size();++i){if(i)s<<',';const auto& input=r.applied_inputs[i];s<<"{\"time\":";tick(s,input.tick);s<<",\"sequence\":";tick(s,input.sequence);s<<",\"button\":"<<unsigned(input.button)<<",\"down\":"<<(input.down?"true":"false")<<'}';}
    s<<"],\"deviceReads\":[";for(size_t i=0;i<r.device_reads.size();++i){if(i)s<<',';const auto&d=r.device_reads[i];s<<"{\"time\":";tick(s,d.tick);s<<",\"address\":"<<d.address<<",\"value\":"<<d.value<<'}';}
    s<<"],\"configuration\":{\"system\":"<<quoted(r.system)<<",\"caches\":"<<(r.configuration.caches?"true":"false")<<",\"dram\":"<<(r.configuration.dram?"true":"false")
      <<",\"width\":"<<r.configuration.width<<",\"height\":"<<r.configuration.height<<",\"timerDivisor\":"<<r.configuration.timer_divisor
      <<",\"videoBytes\":"<<r.configuration.videoCapacityBytes()<<",\"consoleDepth\":"<<r.configuration.console_depth<<",\"buttonDepth\":"<<r.configuration.button_depth
      <<",\"timerFrequencyHz\":"<<r.configuration.timer_frequency_hz<<",\"quantumEvents\":"<<r.quantum_events<<",\"period\":";tick(s,r.period);s<<",\"startTime\":";tick(s,r.start_time);
    s<<"},\"events\":{\"processed\":"<<r.events.processed_events<<",\"scheduled\":"<<r.events.scheduled_events<<",\"maximumQueue\":"<<r.events.maximum_event_queue_depth
      <<",\"processedWireUpdates\":"<<r.events.processed_wire_updates<<",\"processedEvaluations\":"<<r.events.processed_component_evaluations
      <<",\"scheduledWireUpdates\":"<<r.events.scheduled_wire_updates<<",\"scheduledEvaluations\":"<<r.events.scheduled_component_evaluations
      <<",\"wireChanges\":"<<r.events.effective_wire_changes<<",\"pinChanges\":"<<r.events.effective_pin_changes<<"}}";return s.str();
}
}
