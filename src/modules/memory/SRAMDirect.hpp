#pragma once
#include "modules/memory/SRAM.tpp"
#include "modules/memory/MemoryStorage.hpp"
#include "components/capabilities/MemoryStateView.hpp"

namespace circuit::sram {
inline LogicValue writeCell(LogicValue q, LogicValue wl, LogicValue drive, LogicValue bl, LogicValue bar) {
    if (wl == LogicValue::LOW || drive == LogicValue::LOW) return q;
    if (wl == LogicValue::HIGH && drive == LogicValue::HIGH && binary(bl) && bar == invert(bl)) return bl;
    return LogicValue::UNKNOWN;
}
class CellDirect : public BasicComponent {
public:
    explicit CellDirect(std::string name) : BasicComponent(std::move(name), 1, [](IOComponent* s) {
        for (const auto* p : {"WL", "DRIVE", "BL", "BL_BAR"}) s->addPin(p, PinType::INPUT);
        for (const auto* p : {"Q", "Q_BAR"}) s->addPin(p, PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override { return "SRAMCell"; }
    void evaluate(size_t t, Simulator& sim) override {
        q_ = writeCell(q_, getInputValue("WL"), getInputValue("DRIVE"), getInputValue("BL"), getInputValue("BL_BAR"));
        _updateOutputWire(sim, "Q", q_, t); _updateOutputWire(sim, "Q_BAR", invert(q_), t);
    }
private: LogicValue q_ = LogicValue::UNKNOWN;
};
class RowDirect : public BasicComponent {
public:
    RowDirect(std::string name, size_t columns) : BasicComponent(std::move(name), 1, [columns](IOComponent* s) {
        if (!columns || columns > 4096) throw std::invalid_argument("Invalid SRAM row width");
        s->addPin("WL", PinType::INPUT);
        for (size_t c = 0; c < columns; ++c) {
            for (const auto* p : {"DRIVE_", "BL_", "BL_BAR_"}) s->addPin(p + std::to_string(c), PinType::INPUT);
            s->addPin("Q_" + std::to_string(c), PinType::OUTPUT);
        }
    }), data_(columns, LogicValue::UNKNOWN), type_("SRAMRow<" + std::to_string(columns) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(size_t t, Simulator& sim) override {
        for (size_t c = 0; c < data_.size(); ++c) {
            const auto n = std::to_string(c);
            data_[c] = writeCell(data_[c], getInputValue("WL"), getInputValue("DRIVE_" + n), getInputValue("BL_" + n), getInputValue("BL_BAR_" + n));
            _updateOutputWire(sim, "Q_" + n, data_[c], t);
        }
    }
private: Bits data_; std::string type_;
};

template<size_t W> class BankDirect : public BasicComponent {
public:
    BankDirect(std::string name, SRAMDeviceConfig c)
        : BasicComponent(std::move(name), 1, [c](IOComponent* s) { defineSRAMBankPins<W>(s, c); }), config_(c),
          data_(c.depth_words * W, LogicValue::UNKNOWN), bitlines_(c.columns(W)),
          published_(c.columns(W), std::array<LogicValue, 3>{LogicValue::UNKNOWN, LogicValue::UNKNOWN, LogicValue::UNKNOWN}),
          type_("SRAMStorageBank<" + std::to_string(W) + "," + std::to_string(c.depth_words) + "," + std::to_string(c.words_per_row) + ">") { c.validate(W, true); }
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(size_t t, Simulator& sim) override {
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

template<size_t W> class DeviceDirect : public BasicComponent, public MemoryDeviceStorage {
public:
    DeviceDirect(std::string name, SRAMDeviceConfig c)
        : BasicComponent(std::move(name), 1, defineSRAMPins<W>),
          MemoryDeviceStorage(c.depth_words, W, LogicValue::UNKNOWN, c.record_history, c.storage_limits),
          config_(checked(c)), sequence_(W) {}
    const char* getTypeName() const override { return memoryDisplayType(SRAM<W>::TypeName.c_str()); }
    const MemoryStorage& memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t address, const std::vector<uint8_t>& bytes) override { initializeMemoryBytes(address, bytes); }
    void evaluate(size_t t, Simulator& sim) override {
        started_ = true;
        const auto previous = sequence_.phase;
        if (auto due = sequence_.update(*this, t, config_.phase_ticks)) sim.scheduleEvent(std::make_shared<ComponentEvalEvent>(*due, this->shared_from_this()));
        const auto word = number(sequence_.address);
        const bool selected = known(sequence_.address) && word < config_.depth_words;
        if (previous == AccessSequence::Phase::Release && sequence_.phase == AccessSequence::Phase::Access &&
            sequence_.write_enable && selected) {
            for (size_t byte = 0; byte < W / 8; ++byte) if (sequence_.mask[byte] == LogicValue::HIGH) {
                Bits value(sequence_.data.begin() + byte * 8, sequence_.data.begin() + (byte + 1) * 8);
                storage_.writeLogic(t, word * (W / 8) + byte, value, true);
            }
        }
        Bits out(W, LogicValue::HIGH_Z);
        if (sequence_.output_enable) out = selected ? storage_.readLogic(word * (W / 8), W / 8) : Bits(W, LogicValue::UNKNOWN);
        this->template _updateOutputWire<W>(sim, "READ_DATA", out, t);
    }
private:
    static SRAMDeviceConfig checked(SRAMDeviceConfig c) { c.validate(W); return c; }
    SRAMDeviceConfig config_; AccessSequence sequence_;
};
}
