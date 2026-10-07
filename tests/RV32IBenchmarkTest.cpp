#include "analysis/RV32IBenchmarks.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include <algorithm>
#include <iostream>

using namespace circuit::analysis;
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
size_t rows=0;
void oracle(const std::string& name,size_t count,const RV32IBenchmarkOptions& options) {
    const auto image=buildRV32IBenchmark(name,count,options);
    const auto again=buildRV32IBenchmark(name,count,options);
    check(image.program.bytes()==again.program.bytes(),"Nondeterministic program");
    check(image.body_begin<image.body_end&&image.body_end<image.program.sizeBytes(),"Invalid ROI");
    rv32i::RV32IFunctionalMemory instructions(65536),data(65536);
    instructions.loadBytes(0,image.program.bytes());
    for(const auto& block:image.initial_data)data.loadBytes(block.address,block.bytes);
    rv32i::RV32IState state;size_t steps=0,visits=0;
    while(!state.halted&&!state.trapped&&steps++<2000000) {
        visits+=state.pc==image.body_begin;
        rv32i::RV32IInstructionOracle::step(state,instructions,data);
    }
    check(state.halted&&!state.trapped,"Benchmark failed to halt normally");
    check(visits==count+options.training_iterations,"Outer iteration/ROI marker count");
    check(state.x[10]==image.expected_result,"High-level result disagrees with instruction execution");
    for(const auto& block:image.expected_data)
        check(data.readBytes(block.address,block.bytes.size())==block.bytes,"High-level output image mismatch");
    if(name=="calls")check(state.x[2]==0x4000,"Stack not restored");
    if(name=="branch-alias")check(image.branch_pcs.at(1)-image.branch_pcs.at(0)==4*options.branch_spacing,"Wrong alias spacing");
    ++rows;
}
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"Expected invalid configuration rejection");}
}
int main() {
    try {
        for(const auto& name:rv32iBenchmarkNames())for(size_t size:{1,4,16,256}) {
            RV32IBenchmarkOptions o;o.size=size;
            oracle(name,size==256?1:3,o);
            o.seed=0xc001d00d;o.measure_body=true;o.training_iterations=2;
            if(size!=256)oracle(name,2,o);
        }
        for(size_t distance:{2,4,8}){RV32IBenchmarkOptions o;o.distance=distance;oracle("dependency",3,o);}
        for(auto pattern:{"taken","not-taken","bursts","irregular"}) {
            RV32IBenchmarkOptions o;o.pattern=pattern;oracle("branch-pattern",65,o);
        }
        for(size_t spacing:{4,8,32,256}){RV32IBenchmarkOptions o;o.branch_spacing=spacing;oracle("branch-alias",3,o);}
        RV32IBenchmarkOptions o;o.size=8;
        auto immediate=buildRV32IBenchmark("load-use",4,o),scheduled=buildRV32IBenchmark("load-scheduled",4,o);
        check(immediate.expected_result==scheduled.expected_result,"Paired schedules do different work");
        std::vector<uint32_t> a,b;
        for(size_t i=0;i<immediate.program.sizeBytes()/4;++i)a.push_back(immediate.program.wordAt(i));
        for(size_t i=0;i<scheduled.program.sizeBytes()/4;++i)b.push_back(scheduled.program.wordAt(i));
        std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());check(a==b,"Paired instruction multiset differs");
        rejects([&]{buildRV32IBenchmark("unknown",1,o);});
        rejects([&]{buildRV32IBenchmark("independent",0,o);});
        rejects([&]{auto p=o;p.size=257;buildRV32IBenchmark("sort",1,p);});
        rejects([&]{auto p=o;p.distance=3;buildRV32IBenchmark("dependency",1,p);});
        rejects([&]{auto p=o;p.seed=0;buildRV32IBenchmark("scan",1,p);});
        rejects([&]{auto p=o;p.training_iterations=1;buildRV32IBenchmark("scan",1,p);});
        rejects([&]{auto p=o;p.measure_body=true;p.training_iterations=1;buildRV32IBenchmark("scan",1000000,p);});
        std::cout<<rows<<" generated benchmark/oracle cases passed, plus parameter and paired-schedule checks\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
