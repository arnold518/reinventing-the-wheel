#include "modules/memory/MemoryStorage.hpp"
#include "benchmark/Benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

// Host storage microbenchmark only: these milliseconds are not hardware latency.
int main() {
    using Clock = std::chrono::steady_clock;
    constexpr size_t writes = 16384;
    for (size_t capacity : {16384, 262144, 4194304}) {
        for (bool history : {false, true}) {
            std::vector<double> samples;
            size_t data_bytes = 0, history_bytes = 0;
            for (size_t trial = 0; trial < 6; ++trial) {
                MemoryStorage store(capacity / 4, 32, LogicValue::LOW, history);
                std::vector<LogicValue> value(32, LogicValue::HIGH);
                const auto start = Clock::now();
                for (size_t n = 0; n < writes; ++n) {
                    // Fixed working set separates capacity from access/history cost.
                    const auto address = static_cast<uint32_t>(((n * 997) % 4096) * 4);
                    value[n % 32] = (n & 1) ? LogicValue::HIGH : LogicValue::LOW;
                    store.writeLogic(n + 1, address, value, true);
                    if (store.readLogic(address, 4) != value) return 1;
                }
                const auto ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                if (trial) samples.push_back(ms);
                data_bytes = store.allocatedDataBytes();
                history_bytes = store.historicalByteCount();
            }
            std::sort(samples.begin(), samples.end());
            circuit::benchmark::RunRecord record;
            record.workload={"memory-storage/write-read","memory",
                "Fixed-working-set host storage write/read pairs",{"host-microbenchmark"},
                {{"write_read_pairs",std::to_string(writes)}}};
            record.target={"memory-storage","memory",{{"capacity_bytes",std::to_string(capacity)},
                {"history",history?"true":"false"}}};
            record.status=circuit::benchmark::RunStatus::Completed;record.validated=true;
            record.host_seconds=samples[2]/1000.0;
            circuit::benchmark::addMetric(record,"write_read_pairs",writes,"operations");
            circuit::benchmark::addMetric(record,"allocated_data_bytes",data_bytes,"bytes");
            circuit::benchmark::addMetric(record,"bytes_with_history",history_bytes,"bytes");
            std::cout<<circuit::benchmark::toJson(record)<<'\n';
        }
    }
}
