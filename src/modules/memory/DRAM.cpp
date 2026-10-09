#include "modules/memory/DRAM.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include <algorithm>

using namespace circuit;
using namespace circuit::memory_device;
namespace {
void retentionValid(size_t t) {
    if (t < 1024 || t > 1000000000)
        throw std::invalid_argument("retention_ticks must be 1024..1000000000");
}
void cellPins(IOComponent *s) {
    for (const auto *n : {"WL", "RESTORE", "D"})
        s->addPin(n, PinType::INPUT);
    for (const auto *n : {"Q", "SAMPLE"})
        s->addPin(n, PinType::OUTPUT);
}
void rowPins(IOComponent *s, size_t cols, size_t retention) {
    retentionValid(retention);
    if (!cols || cols > 512)
        throw std::invalid_argument("DRAM row columns must be 1..512");
    s->addPin("WL", PinType::INPUT);
    for (size_t c = 0; c < cols; ++c) {
        auto n = std::to_string(c);
        for (const auto *p : {"RESTORE_", "D_"})
            s->addPin(p + n, PinType::INPUT);
        for (const auto *p : {"Q_", "SAMPLE_"})
            s->addPin(p + n, PinType::OUTPUT);
    }
}
void sensePins(IOComponent *s, size_t rows) {
    if (!rows || rows > 512)
        throw std::invalid_argument("Invalid DRAM column rows");
    for (const auto *p : {"PRECHARGE", "SENSE", "WRITE", "D"})
        s->addPin(p, PinType::INPUT);
    for (size_t r = 0; r < rows; ++r)
        for (const auto *p : {"WL_", "SAMPLE_"})
            s->addPin(p + std::to_string(r), PinType::INPUT);
    s->addPin("Q", PinType::OUTPUT);
    s->addPin("RESTORE", PinType::OUTPUT);
}
void bankPins(IOComponent *s, size_t width, DRAMDeviceConfig c) {
    c.validate(width, true);
    for (const auto *p : {"PRECHARGE", "SENSE"})
        s->addPin(p, PinType::INPUT);
    for (size_t r = 0; r < c.rows(); ++r)
        s->addPin("WL_" + std::to_string(r), PinType::INPUT);
    for (size_t col = 0; col < c.columns(width); ++col) {
        auto n = std::to_string(col);
        for (const auto *p : {"DRIVE_", "DATA_"})
            s->addPin(p + n, PinType::INPUT);
        s->addPin("COL_" + n, PinType::OUTPUT);
    }
}
void devicePins(IOComponent *s, size_t width, bool control = false) {
    s->addPin("CLK", PinType::INPUT);
    s->addPin<3>("CMD", PinType::INPUT);
    s->addPin<32>("ADDR", PinType::INPUT);
    s->addPinDynamic("WRITE_DATA", PinType::INPUT, width);
    s->addPinDynamic("WRITE_MASK", PinType::INPUT, width / 8);
    for (const auto *p : {"BUSY", "ERROR", "READ_VALID", "ROW_OPEN"})
        s->addPin(p, PinType::OUTPUT);
    if (!control)
        s->addPinDynamic("READ_DATA", PinType::OUTPUT, width);
    else {
        for (const auto *p : {"ROW_ADDR", "COLUMN_ADDR"})
            s->addPin<32>(p, PinType::OUTPUT);
        s->addPinDynamic("DATA", PinType::OUTPUT, width);
        s->addPinDynamic("MASK", PinType::OUTPUT, width / 8);
        for (const auto *p : {"PRECHARGE", "SENSE", "ROW_ENABLE", "WRITE_ENABLE", "OUTPUT_ENABLE"})
            s->addPin(p, PinType::OUTPUT);
    }
}
// The same finite command sequencer drives the structural control primitive and
// compact device. Storage/sensing are independently represented below.
struct Commands {
    DRAMDeviceConfig config;
    size_t width, command = 0, stage = 0, due = 0, row = 0, col = 0, refresh_row = 0;
    bool busy = false, error = false, open = false, precharge = true, sense = false, wl = false,
         write = false, output = false;
    LogicValue last_clk = X;
    Bits data, mask;
    Commands(size_t w, DRAMDeviceConfig c) : config(c), width(w), data(w, X), mask(w / 8, L) {
        c.validate(w);
    }
    std::optional<size_t> update(const IOComponent &io, size_t t) {
        auto clk = io.getInputValue("CLK");
        bool edge = last_clk == L && clk == H;
        last_clk = clk;
        // Completion happens before a new command at exactly the deadline.
        if (due && t >= due) {
            due = 0;
            ++stage;
            if (command == 1) {
                if (stage == 1)
                    wl = true;
                else if (stage == 2)
                    sense = true;
                else {
                    open = true;
                    busy = false;
                }
            }
            if (command == 2) {
                output = true;
                busy = false;
            }
            if (command == 3) {
                if (stage == 1)
                    write = true;
                else if (stage == 2)
                    write = false;
                else
                    busy = false;
            }
            if (command == 4) {
                if (stage == 1) {
                    wl = false;
                    sense = false;
                } else if (stage == 2)
                    precharge = true;
                else {
                    open = false;
                    busy = false;
                }
            }
            if (command == 5) {
                if (stage == 1)
                    wl = true;
                else if (stage == 2)
                    sense = true;
                else if (stage == 3) {
                    wl = false;
                    sense = false;
                } else if (stage == 4)
                    precharge = true;
                else {
                    refresh_row = (refresh_row + 1) % config.rows();
                    busy = false;
                }
            }
            if (busy)
                due = t + config.phase_ticks;
        }
        if (edge) {
            auto cmd = io.getInputPin<3>("CMD")->getValueAsVector();
            auto address = io.getInputPin<32>("ADDR")->getValueAsVector();
            auto m = io.getInputPinDynamic("WRITE_MASK")->getValueAsVector();
            if (!known(cmd)) {
                error = true;
                // An undecodable idle request invalidates the previous read,
                // just like a reserved binary command. Never cancel an active
                // command: its scheduled completion still owns the data path.
                if (!busy)
                    output = false;
            } else if (number(cmd) != 0) {
                auto op = number(cmd);
                auto a = number(address);
                if (busy) {
                    error = true;
                } else if (op > 5 || ((op == 1 || op == 2 || op == 3) && !known(address)) ||
                           (op == 1 && (open || a >= config.rows())) ||
                           ((op == 2 || op == 3) && (!open || a >= config.words_per_row)) ||
                           (op == 3 && !known(m)) || (op == 4 && !open) || (op == 5 && open)) {
                    error = true;
                    output = false;
                } else {
                    error = false;
                    output = false;
                    busy = true;
                    command = op;
                    stage = 0;
                    due = t + config.phase_ticks;
                    if (op == 1 || op == 5) {
                        row = op == 1 ? a : refresh_row;
                        precharge = false;
                        wl = false;
                        sense = false;
                    }
                    if (op == 2 || op == 3)
                        col = a;
                    if (op == 3) {
                        data = io.getInputPinDynamic("WRITE_DATA")->getValueAsVector();
                        mask = m;
                    }
                }
            }
        }
        return due ? std::optional<size_t>(due) : std::nullopt;
    }
};
class DRAMCommandControl : public Primitive {
  public:
    DRAMCommandControl(std::string n, size_t width, DRAMDeviceConfig c)
        : Primitive(std::move(n), "DRAMCommandControl<" + std::to_string(width) + ">",
                    [width](IOComponent *s) { devicePins(s, width, true); }),
          state_(width, c) {}
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::SelfTimed; }
    void evaluate(size_t t, Simulator &sim) override {
        auto previous = state_.due;
        if (auto due = state_.update(*this, t); due && (*due != previous || t >= previous))
            wake(sim, *due);
        for (const auto &[p, v] : std::vector<std::pair<std::string, bool>>{{"BUSY", state_.busy},
                                                                            {"ERROR", state_.error},
                                                                            {"READ_VALID", state_.output},
                                                                            {"ROW_OPEN", state_.open},
                                                                            {"PRECHARGE", state_.precharge},
                                                                            {"SENSE", state_.sense},
                                                                            {"ROW_ENABLE", state_.wl},
                                                                            {"WRITE_ENABLE", state_.write},
                                                                            {"OUTPUT_ENABLE", state_.output}})
            publish(sim, t, p, v ? H : L);
        publish(sim, t, "ROW_ADDR", bits(32, state_.row));
        publish(sim, t, "COLUMN_ADDR", bits(32, state_.col));
        publish(sim, t, "DATA", state_.data);
        publish(sim, t, "MASK", state_.mask);
    }

  private:
    Commands state_;
};
// Compact row state: deliberately no flip-flop or SRAM model underneath.
struct Charge {
    LogicValue q = X, sample = X, last = L;
    size_t closed = 0;
    bool restored = false;
    void update(size_t t, size_t retention, LogicValue wl, LogicValue restore, LogicValue d) {
        if (!binary(wl)) {
            q = sample = X;
            restored = false;
            last = wl;
            return;
        }
        if (wl == H && last != H) {
            sample = t - closed >= retention ? X : q;
            q = X;
            restored = false;
        }
        if (wl == H && restore != L) {
            q = restore == H ? stored(d) : X;
            restored = restore == H;
        }
        if (wl == L && last == H) {
            if (!restored)
                q = X;
            sample = X;
            closed = t;
            restored = false;
        }
        last = wl;
    }
};
class DRAMRowBehavioral : public Primitive {
  public:
    DRAMRowBehavioral(std::string n, size_t columns, size_t retention)
        : Primitive(std::move(n), "DRAMRow<" + std::to_string(columns) + ">",
                    [=](IOComponent *s) { rowPins(s, columns, retention); }),
          retention_(retention), cells_(columns) {}
    void evaluate(size_t t, Simulator &sim) override {
        for (size_t c = 0; c < cells_.size(); ++c) {
            auto n = std::to_string(c);
            auto &cell = cells_[c];
            cell.update(t, retention_, getInputValue("WL"), getInputValue("RESTORE_" + n),
                        getInputValue("D_" + n));
            publish(sim, t, "Q_" + n, cell.q);
            publish(sim, t, "SAMPLE_" + n, cell.sample);
        }
    }

  private:
    size_t retention_;
    std::vector<Charge> cells_;
};
class DRAMBankBehavioral : public Primitive {
  public:
    DRAMBankBehavioral(std::string n, size_t width, DRAMDeviceConfig c)
        : Primitive(std::move(n),
                    "DRAMStorageBank<" + std::to_string(width) + "," + std::to_string(c.depth_words) + "," +
                        std::to_string(c.words_per_row) + ">",
                    [=](IOComponent *s) { bankPins(s, width, c); }),
          width_(width), config_(c), cells_(c.depth_words * width), buffer_(c.columns(width), X) {}
    void evaluate(size_t t, Simulator &sim) override {
        size_t count = 0, row = 0;
        bool valid = true;
        Bits wl;
        for (size_t r = 0; r < config_.rows(); ++r) {
            auto v = getInputValue("WL_" + std::to_string(r));
            wl.push_back(v);
            valid &= binary(v);
            if (v == H) {
                ++count;
                row = r;
            }
        }
        auto pc = getInputValue("PRECHARGE"), se = getInputValue("SENSE");
        for (size_t r = 0; r < wl.size(); ++r)
            for (size_t c = 0; c < buffer_.size(); ++c)
                cells_[r * buffer_.size() + c].update(t, config_.retention_ticks, wl[r], L, X);
        bool restore = false;
        if (pc == H && valid && count == 0) {
            prepared_ = true;
            sensed_ = false;
            std::fill(buffer_.begin(), buffer_.end(), X);
        } else if (pc != L || !valid || count > 1 || !binary(se)) {
            prepared_ = false;
            sensed_ = false;
            std::fill(buffer_.begin(), buffer_.end(), X);
        } else if (se == H && count == 1) {
            if (!prepared_ || (sensed_ && selected_ != row)) {
                std::fill(buffer_.begin(), buffer_.end(), X);
                prepared_ = false;
                sensed_ = false;
            } else {
                if (!sensed_) {
                    for (size_t c = 0; c < buffer_.size(); ++c)
                        buffer_[c] = cells_[row * buffer_.size() + c].sample;
                    selected_ = row;
                    sensed_ = true;
                }
                restore = true;
                for (size_t c = 0; c < buffer_.size(); ++c) {
                    auto d = getInputValue("DRIVE_" + std::to_string(c));
                    if (d != L)
                        buffer_[c] = d == H ? stored(getInputValue("DATA_" + std::to_string(c))) : X;
                }
            }
        }
        if (restore)
            for (size_t c = 0; c < buffer_.size(); ++c)
                cells_[row * buffer_.size() + c].update(t, config_.retention_ticks, H, H, buffer_[c]);
        for (size_t c = 0; c < buffer_.size(); ++c)
            publish(sim, t, "COL_" + std::to_string(c), buffer_[c]);
    }

  private:
    size_t width_;
    DRAMDeviceConfig config_;
    std::vector<Charge> cells_;
    Bits buffer_;
    bool prepared_ = true, sensed_ = false;
    size_t selected_ = 0;
};
class DRAMBehavioral : public Primitive, public MemoryDeviceStorage {
  public:
    const char* getTypeName() const override { return memoryDisplayType(Primitive::getTypeName()); }
    DRAMBehavioral(std::string n, size_t width, DRAMDeviceConfig c)
        : Primitive(std::move(n), "DRAM<" + std::to_string(width) + ">",
                    [width](IOComponent *s) { devicePins(s, width); }),
          MemoryDeviceStorage(c.depth_words, width, X, c.record_history, c.storage_limits),
          width_(width), state_(width, c), buffer_(c.columns(width), X) {}
    const MemoryStorage &memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t> &) override {
        throw std::logic_error("Initialize DRAM through ACT/WRITE commands");
    }
    void evaluate(size_t t, Simulator &sim) override {
        started_ = true;
        bool old_wl = state_.wl, old_sense = state_.sense, old_write = state_.write;
        auto previous = state_.due;
        if (auto due = state_.update(*this, t); due && (*due != previous || t >= previous))
            wake(sim, *due);
        auto start = state_.row * state_.config.words_per_row * (width_ / 8);
        if (!old_wl && state_.wl) {
            const auto it = closed_.find(state_.row);
            const size_t closed = it == closed_.end() ? 0 : it->second;
            buffer_ = t - closed >= state_.config.retention_ticks
                          ? Bits(buffer_.size(), X)
                          : storage_.readLogic(start, buffer_.size() / 8);
            storage_.writeLogic(t, start, Bits(buffer_.size(), X));
        }
        if (!old_sense && state_.sense)
            storage_.writeLogic(t, start, buffer_);
        if (!old_write && state_.write) {
            // Only selected byte lanes are logical writes. Sampling and
            // restoring the rest of the row above are internal DRAM activity.
            for (size_t byte = 0; byte < width_ / 8; ++byte) if (state_.mask[byte] == H) {
                Bits lane(8);
                for (size_t bit = 0; bit < 8; ++bit) {
                    const auto b = byte * 8 + bit;
                    lane[bit] = buffer_[state_.col * width_ + b] = stored(state_.data[b]);
                }
                storage_.writeLogic(t, start + state_.col * (width_ / 8) + byte, lane, true);
            }
        }
        if (old_wl && !state_.wl) {
            const auto budget = state_.config.storage_limits.max_resident_bytes;
            if (!closed_.contains(state_.row) &&
                (closed_.size() + 1) > (budget - std::min(budget, storage_.allocatedDataBytes())) / 128)
                throw std::length_error("DRAM row metadata budget exceeded");
            closed_[state_.row] = t;
        }
        // Allocating another data page must also respect metadata already held.
        const auto budget = state_.config.storage_limits.max_resident_bytes;
        if (closed_.size() > (budget - std::min(budget, storage_.allocatedDataBytes())) / 128)
            throw std::length_error("DRAM row metadata budget exceeded");
        if (state_.precharge)
            std::fill(buffer_.begin(), buffer_.end(), X);
        publish(sim, t, "BUSY", state_.busy ? H : L);
        publish(sim, t, "ERROR", state_.error ? H : L);
        publish(sim, t, "READ_VALID", state_.output ? H : L);
        publish(sim, t, "ROW_OPEN", state_.open ? H : L);
        publish(sim, t, "READ_DATA",
                state_.output
                    ? Bits(buffer_.begin() + state_.col * width_, buffer_.begin() + (state_.col + 1) * width_)
                    : Bits(width_, Z));
    }

  private:
    size_t width_;
    Commands state_;
    std::map<size_t, size_t> closed_;
    Bits buffer_;
};
} // namespace
void DRAMDeviceConfig::validate(size_t width, bool expanded) const {
    Geometry::validate(width, expanded);
    retentionValid(retention_ticks);
    if (phase_ticks < 32 || phase_ticks > 10000 || retention_ticks <= 8 * phase_ticks)
        throw std::invalid_argument("DRAM phase_ticks=32..10000 and retention_ticks>8*phase_ticks required");
}
DRAMDeviceConfig dramConfigFromParameters(const ParameterMap &p) {
    keys(p, {"depth_words", "words_per_row", "phase_ticks", "retention_ticks", "record_history",
             "paged", "max_resident_bytes", "max_history_bytes"});
    DRAMDeviceConfig c;
    readGeometry(p, c);
    c.phase_ticks = parameter(p, "phase_ticks", 32);
    c.retention_ticks = parameter(p, "retention_ticks", 100000);
    return c;
}
ParameterMap dramParameters(const DRAMDeviceConfig &c) {
    auto p = geometryParameters(c);
    p.insert({{"phase_ticks", std::to_string(c.phase_ticks)},
            {"retention_ticks", std::to_string(c.retention_ticks)}});
    return p;
}
DRAMCell::DRAMCell(std::string n, size_t retention)
    : Primitive(std::move(n), "DRAMCell", cellPins), retention_(retention) {
    retentionValid(retention);
}
void DRAMCell::evaluate(size_t t, Simulator &sim) {
    auto wl = getInputValue("WL"), restore = getInputValue("RESTORE");
    if (!binary(wl)) {
        charge_ = sample_ = X;
        restored_ = false;
    } else {
        if (wl == H && last_wl_ != H) {
            if (t - closed_ >= retention_)
                charge_ = X;
            sample_ = charge_;
            charge_ = X;
            restored_ = false;
        }
        if (wl == H && restore != L) {
            charge_ = restore == H ? stored(getInputValue("D")) : X;
            restored_ = restore == H;
        }
        if (wl == L && last_wl_ == H) {
            if (!restored_)
                charge_ = X;
            sample_ = X;
            closed_ = t;
            restored_ = false;
        }
    }
    last_wl_ = wl;
    publish(sim, t, "Q", charge_);
    publish(sim, t, "SAMPLE", sample_);
}
DRAMRow::DRAMRow(std::string n, size_t columns, size_t retention)
    : IOComponent(std::move(n), [=](IOComponent *s) { rowPins(s, columns, retention); }), columns_(columns),
      retention_(retention), type_("DRAMRow<" + std::to_string(columns) + ">") {
}
void DRAMRow::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    for (size_t c = 0; c < columns_; ++c) {
        auto n = std::to_string(c);
        auto cell = child(b, *this, "memory.dram.cell", "CELL_" + n,
                          {{"retention_ticks", std::to_string(retention_)}});
        w.connect(getInputPin("WL"), cell->getInputPin("WL"));
        for (const auto *p : {"RESTORE", "D"})
            w.connect(getInputPin(std::string(p) + "_" + n), cell->getInputPin(p));
        for (const auto *p : {"Q", "SAMPLE"})
            w.connect(cell->getOutputPin(p), getOutputPin(std::string(p) + "_" + n));
    }
    w.finish();
}
DRAMSenseColumn::DRAMSenseColumn(std::string n, size_t rows)
    : Primitive(std::move(n), "DRAMSenseColumn<" + std::to_string(rows) + ">",
                [rows](IOComponent *s) { sensePins(s, rows); }),
      rows_(rows) {
}
void DRAMSenseColumn::evaluate(size_t t, Simulator &sim) {
    size_t count = 0, row = 0;
    bool valid = true;
    for (size_t r = 0; r < rows_; ++r) {
        auto v = getInputValue("WL_" + std::to_string(r));
        valid &= binary(v);
        if (v == H) {
            ++count;
            row = r;
        }
    }
    auto pc = getInputValue("PRECHARGE"), se = getInputValue("SENSE");
    bool restore = false;
    if (pc == H && valid && count == 0) {
        prepared_ = true;
        sensed_ = false;
        buffer_ = X;
    } else if (pc != L || !valid || count > 1 || !binary(se)) {
        prepared_ = false;
        sensed_ = false;
        buffer_ = X;
    } else if (se == H && count == 1) {
        if (!prepared_ || (sensed_ && selected_ != row)) {
            prepared_ = false;
            sensed_ = false;
            buffer_ = X;
        } else {
            if (!sensed_) {
                buffer_ = stored(getInputValue("SAMPLE_" + std::to_string(row)));
                sensed_ = true;
                selected_ = row;
            }
            restore = true;
            auto wr = getInputValue("WRITE");
            if (wr != L)
                buffer_ = wr == H ? stored(getInputValue("D")) : X;
        }
    }
    publish(sim, t, "Q", buffer_);
    publish(sim, t, "RESTORE", restore ? H : L);
}
DRAMBank::DRAMBank(std::string n, size_t width, DRAMDeviceConfig c)
    : IOComponent(std::move(n), [=](IOComponent *s) { bankPins(s, width, c); }), width_(width), config_(c),
      type_("DRAMStorageBank<" + std::to_string(width) + "," + std::to_string(c.depth_words) + "," +
            std::to_string(c.words_per_row) + ">") {
    c.validate(width, true);
}
void DRAMBank::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    std::vector<std::shared_ptr<IOComponent>> rows;
    for (size_t r = 0; r < config_.rows(); ++r) {
        auto n = std::to_string(r);
        rows.push_back(child(b, *this, "memory.dram.row", "ROW_" + n,
                             {{"columns", std::to_string(config_.columns(width_))},
                              {"retention_ticks", std::to_string(config_.retention_ticks)}}));
        w.connect(getInputPin("WL_" + n), rows.back()->getInputPin("WL"));
    }
    for (size_t c = 0; c < config_.columns(width_); ++c) {
        auto n = std::to_string(c);
        auto sense = child(b, *this, "memory.dram.sense-column", "COLUMN_" + n,
                           {{"rows", std::to_string(config_.rows())}});
        for (const auto *p : {"PRECHARGE", "SENSE"})
            w.connect(getInputPin(p), sense->getInputPin(p));
        w.connect(getInputPin("DRIVE_" + n), sense->getInputPin("WRITE"));
        w.connect(getInputPin("DATA_" + n), sense->getInputPin("D"));
        for (size_t r = 0; r < rows.size(); ++r) {
            auto rn = std::to_string(r);
            w.connect(getInputPin("WL_" + rn), sense->getInputPin("WL_" + rn));
            w.connect(rows[r]->getOutputPin("SAMPLE_" + n), sense->getInputPin("SAMPLE_" + rn));
            w.connect(sense->getOutputPin("Q"), rows[r]->getInputPin("D_" + n));
            w.connect(sense->getOutputPin("RESTORE"), rows[r]->getInputPin("RESTORE_" + n));
        }
        w.connect(sense->getOutputPin("Q"), getOutputPin("COL_" + n));
    }
    w.finish();
}
DRAMDevice::DRAMDevice(std::string n, size_t width, DRAMDeviceConfig c)
    : IOComponent(std::move(n), [width](IOComponent *s) { devicePins(s, width); }), width_(width), config_(c),
      type_("DRAM<" + std::to_string(width) + ">") {
    c.validate(width, true);
}
void DRAMDevice::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto suffix = std::to_string(width_);
    auto control =
        child(b, *this, "memory.dram.command-control." + suffix, "COMMAND_CONTROL", dramParameters(config_));
    auto decoder = child(b, *this, "memory.shared.row-decoder", "ROW_DECODE",
                         {{"rows", std::to_string(config_.rows())}});
    auto bank = child(b, *this, "memory.dram.bank." + suffix, "STORAGE", dramParameters(config_));
    auto mux = child(b, *this, "memory.shared.column-mux." + suffix, "COLUMN_SELECT",
                     {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto driver = child(b, *this, "memory.shared.write-distributor." + suffix, "WRITE_DRIVERS",
                        {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto output = child(b, *this, "memory.shared.output-buffer." + suffix, "OUTPUT");
    for (const auto &[n, p] : getAllInputPins())
        w.connect(p, control->getInputPinDynamic(n));
    for (const auto *n : {"BUSY", "ERROR", "READ_VALID", "ROW_OPEN"})
        w.connect(control->getOutputPin(n), getOutputPin(n));
    for (const auto *n : {"PRECHARGE", "SENSE"})
        w.connect(control->getOutputPin(n), bank->getInputPin(n));
    w.connect(control->getOutputPin<32>("ROW_ADDR"), decoder->getInputPin<32>("ROW_ADDR"));
    w.connect(control->getOutputPin("ROW_ENABLE"), decoder->getInputPin("ENABLE"));
    for (size_t r = 0; r < config_.rows(); ++r) {
        auto n = "WL_" + std::to_string(r);
        w.connect(decoder->getOutputPin(n), bank->getInputPin(n));
    }
    for (const auto &target : {mux, driver})
        w.connect(control->getOutputPin<32>("COLUMN_ADDR"), target->getInputPin<32>("COLUMN_ADDR"));
    for (const auto *p : {"DATA", "MASK"})
        w.connect(control->getOutputPinDynamic(p), driver->getInputPinDynamic(p));
    w.connect(control->getOutputPin("WRITE_ENABLE"), driver->getInputPin("ENABLE"));
    for (size_t c = 0; c < config_.columns(width_); ++c) {
        auto n = std::to_string(c);
        w.connect(bank->getOutputPin("COL_" + n), mux->getInputPin("COL_" + n));
        for (const auto *p : {"DRIVE_", "DATA_"})
            w.connect(driver->getOutputPin(p + n), bank->getInputPin(p + n));
    }
    w.connect(mux->getOutputPinDynamic("DATA"), output->getInputPinDynamic("DATA"));
    w.connect(control->getOutputPin("OUTPUT_ENABLE"), output->getInputPin("ENABLE"));
    w.connect(output->getOutputPinDynamic("OUT"), getOutputPinDynamic("READ_DATA"));
    w.finish();
}
void registerDRAMComponents(ComponentCatalog &c) {
    auto retention = [](const ParameterMap &p) {
        keys(p, {"retention_ticks"});
        auto t = parameter(p, "retention_ticks", 100000);
        retentionValid(t);
        return t;
    };
    registerBlock(
        c, "memory.dram.cell", "DRAMCell",
        [retention](const ParameterMap &p) {
            retention(p);
            return describe(cellPins);
        },
        [retention](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<DRAMCell>(ctx, n, retention(p));
        },
        {}, "DRAMCellTest", true);
    auto row = [](const ParameterMap &p) {
        keys(p, {"columns", "retention_ticks"});
        return std::pair{parameter(p, "columns", 4), parameter(p, "retention_ticks", 100000)};
    };
    registerBlock(
        c, "memory.dram.row", "DRAMRow",
        [row](const ParameterMap &p) {
            auto [cols, t] = row(p);
            return describe([=](IOComponent *s) { rowPins(s, cols, t); });
        },
        [row](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            auto [cols, t] = row(p);
            return Component::createWithContext<DRAMRow>(ctx, n, cols, t);
        },
        [row](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            auto [cols, t] = row(p);
            return Component::createWithContext<DRAMRowBehavioral>(ctx, n, cols, t);
        },
        "DRAMRowTest");
    auto rows = [](const ParameterMap &p) {
        keys(p, {"rows"});
        return parameter(p, "rows", 2);
    };
    registerBlock(
        c, "memory.dram.sense-column", "DRAMSenseColumn",
        [rows](const ParameterMap &p) {
            return describe([r = rows(p)](IOComponent *s) { sensePins(s, r); });
        },
        [rows](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<DRAMSenseColumn>(ctx, n, rows(p));
        },
        {}, "DRAMSenseColumnTest", true);
    for (size_t width : {8, 16, 32}) {
        auto suffix = std::to_string(width);
        registerBlock(
            c, "memory.dram.bank." + suffix, "DRAMStorageBank<" + suffix + ">",
            [width](const ParameterMap &p) {
                return describe(
                    [width, cfg = dramConfigFromParameters(p)](IOComponent *s) { bankPins(s, width, cfg); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<DRAMBank>(ctx, n, width, dramConfigFromParameters(p));
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<DRAMBankBehavioral>(ctx, n, width,
                                                                    dramConfigFromParameters(p));
            },
            "DRAMStorageBankTest");
        registerBlock(
            c, "memory.dram.command-control." + suffix, "DRAMCommandControl<" + suffix + ">",
            [width](const ParameterMap &p) {
                dramConfigFromParameters(p).validate(width);
                return describe([width](IOComponent *s) { devicePins(s, width, true); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<DRAMCommandControl>(ctx, n, width,
                                                                        dramConfigFromParameters(p));
            },
            {}, "DRAMCommandControlTest", true);
        registerBlock(
            c, "memory.dram.device." + suffix, "DRAM<" + suffix + ">",
            [width](const ParameterMap &p) {
                dramConfigFromParameters(p).validate(width);
                return describe([width](IOComponent *s) { devicePins(s, width); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<DRAMDevice>(ctx, n, width, dramConfigFromParameters(p));
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<DRAMBehavioral>(ctx, n, width, dramConfigFromParameters(p));
            },
            "DRAMTest", false,
            [width](const ParameterMap &p) {
                dramConfigFromParameters(p).validate(width, true);
                return true;
            });
    }
}
