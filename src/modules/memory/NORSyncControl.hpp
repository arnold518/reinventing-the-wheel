#pragma once
#include "modules/memory/DigitalMemory.hpp"
#include "components/TimingProfile.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include <cstdint>
#include <map>
#include <vector>

namespace circuit::memory_detail {

// The gate-level NOR flash command control (FlashControl in NORFlash.cpp),
// cycle for cycle, for the compact flash device and the fused endpoint. Same
// timing rules as DRAMSyncControl: steps start one flip-flop clock-to-output
// (worst case) and one AND after the accepting edge; each step adds its
// interval and its gates; the chain end reaches the sampler through a
// two-level OR tree; BUSY is released one edge after completion.
class NORSyncControl {
public:
    enum Command : unsigned { None = 0, Read = 1, Program = 2, Erase = 3, WriteEnable = 4, WriteDisable = 5 };
    using Bits = std::vector<LogicValue>;
    NORSyncControl(size_t width, size_t depth, size_t rows, size_t program_ticks, size_t erase_ticks)
        : depth_(depth), data(width, LogicValue::LOW), mask(width / 8, LogicValue::LOW) {
        const auto& m = timing::activeTimingProfile().memory;
        const size_t gate = timing::leafDelay(timing::Leaf::And);
        // The run signal is a three-input AND (command, busy, not repeat): two levels.
        launch_ = dflipflop_timing::clockToOutputMaximum() + 2 * gate;
        or_tree_ = 2 * timing::leafDelay(timing::Leaf::Or);
        size_t row_bits = 0; while ((size_t{1} << row_bits) < rows) ++row_bits;
        size_t decode_depth = 1; while ((size_t{1} << (decode_depth - 1)) < row_bits + 1) ++decode_depth;
        stages_[Read] = {m.norRead(rows) + (decode_depth + 1) * gate};
        const auto pulse = [&](size_t first) {
            return std::vector<size_t>{first + gate, first + gate + m.nor_pulse_end + gate,
                                       first + gate + m.nor_pulse_end + gate + m.nor_recovery + gate};
        };
        stages_[Program] = pulse(program_ticks);
        stages_[Erase] = pulse(erase_ticks);
    }
    void edge(size_t t, const Bits& cmd, const Bits& addr, const Bits& write_data, const Bits& write_mask) {
        using memory_device::known;
        using memory_device::number;
        // A repeated command's chain starts one edge after it was taken.
        if (delayed_start_) { started_ = t; delayed_start_ = false; }
        const bool cmd_known = known(cmd);
        const unsigned code = cmd_known ? number(cmd) : 0;
        const bool addr_ok = known(addr) && number(addr) < depth_;
        const bool legal = cmd_known && ((code == Read && addr_ok) ||
            (code == Program && addr_ok && wel && known(write_mask)) || (code == Erase && addr_ok && wel) ||
            code == WriteEnable || code == WriteDisable);
        const bool issued = cmd_known && code != 0;
        const bool available = !busy || releasing;
        const bool accept = issued && legal && available;
        const bool unknown = !cmd_known;
        const bool clear_valid = accept || (unknown && available) || (issued && available && !legal);
        const bool start = accept && (code == Read || code == Program || code == Erase);
        const bool sampled = busy && command != None && t >= started_ + launch_ + stages_.at(command).back() + or_tree_;
        const bool complete = busy && done_q_ && !releasing && !fresh_;
        error = unknown || (issued && (!available || !legal)) || (error && !accept);
        valid = !clear_valid && (valid || (complete && command == Read));
        wel = (accept && code == WriteEnable) ||
              (wel && !(accept && (code == WriteDisable || code == Program || code == Erase)));
        const bool next_busy = start || (busy && !releasing);
        releasing = complete;
        const bool repeated = start && busy && code == command;
        if (start) {
            command = code; address = number(addr); started_ = repeated ? NOT_STARTED : t;
            delayed_start_ = repeated;
            if (code == Program) { data = write_data; mask = write_mask; }
        }
        busy = next_busy;
        done_q_ = sampled;
        fresh_ = accept;
    }
    bool busyShown() const { return busy && !releasing; }
    size_t nextStep(size_t t) const {
        if (!busy || command == None || started_ == NOT_STARTED) return 0;
        for (auto at : stages_.at(command)) if (started_ + launch_ + at > t) return started_ + launch_ + at;
        // The chain's end reaching the sampler (the gates' OR tree).
        if (const size_t end = started_ + launch_ + stages_.at(command).back() + or_tree_; end > t) return end;
        return 0;
    }
    bool programPulse(size_t t) const { return running(Program) && reached(0, t) && !reached(1, t); }
    bool erasePulse(size_t t) const { return running(Erase) && reached(0, t) && !reached(1, t); }

    bool busy = false, error = false, valid = false, wel = false, releasing = false;
    unsigned command = None;
    uint32_t address = 0;
    Bits data, mask;

private:
    bool running(unsigned c) const { return busy && command == c; }
    bool reached(size_t stage, size_t t) const {
        const auto& at = stages_.at(command);
        return stage < at.size() && t >= started_ + launch_ + at[stage];
    }
    size_t depth_, launch_ = 0, or_tree_ = 0, started_ = 0;
    static constexpr size_t NOT_STARTED = static_cast<size_t>(-1) / 2;
    bool done_q_ = false, fresh_ = false, delayed_start_ = false;
    std::map<unsigned, std::vector<size_t>> stages_;
};

} // namespace circuit::memory_detail
