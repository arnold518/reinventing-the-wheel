#include "analysis/RV32IExperiment.hpp"
#include "benchmark/Benchmark.hpp"

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
    const std::filesystem::path& elf,const uint32_t tohost,const size_t maximum_cycles,
    const std::string& core) {
    circuit::analysis::RV32IExperimentConfig config;
    config.core=core;config.fidelity="behavioral";config.elf=elf.string();
    config.tohost=tohost;config.max_cycles=maximum_cycles;
    config.max_events=2'000'000'000ULL;config.period=4000;
    return circuit::analysis::runRV32IExperiment(config);
}

void usage(const char* executable) {
    std::cerr<<"Usage: "<<executable
        <<" <rv32i-elf> [tohost-address] [maximum-cycles] [core]\n"
        <<"Core: single-cycle, five-stage, or both\n";
}
}

int main(int argc,char** argv) {
    if(argc<2||argc>5){usage(argv[0]);return 2;}
    try {
        const std::filesystem::path elf(argv[1]);
        const auto tohost=argc>=3?parseUnsigned(argv[2],"tohost address"):uint64_t{0x3ffc0};
        const auto maximum=argc>=4?parseUnsigned(argv[3],"maximum cycle count"):uint64_t{10'000'000};
        if(tohost>UINT32_MAX||!maximum||maximum>SIZE_MAX)throw std::out_of_range("Benchmark limit exceeds host width");
        const std::string selection=argc>=5?argv[4]:"both";
        if(selection!="single-cycle"&&selection!="five-stage"&&selection!="both")
            throw std::invalid_argument("Invalid core: "+selection);
        if(selection=="single-cycle"||selection=="both")
            std::cout<<circuit::benchmark::toJson(run(elf,uint32_t(tohost),size_t(maximum),"single").benchmark_run)<<'\n';
        if(selection=="five-stage"||selection=="both")
            std::cout<<circuit::benchmark::toJson(run(elf,uint32_t(tohost),size_t(maximum),"advanced").benchmark_run)<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
