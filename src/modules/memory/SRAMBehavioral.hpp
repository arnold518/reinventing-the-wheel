#pragma once

#include "simulator/SimulationTime.hpp"
#include "modules/memory/SRAM.tpp"
#include "modules/memory/MemoryStorage.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "MemoryRestartTiming.hpp"
#include "components/TimingProfile.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"

namespace circuit::sram {
inline LogicValue writeCell(LogicValue q, LogicValue wl, LogicValue drive, LogicValue bl, LogicValue bar) {
    if (wl == LogicValue::LOW || drive == LogicValue::LOW) return q;
    if (wl == LogicValue::HIGH && drive == LogicValue::HIGH && binary(bl) && bar == invert(bl)) return bl;
    return LogicValue::UNKNOWN;
}
class CellBehavioral : public BasicComponent {
public:
    explicit CellBehavioral(std::string name) : BasicComponent(std::move(name), 1, [](IOComponent* s) {
        for (const auto* p : {"WL", "DRIVE", "BL", "BL_BAR"}) s->addPin(p, PinType::INPUT);
        for (const auto* p : {"Q", "Q_BAR"}) s->addPin(p, PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override { return "SRAMCell"; }
    void evaluate(SimulationTime t, Simulator& sim) override {
        q_ = writeCell(q_, getInputValue("WL"), getInputValue("DRIVE"), getInputValue("BL"), getInputValue("BL_BAR"));
        _updateOutputWire(sim, "Q", q_, t); _updateOutputWire(sim, "Q_BAR", invert(q_), t);
    }
private: LogicValue q_ = LogicValue::UNKNOWN;
};
class RowBehavioral : public BasicComponent {
public:
    RowBehavioral(std::string name, size_t columns) : BasicComponent(std::move(name), 1, [columns](IOComponent* s) {
        if (!columns || columns > 4096) throw std::invalid_argument("Invalid SRAM row width");
        s->addPin("WL", PinType::INPUT);
        for (size_t c = 0; c < columns; ++c) {
            for (const auto* p : {"DRIVE_", "BL_", "BL_BAR_"}) s->addPin(p + std::to_string(c), PinType::INPUT);
            s->addPin("Q_" + std::to_string(c), PinType::OUTPUT);
        }
    }), data_(columns, LogicValue::UNKNOWN), type_("SRAMRow<" + std::to_string(columns) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(SimulationTime t, Simulator& sim) override {
        for (size_t c = 0; c < data_.size(); ++c) {
            const auto n = std::to_string(c);
            data_[c] = writeCell(data_[c], getInputValue("WL"), getInputValue("DRIVE_" + n), getInputValue("BL_" + n), getInputValue("BL_BAR_" + n));
            _updateOutputWire(sim, "Q_" + n, data_[c], t);
        }
    }
private: Bits data_; std::string type_;
};

template<size_t W> class BankBehavioral : public BasicComponent {
public:
    BankBehavioral(std::string name, SRAMDeviceConfig c)
        : BasicComponent(std::move(name), 1, [c](IOComponent* s) { defineSRAMBankPins<W>(s, c); }), config_(c),
          data_(c.depth_words * W, LogicValue::UNKNOWN), bitlines_(c.columns(W)),
          published_(c.columns(W), std::array<LogicValue, 3>{LogicValue::UNKNOWN, LogicValue::UNKNOWN, LogicValue::UNKNOWN}),
          type_("SRAMStorageBank<" + std::to_string(W) + "," + std::to_string(c.depth_words) + "," + std::to_string(c.words_per_row) + ">") { c.validate(W, true); }
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(SimulationTime t, Simulator& sim) override {
        Bits wl;
        for (size_t r = 0; r < config_.rows(); ++r) wl.push_back(this->getInputValue("WL_" + std::to_string(r)));
        for (size_t c = 0; c < config_.columns(W); ++c) {
            const auto n = std::to_string(c);
            Bits cells;
            for (size_t r = 0; r < wl.size(); ++r) cells.push_back(data_[r * config_.columns(W) + c]);
            auto& state = bitlines_[c];
            state.update(this->getInputValue("PRECHARGE_" + n), this->getInputValue("DRIVE_" + n),
                         this->getInputValue("WRITE_BL_" + n), this->getInputValue("WRITE_BL_BAR_" + n), wl, cells);
            for (size_t r = 0; r < wl.size(); ++r) {
                auto& q = data_[r * config_.columns(W) + c];
                q = writeCell(q, wl[r], state.cell_drive, state.cell_bl, state.cell_bar);
            }
            const std::array<LogicValue, 3> values{state.bl, state.bar, state.valid};
            const std::array<std::string, 3> pins{"BL_" + n, "BL_BAR_" + n, "VALID_" + n};
            for (size_t i = 0; i < values.size(); ++i) if (values[i] != published_[c][i]) {
                this->_updateOutputWire(sim, pins[i], values[i], t);
                published_[c][i] = values[i];
            }
        }
    }
private:
    SRAMDeviceConfig config_; Bits data_; std::vector<BitlineState> bitlines_;
    std::vector<std::array<LogicValue, 3>> published_; std::string type_;
};

// The SRAM device as one part. Storage, access rules and completion follow
// the gate-level device: an access restarts on any change of address, CE_N,
// WE_N, data or mask, READY and READ_DATA are timed by the table proven on
// the gates (MemoryRestartTiming.hpp), the write is applied when the access
// completes, and OE_N only switches the output.
template<size_t W> class DeviceBehavioral : public BasicComponent, public MemoryDeviceStorage {
public:
    DeviceBehavioral(std::string name, SRAMDeviceConfig c)
        : BasicComponent(std::move(name), 1, [clocked = c.clocked](IOComponent* s) { defineSRAMPins<W>(s, clocked); }),
          MemoryDeviceStorage(c.depth_words, W, LogicValue::UNKNOWN, c.record_history, c.storage_limits),
          config_(checked(c)), ready_({LogicValue::LOW}), data_(Bits(W, LogicValue::HIGH_Z)) {}
    const char* getTypeName() const override { return SRAM<W>::TypeName.c_str(); }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::SelfTimed; }
    const MemoryStorage& memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t address, const std::vector<uint8_t>& bytes) override { initializeMemoryBytes(address, bytes); }
    void evaluate(SimulationTime t, Simulator& sim) override {
        using namespace circuit::memory_detail;
        if (complete_at_ && t >= complete_at_) { complete_at_ = 0; finish(t); }
        if (config_.clocked) { evaluateClocked(t, sim); return; }
        const auto read = [&](const char* pin) { return this->getInputPinDynamic(pin)->getValueAsVector(); };
        Signature now{read("WORD_ADDR"), read("WRITE_DATA"), read("WRITE_MASK"), this->getInputValue("CE_N"), this->getInputValue("WE_N")};
        const auto oe = this->getInputValue("OE_N");
        std::vector<std::string> changed;
        // Data and mask restart an access only while it writes.
        const bool writing = now.we == LogicValue::LOW || current_.we == LogicValue::LOW;
        if (!started_ || now.address != current_.address) changed.push_back("WORD_ADDR");
        if (!started_ || (writing && now.data != current_.data)) changed.push_back("WRITE_DATA");
        if (!started_ || (writing && now.mask != current_.mask)) changed.push_back("WRITE_MASK");
        if (!started_ || now.ce != current_.ce) changed.push_back("CE_N");
        if (!started_ || now.we != current_.we) changed.push_back("WE_N");
        const bool oe_changed = !started_ || oe != oe_;
        started_ = true; oe_ = oe;
        if (changed.empty()) { current_.data = now.data; current_.mask = now.mask; }
        if (!changed.empty()) {
            current_ = now;
            const auto& timing = circuit::timing::activeTimingProfile().memory;
            const SimulationTime gate = circuit::timing::leafDelay(circuit::timing::Leaf::And);
            const SimulationTime full = circuit::memory::memoryTimerSettle(timing.sram_settle, 34 + W + W / 8, true) + timing.sram_precharge + timing.sramBitline(config_.rows()) + 2 * circuit::timing::leafDelay(circuit::timing::Leaf::MemoryPrimitive) + 14 * gate;
            // The arcs proven for this operation (WE_N), when it is known.
            // While WE_N itself changes the operation is in transition: the
            // table's overall arcs apply.
            const bool switching = std::find(changed.begin(), changed.end(), "WE_N") != changed.end();
            const auto operation = binary(now.we) && !switching
                ? std::optional<uint64_t>(now.we == LogicValue::HIGH ? 1 : 0) : std::nullopt;
            // A new operation costs the output's largest delay over every input.
            static const std::vector<std::string> every{"WORD_ADDR", "WRITE_DATA", "WRITE_MASK", "CE_N", "WE_N", "OE_N"};
            const auto& from = switching ? every : changed;
            auto ready = arcBounds(this->getDelays(), from, "READY", {gate, full}, operation);
            auto data = arcBounds(this->getDelays(), from, "READ_DATA", {gate, full}, operation);
            ready.minimum = safeEarliest(this->getDelays(), changed, "READY", gate, switching ? std::nullopt : operation);
            data.minimum = safeEarliest(this->getDelays(), changed, "READ_DATA", gate, switching ? std::nullopt : operation);
            // The gates withdraw the old word when the output enable falls. A
            // transition can take longer than the table's minimum (unequal
            // gate delays), so the withdrawal waits for the whole path it
            // takes in SRAMAccessControl: for a restart, the change detector,
            // the condition gate, the OR tree, STEADY, STARTED, a stage's reset
            // gate, the output-enable tree and the output buffer; for a
            // deselect, the CE_N settle line instead of the detector.
            {
                using circuit::timing::Leaf;
                using circuit::timing::leafDelay;
                constexpr size_t inputs = 34 + W + W / 8;
                size_t levels = 0; while ((size_t{1} << levels) < inputs) ++levels;
                const SimulationTime tail = leafDelay(Leaf::Not) + 4 * leafDelay(Leaf::And) + leafDelay(Leaf::MemoryPrimitive);
                const SimulationTime restart = leafDelay(Leaf::Xor) + leafDelay(Leaf::And) + levels * leafDelay(Leaf::Or) + tail;
                const SimulationTime deselect = circuit::memory::memoryTimerSettle(timing.sram_settle, inputs, true) + tail;
                const bool deselecting = std::find(changed.begin(), changed.end(), "CE_N") != changed.end() && now.ce != LogicValue::LOW;
                data.minimum = std::min(data.maximum, std::max(data.minimum, deselecting ? std::max(restart, deselect) : restart));
            }
            // A drop already on its way stays on time: a second change before it
            // cannot postpone it (the gates drop at the first change).
            const auto low = ready_.pendingAt({LogicValue::LOW});
            const auto off = data_.pendingAt(Bits(W, LogicValue::HIGH_Z));
            ready_.clear(); data_.clear();
            ready_.at(std::min(checkedTimeAdd(t, ready.minimum), low.value_or(checkedTimeAdd(t, ready.minimum))), {LogicValue::LOW});
            ready_.at(checkedTimeAdd(t, ready.maximum), {completion()});
            data_from_ = std::min(checkedTimeAdd(t, data.minimum), off.value_or(checkedTimeAdd(t, data.minimum)));
            data_.at(data_from_, Bits(W, LogicValue::HIGH_Z));
            // A read's word cannot change before it completes: no other access runs.
            data_at_ = checkedTimeAdd(t, data.maximum);
            data_.at(data_at_, output());
            complete_at_ = checkedTimeAdd(t, ready.maximum);
        } else if (oe_changed) {
            // OE_N only switches the output buffer; before completion the
            // buffer still waits for the access.
            const auto out = arcBounds(this->getDelays(), {"OE_N"}, "READ_DATA", {1, 1});
            data_.clear();
            if (t < data_from_) data_.at(data_from_, Bits(W, LogicValue::HIGH_Z));
            data_.at(std::max(data_at_, checkedTimeAdd(t, out.maximum)), output());
        }
        ready_.apply(sim, this->getOutputPin("READY"), t);
        data_.apply(sim, this->template getOutputPin<W>("READ_DATA"), t);
        for (const auto when : {ready_.next(), data_.next(), complete_at_ ? std::optional<SimulationTime>(complete_at_) : std::nullopt})
            if (when && *when > t) sim.scheduleEvent(std::make_shared<ComponentEvalEvent>(*when, this->shared_from_this()));
    }
private:
    // The clocked macro (SRAMAccessControl::buildClocked): a rising edge with
    // CE_N low registers the inputs and starts an access, timed from the edge
    // by the launches proven on the gates; an edge with CE_N high leaves the
    // last access, its data and READY as they are. An unknown CE_N at an edge
    // makes the access token, and so READY and the data, unknown. OE_N only
    // switches the output.
    void evaluateClocked(SimulationTime t, Simulator& sim) {
        using namespace circuit::memory_detail;
        const auto clock = this->getInputValue("CLK");
        const bool edge = clock_ == LogicValue::LOW && clock == LogicValue::HIGH;
        clock_ = clock;
        const auto oe = this->getInputValue("OE_N");
        const bool oe_changed = !started_ || oe != oe_;
        started_ = true; oe_ = oe;
        const auto ce = this->getInputValue("CE_N");
        if (edge && ce != LogicValue::HIGH) {
            const auto read = [&](const char* pin) { return this->getInputPinDynamic(pin)->getValueAsVector(); };
            current_ = Signature{read("WORD_ADDR"), read("WRITE_DATA"), read("WRITE_MASK"), ce, this->getInputValue("WE_N")};
            accessed_ = true;
            const auto [ready, data] = clockedLaunches();
            const auto [ready_off, data_off] = clockedWithdrawal();
            ready_.clear(); data_.clear();
            ready_.at(checkedTimeAdd(t, std::min(ready.maximum, std::max(ready.minimum, ready_off))), {LogicValue::LOW});
            ready_.at(checkedTimeAdd(t, ready.maximum), {completion()});
            data_from_ = checkedTimeAdd(t, std::min(data.maximum, std::max(data.minimum, data_off)));
            data_.at(data_from_, Bits(W, LogicValue::HIGH_Z));
            data_at_ = checkedTimeAdd(t, data.maximum);
            data_.at(data_at_, output());
            complete_at_ = checkedTimeAdd(t, ready.maximum);
        } else if (oe_changed && accessed_) {
            const auto out = arcBounds(this->getDelays(), {"OE_N"}, "READ_DATA", {1, 1});
            data_.clear();
            if (t < data_from_) data_.at(data_from_, Bits(W, LogicValue::HIGH_Z));
            data_.at(std::max(data_at_, checkedTimeAdd(t, out.maximum)), output());
        }
        ready_.apply(sim, this->getOutputPin("READY"), t);
        data_.apply(sim, this->template getOutputPin<W>("READ_DATA"), t);
        for (const auto when : {ready_.next(), data_.next(), complete_at_ ? std::optional<SimulationTime>(complete_at_) : std::nullopt})
            if (when && *when > t) sim.scheduleEvent(std::make_shared<ComponentEvalEvent>(*when, this->shared_from_this()));
    }
    // The latest the gates take READY and the old word away after an edge:
    // the registers' clock-to-Q (at most), then for READY the token's change
    // detector (XOR), STEADY (NOT), STARTED and a stage's reset gate (two
    // ANDs); for the word the registered WE_N's settle line (a write turns
    // the output off once it is through), the output-enable tree (two ANDs)
    // and the output buffer. Every subarray takes each access, so an array of
    // subarrays adds the bus driver and the global wire. A table's minimum
    // comes from a shorter path than a withdrawal can take, so the compact
    // part waits for the longest one (late is allowed, early is not).
    std::pair<SimulationTime, SimulationTime> clockedWithdrawal() const {
        using circuit::timing::Leaf;
        using circuit::timing::leafDelay;
        const SimulationTime flop = circuit::dflipflop_timing::clockToOutputMaximum();
        const SimulationTime reset = flop + leafDelay(Leaf::Xor) + leafDelay(Leaf::Not) + 2 * leafDelay(Leaf::And);
        const SimulationTime held = flop + circuit::sram::clockedSettle(config_.rows());
        SimulationTime bus = 0;
        if (const size_t count = sramSubarrays(config_); count > 1) {
            size_t side = 1; while (side * side < count) ++side;
            bus = leafDelay(Leaf::MemoryPrimitive) + circuit::timing::activeTimingProfile().memory.sram_wire_per_side * side;
        }
        return {reset + bus, std::max(reset, held) + 2 * leafDelay(Leaf::And) + leafDelay(Leaf::MemoryPrimitive) + bus};
    }
    // READY and READ_DATA after the edge: the proven launches, or the
    // structure's own sum where no table applies (the register, the settle
    // window, the stages and their gates).
    std::pair<DelayRange, DelayRange> clockedLaunches() const {
        using circuit::timing::Leaf;
        using circuit::timing::leafDelay;
        const auto* table = this->getDelays();
        const auto launch = [&](const char* pin) -> std::optional<DelayRange> {
            if (!table) return std::nullopt;
            const auto found = table->launches.find(pin);
            return found == table->launches.end() ? std::nullopt : std::optional<DelayRange>(found->second);
        };
        const auto& timing = circuit::timing::activeTimingProfile().memory;
        const SimulationTime gate = leafDelay(Leaf::And);
        const SimulationTime fallback = circuit::dflipflop_timing::clockToOutputMaximum() + circuit::sram::clockedSettle(config_.rows())
            + std::max(timing.sram_precharge, timing.sramBitline(config_.rows())) + timing.sramBitline(config_.rows())
            + 2 * leafDelay(Leaf::MemoryPrimitive) + 14 * gate;
        const DelayRange guess{gate, fallback};
        return {launch("READY").value_or(guess), launch("READ_DATA").value_or(guess)};
    }
    struct Signature { Bits address, data, mask; LogicValue ce = LogicValue::UNKNOWN, we = LogicValue::UNKNOWN; };
    static SRAMDeviceConfig checked(SRAMDeviceConfig c) { c.validate(W); return c; }
    LogicValue completion() const {
        if (current_.ce == LogicValue::HIGH) return LogicValue::LOW;
        // In an array of subarrays an unknown select bit leaves the subarray,
        // and so READY, unknown (the gates' global decoder).
        if (sramSubarrays(config_) > 1) {
            size_t low_bits = 0; while ((size_t{1} << low_bits) < SRAM_SUBARRAY_ROWS * config_.words_per_row) ++low_bits;
            const bool power_of_two = (config_.depth_words & (config_.depth_words - 1)) == 0;
            for (size_t bit = power_of_two ? low_bits : 0; bit < current_.address.size(); ++bit)
                if (!binary(current_.address[bit])) return LogicValue::UNKNOWN;
        }
        // A steady unknown address still completes (its data are unknown).
        return current_.ce == LogicValue::LOW && binary(current_.we) ? LogicValue::HIGH : LogicValue::UNKNOWN;
    }
    bool selected() const { return known(current_.address) && number(current_.address) < config_.depth_words; }
    Bits output() const {
        // A clocked macro's unknown access token leaves its output enable unknown.
        if (config_.clocked && !binary(current_.ce) && oe_ != LogicValue::HIGH) return Bits(W, LogicValue::UNKNOWN);
        if (current_.ce != LogicValue::LOW || current_.we != LogicValue::HIGH || oe_ != LogicValue::LOW) return Bits(W, LogicValue::HIGH_Z);
        return selected() ? storage_.readLogic(number(current_.address) * (W / 8), W / 8) : Bits(W, LogicValue::UNKNOWN);
    }
    // A write lands when the access completes, as in the gate-level array.
    void finish(SimulationTime t) {
        if (current_.ce != LogicValue::LOW || current_.we != LogicValue::LOW || !selected()) return;
        for (size_t byte = 0; byte < W / 8; ++byte) if (current_.mask[byte] == LogicValue::HIGH) {
            Bits value(current_.data.begin() + byte * 8, current_.data.begin() + (byte + 1) * 8);
            storage_.writeLogic(t, number(current_.address) * (W / 8) + byte, value, true);
        }
    }
    SRAMDeviceConfig config_;
    Signature current_;
    LogicValue oe_ = LogicValue::UNKNOWN, clock_ = LogicValue::UNKNOWN;
    bool accessed_ = false;
    SimulationTime complete_at_ = 0, data_from_ = 0, data_at_ = 0;
    circuit::memory_detail::TimedOutput<1> ready_;
    circuit::memory_detail::TimedOutput<W> data_;
};
}
