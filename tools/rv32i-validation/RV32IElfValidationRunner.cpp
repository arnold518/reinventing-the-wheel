#include "analysis/RV32IExperiment.hpp"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
uint64_t parseUnsigned(const std::string& text,const std::string& label) {
    const bool hex=text.size()>2&&text[0]=='0'&&(text[1]=='x'||text[1]=='X');
    uint64_t value=0;
    const auto result=std::from_chars(text.data()+(hex?2:0),text.data()+text.size(),value,hex?16:10);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())
        throw std::invalid_argument("Invalid "+label+": "+text);
    return value;
}

circuit::analysis::RV32IExperimentResult run(
    const std::filesystem::path& elf,const uint32_t tohost,const size_t maximum,
    const std::string& fidelity) {
    circuit::analysis::RV32IExperimentConfig config;
    config.core="single";config.fidelity=fidelity;config.elf=elf.string();
    config.tohost=tohost;config.max_cycles=maximum;config.max_events=500'000'000;
    config.period=4000;
    return circuit::analysis::runRV32IExperiment(config);
}
}

int main(int argc,char** argv) {
    if(argc<2||argc>5) {
        std::cerr<<"Usage: "<<argv[0]<<" <rv32i-elf> [tohost] [maximum-instructions] [behavioral|structural|both]\n";
        return 2;
    }
    try {
        const std::filesystem::path elf(argv[1]);
        const auto tohost=argc>=3?parseUnsigned(argv[2],"tohost address"):uint64_t{0x3ffc0};
        const auto maximum=argc>=4?parseUnsigned(argv[3],"maximum instruction count"):uint64_t{100000};
        const std::string fidelity=argc>=5?argv[4]:"both";
        if(tohost>UINT32_MAX||!maximum||maximum>SIZE_MAX)throw std::out_of_range("Validation limit exceeds host width");
        circuit::analysis::RV32IExperimentResult result;
        if(fidelity=="behavioral")result=run(elf,uint32_t(tohost),size_t(maximum),"behavioral");
        else if(fidelity=="structural")result=run(elf,uint32_t(tohost),size_t(maximum),"reference");
        else if(fidelity=="both") {
            const auto behavioral=run(elf,uint32_t(tohost),size_t(maximum),"behavioral");
            result=run(elf,uint32_t(tohost),size_t(maximum),"reference");
            if(behavioral.state_fingerprint!=result.state_fingerprint
                ||behavioral.workload_fingerprint!=result.workload_fingerprint
                ||behavioral.attempts!=result.attempts)
                throw std::runtime_error("Behavioral/structural ELF mismatch");
        } else throw std::invalid_argument("Invalid fidelity: "+fidelity);
        std::cout<<"PASS "<<elf.string()<<" fidelity="<<fidelity
            <<" instructions="<<result.retired<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
