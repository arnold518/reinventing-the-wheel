#pragma once

#include "simulator/SimulationTime.hpp"
#include "basic/LogicValue.hpp"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

struct MemoryActivity {
    std::string phase;
    bool maintenance = false;
    std::map<std::string, std::vector<LogicValue>> signals;
    bool operator==(const MemoryActivity&) const = default;
};

// Optional debug history of the existing native sequencer, never control state.
// No scheduling, pins, or register cells are introduced by this capability.
class MemoryActivityView {
public:
    virtual ~MemoryActivityView() = default;
    void appendMemoryActivityTimes(std::vector<SimulationTime>& times) const {
        for (const auto& [time, activity] : history_) {
            (void)activity;
            times.push_back(time);
        }
    }
    MemoryActivity memoryActivityAtTime(SimulationTime time) const {
        auto it = std::upper_bound(history_.begin(), history_.end(), time,
            [](SimulationTime t, const auto& entry) { return t < entry.first; });
        return it == history_.begin() ? MemoryActivity{} : std::prev(it)->second;
    }
protected:
    void recordMemoryActivity(SimulationTime time, MemoryActivity activity) {
        if (!history_.empty() && history_.back().second == activity) return;
        if (!history_.empty() && history_.back().first == time) history_.back().second = std::move(activity);
        else history_.emplace_back(time, std::move(activity));
    }
private:
    std::vector<std::pair<SimulationTime, MemoryActivity>> history_;
};
