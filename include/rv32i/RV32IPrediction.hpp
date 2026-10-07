#pragma once

#include "components/selection/SelectionTypes.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace rv32i {
enum class PredictionPolicy { NotTaken, Taken, BackwardTaken, OneBit, TwoBit };
inline const char* predictionPolicyName(PredictionPolicy policy) {
    switch (policy) {
        case PredictionPolicy::NotTaken: return "not-taken";
        case PredictionPolicy::Taken: return "taken";
        case PredictionPolicy::BackwardTaken: return "backward-taken";
        case PredictionPolicy::OneBit: return "one-bit";
        case PredictionPolicy::TwoBit: return "two-bit";
    }
    throw std::invalid_argument("Unknown prediction policy");
}
struct PredictionConfig {
    PredictionPolicy policy = PredictionPolicy::NotTaken;
    size_t entries = 16;
    void validate() const {
        (void)predictionPolicyName(policy);
        if (!entries || entries > 256 || (entries & (entries - 1)))
            throw std::invalid_argument("Prediction table needs a power-of-two entry count in 1..256");
    }
    circuit::ParameterMap parameters() const {
        validate();
        return {{"prediction", predictionPolicyName(policy)}, {"prediction_entries", std::to_string(entries)}};
    }
};
inline PredictionConfig predictionConfig(const circuit::ParameterMap& parameters) {
    PredictionConfig config;
    for (const auto& [key, value] : parameters) {
        if (key == "prediction") {
            bool found = false;
            for (auto policy : {PredictionPolicy::NotTaken, PredictionPolicy::Taken,
                                PredictionPolicy::BackwardTaken, PredictionPolicy::OneBit, PredictionPolicy::TwoBit}) {
                if (value == predictionPolicyName(policy)) { config.policy = policy; found = true; break; }
            }
            if (!found) throw std::invalid_argument("Unknown prediction policy: " + value);
        } else if (key == "prediction_entries") {
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("Invalid prediction_entries");
            size_t used = 0;
            config.entries = std::stoull(value, &used);
            if (used != value.size()) throw std::invalid_argument("Invalid prediction_entries");
        } else throw std::invalid_argument("Unknown prediction parameter: " + key);
    }
    config.validate();
    return config;
}
inline bool supportsPredictionParameters(const circuit::ParameterMap& parameters) {
    try { (void)predictionConfig(parameters); return true; } catch (const std::exception&) { return false; }
}

struct BranchPredecode {
    bool conditional = false, direct_jump = false, backward = false;
    uint32_t target = 0, fallthrough = 0;
};
// Binary reference for the structural opcode/field/target network. Prediction
// does not establish legality: full Decode and Execute remain authoritative.
inline BranchPredecode predecodeBranch(uint32_t pc, uint32_t instruction) {
    const unsigned opcode = instruction & 127, funct3 = (instruction >> 12) & 7;
    const bool branch = opcode == 0x63 && funct3 != 2 && funct3 != 3;
    const bool jump = opcode == 0x6f;
    uint32_t immediate = 0;
    if (branch) {
        immediate = ((instruction >> 31) << 12) | (((instruction >> 7) & 1) << 11)
                  | (((instruction >> 25) & 63) << 5) | (((instruction >> 8) & 15) << 1);
        if (immediate & 4096) immediate |= 0xffffe000u;
    } else if (jump) {
        immediate = ((instruction >> 31) << 20) | (instruction & 0xff000)
                  | (((instruction >> 20) & 1) << 11) | (((instruction >> 21) & 1023) << 1);
        if (immediate & 0x100000) immediate |= 0xffe00000u;
    }
    return {branch, jump, bool(immediate & 0x80000000u), pc + immediate, pc + 4};
}
inline unsigned nextPredictionCounter(unsigned current, bool taken, bool one_bit) {
    current &= 3;
    return one_bit ? (taken ? 3 : 0) : taken ? (current == 3 ? 3 : current + 1)
                                                            : (current == 0 ? 0 : current - 1);
}
// Compact state used only by behavioral fidelities. Structural prediction has
// matching FF entries and counter-update gates, not a shadow copy of this model.
class PredictionState {
public:
    explicit PredictionState(PredictionConfig config = {}) : config_(checked(config)), counters_(config_.entries, 0) {}
    void reset() { std::fill(counters_.begin(), counters_.end(), 0); }
    unsigned counter(uint32_t pc) const { return counters_[(pc >> 2) & (config_.entries - 1)]; }
    bool direction(uint32_t pc, bool backward) const {
        switch (config_.policy) {
            case PredictionPolicy::NotTaken: return false;
            case PredictionPolicy::Taken: return true;
            case PredictionPolicy::BackwardTaken: return backward;
            case PredictionPolicy::OneBit: case PredictionPolicy::TwoBit: return counter(pc) >= 2;
        }
        return false;
    }
    uint32_t predict(uint32_t pc, uint32_t instruction) const {
        const auto d = predecodeBranch(pc, instruction);
        return d.direct_jump || (d.conditional && direction(pc, d.backward)) ? d.target : d.fallthrough;
    }
    void update(uint32_t pc, bool taken) {
        if (config_.policy != PredictionPolicy::OneBit && config_.policy != PredictionPolicy::TwoBit) return;
        auto& value = counters_[(pc >> 2) & (config_.entries - 1)];
        value = nextPredictionCounter(value, taken, config_.policy == PredictionPolicy::OneBit);
    }
private:
    static PredictionConfig checked(PredictionConfig config) { config.validate(); return config; }
    PredictionConfig config_;
    std::vector<unsigned> counters_;
};
}
