#include "tests/MemoryEndpointTests.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "benchmark/Benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

size_t countComponents(const std::shared_ptr<Component>& c) {
    size_t count=1;
    for(const auto& child:c->getChildren()) count+=countComponents(child);
    return count;
}
int main() {
    using namespace circuit;
    using namespace circuit::test;
    using Clock=std::chrono::steady_clock;
    // Same validated request stream and history settings for both fidelities.
    // These are host simulation timings, not estimates of chip performance.
    for(const auto& test : std::vector<std::shared_ptr<ComponentScenarioTest>>{
        std::make_shared<MemoryEndpointFFTest>(),std::make_shared<MemoryEndpointSRAMTest>(),
        std::make_shared<MemoryEndpointDRAMTest>(),std::make_shared<MemoryEndpointROMTest>(),
        std::make_shared<MemoryEndpointFlashTest>()}) {
        const auto& spec=test->getSpec();
        for(const auto fidelity:{Fidelity::Structural,Fidelity::Behavioral}) {
            std::vector<double> samples;
            size_t components=0,events=0,simulated_ticks=0;
            for(size_t i=0;i<4;++i) {
                const auto start=Clock::now();
                auto result=ComponentTestRunner().run(spec,spec.scenarios.front(),
                    withExactFidelity(canonicalDefaultProfile(),spec.instance_name,fidelity));
                const auto ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
                if(i) samples.push_back(ms);
                components=countComponents(result.root);
                events=result.simulator->getPerformanceCounters().processed_events;
                simulated_ticks=result.checkpoints.empty()?0:result.checkpoints.back().actual_time;
            }
            std::sort(samples.begin(),samples.end());
            benchmark::RunRecord record;
            record.workload={spec.id+"/"+spec.scenarios.front().id,"memory",
                "Validated memory endpoint request/response scenario",{"component-scenario"},{}};
            record.target={spec.contract_id+"/"+toString(fidelity),"memory",
                {{"contract",spec.contract_id},{"fidelity",toString(fidelity)}}};
            record.status=benchmark::RunStatus::Completed;record.validated=true;
            record.simulated_ticks=simulated_ticks;record.host_seconds=samples[1]/1000.0;
            benchmark::addMetric(record,"checkpoints",spec.scenarios.front().actions.size(),"checkpoints");
            benchmark::addMetric(record,"components",components,"components");
            benchmark::addMetric(record,"processed_events",events,"events");
            std::cout<<benchmark::toJson(record)<<'\n';
        }
    }
    for(size_t size:{4096u,1u<<20,1u<<28}) {
        MemoryStorage store(size,8,LogicValue::UNKNOWN,false,{true,16384,0});
        store.writeLogic(1,size-1,logicBits(8,42));
        benchmark::RunRecord record;
        record.workload={"memory-storage/sparse-capacity","memory",
            "Sparse backing-store capacity check",{"capacity"},{{"logical_bytes",std::to_string(size)}}};
        record.target={"memory-storage/sparse","memory",{}};
        record.status=benchmark::RunStatus::Completed;record.validated=true;
        benchmark::addMetric(record,"logical_bytes",size,"bytes");
        benchmark::addMetric(record,"resident_content_bytes",store.allocatedDataBytes(),"bytes");
        std::cout<<benchmark::toJson(record)<<'\n';
    }
}
