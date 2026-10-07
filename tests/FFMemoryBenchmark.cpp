#include "tests/FFMemoryTests.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "benchmark/Benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

size_t componentCount(const std::shared_ptr<Component>& root) {
    size_t count = 1;
    for (const auto& child : root->getChildren()) count += componentCount(child);
    return count;
}

// Wall time measures simulator work, not physical FF memory latency.
int main() {
    using Clock = std::chrono::steady_clock;
    using namespace circuit;
    using namespace circuit::test;
    ComponentTestRunner runner;
    const auto spec = ffMemorySpec(32, 16);
    std::vector<PinSnapshot> baseline;
    for (const std::string mode : {"structural", "cells", "words", "bank", "behavioral"}) {
        const auto profile = ffMemoryProfile(mode, 32, spec.instance_name);
        std::vector<double> buildTimes, runTimes;
        size_t count = 0, simulated_ticks = 0;
        uint64_t events = 0;
        for (size_t trial = 0; trial < 4; ++trial) {
            {
                const auto start = Clock::now();
                const auto built = builtinComponentCatalog().createRoot(
                    {spec.contract_id, spec.instance_name, spec.parameters, {}, {}, {}}, profile);
                const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                count = componentCount(built.root);
                if (trial) buildTimes.push_back(ms);
            }
            const auto start = Clock::now();
            const auto result = runner.run(spec, spec.scenarios.front(), profile);
            const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (trial) runTimes.push_back(ms);
            events = result.simulator->getPerformanceCounters().processed_events;
            simulated_ticks=result.checkpoints.empty()?0:result.checkpoints.back().actual_time;
            if (baseline.empty()) {
                for (const auto& checkpoint : result.checkpoints) baseline.push_back(checkpoint.pins);
            } else {
                if (baseline.size() != result.checkpoints.size()) return 1;
                for (size_t i = 0; i < baseline.size(); ++i)
                    if (baseline[i] != result.checkpoints[i].pins) return 1;
            }
        }
        std::sort(buildTimes.begin(), buildTimes.end());
        std::sort(runTimes.begin(), runTimes.end());
        benchmark::RunRecord record;
        record.workload={spec.id+"/"+spec.scenarios.front().id,"memory",
            "Validated FF memory scenario",{"component-scenario"},{}};
        record.target={"ff-memory/"+mode,"memory",{{"mode",mode},{"width","32"},{"depth_words","16"}}};
        record.status=benchmark::RunStatus::Completed;record.validated=true;
        record.simulated_ticks=simulated_ticks;record.host_seconds=runTimes[1]/1000.0;
        benchmark::addMetric(record,"components",count,"components");
        benchmark::addMetric(record,"processed_events",events,"events");
        benchmark::addMetric(record,"checkpoints",baseline.size(),"checkpoints");
        benchmark::addMetric(record,"median_build_seconds",buildTimes[1]/1000.0,"seconds");
        std::cout<<benchmark::toJson(record)<<'\n';
    }
}
