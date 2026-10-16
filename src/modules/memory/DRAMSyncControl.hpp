#pragma once
#include "modules/memory/DigitalMemory.hpp"
#include "components/TimingProfile.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace circuit::memory_detail {

// The gate-level DRAM command control (DRAMCommandControl in DRAM.cpp),
// cycle for cycle, for the compact DRAM device and the fused endpoint. State
// bits change at rising clock edges; the analog steps of a command start one
// flip-flop clock-to-output and one AND gate after the accepting edge, each
// step taking its interval plus the AND that gates it; the end of the chain
// reaches the one-stage sampler through a three-level OR tree. Completion
// updates the open row, READ_VALID and the refresh row and releases BUSY one
// edge later. The worst-case clock-to-output of the flip-flop is used, so no
// step is ever earlier than in the gates.
// Gates a DRAM step copies so that it outlasts the row decoder (ROW_ENABLE
// to a wordline, MatrixRowDecoder): the enable AND, the AND tree over the
// enable and the row bits, and one gate of margin for the control's own
// wordline and sense formulas.
inline size_t dramDecodeReplica(size_t rows) {
    size_t row_bits = 0; while ((size_t{1} << row_bits) < rows) ++row_bits;
    size_t tree = 0; while ((size_t{1} << tree) < row_bits + 1) ++tree;
    return 1 + tree + 1;
}

class DRAMSyncControl {
public:
    enum Command : unsigned { None = 0, Activate = 1, Read = 2, Write = 3, Precharge = 4, Refresh = 5 };
    using Bits = std::vector<LogicValue>;
    DRAMSyncControl(size_t width, size_t rows, size_t words_per_row)
        : width_(width), rows_(rows), words_per_row_(words_per_row), data(width, LogicValue::LOW),
          mask(width / 8, LogicValue::LOW) {
        const auto& m = timing::activeTimingProfile().memory;
        const size_t gate = timing::leafDelay(timing::Leaf::And);
        // The run signal is a three-input AND (command, busy, not repeat): two levels.
        launch_ = dflipflop_timing::clockToOutputMaximum() + 2 * gate;
        or_tree_ = 3 * timing::leafDelay(timing::Leaf::Or);
        const auto steps = [&](std::vector<size_t> intervals) {
            std::vector<size_t> at; size_t sum = 0;
            for (auto i : intervals) { sum += i + gate; at.push_back(sum); }
            return at;
        };
        // The sense and precharge steps carry the decoder replica (DRAM.cpp).
        const size_t replica = dramDecodeReplica(rows) * gate;
        const size_t sense = m.dramSense(rows) + timing::leafDelay(timing::Leaf::MemoryPrimitive) + replica;
        const size_t precharge = m.dram_precharge + replica;
        stages_[Activate] = steps({m.dram_wordline, sense, m.dram_restore});
        stages_[Read] = steps({m.dram_read});
        stages_[Write] = steps({m.dram_write_on, m.dram_write_off, m.dram_write_recovery});
        stages_[Precharge] = steps({m.dram_close, precharge, m.dram_equalize});
        stages_[Refresh] = steps({m.dram_wordline, sense, m.dram_close, precharge, m.dram_equalize});
    }

    // A rising edge at time `t` with the inputs as sampled there.
    void edge(size_t t, const Bits& cmd, const Bits& addr, const Bits& write_data, const Bits& write_mask) {
        using memory_device::known;
        using memory_device::number;
        // A repeated command's chain starts one edge after it was taken.
        if (delayed_start_) { started_ = t; delayed_start_ = false; }
        const bool cmd_known = known(cmd);
        const unsigned code = cmd_known ? number(cmd) : 0;
        const bool nop = cmd_known && code == 0;
        const bool addr_known = known(addr);
        const uint32_t a = addr_known ? number(addr) : 0;
        const bool legal = cmd_known && (
            (code == Activate && addr_known && !open && a < rows_) ||
            (code == Read && addr_known && open && a < words_per_row_) ||
            (code == Write && addr_known && known(write_mask) && open && a < words_per_row_) ||
            (code == Precharge && open) || (code == Refresh && !open));
        const bool issued = cmd_known && !nop;
        // Taken while idle or in the releasing cycle of the last command.
        const bool available = !busy || releasing;
        const bool accept = issued && legal && available;
        const bool unknown = !cmd_known;
        const bool error_event = unknown || (issued && (!available || !legal));
        const bool clear_valid = accept || (unknown && available) || (issued && available && !legal);
        const bool sampled = busy && command != None && t >= chainDone();
        const bool complete = busy && done_q_ && !releasing && !fresh_;
        const unsigned finished = command;
        error = error_event || (error && !accept);
        valid = !clear_valid && (valid || (complete && finished == Read));
        const bool next_busy = accept || (busy && !releasing);
        releasing = complete || (releasing && !releasing);
        if (complete && finished == Activate) open = true;
        if (complete && finished == Precharge) open = false;
        if (complete && finished == Refresh) refresh_row = refresh_row + 1 >= rows_ ? 0 : refresh_row + 1;
        const bool repeated = accept && busy && code == command;
        if (accept) {
            command = code;
            if (code == Activate) row = a;
            if (code == Refresh) row = refresh_row_before(complete && finished == Refresh);
            if (code == Read || code == Write) column = a;
            if (code == Write) { data = write_data; mask = write_mask; }
            started_ = t;
            if (repeated) { started_ = NOT_STARTED; delayed_start_ = true; }
        }
        busy = next_busy;
        done_q_ = sampled;
        fresh_ = accept;
    }
    // BUSY as the device shows it: low in the releasing cycle.
    bool busyShown() const { return busy && !releasing; }
    // Earliest future time at which an analog control may change.
    size_t nextStep(size_t t) const {
        if (!busy || command == None || started_ == NOT_STARTED) return 0;
        for (auto at : stages_.at(command)) if (started_ + launch_ + at > t) return started_ + launch_ + at;
        // The chain's end reaching the sampler (the gates' OR tree).
        if (const size_t end = chainDone(); end > t) return end;
        return 0;
    }
    // The analog controls at time `t` (the gates' formulas).
    bool wordline(size_t t) const {
        return (open && !(running(Precharge) && reached(0, t))) || (running(Activate) && reached(0, t)) ||
               (running(Refresh) && reached(0, t) && !reached(2, t));
    }
    bool sensing(size_t t) const {
        return (open && !(running(Precharge) && reached(0, t))) || (running(Activate) && reached(1, t)) ||
               (running(Refresh) && reached(1, t) && !reached(2, t));
    }
    bool precharging(size_t t) const {
        return (!open && !running(Activate) && !(running(Refresh) && !reached(3, t))) || (running(Precharge) && reached(1, t));
    }
    bool writing(size_t t) const { return running(Write) && reached(0, t) && !reached(1, t); }

    bool busy = false, error = false, valid = false, open = false, releasing = false;
    unsigned command = None;
    uint32_t row = 0, column = 0;
    size_t refresh_row = 0;
    Bits data, mask;

private:
    bool running(unsigned c) const { return busy && command == c; }
    bool reached(size_t stage, size_t t) const {
        const auto& at = stages_.at(command);
        return stage < at.size() && t >= started_ + launch_ + at[stage];
    }
    size_t chainDone() const { return started_ + launch_ + stages_.at(command).back() + or_tree_; }
    // A refresh accepted on the edge that completes the previous refresh
    // uses the counter as it was before that edge (registers update together).
    size_t refresh_row_before(bool advanced) const {
        if (!advanced) return refresh_row;
        return refresh_row == 0 ? rows_ - 1 : refresh_row - 1;
    }
    size_t width_, rows_, words_per_row_;
    size_t launch_ = 0, or_tree_ = 0, started_ = 0;
    static constexpr size_t NOT_STARTED = static_cast<size_t>(-1) / 2;
    bool done_q_ = false, fresh_ = false, delayed_start_ = false;
    std::map<unsigned, std::vector<size_t>> stages_;
};

} // namespace circuit::memory_detail
