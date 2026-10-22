#include "runtime/RecordedGuestFixtures.hpp"
#include <stdexcept>
#include "components/TimingProfile.hpp"
namespace circuit::runtime {
namespace {
struct Fixture {const char* name;const char* elf_sha;const char* script_sha;const char* hex;unsigned auto_ticks;std::vector<ButtonInput> input;};
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
        if(f.auto_ticks){options.max_instructions=1000000;options.max_events=2000000000;}
        return {f.name,f.elf_sha,f.script_sha,bytes(f.hex),f.input,options};
    }
    throw std::invalid_argument("Unknown recorded guest: "+std::string(name));
}
std::string runRecordedGuestFixtureJson(const std::string& name,bool caches,bool dram,size_t max_cycles,std::function<void(uint64_t,uint64_t,uint64_t)> progress,size_t quantum_events,bool expanded_core,bool compact_children,size_t width,size_t height){
    if (!circuit::timing::unitTiming()) throw std::invalid_argument("Recorded fixtures use the unit timing profile");
    auto f=recordedGuestFixture(name);f.options.machine.caches=caches;f.options.machine.dram=dram;f.options.max_cycles=max_cycles;f.options.progress=std::move(progress);
    f.options.machine.width=width;f.options.machine.height=height;
    f.options.quantum_events=quantum_events;f.options.expanded_core=expanded_core;f.options.compact_children=compact_children;
    const auto result=runRecordedGuest(f.elf,f.input,f.options);auto json=recordedGuestJson(result);
    json.insert(1,"\"guest\":\""+f.name+"\",\"elfSha256\":\""+f.elf_sha256+"\",\"scriptSha256\":\""+f.script_sha256+"\",");return json;
}
}
