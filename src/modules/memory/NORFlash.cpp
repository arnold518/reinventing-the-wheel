#include "modules/memory/NORFlash.hpp"
#include "modules/memory/MemoryStorage.hpp"
using namespace circuit;
using namespace circuit::memory_device;
namespace {
LogicValue programBit(LogicValue q, LogicValue d) {
    return q == L || d == L ? L : q == H && d == H ? H : X;
}
LogicValue updateCell(uint8_t &initialized, LogicValue q, LogicValue wl, LogicValue prog, LogicValue erase,
                      LogicValue d) {
    // Power-up wiring settles before the first defined control tuple. This does
    // not silently treat later unknown controls as an inactive write.
    if (!initialized) {
        if (!binary(wl) || !binary(prog) || !binary(erase))
            return q;
        initialized = 1;
    }
    const auto enabled = wl == L || prog == L ? L : wl == H && prog == H ? H : X;
    if (erase == H)
        return enabled == L ? H : X;
    if (erase != L)
        return X;
    if (enabled == L)
        return q;
    if (enabled == H)
        return programBit(q, d);
    return q == L ? L : X;
}
void cellPins(IOComponent *s) {
    for (auto p : {"WL", "PROGRAM", "ERASE", "D"})
        s->addPin(p, PinType::INPUT);
    s->addPin("Q", PinType::OUTPUT);
}
void rowPins(IOComponent *s, size_t cols) {
    if (!cols || cols > 512)
        throw std::invalid_argument("Flash columns must be 1..512");
    for (auto p : {"WL", "ERASE"})
        s->addPin(p, PinType::INPUT);
    for (size_t c = 0; c < cols; ++c) {
        auto n = std::to_string(c);
        for (auto p : {"PROGRAM_", "D_"})
            s->addPin(p + n, PinType::INPUT);
        s->addPin("Q_" + n, PinType::OUTPUT);
    }
}
void bankPins(IOComponent *s, size_t width, NORFlashConfig c) {
    c.validate(width, true);
    for (size_t r = 0; r < c.rows(); ++r)
        for (auto p : {"WL_", "ERASE_"})
            s->addPin(p + std::to_string(r), PinType::INPUT);
    for (size_t col = 0; col < c.columns(width); ++col) {
        auto n = std::to_string(col);
        for (auto p : {"DRIVE_", "DATA_"})
            s->addPin(p + n, PinType::INPUT);
        s->addPin("COL_" + n, PinType::OUTPUT);
    }
}
void devicePins(IOComponent *s, size_t width, NORFlashConfig c, bool control = false) {
    s->addPin("CLK", PinType::INPUT);
    s->addPin<3>("CMD", PinType::INPUT);
    s->addPin<32>("WORD_ADDR", PinType::INPUT);
    s->addPinDynamic("WRITE_DATA", PinType::INPUT, width);
    s->addPinDynamic("WRITE_MASK", PinType::INPUT, width / 8);
    for (auto p : {"BUSY", "ERROR", "READ_VALID", "WRITE_ENABLED"})
        s->addPin(p, PinType::OUTPUT);
    if (!control)
        s->addPinDynamic("READ_DATA", PinType::OUTPUT, width);
    else {
        for (auto p : {"ROW_ADDR", "COLUMN_ADDR"})
            s->addPin<32>(p, PinType::OUTPUT);
        s->addPinDynamic("DATA", PinType::OUTPUT, width);
        s->addPinDynamic("MASK", PinType::OUTPUT, width / 8);
        for (auto p : {"ROW_ENABLE", "PROGRAM_ENABLE", "OUTPUT_ENABLE"})
            s->addPin(p, PinType::OUTPUT);
        for (size_t r = 0; r < c.rows(); ++r)
            s->addPin("ERASE_" + std::to_string(r), PinType::OUTPUT);
    }
}
struct Commands {
    size_t width;
    NORFlashConfig config;
    size_t due = 0, op = 0, stage = 0, address = 0;
    bool busy = false, error = false, wel = false, valid = false, program = false, erase = false,
         select = false;
    LogicValue last_clk = X;
    Bits data, mask;
    Commands(size_t w, NORFlashConfig c) : width(w), config(c), data(w, X), mask(w / 8, L) { c.validate(w); }
    void update(const IOComponent &io, size_t t) {
        if (due && t >= due) {
            due = 0;
            ++stage;
            if (op == 1) {
                valid = true;
                busy = false;
            } else if (stage == 1) {
                program = op == 2;
                erase = op == 3;
                due = t + config.phase_ticks;
            } else if (stage == 2) {
                program = false;
                erase = false;
                select = false;
                due = t + config.phase_ticks;
            } else
                busy = false;
        }
        auto clk = io.getInputValue("CLK");
        bool edge = last_clk == L && clk == H;
        last_clk = clk;
        if (!edge)
            return;
        auto cmd = io.getInputPin<3>("CMD")->getValueAsVector();
        if (!known(cmd)) {
            error = true;
            if (!busy)
                valid = false;
            return;
        }
        auto code = number(cmd);
        if (code == 0)
            return;
        if (busy) {
            error = true;
            return;
        }
        auto a = io.getInputPin<32>("WORD_ADDR")->getValueAsVector();
        auto m = io.getInputPinDynamic("WRITE_MASK")->getValueAsVector();
        if (code > 5 || (code <= 3 && (!known(a) || number(a) >= config.depth_words)) ||
            ((code == 2 || code == 3) && !wel) || (code == 2 && !known(m))) {
            error = true;
            valid = false;
            return;
        }
        error = false;
        valid = false;
        select = false;
        if (code == 4) {
            wel = true;
            return;
        }
        if (code == 5) {
            wel = false;
            return;
        }
        op = code;
        stage = 0;
        address = number(a);
        busy = true;
        select = code != 3;
        if (code == 1)
            due = t + config.phase_ticks;
        else {
            wel = false;
            data = io.getInputPinDynamic("WRITE_DATA")->getValueAsVector();
            mask = m;
            due = t + (code == 2 ? config.program_ticks : config.erase_ticks);
        }
    }
};
class FlashControl : public Primitive {
  public:
    FlashControl(std::string n, size_t w, NORFlashConfig c)
        : Primitive(std::move(n), "NORFlashCommandControl<" + std::to_string(w) + ">",
                    [=](IOComponent *s) { devicePins(s, w, c, true); }),
          state_(w, c) {}
    void evaluate(size_t t, Simulator &sim) override {
        auto previous = state_.due;
        state_.update(*this, t);
        if (state_.due && (state_.due != previous || t >= previous))
            wake(sim, state_.due);
        for (const auto &[p, v] :
             std::vector<std::pair<std::string, bool>>{{"BUSY", state_.busy},
                                                       {"ERROR", state_.error},
                                                       {"READ_VALID", state_.valid},
                                                       {"WRITE_ENABLED", state_.wel},
                                                       {"ROW_ENABLE", state_.select},
                                                       {"PROGRAM_ENABLE", state_.program},
                                                       {"OUTPUT_ENABLE", state_.valid}})
            publish(sim, t, p, v ? H : L);
        publish(sim, t, "ROW_ADDR", bits(32, state_.address / state_.config.words_per_row));
        publish(sim, t, "COLUMN_ADDR", bits(32, state_.address % state_.config.words_per_row));
        publish(sim, t, "DATA", state_.data);
        publish(sim, t, "MASK", state_.mask);
        for (size_t r = 0; r < state_.config.rows(); ++r)
            publish(sim, t, "ERASE_" + std::to_string(r),
                    state_.erase && r * state_.config.words_per_row / state_.config.block_words ==
                                        state_.address / state_.config.block_words
                        ? H
                        : L);
    }

  private:
    Commands state_;
};
class RowDirect : public Primitive {
  public:
    RowDirect(std::string n, size_t cols)
        : Primitive(std::move(n), "NORFlashRow<" + std::to_string(cols) + ">",
                    [cols](IOComponent *s) { rowPins(s, cols); }),
          values_(cols, H), initialized_(cols, 0) {}
    void evaluate(size_t t, Simulator &sim) override {
        for (size_t c = 0; c < values_.size(); ++c) {
            auto n = std::to_string(c);
            values_[c] =
                updateCell(initialized_[c], values_[c], getInputValue("WL"), getInputValue("PROGRAM_" + n),
                           getInputValue("ERASE"), getInputValue("D_" + n));
            publish(sim, t, "Q_" + n, values_[c]);
        }
    }

  private:
    Bits values_;
    std::vector<uint8_t> initialized_;
};
class BankDirect : public Primitive {
  public:
    BankDirect(std::string n, size_t w, NORFlashConfig c)
        : Primitive(std::move(n),
                    "NORFlashStorageBank<" + std::to_string(w) + "," + std::to_string(c.depth_words) + "," +
                        std::to_string(c.words_per_row) + ">",
                    [=](IOComponent *s) { bankPins(s, w, c); }),
          width_(w), config_(c), values_(w * c.depth_words, H), initialized_(w * c.depth_words, 0) {}
    void evaluate(size_t t, Simulator &sim) override {
        size_t count = 0, row = 0;
        bool valid = true;
        for (size_t r = 0; r < config_.rows(); ++r) {
            auto wl = getInputValue("WL_" + std::to_string(r));
            valid &= binary(wl);
            if (wl == H) {
                ++count;
                row = r;
            }
            for (size_t c = 0; c < config_.columns(width_); ++c) {
                auto n = std::to_string(c);
                auto index = r * config_.columns(width_) + c;
                auto &q = values_[index];
                q = updateCell(initialized_[index], q, wl, getInputValue("DRIVE_" + n),
                               getInputValue("ERASE_" + std::to_string(r)), getInputValue("DATA_" + n));
            }
        }
        for (size_t c = 0; c < config_.columns(width_); ++c)
            publish(sim, t, "COL_" + std::to_string(c),
                    valid && count == 1 ? values_[row * config_.columns(width_) + c] : X);
    }

  private:
    size_t width_;
    NORFlashConfig config_;
    Bits values_;
    std::vector<uint8_t> initialized_;
};
class FlashDirect : public Primitive, public MemoryDeviceStorage {
  public:
    const char* getTypeName() const override { return memoryDisplayType(Primitive::getTypeName()); }
    FlashDirect(std::string n, size_t w, NORFlashConfig c)
        : Primitive(std::move(n), "NORFlash<" + std::to_string(w) + ">",
                    [=](IOComponent *s) { devicePins(s, w, c); }),
          MemoryDeviceStorage(c.depth_words, w, H, c.record_history, c.storage_limits), state_(w, c) {}
    const MemoryStorage &memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t> &) override {
        throw std::logic_error("Initialize flash through WREN/PROGRAM commands");
    }
    void evaluate(size_t t, Simulator &sim) override {
        started_ = true;
        bool was_program = state_.program, was_erase = state_.erase;
        auto previous = state_.due;
        state_.update(*this, t);
        if (state_.due && (state_.due != previous || t >= previous))
            wake(sim, state_.due);
        if (state_.program && !was_program) {
            auto q = storage_.readLogic(state_.address * (state_.width / 8), state_.width / 8);
            for (size_t bit = 0; bit < state_.width; ++bit)
                if (state_.mask[bit / 8] == H)
                    q[bit] = programBit(q[bit], state_.data[bit]);
            for (size_t byte = 0; byte < state_.width / 8; ++byte) if (state_.mask[byte] == H)
                storage_.writeLogic(t, state_.address * (state_.width / 8) + byte,
                                    Bits(q.begin() + byte * 8, q.begin() + (byte + 1) * 8), true);
        }
        if (state_.erase && !was_erase) {
            auto first = state_.address / state_.config.block_words * state_.config.block_words;
            // Bound temporary storage even for a device-sized erase block.
            const Bits erased(state_.width, H);
            for (size_t word = first; word < first + state_.config.block_words; ++word)
                storage_.writeLogic(t, word * (state_.width / 8), erased, true);
        }
        for (const auto &[p, v] : std::vector<std::pair<std::string, bool>>{{"BUSY", state_.busy},
                                                                            {"ERROR", state_.error},
                                                                            {"READ_VALID", state_.valid},
                                                                            {"WRITE_ENABLED", state_.wel}})
            publish(sim, t, p, v ? H : L);
        publish(sim, t, "READ_DATA",
                state_.valid ? storage_.readLogic(state_.address * (state_.width / 8), state_.width / 8)
                             : Bits(state_.width, Z));
    }

  private:
    Commands state_;
};
} // namespace
void NORFlashConfig::validate(size_t w, bool expanded) const {
    Geometry::validate(w, expanded);
    if (!block_words || block_words > depth_words || block_words % words_per_row || depth_words % block_words)
        throw std::invalid_argument("Flash erase blocks must contain whole rows and divide capacity");
    if (phase_ticks < 32 || phase_ticks > 10000 || program_ticks < phase_ticks || program_ticks > 1000000 ||
        erase_ticks < program_ticks || erase_ticks > 10000000)
        throw std::invalid_argument("Invalid flash timing");
}
NORFlashConfig norFlashConfigFromParameters(const ParameterMap &p) {
    keys(p, {"depth_words", "words_per_row", "block_words", "phase_ticks", "program_ticks", "erase_ticks",
             "record_history", "paged", "max_resident_bytes", "max_history_bytes"});
    NORFlashConfig c;
    readGeometry(p, c);
    c.block_words = parameter(p, "block_words", 2);
    c.phase_ticks = parameter(p, "phase_ticks", 32);
    c.program_ticks = parameter(p, "program_ticks", 128);
    c.erase_ticks = parameter(p, "erase_ticks", 256);
    return c;
}
ParameterMap norFlashParameters(const NORFlashConfig &c) {
    auto p = geometryParameters(c);
    p.insert({{"block_words", std::to_string(c.block_words)},
            {"phase_ticks", std::to_string(c.phase_ticks)},
            {"program_ticks", std::to_string(c.program_ticks)},
            {"erase_ticks", std::to_string(c.erase_ticks)}});
    return p;
}
NORFlashCell::NORFlashCell(std::string n) : Primitive(std::move(n), "NORFlashCell", cellPins) {
}
void NORFlashCell::evaluate(size_t t, Simulator &sim) {
    value_ = updateCell(initialized_, value_, getInputValue("WL"), getInputValue("PROGRAM"),
                        getInputValue("ERASE"), getInputValue("D"));
    publish(sim, t, "Q", value_);
}
NORFlashRow::NORFlashRow(std::string n, size_t c)
    : IOComponent(std::move(n), [c](IOComponent *s) { rowPins(s, c); }), columns_(c),
      type_("NORFlashRow<" + std::to_string(c) + ">") {
}
void NORFlashRow::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    for (size_t c = 0; c < columns_; ++c) {
        auto n = std::to_string(c);
        auto cell = child(b, *this, "memory.nor-flash.cell", "CELL_" + n);
        for (auto p : {"WL", "ERASE"})
            w.connect(getInputPin(p), cell->getInputPin(p));
        for (auto p : {"PROGRAM", "D"})
            w.connect(getInputPin(std::string(p) + "_" + n), cell->getInputPin(p));
        w.connect(cell->getOutputPin("Q"), getOutputPin("Q_" + n));
    }
    w.finish();
}
NORFlashBank::NORFlashBank(std::string n, size_t w, NORFlashConfig c)
    : IOComponent(std::move(n), [=](IOComponent *s) { bankPins(s, w, c); }), width_(w), config_(c),
      type_("NORFlashStorageBank<" + std::to_string(w) + "," + std::to_string(c.depth_words) + "," +
            std::to_string(c.words_per_row) + ">") {
    c.validate(w, true);
}
void NORFlashBank::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    std::vector<std::shared_ptr<IOComponent>> rows;
    for (size_t r = 0; r < config_.rows(); ++r) {
        auto n = std::to_string(r);
        auto row = child(b, *this, "memory.nor-flash.row", "ROW_" + n,
                         {{"columns", std::to_string(config_.columns(width_))}});
        rows.push_back(row);
        w.connect(getInputPin("WL_" + n), row->getInputPin("WL"));
        w.connect(getInputPin("ERASE_" + n), row->getInputPin("ERASE"));
        for (size_t c = 0; c < config_.columns(width_); ++c) {
            auto cn = std::to_string(c);
            w.connect(getInputPin("DRIVE_" + cn), row->getInputPin("PROGRAM_" + cn));
            w.connect(getInputPin("DATA_" + cn), row->getInputPin("D_" + cn));
        }
    }
    for (size_t c = 0; c < config_.columns(width_); ++c) {
        auto cn = std::to_string(c);
        auto col = child(b, *this, "memory.matrix.read-column", "COLUMN_" + cn,
                         {{"rows", std::to_string(config_.rows())}});
        for (size_t r = 0; r < rows.size(); ++r) {
            auto rn = std::to_string(r);
            w.connect(getInputPin("WL_" + rn), col->getInputPin("WL_" + rn));
            w.connect(rows[r]->getOutputPin("Q_" + cn), col->getInputPin("Q_" + rn));
        }
        w.connect(col->getOutputPin("OUT"), getOutputPin("COL_" + cn));
    }
    w.finish();
}
NORFlashDevice::NORFlashDevice(std::string n, size_t w, NORFlashConfig c)
    : IOComponent(std::move(n), [=](IOComponent *s) { devicePins(s, w, c); }), width_(w), config_(c),
      type_("NORFlash<" + std::to_string(w) + ">") {
    c.validate(w, true);
}
void NORFlashDevice::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto suffix = std::to_string(width_);
    auto control = child(b, *this, "memory.nor-flash.command-control." + suffix, "COMMAND_CONTROL",
                         norFlashParameters(config_));
    auto decoder = child(b, *this, "memory.matrix.row-decoder", "ROW_DECODE",
                         {{"rows", std::to_string(config_.rows())}});
    auto bank = child(b, *this, "memory.nor-flash.bank." + suffix, "STORAGE", norFlashParameters(config_));
    auto mux = child(b, *this, "memory.matrix.column-mux." + suffix, "COLUMN_SELECT",
                     {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto driver = child(b, *this, "memory.matrix.write-distributor." + suffix, "WRITE_DRIVERS",
                        {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto output = child(b, *this, "memory.matrix.output-buffer." + suffix, "OUTPUT");
    for (const auto &[pin, p] : getAllInputPins())
        w.connect(p, control->getInputPinDynamic(pin));
    w.connect(control->getOutputPin<32>("ROW_ADDR"), decoder->getInputPin<32>("ROW_ADDR"));
    w.connect(control->getOutputPin("ROW_ENABLE"), decoder->getInputPin("ENABLE"));
    w.connect(control->getOutputPin<32>("COLUMN_ADDR"), mux->getInputPin<32>("COLUMN_ADDR"));
    w.connect(control->getOutputPin<32>("COLUMN_ADDR"), driver->getInputPin<32>("COLUMN_ADDR"));
    w.connect(control->getOutputPinDynamic("DATA"), driver->getInputPinDynamic("DATA"));
    w.connect(control->getOutputPinDynamic("MASK"), driver->getInputPinDynamic("MASK"));
    w.connect(control->getOutputPin("PROGRAM_ENABLE"), driver->getInputPin("ENABLE"));
    for (size_t r = 0; r < config_.rows(); ++r) {
        auto n = std::to_string(r);
        w.connect(decoder->getOutputPin("WL_" + n), bank->getInputPin("WL_" + n));
        w.connect(control->getOutputPin("ERASE_" + n), bank->getInputPin("ERASE_" + n));
    }
    for (size_t c = 0; c < config_.columns(width_); ++c) {
        auto n = std::to_string(c);
        w.connect(bank->getOutputPin("COL_" + n), mux->getInputPin("COL_" + n));
        for (auto p : {"DRIVE_", "DATA_"})
            w.connect(driver->getOutputPin(p + n), bank->getInputPin(p + n));
    }
    w.connect(mux->getOutputPinDynamic("DATA"), output->getInputPinDynamic("DATA"));
    w.connect(control->getOutputPin("OUTPUT_ENABLE"), output->getInputPin("ENABLE"));
    w.connect(output->getOutputPinDynamic("OUT"), getOutputPinDynamic("READ_DATA"));
    for (auto p : {"BUSY", "ERROR", "READ_VALID", "WRITE_ENABLED"})
        w.connect(control->getOutputPin(p), getOutputPin(p));
    w.finish();
}
void registerNORFlashComponents(ComponentCatalog &c) {
    registerBlock(
        c, "memory.nor-flash.cell", "NORFlashCell",
        [](const ParameterMap &p) {
            keys(p, {});
            return describe(cellPins);
        },
        [](const std::string &n, const ParameterMap &, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<NORFlashCell>(ctx, n);
        },
        {}, "NORFlashCellTest", true);
    registerBlock(
        c, "memory.nor-flash.row", "NORFlashRow",
        [](const ParameterMap &p) {
            keys(p, {"columns"});
            return describe([cols = parameter(p, "columns", 4)](IOComponent *s) { rowPins(s, cols); });
        },
        [](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<NORFlashRow>(ctx, n, parameter(p, "columns", 4));
        },
        [](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<RowDirect>(ctx, n, parameter(p, "columns", 4));
        },
        "NORFlashRowTest");
    for (size_t width : {8, 16, 32}) {
        auto suffix = std::to_string(width);
        registerBlock(
            c, "memory.nor-flash.bank." + suffix, "NORFlashStorageBank<" + suffix + ">",
            [width](const ParameterMap &p) {
                return describe([=](IOComponent *s) { bankPins(s, width, norFlashConfigFromParameters(p)); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<NORFlashBank>(ctx, n, width,
                                                                  norFlashConfigFromParameters(p));
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<BankDirect>(ctx, n, width,
                                                                norFlashConfigFromParameters(p));
            },
            "NORFlashStorageBankTest");
        registerBlock(
            c, "memory.nor-flash.command-control." + suffix, "NORFlashCommandControl<" + suffix + ">",
            [width](const ParameterMap &p) {
                auto cfg = norFlashConfigFromParameters(p);
                cfg.validate(width, true);
                return describe([=](IOComponent *s) { devicePins(s, width, cfg, true); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<FlashControl>(ctx, n, width,
                                                                  norFlashConfigFromParameters(p));
            },
            {}, "NORFlashCommandControlTest", true);
        registerBlock(
            c, "memory.nor-flash.device." + suffix, "NORFlash<" + suffix + ">",
            [width](const ParameterMap &p) {
                auto cfg = norFlashConfigFromParameters(p);
                cfg.validate(width);
                return describe([=](IOComponent *s) { devicePins(s, width, cfg); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<NORFlashDevice>(ctx, n, width,
                                                                    norFlashConfigFromParameters(p));
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<FlashDirect>(ctx, n, width,
                                                                 norFlashConfigFromParameters(p));
            },
            "NORFlashTest", false,
            [width](const ParameterMap &p) {
                norFlashConfigFromParameters(p).validate(width, true);
                return true;
            });
    }
}
