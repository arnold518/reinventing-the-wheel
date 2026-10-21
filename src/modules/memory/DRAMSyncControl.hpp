#pragma once

#include "simulator/SimulationTime.hpp"
#include <array>
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
// reaches the one-stage sampler through a three-level OR tree. A completion
// is shown at once (BUSY low, READ_VALID, the new open state) and takes
// effect at the next edge, which may already take the next command. The
// worst-case clock-to-output of the flip-flop is used, so no step is ever
// earlier than in the gates.
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
    // ReadBurst (code 6) runs as Read; its completion is followed by the next
    // columns of the block, one per edge.
    enum Command : unsigned { None = 0, Activate = 1, Read = 2, Write = 3, Precharge = 4, Refresh = 5, ReadBurst = 6 };
    using Bits = std::vector<LogicValue>;
    DRAMSyncControl(size_t width, size_t rows, size_t words_per_row, size_t burst_words = 2)
        : width_(width), rows_(rows), words_per_row_(words_per_row), burst_words_(burst_words), data(width, LogicValue::LOW),
          mask(width / 8, LogicValue::LOW) {
        const auto& m = timing::activeTimingProfile().memory;
        const SimulationTime gate = timing::leafDelay(timing::Leaf::And);
        // The run signal is a three-input AND (command, busy, not repeat): two levels.
        launch_ = checkedTimeAdd(dflipflop_timing::clockToOutputMaximum(), checkedTimeMultiply(gate, 2));
        or_tree_ = checkedTimeMultiply(timing::leafDelay(timing::Leaf::Or), 3);
        const auto steps = [&](std::vector<SimulationTime> intervals) {
            std::vector<SimulationTime> at; SimulationTime sum = 0;
            for (auto i : intervals) { sum = checkedTimeAdd(sum, checkedTimeAdd(i, gate)); at.push_back(sum); }
            return at;
        };
        // The sense and precharge steps carry the decoder replica (DRAM.cpp).
        const SimulationTime replica = checkedTimeMultiply(gate, dramDecodeReplica(rows));
        const SimulationTime sense = checkedTimeAdd(checkedTimeAdd(m.dramSense(rows), timing::leafDelay(timing::Leaf::MemoryPrimitive)), replica);
        const SimulationTime precharge = checkedTimeAdd(m.dram_precharge, replica);
        stages_[Activate] = steps({m.dram_wordline, sense, m.dram_restore});
        stages_[Read] = steps({m.dram_read});
        stages_[Write] = steps({m.dram_write_on, m.dram_write_off, m.dram_write_recovery});
        stages_[Precharge] = steps({m.dram_close, precharge, m.dram_equalize});
        std::vector<SimulationTime> refresh{m.dram_wordline, sense};
        if (checkedTimeAdd(m.dram_refresh_restore, m.dram_refresh_extra)) refresh.push_back(checkedTimeAdd(m.dram_refresh_restore, m.dram_refresh_extra));
        for (auto step : {m.dram_close, precharge, m.dram_equalize}) refresh.push_back(step);
        refresh_close_ = refresh.size() - 3;
        stages_[Refresh] = steps(refresh);
    }

    // A rising edge at time `t` with the inputs as sampled there.
    void edge(SimulationTime t, const Bits& cmd, const Bits& addr, const Bits& write_data, const Bits& write_mask) {
        using memory_device::known;
        using memory_device::number;
        // A repeated command's chain starts one edge after it was taken.
        if (delayed_start_) { started_ = t; delayed_start_ = false; }
        const bool cmd_known = known(cmd);
        const unsigned raw = cmd_known ? number(cmd) : 0;
        // (A burst longer than a row cannot be taken: an unknown code.)
        const bool burst_command = raw == ReadBurst && burst_words_ <= words_per_row_;
        const unsigned code = burst_command ? unsigned(Read) : raw == ReadBurst ? 7u : raw;
        const bool nop = cmd_known && code == 0;
        const bool addr_known = known(addr);
        const uint32_t a = addr_known ? number(addr) : 0;
        const bool complete = completing();
        const bool more = moreColumns(), final_complete = complete && !more;
        const bool open_now = openShown();
        const bool legal = cmd_known && (
            (code == Activate && addr_known && !open_now && a < rows_) ||
            (code == Read && addr_known && open_now && a < words_per_row_) ||
            (code == Write && addr_known && known(write_mask) && open_now && a < words_per_row_) ||
            (code == Precharge && open_now) || (code == Refresh && !open_now));
        const bool issued = cmd_known && !nop;
        // Taken while idle, in the completing cycle of the last command, or in
        // a releasing cycle (a completion no command followed).
        const bool available = !busy || releasing || final_complete;
        const bool accept = issued && legal && available;
        const bool unknown = !cmd_known;
        const bool error_event = unknown || (issued && (!available || !legal));
        const bool clear_valid = accept || (unknown && available) || (issued && available && !legal);
        const bool sampled = busy && command != None && started_ != NOT_STARTED && t >= chainDone();
        const unsigned finished = command;
        error = error_event || (error && !accept);
        valid = !clear_valid && (valid || (complete && finished == Read));
        const bool next_busy = accept || (busy && !releasing);
        releasing = final_complete && !accept;
        // The next column of the block during a burst.
        if (complete && more) {
            const uint32_t mask = uint32_t(burst_words_ - 1);
            column = (column & ~mask) | ((column + 1) & mask);
            --burst_left_;
        }
        if (complete && finished == Activate) open = true;
        if (complete && finished == Precharge) open = false;
        if (complete && finished == Refresh) refresh_row = refresh_row + 1 >= rows_ ? 0 : refresh_row + 1;
        const bool repeated = accept && busy && code == command;
        if (accept) {
            if (code < accepted.size()) ++accepted[code];
            command = code;
            if (code == Activate) row = a;
            if (code == Refresh) row = refresh_row_before(complete && finished == Refresh);
            if (code == Read || code == Write) column = a;
            burst_ = burst_command;
            burst_left_ = burst_command && burst_words_ > 1 ? burst_words_ - 1 : 0;
            if (code == Write) { data = write_data; mask = write_mask; }
            started_ = t;
            if (repeated) { started_ = NOT_STARTED; delayed_start_ = true; }
        }
        busy = next_busy;
        done_q_ = sampled;
        fresh_ = accept;
    }
    // The last command's completion, seen by the sampler and not yet in
    // effect (the gates' COMPLETE).
    bool completing() const { return busy && done_q_ && !releasing && !fresh_; }
    // Columns of a burst still to come (the gates' MORE_COLUMNS).
    bool moreColumns() const { return burst_ && burst_left_ > 0; }
    // BUSY as the device shows it: low once the final completion is seen.
    bool busyShown() const { return busy && !releasing && !(completing() && !moreColumns()); }
    // READ_VALID (and the output enable) as shown.
    bool validShown() const { return valid || (completing() && command == Read); }
    // The open state as shown: a finishing ACTIVATE opens, a finishing
    // PRECHARGE closes, already in the completing cycle.
    bool openShown() const {
        return (open && !(completing() && command == Precharge)) || (completing() && command == Activate);
    }
    // Earliest future time at which an analog control may change.
    SimulationTime nextStep(SimulationTime t) const {
        if (!busy || command == None || started_ == NOT_STARTED) return 0;
        for (auto at : stages_.at(command)) if (stageAt(at) > t) return stageAt(at);
        // The chain's end reaching the sampler (the gates' OR tree).
        if (const SimulationTime end = chainDone(); end > t) return end;
        return 0;
    }
    // The analog controls at time `t` (the gates' formulas).
    bool wordline(SimulationTime t) const {
        return (open && !(running(Precharge) && reached(0, t))) || (running(Activate) && reached(0, t)) ||
               (running(Refresh) && reached(0, t) && !reached(refresh_close_, t));
    }
    bool sensing(SimulationTime t) const {
        return (open && !(running(Precharge) && reached(0, t))) || (running(Activate) && reached(1, t)) ||
               (running(Refresh) && reached(1, t) && !reached(refresh_close_, t));
    }
    bool precharging(SimulationTime t) const {
        return (!open && !running(Activate) && !(running(Refresh) && !reached(refresh_close_ + 1, t))) || (running(Precharge) && reached(1, t));
    }
    bool writing(SimulationTime t) const { return running(Write) && reached(0, t) && !reached(1, t); }

    bool busy = false, error = false, valid = false, open = false, releasing = false;
    unsigned command = None;
    uint32_t row = 0, column = 0;
    size_t refresh_row = 0;
    // Commands accepted, by code (instrumentation, not control state).
    std::array<uint64_t, 7> accepted{};
    Bits data, mask;

private:
    bool running(unsigned c) const { return busy && command == c; }
    bool reached(size_t stage, SimulationTime t) const {
        const auto& at = stages_.at(command);
        return started_ != NOT_STARTED && stage < at.size() && t >= stageAt(at[stage]);
    }
    SimulationTime stageAt(SimulationTime offset) const {
        return started_ == NOT_STARTED ? MAX_SIMULATION_TIME
            : checkedTimeAdd(checkedTimeAdd(started_, launch_), offset);
    }
    SimulationTime chainDone() const {
        return started_ == NOT_STARTED ? MAX_SIMULATION_TIME
            : checkedTimeAdd(stageAt(stages_.at(command).back()), or_tree_);
    }
    // A refresh accepted on the edge that completes the previous refresh
    // uses the counter as it was before that edge (registers update together).
    size_t refresh_row_before(bool advanced) const {
        if (!advanced) return refresh_row;
        return refresh_row == 0 ? rows_ - 1 : refresh_row - 1;
    }
    size_t width_, rows_, words_per_row_;
    SimulationTime launch_ = 0, or_tree_ = 0, started_ = 0;
    static constexpr SimulationTime NOT_STARTED = MAX_SIMULATION_TIME;
    size_t refresh_close_ = 2;
    bool done_q_ = false, fresh_ = false, delayed_start_ = false;
    size_t burst_words_;
    bool burst_ = false;
    size_t burst_left_ = 0;
    std::map<unsigned, std::vector<SimulationTime>> stages_;
};

} // namespace circuit::memory_detail
