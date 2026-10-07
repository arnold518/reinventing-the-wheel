#include "analysis/RV32IWorkloads.hpp"

#include "rv32i/RV32IElfImage.hpp"
#include "tests/RV32IProgramCases.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace circuit::analysis {
namespace {
void require(const bool value, const std::string& message) {
    if (!value) throw std::invalid_argument(message);
}

rv32i::RV32IProgram legacyProgram(const std::string& name, const size_t count) {
    const auto addi=[](unsigned rd,unsigned rs,int imm){return (uint32_t(imm)&4095)<<20|rs<<15|rd<<7|0x13U;};
    const auto bne=[](unsigned rs,int offset) {
        const uint32_t v=uint32_t(offset)&8191;
        return (v>>12)<<31|((v>>5)&63)<<25|rs<<15|1U<<12|((v>>1)&15)<<8|((v>>11)&1)<<7|0x63U;
    };
    std::vector<uint32_t> words{uint32_t((count+2048)&0xfffff000U)|1U<<7|0x37U,
        addi(1,1,int(count&4095)),addi(2,0,0)};
    const size_t outer=words.size();
    if(name=="dependent") {
        words.push_back(addi(2,2,1));words.push_back((2U<<20)|(2U<<15)|(3U<<7)|0x33U);
        words.push_back((3U<<20)|(2U<<15)|(2U<<7)|0x33U);
    } else if(name=="alternating") {
        words.push_back((1U<<20)|(2U<<15)|(4U<<12)|(2U<<7)|0x13U);
        words.push_back(bne(2,8));words.push_back(addi(3,3,1));
    } else {
        words.push_back(addi(2,0,8));const size_t inner=words.size();
        words.push_back(addi(3,3,1));words.push_back(addi(2,2,-1));
        words.push_back(bne(2,int(inner*4)-int(words.size()*4)));
    }
    words.push_back(addi(1,1,-1));words.push_back(bne(1,int(outer*4)-int(words.size()*4)));
    words.push_back(0x00100073);
    return rv32i::RV32IProgram::fromWords(words);
}

std::string number(const size_t value) { return std::to_string(value); }

void fingerprintBlock(uint64_t& hash, const RV32IWorkloadBlock& block) {
    hash^=block.instruction;hash*=1099511628211ULL;
    for(unsigned byte=0;byte<4;++byte){hash^=(block.address>>(8*byte))&255;hash*=1099511628211ULL;}
    for(const auto byte:block.bytes){hash^=byte;hash*=1099511628211ULL;}
}

benchmark::WorkloadDescriptor generatedDescriptor(
    const std::string& name, const std::string& description, const std::string& kind) {
    return {name,"rv32i",description,{"generated",kind},{}};
}
}

const std::vector<std::string>& rv32iGeneratedWorkloadNames() {
    static const std::vector<std::string> names=[] {
        std::vector<std::string> result{"dependent","alternating","nested"};
        result.insert(result.end(),rv32iBenchmarkNames().begin(),rv32iBenchmarkNames().end());
        return result;
    }();
    return names;
}

bool isRV32IGeneratedWorkload(const std::string& name) {
    const auto& names=rv32iGeneratedWorkloadNames();
    return std::find(names.begin(),names.end(),name)!=names.end();
}

const std::vector<benchmark::WorkloadDescriptor>& rv32iWorkloadCatalog() {
    static const std::vector<benchmark::WorkloadDescriptor> catalog=[] {
        std::vector<benchmark::WorkloadDescriptor> result;
        for(size_t program=1;program<=RV32IProgramCaseCount;++program) {
            const auto id="program-"+std::string(program<10?"0":"")+std::to_string(program);
            result.push_back({id,"rv32i",rv32iProgramCase(program).name,
                {"fixed",rv32iProgramCaseIsPerformance(program)?"performance":"correctness"},
                {{"program",std::to_string(program)}}});
        }
        result.push_back(generatedDescriptor("dependent","Dependent integer loop","legacy"));
        result.push_back(generatedDescriptor("alternating","Alternating branch loop","legacy"));
        result.push_back(generatedDescriptor("nested","Nested loop","legacy"));
        for(const auto& name:rv32iBenchmarkNames())
            result.push_back(generatedDescriptor(name,"Parameterized "+name+" workload","parameterized"));
        return result;
    }();
    return catalog;
}

RV32IPreparedWorkload prepareRV32IWorkload(const RV32IWorkloadRequest& request) {
    require(request.capacity_bytes>0,"Workload memory capacity must be positive");
    require(request.workload.empty()||request.elf.empty(),"Choose workload or ELF, not both");
    require(request.iterations>0&&request.iterations<=1000000,"Iterations must be 1..1000000");
    RV32IPreparedWorkload prepared;
    prepared.tohost=request.tohost;
    prepared.measurement_start_pc=request.start_pc;
    prepared.measurement_stop_pc=request.stop_pc;
    prepared.training_iterations=request.benchmark.training_iterations;
    if(isRV32IBenchmark(request.workload)) {
        const auto image=buildRV32IBenchmark(request.workload,request.iterations,request.benchmark);
        prepared.descriptor=generatedDescriptor(request.workload,image.description,"parameterized");
        prepared.descriptor.parameters={{"iterations",number(request.iterations)},
            {"size",number(request.benchmark.size)},{"seed",number(request.benchmark.seed)},
            {"distance",number(request.benchmark.distance)},{"pattern",request.benchmark.pattern},
            {"branch_spacing",number(request.benchmark.branch_spacing)},
            {"training_iterations",number(request.benchmark.training_iterations)}};
        prepared.blocks.push_back({true,0,image.program.bytes()});
        for(const auto& data:image.initial_data)prepared.blocks.push_back({false,data.address,data.bytes});
        prepared.expected_result=image.expected_result;
        prepared.expected_data=image.expected_data;
        prepared.branch_pcs=image.branch_pcs;
        if(request.benchmark.measure_body) {
            prepared.measurement_start_pc=image.body_begin;
            prepared.measurement_stop_pc=image.body_end;
        }
    } else if(!request.workload.empty()) {
        require(isRV32IGeneratedWorkload(request.workload),"Unknown generated RV32I workload");
        prepared.descriptor=generatedDescriptor(request.workload,
            request.workload+" generated loop","legacy");
        prepared.descriptor.parameters={{"iterations",number(request.iterations)}};
        prepared.blocks.push_back({true,0,legacyProgram(request.workload,request.iterations).bytes()});
    } else if(request.elf.empty()) {
        require(request.program>=1&&request.program<=RV32IProgramCaseCount,
            "Program number outside shared corpus");
        const auto program=rv32iProgramCase(request.program);
        require(program.initial_pc==0,"Experiment requires reset PC zero");
        for(const auto value:program.initial_registers)
            require(value==0,"Experiment requires initial registers zero");
        prepared.descriptor=rv32iWorkloadCatalog().at(request.program-1);
        prepared.blocks.push_back({true,program.program_base,program.program.bytes()});
        for(const auto& data:program.initial_data)prepared.blocks.push_back({false,data.address,data.bytes});
    } else {
        const auto image=rv32i::RV32IElfImage::fromFile(request.elf);
        image.requireFitsMemory(request.capacity_bytes);
        require(image.entryPoint()==0,"ELF reset entry must be zero");
        require(request.tohost<=request.capacity_bytes-4,"tohost outside memory");
        const auto filename=std::filesystem::path(request.elf).filename().string();
        prepared.descriptor={"elf/"+filename,"rv32i","Bare-metal RV32I ELF",
            {"compiled","external"},{{"file",filename}}};
        for(const auto& segment:image.segments()) {
            prepared.blocks.push_back({true,segment.address,segment.bytes});
            prepared.blocks.push_back({false,segment.address,segment.bytes});
        }
        prepared.completion=RV32IWorkloadCompletion::TohostVerdict;
    }
    uint64_t hash=14695981039346656037ULL;
    for(const auto& block:prepared.blocks)fingerprintBlock(hash,block);
    std::ostringstream digest;digest<<std::hex<<hash;prepared.fingerprint=digest.str();
    benchmark::validate(prepared.descriptor);
    return prepared;
}

} // namespace circuit::analysis
