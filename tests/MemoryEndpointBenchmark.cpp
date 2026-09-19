#include "tests/MemoryEndpointTests.hpp"
#include "modules/memory/MemoryStorage.hpp"
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
            size_t components=0,events=0;
            for(size_t i=0;i<4;++i) {
                const auto start=Clock::now();
                auto result=ComponentTestRunner().run(spec,spec.scenarios.front(),
                    withExactFidelity(canonicalDefaultProfile(),spec.instance_name,fidelity));
                const auto ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
                if(i) samples.push_back(ms);
                components=countComponents(result.root);
                events=result.simulator->getPerformanceCounters().processed_events;
            }
            std::sort(samples.begin(),samples.end());
            std::cout<<"{\"test\":\""<<spec.id<<"\",\"fidelity\":\""<<toString(fidelity)
                <<"\",\"checkpoints\":"<<spec.scenarios.front().actions.size()<<",\"components\":"<<components
                <<",\"events\":"<<events<<",\"medianBuildAndRunMs\":"<<samples[1]<<"}\n";
        }
    }
    for(size_t size:{4096u,1u<<20,1u<<28}) {
        MemoryStorage store(size,8,LogicValue::UNKNOWN,false,{true,16384,0});
        store.writeLogic(1,size-1,logicBits(8,42));
        std::cout<<"{\"logicalBytes\":"<<size<<",\"residentContentBytes\":"<<store.allocatedDataBytes()<<"}\n";
    }
}
