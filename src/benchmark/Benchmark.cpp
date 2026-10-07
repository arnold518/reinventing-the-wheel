#include "benchmark/Benchmark.hpp"

#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace circuit::benchmark {
namespace {
std::string quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4)
                             << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"';
    return out.str();
}

void stringMap(std::ostringstream& out, const std::map<std::string, std::string>& values) {
    out << '{';
    bool first = true;
    for (const auto& [key, value] : values) {
        if (!first) out << ',';
        first = false;
        out << quote(key) << ':' << quote(value);
    }
    out << '}';
}

void workloadJson(std::ostringstream& out, const WorkloadDescriptor& descriptor) {
    out << "{\"id\":" << quote(descriptor.id)
        << ",\"domain\":" << quote(descriptor.domain)
        << ",\"description\":" << quote(descriptor.description)
        << ",\"tags\":[";
    for (size_t index = 0; index < descriptor.tags.size(); ++index) {
        if (index) out << ',';
        out << quote(descriptor.tags[index]);
    }
    out << "],\"parameters\":";
    stringMap(out, descriptor.parameters);
    out << '}';
}
}

std::string runStatusName(const RunStatus status) {
    switch (status) {
        case RunStatus::Completed: return "completed";
        case RunStatus::Failed: return "failed";
        case RunStatus::Incomplete: return "incomplete";
        case RunStatus::Unsupported: return "unsupported";
    }
    throw std::invalid_argument("Unknown benchmark run status");
}

void validate(const WorkloadDescriptor& descriptor) {
    if (descriptor.id.empty() || descriptor.domain.empty())
        throw std::invalid_argument("Benchmark workload requires an id and domain");
}

void validate(const TargetDescriptor& descriptor) {
    if (descriptor.id.empty() || descriptor.domain.empty())
        throw std::invalid_argument("Benchmark target requires an id and domain");
}

void validate(const RunRecord& record) {
    if (record.schema != 1) throw std::invalid_argument("Unsupported benchmark result schema");
    validate(record.workload);
    validate(record.target);
    if (!std::isfinite(record.host_seconds) || record.host_seconds < 0)
        throw std::invalid_argument("Benchmark host time must be finite and nonnegative");
    if (record.validated && record.status != RunStatus::Completed)
        throw std::invalid_argument("Only a completed benchmark can be validated");
    std::set<std::string> names;
    for (const auto& metric : record.metrics) {
        if (metric.name.empty() || metric.unit.empty() || !std::isfinite(metric.value))
            throw std::invalid_argument("Benchmark metric requires a name, unit and finite value");
        if (!names.insert(metric.name).second)
            throw std::invalid_argument("Duplicate benchmark metric: " + metric.name);
    }
}

void addMetric(RunRecord& record, std::string name, const double value, std::string unit) {
    record.metrics.push_back({std::move(name), value, std::move(unit)});
}

std::string toJson(const WorkloadDescriptor& descriptor) {
    validate(descriptor);
    std::ostringstream out;
    workloadJson(out, descriptor);
    return out.str();
}

std::string toJson(const RunRecord& record) {
    validate(record);
    std::ostringstream out;
    out << std::setprecision(12) << "{\"schema\":" << record.schema << ",\"workload\":";
    workloadJson(out, record.workload);
    out << ",\"target\":{\"id\":" << quote(record.target.id)
        << ",\"domain\":" << quote(record.target.domain) << ",\"parameters\":";
    stringMap(out, record.target.parameters);
    out << "},\"status\":" << quote(runStatusName(record.status))
        << ",\"validated\":" << (record.validated ? "true" : "false")
        << ",\"simulatedTicks\":";
    if (record.simulated_ticks) out << *record.simulated_ticks;
    else out << "null";
    out << ",\"hostSeconds\":" << record.host_seconds << ",\"metrics\":[";
    for (size_t index = 0; index < record.metrics.size(); ++index) {
        if (index) out << ',';
        const auto& metric = record.metrics[index];
        out << "{\"name\":" << quote(metric.name) << ",\"value\":" << metric.value
            << ",\"unit\":" << quote(metric.unit) << '}';
    }
    out << "],\"fingerprints\":";
    stringMap(out, record.fingerprints);
    out << '}';
    return out.str();
}

} // namespace circuit::benchmark
