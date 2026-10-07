#include "tests/SRAMTests.hpp"
#include "benchmark/Benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

size_t countSRAMComponents(const std::shared_ptr<Component>& c) {
    size_t count = 1;
    for (auto child : c->getChildren()) count += countSRAMComponents(child);
    return count;
}
int main() {
    using namespace circuit;
    using namespace circuit::test;
    const auto spec = sramDeviceSpec(32, 8, 2);
    std::vector<PinSnapshot> baseline;
    for (const std::string mode : {"structural", "bank", "behavioral"}) {
        auto profile = withExactFidelity(canonicalDefaultProfile(), "SRAM_ROOT", mode == "behavioral" ? Fidelity::Behavioral : Fidelity::Structural);
        if (mode == "bank") profile = withExactFidelity(profile, "SRAM_ROOT.STORAGE", Fidelity::Behavioral);
        std::vector<double> times;
        size_t count = 0, simulated_ticks = 0; uint64_t events = 0;
        for (unsigned trial = 0; trial < 3; ++trial) {
            const auto start = std::chrono::steady_clock::now();
            auto result = ComponentTestRunner().run(spec, spec.scenarios.front(), profile);
            times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
            count = countSRAMComponents(result.root); events = result.simulator->getPerformanceCounters().processed_events;
            simulated_ticks=result.checkpoints.empty()?0:result.checkpoints.back().actual_time;
            if (baseline.empty()) for (auto checkpoint : result.checkpoints) baseline.push_back(checkpoint.pins);
            else {
                if (baseline.size() != result.checkpoints.size()) return 1;
                for (size_t i = 0; i < baseline.size(); ++i) if (baseline[i] != result.checkpoints[i].pins) return 1;
            }
        }
        std::sort(times.begin(), times.end());
        benchmark::RunRecord record;
        record.workload={spec.id+"/"+spec.scenarios.front().id,"memory",
            "Validated SRAM device scenario",{"component-scenario"},{}};
        record.target={"sram/"+mode,"memory",{{"mode",mode},{"width","32"},
            {"depth_words","8"},{"words_per_row","2"}}};
        record.status=benchmark::RunStatus::Completed;record.validated=true;
        record.simulated_ticks=simulated_ticks;record.host_seconds=times[1]/1000.0;
        benchmark::addMetric(record,"components",count,"components");
        benchmark::addMetric(record,"processed_events",events,"events");
        benchmark::addMetric(record,"checkpoints",baseline.size(),"checkpoints");
        std::cout<<benchmark::toJson(record)<<'\n';
    }
}
