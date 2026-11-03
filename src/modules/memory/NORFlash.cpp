#include "modules/memory/NORFlash.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/memory/MemoryRangeCheck.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/storage/Register.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/TimingProfile.hpp"
#include "MemoryHandshakeWiring.hpp"
#include <algorithm>
#include "NORSyncControl.hpp"
#include <bit>
#include <tuple>
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
// The NOR flash command control as gates, synchronous on the device clock
// like the DRAM command control: commands, address and data are latched on
// a rising edge; BUSY, ERROR, READ_VALID and WRITE_ENABLED are state bits.
// READ waits for a replica of the read column; PROGRAM and ERASE run a pulse
// interval and two recovery steps; the end of each chain is sampled back
// into the clock, and BUSY is released one edge after completion. WREN and
// WRDI act on the edge alone. The block-erase row decoder is gates.
class FlashControl : public IOComponent {
  public:
    FlashControl(std::string n, size_t w, NORFlashConfig c)
        : IOComponent(std::move(n), [=](IOComponent *s) { devicePins(s, w, c, true); }), width_(w), config_(c),
          type_("NORFlashCommandControl<" + std::to_string(w) + ">") {
        if (!std::has_single_bit(c.block_words) || !std::has_single_bit(c.words_per_row))
            throw std::invalid_argument("The gate-level flash control needs power-of-two erase blocks and rows");
    }
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &b) override {
        using PinPtr = memory_detail::StructuralWiring::PinPtr;
        memory_detail::StructuralWiring w(b);
        const auto &memory = timing::activeTimingProfile().memory;
        const SimulationTime slowest = std::max({timing::leafDelay(timing::Leaf::Not), timing::leafDelay(timing::Leaf::Nand),
                                         timing::leafDelay(timing::Leaf::And), timing::leafDelay(timing::Leaf::Or)});
        const SimulationTime gate = timing::leafDelay(timing::Leaf::And);
        auto por = b.addNewComponent<memory::PowerOnReset>("POWER_ON_RESET", 16 * slowest);
        const auto reset = por->getOutputPin("OUT");
        auto state = [&](const std::string &name) {
            auto bit = b.add(families::ClockedStateBit, name);
            w.connect(getInputPin("CLK"), bit->getInputPin("CLK"));
            w.connect(reset, bit->getInputPin("RST"));
            return bit;
        };
        auto reg = [&](const std::string &name, size_t bits) {
            auto r = b.add(families::registerWord(bits), name);
            w.connect(getInputPin("CLK"), r->getInputPin("CLK"));
            w.connect(reset, r->getInputPin("RST"));
            return r;
        };
        auto cmd = bitAdapter(b, "COMMAND_BITS", 3, true);
        w.connect(getInputPin<3>("CMD"), cmd->getInputPin<3>("IN"));
        auto known_cmd = b.addNewComponent<memory::KnownBits>("COMMAND_KNOWN", 3);
        auto known_addr = b.addNewComponent<memory::KnownBits>("ADDRESS_KNOWN", 32);
        auto known_mask = b.addNewComponent<memory::KnownBits>("MASK_KNOWN", width_ / 8);
        w.connect(getInputPin<3>("CMD"), known_cmd->getInputPinDynamic("IN"));
        w.connect(getInputPin<32>("WORD_ADDR"), known_addr->getInputPinDynamic("IN"));
        w.connect(getInputPinDynamic("WRITE_MASK"), known_mask->getInputPinDynamic("IN"));
        auto c0 = cmd->getOutputPin("OUT_0"), c1 = cmd->getOutputPin("OUT_1"), c2 = cmd->getOutputPin("OUT_2");
        auto n0 = w.invert("NOT_C0", c0), n1 = w.invert("NOT_C1", c1), n2 = w.invert("NOT_C2", c2);
        auto is_nop = w.reduce("IS_NOP", {n0, n1, n2}, true);
        auto is_read = w.reduce("IS_READ", {c0, n1, n2}, true);
        auto is_program = w.reduce("IS_PROGRAM", {n0, c1, n2}, true);
        auto is_erase = w.reduce("IS_ERASE", {c0, c1, n2}, true);
        auto is_wren = w.reduce("IS_WREN", {n0, n1, c2}, true);
        auto is_wrdi = w.reduce("IS_WRDI", {c0, n1, c2}, true);
        auto address = bitAdapter(b, "ADDRESS_BITS", 32, true);
        w.connect(getInputPin<32>("WORD_ADDR"), address->getInputPin<32>("IN"));
        std::vector<PinPtr> address_bits;
        for (size_t i = 0; i < 32; ++i) address_bits.push_back(address->getOutputPin("OUT_" + std::to_string(i)));
        auto addr_ok = w.both("ADDRESS_OK", known_addr->getOutputPin("KNOWN"),
                              memory_detail::addressBelow(w, address_bits, config_.depth_words, "RANGE"));
        auto busy = state("BUSY"), error = state("ERROR"), valid = state("READ_VALID"), wel = state("WRITE_ENABLED"),
             releasing = state("RELEASING"), fresh = state("FRESH"), repeat = state("REPEAT"), c_read = state("COMMAND_READ"), c_program = state("COMMAND_PROGRAM"),
             c_erase = state("COMMAND_ERASE");
        const auto B = busy->getOutputPin("Q"), WEL = wel->getOutputPin("Q"), R = releasing->getOutputPin("Q");
        auto legal = w.reduce("LEGAL_COMMAND", {
            w.both("LEGAL_READ", is_read, addr_ok),
            w.reduce("LEGAL_PROGRAM", {is_program, addr_ok, WEL, known_mask->getOutputPin("KNOWN")}, true),
            w.reduce("LEGAL_ERASE", {is_erase, addr_ok, WEL}, true), is_wren, is_wrdi}, false);
        const auto cmd_known = known_cmd->getOutputPin("KNOWN");
        auto issued = w.both("ISSUED", cmd_known, w.invert("NOT_NOP", is_nop));
        // Commands are taken while idle or in the releasing cycle (DRAM.cpp).
        auto idle = w.either("IDLE", w.invert("NOT_BUSY", B), R);
        auto accept = w.reduce("ACCEPT", {issued, legal, idle}, true);
        auto unknown = w.invert("UNKNOWN_COMMAND", cmd_known);
        auto illegal = w.invert("ILLEGAL", legal);
        auto not_idle = w.invert("NOT_IDLE", idle);
        auto refused = w.both("REFUSED", issued, w.either("BUSY_OR_ILLEGAL", not_idle, illegal));
        auto unknown_idle = w.both("UNKNOWN_IDLE", unknown, idle);
        auto not_legal = w.invert("NOT_LEGAL", legal);
        auto issued_idle = w.both("ISSUED_IDLE", issued, idle);
        auto clear_valid = w.reduce("CLEAR_VALID", {accept, unknown_idle, w.both("ILLEGAL_IDLE", issued_idle, not_legal)}, false);
        auto starts = w.reduce("STARTS", {is_read, is_program, is_erase}, false);
        auto start = w.both("START", accept, starts);
        // A repeated command taken in the releasing cycle restarts its chain (DRAM.cpp).
        w.connect(w.reduce("NEXT_REPEAT", {start, B, w.reduce("SAME_COMMAND", {
            w.both("SAME_READ", c_read->getOutputPin("Q"), is_read), w.both("SAME_PROGRAM", c_program->getOutputPin("Q"), is_program),
            w.both("SAME_ERASE", c_erase->getOutputPin("Q"), is_erase)}, false)}, true), repeat->getInputPin("D"));
        const auto restarting = w.invert("NOT_REPEAT", repeat->getOutputPin("Q"));
        auto run = [&](const std::shared_ptr<IOComponent> &command, const std::string &name) {
            return w.reduce("RUN_" + name, {command->getOutputPin("Q"), B, restarting}, true);
        };
        auto r_read = run(c_read, "READ"), r_program = run(c_program, "PROGRAM"), r_erase = run(c_erase, "ERASE");
        auto chain = [&](const std::string &name, PinPtr go, std::vector<std::pair<SimulationTime, size_t>> steps) {
            std::vector<PinPtr> stages;
            auto previous = go;
            for (size_t k = 0; k < steps.size(); ++k) {
                const auto n = name + "_" + std::to_string(k + 1);
                auto line = b.addNewComponent<memory::MemoryDelayLine>("STEP_" + n, steps[k].first, gate);
                w.connect(previous, line->getInputPin("IN"));
                previous = line->getOutputPin("OUT");
                for (size_t g = 0; g < steps[k].second; ++g) previous = w.both("STAGE_" + n + "_" + std::to_string(g), previous, go);
                stages.push_back(previous);
            }
            return stages;
        };
        const size_t rows = config_.rows();
        // A read lasts the read column's sensing plus a replica of the decoder.
        size_t row_bits = 0; while ((size_t{1} << row_bits) < rows) ++row_bits;
        size_t decode_depth = 1; while ((size_t{1} << (decode_depth - 1)) < row_bits + 1) ++decode_depth;
        auto rd = chain("READ", r_read, {{memory.norRead(rows), decode_depth + 1}});
        auto pg = chain("PROGRAM", r_program, {{config_.program_ticks, 1}, {memory.nor_pulse_end, 1}, {memory.nor_recovery, 1}});
        auto er = chain("ERASE", r_erase, {{config_.erase_ticks, 1}, {memory.nor_pulse_end, 1}, {memory.nor_recovery, 1}});
        auto done_async = w.reduce("CHAIN_DONE", {rd.back(), pg.back(), er.back()}, false);
        auto sampler = b.addNewComponent<memory::Synchronizer>("DONE_SAMPLER", 1);
        w.connect(getInputPin("CLK"), sampler->getInputPin("CLK"));
        w.connect(reset, sampler->getInputPin("RST"));
        w.connect(done_async, sampler->getInputPin("D"));
        auto complete = w.reduce("COMPLETE", {B, sampler->getOutputPin("Q"), w.invert("NOT_RELEASING", R),
                                              w.invert("NOT_FRESH", fresh->getOutputPin("Q"))}, true);
        w.connect(accept, fresh->getInputPin("D"));
        auto keep = [&](const std::shared_ptr<IOComponent> &bit, PinPtr set, PinPtr clear, const std::string &name) {
            w.connect(w.either("NEXT_" + name, set, w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NO_CLEAR_" + name, clear))),
                      bit->getInputPin("D"));
        };
        keep(error, w.either("ERROR_EVENT", unknown, refused), accept, "ERROR");
        {
            auto source = w.either("VALID_SOURCE", valid->getOutputPin("Q"), w.both("READ_DONE", complete, c_read->getOutputPin("Q")));
            w.connect(w.both("NEXT_VALID", w.invert("KEEP_VALID", clear_valid), source), valid->getInputPin("D"));
        }
        keep(busy, start, R, "BUSY");
        keep(releasing, complete, R, "RELEASING");
        {
            auto disable = w.both("DISABLE_WRITES", accept, w.reduce("WRITES_END", {is_wrdi, is_program, is_erase}, false));
            auto enable = w.both("ENABLE_WRITES", accept, is_wren);
            keep(wel, enable, disable, "WEL");
        }
        for (auto [bit, decoded, name] : std::vector<std::tuple<std::shared_ptr<IOComponent>, PinPtr, std::string>>{
                 {c_read, is_read, "C_READ"}, {c_program, is_program, "C_PROGRAM"}, {c_erase, is_erase, "C_ERASE"}})
        {
            auto hold = w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NOT_START_" + name, start));
            auto load = w.both("LOAD_" + name, start, decoded);
            w.connect(w.either("NEXT_" + name, load, hold), bit->getInputPin("D"));
        }
        auto addr = reg("ADDRESS", 32), data = reg("DATA", width_), mask = reg("MASK", width_ / 8);
        w.connect(getInputPin<32>("WORD_ADDR"), addr->getInputPin<32>("D"));
        w.connect(start, addr->getInputPin("WE"));
        w.connect(getInputPinDynamic("WRITE_DATA"), data->getInputPinDynamic("D"));
        w.connect(getInputPinDynamic("WRITE_MASK"), mask->getInputPinDynamic("D"));
        auto load_program = w.both("LOAD_PROGRAM_DATA", accept, is_program);
        w.connect(load_program, data->getInputPin("WE"));
        w.connect(load_program, mask->getInputPin("WE"));
        auto held = bitAdapter(b, "HELD_ADDRESS", 32, true);
        w.connect(addr->getOutputPin<32>("Q"), held->getInputPin<32>("IN"));
        std::vector<PinPtr> a;
        for (size_t i = 0; i < 32; ++i) a.push_back(held->getOutputPin("OUT_" + std::to_string(i)));
        const size_t column_bits = std::countr_zero(config_.words_per_row), block_bits = std::countr_zero(config_.block_words);
        const auto low = w.constant("LOW", false);
        auto row_field = bitAdapter(b, "ROW_FIELD", 32, false), column_field = bitAdapter(b, "COLUMN_FIELD", 32, false);
        for (size_t i = 0; i < 32; ++i) {
            w.connect(i + column_bits < 32 ? a[i + column_bits] : low, row_field->getInputPin("IN_" + std::to_string(i)));
            w.connect(i < column_bits ? a[i] : low, column_field->getInputPin("IN_" + std::to_string(i)));
        }
        w.connect(row_field->getOutputPinDynamic("OUT"), getOutputPin<32>("ROW_ADDR"));
        w.connect(column_field->getOutputPinDynamic("OUT"), getOutputPin<32>("COLUMN_ADDR"));
        w.connect(data->getOutputPinDynamic("Q"), getOutputPinDynamic("DATA"));
        w.connect(mask->getOutputPinDynamic("Q"), getOutputPinDynamic("MASK"));
        // The row stays selected after a read (its word is shown) and during a
        // program pulse; an erase selects rows through the block decoder.
        w.connect(w.either("SELECT", c_read->getOutputPin("Q"), w.both("PROGRAM_SELECT", r_program, w.invert("PROGRAM_PULSE_OVER", pg[1]))),
                  getOutputPin("ROW_ENABLE"));
        {
            auto program_off = w.invert("PROGRAM_OFF", pg[1]);
            auto program_on = w.both("PROGRAM_ON", r_program, pg[0]);
            w.connect(w.both("PROGRAM_PULSE", program_on, program_off), getOutputPin("PROGRAM_ENABLE"));
        }
        auto erase_off = w.invert("ERASE_OFF", er[1]);
        auto erase_on = w.both("ERASE_ON", r_erase, er[0]);
        auto erase_pulse = w.both("ERASE_PULSE", erase_on, erase_off);
        const size_t depth_bits = std::bit_width(config_.depth_words - 1);
        for (size_t r = 0; r < rows; ++r) {
            const size_t block = r * config_.words_per_row / config_.block_words;
            std::vector<PinPtr> match{erase_pulse};
            for (size_t i = block_bits; i < std::max(depth_bits, block_bits); ++i) {
                const bool one = (block >> (i - block_bits)) & 1;
                match.push_back(one ? a[i] : w.invert("BLOCK_" + std::to_string(r) + "_NOT_" + std::to_string(i), a[i]));
            }
            w.connect(w.reduce("ERASE_ROW_" + std::to_string(r), match, true), getOutputPin("ERASE_" + std::to_string(r)));
        }
        w.connect(w.both("BUSY_SHOWN", B, w.invert("NOT_RELEASING_SHOWN", R)), getOutputPin("BUSY"));
        w.connect(error->getOutputPin("Q"), getOutputPin("ERROR"));
        w.connect(valid->getOutputPin("Q"), {getOutputPin("READ_VALID"), getOutputPin("OUTPUT_ENABLE")});
        w.connect(WEL, getOutputPin("WRITE_ENABLED"));
        w.finish();
    }

  private:
    size_t width_;
    NORFlashConfig config_;
    std::string type_;
};
class RowBehavioral : public Primitive {
  public:
    RowBehavioral(std::string n, size_t cols)
        : Primitive(std::move(n), "NORFlashRow<" + std::to_string(cols) + ">",
                    [cols](IOComponent *s) { rowPins(s, cols); }),
          values_(cols, H), initialized_(cols, 0) {}
    void evaluate(SimulationTime t, Simulator &sim) override {
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
class BankBehavioral : public Primitive {
  public:
    BankBehavioral(std::string n, size_t w, NORFlashConfig c)
        : Primitive(std::move(n),
                    "NORFlashStorageBank<" + std::to_string(w) + "," + std::to_string(c.depth_words) + "," +
                        std::to_string(c.words_per_row) + ">",
                    [=](IOComponent *s) { bankPins(s, w, c); }),
          width_(w), config_(c), values_(w * c.depth_words, H), initialized_(w * c.depth_words, 0) {}
    void evaluate(SimulationTime t, Simulator &sim) override {
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
// The NOR flash device as one part: the gate-level command control cycle
// for cycle (NORSyncControl) and the cells as plain storage. A program pulse
// clears the masked bits, an erase pulse sets its block to ones.
class FlashBehavioral : public Primitive, public MemoryDeviceStorage {
  public:
    const char* getTypeName() const override { return Primitive::getTypeName(); }
    FlashBehavioral(std::string n, size_t w, NORFlashConfig c)
        : Primitive(std::move(n), "NORFlash<" + std::to_string(w) + ">",
                    [=](IOComponent *s) { devicePins(s, w, c); }),
          MemoryDeviceStorage(c.depth_words, w, H, c.record_history, c.storage_limits), width_(w), config_(c),
          control_(w, c.depth_words, c.rows(), c.program_ticks, c.erase_ticks) {}
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::SelfTimed; }
    const MemoryStorage &memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t> &) override {
        throw std::logic_error("Initialize flash through WREN/PROGRAM commands");
    }
    void evaluate(SimulationTime t, Simulator &sim) override {
        started_ = true;
        const auto clk = getInputValue("CLK");
        if (previous_clock_ == L && clk == H)
            control_.edge(t, read("CMD"), read("WORD_ADDR"), read("WRITE_DATA"), read("WRITE_MASK"));
        previous_clock_ = clk;
        const bool program = control_.programPulse(t), erase = control_.erasePulse(t);
        if (program && !program_) {
            auto q = storage_.readLogic(control_.address * (width_ / 8), width_ / 8);
            for (size_t bit = 0; bit < width_; ++bit)
                if (control_.mask[bit / 8] == H) q[bit] = programBit(q[bit], control_.data[bit]);
            for (size_t byte = 0; byte < width_ / 8; ++byte) if (control_.mask[byte] == H)
                storage_.writeLogic(t, control_.address * (width_ / 8) + byte,
                                    Bits(q.begin() + byte * 8, q.begin() + (byte + 1) * 8), true);
        }
        if (erase && !erase_) {
            auto first = control_.address / config_.block_words * config_.block_words;
            const Bits erased(width_, H);
            for (size_t word = first; word < first + config_.block_words; ++word)
                storage_.writeLogic(t, word * (width_ / 8), erased, true);
        }
        program_ = program; erase_ = erase;
        if (const auto next = control_.nextStep(t); next > t) wake(sim, next);
        for (const auto &[p, v] : std::vector<std::pair<std::string, bool>>{{"BUSY", control_.busyShown()},
                                                                            {"ERROR", control_.error},
                                                                            {"READ_VALID", control_.valid},
                                                                            {"WRITE_ENABLED", control_.wel}})
            publish(sim, t, p, v ? H : L);
        publish(sim, t, "READ_DATA",
                control_.valid ? storage_.readLogic(control_.address * (width_ / 8), width_ / 8) : Bits(width_, Z));
    }

  private:
    size_t width_;
    NORFlashConfig config_;
    memory_detail::NORSyncControl control_;
    LogicValue previous_clock_ = X;
    bool program_ = false, erase_ = false;
};
} // namespace
void NORFlashConfig::validate(size_t w, bool expanded) const {
    Geometry::validate(w, expanded);
    if (!block_words || block_words > depth_words || block_words % words_per_row || depth_words % block_words)
        throw std::invalid_argument("Flash erase blocks must contain whole rows and divide capacity");
    // Program and erase up to 1 s and 10 s at 1 tick = 1 ps (literal datasheet times).
    if (phase_ticks < 32 || phase_ticks > 10000 || program_ticks < phase_ticks || program_ticks > 1'000'000'000'000ull ||
        erase_ticks < program_ticks || erase_ticks > 10'000'000'000'000ull)
        throw std::invalid_argument("Invalid flash timing");
}
NORFlashConfig norFlashConfigFromParameters(const ParameterMap &p) {
    keys(p, {"depth_words", "words_per_row", "block_words", "phase_ticks", "program_ticks", "erase_ticks",
             "record_history", "paged", "max_resident_bytes", "max_history_bytes"});
    NORFlashConfig c;
    readGeometry(p, c);
    c.block_words = parameter(p, "block_words", 2);
    c.phase_ticks = timeParameter(p, "phase_ticks", 32);
    c.program_ticks = timeParameter(p, "program_ticks", 128);
    c.erase_ticks = timeParameter(p, "erase_ticks", 256);
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
void NORFlashCell::evaluate(SimulationTime t, Simulator &sim) {
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
        auto col = child(b, *this, "memory.shared.read-column", "COLUMN_" + cn,
                         {{"rows", std::to_string(config_.rows())}, {"technology", "nor-flash"}});
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
    auto decoder = child(b, *this, "memory.shared.row-decoder", "ROW_DECODE",
                         {{"rows", std::to_string(config_.rows())}});
    auto bank = child(b, *this, "memory.nor-flash.bank." + suffix, "STORAGE", norFlashParameters(config_));
    auto mux = child(b, *this, "memory.shared.column-mux." + suffix, "COLUMN_SELECT",
                     {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto driver = child(b, *this, "memory.shared.write-distributor." + suffix, "WRITE_DRIVERS",
                        {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto output = child(b, *this, "memory.shared.output-buffer." + suffix, "OUTPUT");
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
            return Component::createWithContext<RowBehavioral>(ctx, n, parameter(p, "columns", 4));
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
                return Component::createWithContext<BankBehavioral>(ctx, n, width,
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
            {}, "NORFlashCommandControlTest");
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
                return Component::createWithContext<FlashBehavioral>(ctx, n, width,
                                                                 norFlashConfigFromParameters(p));
            },
            "NORFlashTest", false,
            [width](const ParameterMap &p) {
                norFlashConfigFromParameters(p).validate(width, true);
                return true;
            });
    }
}
