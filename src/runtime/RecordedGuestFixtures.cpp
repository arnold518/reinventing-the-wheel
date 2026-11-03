#include "runtime/RecordedGuestFixtures.hpp"
#include <stdexcept>
#include "components/TimingProfile.hpp"
namespace circuit::runtime {
namespace {
struct Fixture {const char* name;const char* system;const char* elf_sha;const char* script_sha;size_t width,height,button_depth;const char* hex;
    std::vector<ButtonInput> input;std::vector<std::vector<ButtonTransition>> batches;};
#include "RecordedGuestFixtures.generated.inc"
std::vector<uint8_t> bytes(std::string_view hex){
    auto digit=[](char c)->uint8_t{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::logic_error("Invalid embedded ELF hex");};
    std::vector<uint8_t> result;result.reserve(hex.size()/2);for(size_t i=0;i<hex.size();i+=2)result.push_back((digit(hex[i])<<4)|digit(hex[i+1]));return result;
}
}
std::vector<std::string> recordedGuestNames(){std::vector<std::string> result;for(const auto& f:fixtures())result.push_back(f.name);return result;}
RecordedGuestFixture recordedGuestFixture(std::string_view name){
    for(const auto& f:fixtures())if(name==f.name){
        RecordedGuestOptions options;
        options.system=f.system;
        options.machine.width=f.width;options.machine.height=f.height;options.machine.button_depth=f.button_depth;
        options.presentation_inputs=f.batches;
        if(!f.batches.empty()){
            // A long session: one batch of button changes after each frame.
            options.max_cycles=20000000;options.max_instructions=10000000;options.max_frames=1024;
            options.max_events=20000000000ULL;options.max_device_reads=65536;
        }
        return {f.name,f.elf_sha,f.script_sha,bytes(f.hex),f.input,options};
    }
    throw std::invalid_argument("Unknown recorded guest: "+std::string(name));
}
static std::vector<ButtonInput> retimedInputs(const RecordedGuestFixture& f,SimulationTime period){
    // Same cycle, one tick after the falling edge of the new period.
    std::vector<ButtonInput> result=f.input;
    for(auto& e:result){
        const auto cycle=(e.tick-f.options.origin)/f.options.period;
        e.tick=3*period+cycle*period+period/2+1;
    }
    return result;
}
void retime(RecordedGuestFixture& f,SimulationTime period){
    f.input=retimedInputs(f,period);
    f.options.period=period;f.options.reset_release=2*period;f.options.origin=3*period;
}
std::string runRecordedGuestFixtureJson(const std::string& name,bool caches,bool dram,size_t max_cycles,std::function<void(uint64_t,uint64_t,uint64_t)> progress,size_t quantum_events,bool expanded_core,bool compact_children,size_t width,size_t height,std::string fidelity_profile,std::string system){
    if (!circuit::timing::unitTiming()) throw std::invalid_argument("Recorded fixtures use the unit timing profile");
    auto f=recordedGuestFixture(name);f.options.machine.caches=caches;f.options.machine.dram=dram;f.options.max_cycles=max_cycles;f.options.progress=std::move(progress);
    if(width)f.options.machine.width=width;
    if(height)f.options.machine.height=height;
    f.options.quantum_events=quantum_events;f.options.expanded_core=expanded_core;f.options.compact_children=compact_children;
    f.options.fidelity_profile=std::move(fidelity_profile);
    if(!system.empty())f.options.system=std::move(system);
    const auto result=runRecordedGuest(f.elf,f.input,f.options);auto json=recordedGuestJson(result);
    json.insert(1,"\"guest\":\""+f.name+"\",\"system\":\""+f.options.system+"\",\"elfSha256\":\""+f.elf_sha256+"\",\"scriptSha256\":\""+f.script_sha256+"\",");return json;
}
}
