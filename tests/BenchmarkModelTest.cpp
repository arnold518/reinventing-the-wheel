#include "benchmark/Benchmark.hpp"
#include "analysis/RV32IWorkloads.hpp"

#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void check(const bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F function){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,"Expected rejection");}
}

int main() {
    try {
        using namespace circuit;
        benchmark::RunRecord record;
        record.workload={"memory/read","memory","Read transaction",{"smoke"},{{"size","4"}}};
        record.target={"memory/ff","memory",{{"fidelity","structural"}}};
        record.status=benchmark::RunStatus::Completed;record.validated=true;
        record.simulated_ticks=12;record.host_seconds=0.25;
        benchmark::addMetric(record,"requests",1,"requests");
        record.fingerprints={{"input","abc"}};
        const auto json=benchmark::toJson(record);
        check(json.find("\"status\":\"completed\"")!=std::string::npos,"Missing status");
        check(json.find("\"simulatedTicks\":12")!=std::string::npos,"Missing ticks");
        benchmark::addMetric(record,"requests",2,"requests");
        rejects([&]{benchmark::validate(record);});

        const auto& catalog=analysis::rv32iWorkloadCatalog();
        check(catalog.size()==37,"RV32I catalog must include 22 fixed and 15 generated workloads");
        std::set<std::string> ids;
        for(const auto& workload:catalog)check(ids.insert(workload.id).second,"Duplicate workload id");
        auto fixed=analysis::prepareRV32IWorkload({});
        check(fixed.descriptor.id=="program-20"&&!fixed.blocks.empty(),"Fixed program preparation");
        analysis::RV32IWorkloadRequest generated_request;
        generated_request.workload="scan";generated_request.iterations=2;generated_request.benchmark.size=4;
        auto generated=analysis::prepareRV32IWorkload(generated_request);
        check(generated.expected_result.has_value()&&generated.descriptor.id=="scan","Generated workload preparation");
        std::cout<<"Common benchmark schema and 37-workload RV32I catalog passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
