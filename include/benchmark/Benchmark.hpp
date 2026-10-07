#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace circuit::benchmark {

enum class RunStatus { Completed, Failed, Incomplete, Unsupported };

struct WorkloadDescriptor {
    std::string id;
    std::string domain;
    std::string description;
    std::vector<std::string> tags;
    std::map<std::string, std::string> parameters;
};

struct TargetDescriptor {
    std::string id;
    std::string domain;
    std::map<std::string, std::string> parameters;
};

struct Metric {
    std::string name;
    double value = 0.0;
    std::string unit;
};

// Common envelope shared by processor, memory and future accelerator runners.
// Domain-specific counters remain named metrics instead of bloating this type.
struct RunRecord {
    unsigned schema = 1;
    WorkloadDescriptor workload;
    TargetDescriptor target;
    RunStatus status = RunStatus::Incomplete;
    bool validated = false;
    std::optional<uint64_t> simulated_ticks;
    double host_seconds = 0.0;
    std::vector<Metric> metrics;
    std::map<std::string, std::string> fingerprints;
};

std::string runStatusName(RunStatus status);
void validate(const WorkloadDescriptor& descriptor);
void validate(const TargetDescriptor& descriptor);
void validate(const RunRecord& record);
void addMetric(RunRecord& record, std::string name, double value, std::string unit);
std::string toJson(const WorkloadDescriptor& descriptor);
std::string toJson(const RunRecord& record);

} // namespace circuit::benchmark
