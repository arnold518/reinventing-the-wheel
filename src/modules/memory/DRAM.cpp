#include "modules/memory/DRAM.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/memory/MemoryRangeCheck.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/storage/Register.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/basic/Gate.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/TimingProfile.hpp"
#include "MemoryHandshakeWiring.hpp"
#include "DRAMSyncControl.hpp"
#include <tuple>
#include <algorithm>

using namespace circuit;
using namespace circuit::memory_device;
namespace {
void retentionValid(size_t t) {
    // Up to 1 s at 1 tick = 1 ps (realistic retention is 64 ms).
    if (t < 1024 || t > 1'000'000'000'000ull)
        throw std::invalid_argument("retention_ticks must be 1024..10^12");
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
// The command control of the DRAM device as gates. Synchronous on the
// device clock: commands, addresses and data are latched on a rising edge;
// BUSY, ERROR, READ_VALID and ROW_OPEN are state bits. The analog steps of a
// command run on delay-line chains started by the accepted command; the end
// of a chain is sampled back into the clock by a one-stage resolving
// sampler. Completion updates ROW_OPEN, READ_VALID and the refresh row on
// that edge and releases BUSY one edge later, so wordline and sense never
// drop between a finishing ACTIVATE and the open row taking over.
class DRAMCommandControl : public IOComponent {
  public:
    DRAMCommandControl(std::string n, size_t width, DRAMDeviceConfig c)
        : IOComponent(std::move(n), [width](IOComponent *s) { devicePins(s, width, true); }), width_(width), config_(c),
          type_("DRAMCommandControl<" + std::to_string(width) + ">") {
        c.validate(width);
    }
    const char *getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder &b) override;

  private:
    size_t width_;
    DRAMDeviceConfig config_;
    std::string type_;
};
void DRAMCommandControl::buildInternals(ComponentBuilder &b) {
    using PinPtr = memory_detail::StructuralWiring::PinPtr;
    memory_detail::StructuralWiring w(b);
    const auto &memory = timing::activeTimingProfile().memory;
    const size_t slowest = std::max({timing::leafDelay(timing::Leaf::Not), timing::leafDelay(timing::Leaf::Nand),
                                     timing::leafDelay(timing::Leaf::And), timing::leafDelay(timing::Leaf::Or)});
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
    // Command decode and request checks.
    auto cmd = bitAdapter(b, "COMMAND_BITS", 3, true);
    w.connect(getInputPin<3>("CMD"), cmd->getInputPin<3>("IN"));
    auto known_cmd = b.addNewComponent<memory::KnownBits>("COMMAND_KNOWN", 3);
    auto known_addr = b.addNewComponent<memory::KnownBits>("ADDRESS_KNOWN", 32);
    auto known_mask = b.addNewComponent<memory::KnownBits>("MASK_KNOWN", width_ / 8);
    w.connect(getInputPin<3>("CMD"), known_cmd->getInputPinDynamic("IN"));
    w.connect(getInputPin<32>("ADDR"), known_addr->getInputPinDynamic("IN"));
    w.connect(getInputPinDynamic("WRITE_MASK"), known_mask->getInputPinDynamic("IN"));
    auto c0 = cmd->getOutputPin("OUT_0"), c1 = cmd->getOutputPin("OUT_1"), c2 = cmd->getOutputPin("OUT_2");
    auto n0 = w.invert("NOT_C0", c0), n1 = w.invert("NOT_C1", c1), n2 = w.invert("NOT_C2", c2);
    auto is_nop = w.reduce("IS_NOP", {n0, n1, n2}, true);
    auto is_act = w.reduce("IS_ACTIVATE", {c0, n1, n2}, true);
    auto is_rd = w.reduce("IS_READ", {n0, c1, n2}, true);
    auto is_wr = w.reduce("IS_WRITE", {c0, c1, n2}, true);
    auto is_pre = w.reduce("IS_PRECHARGE", {n0, n1, c2}, true);
    auto is_ref = w.reduce("IS_REFRESH", {c0, n1, c2}, true);
    auto address = bitAdapter(b, "ADDRESS_BITS", 32, true);
    w.connect(getInputPin<32>("ADDR"), address->getInputPin<32>("IN"));
    std::vector<PinPtr> address_bits;
    for (size_t i = 0; i < 32; ++i) address_bits.push_back(address->getOutputPin("OUT_" + std::to_string(i)));
    auto row_ok = memory_detail::addressBelow(w, address_bits, config_.rows(), "ROW_RANGE");
    auto col_ok = memory_detail::addressBelow(w, address_bits, config_.words_per_row, "COLUMN_RANGE");
    auto busy = state("BUSY"), error = state("ERROR"), valid = state("READ_VALID"), open = state("ROW_OPEN"),
         releasing = state("RELEASING");
    auto c_act = state("COMMAND_ACTIVATE"), c_rd = state("COMMAND_READ"), c_wr = state("COMMAND_WRITE"),
         c_pre = state("COMMAND_PRECHARGE"), c_ref = state("COMMAND_REFRESH");
    const auto B = busy->getOutputPin("Q"), OPEN = open->getOutputPin("Q");
    const auto addr_known = known_addr->getOutputPin("KNOWN");
    auto legal = w.reduce("LEGAL_COMMAND", {
        w.reduce("LEGAL_ACTIVATE", {is_act, addr_known, w.invert("CLOSED", OPEN), row_ok}, true),
        w.reduce("LEGAL_READ", {is_rd, addr_known, OPEN, col_ok}, true),
        w.reduce("LEGAL_WRITE", {is_wr, addr_known, known_mask->getOutputPin("KNOWN"), OPEN, col_ok}, true),
        w.both("LEGAL_PRECHARGE", is_pre, OPEN),
        w.both("LEGAL_REFRESH", is_ref, w.invert("CLOSED_FOR_REFRESH", OPEN))}, false);
    const auto cmd_known = known_cmd->getOutputPin("KNOWN");
    auto issued = w.both("ISSUED", cmd_known, w.invert("NOT_NOP", is_nop));
    auto idle = w.invert("IDLE", B);
    auto accept = w.reduce("ACCEPT", {issued, legal, idle}, true);
    auto unknown = w.invert("UNKNOWN_COMMAND", cmd_known);
    auto refused = w.both("REFUSED", issued, w.either("BUSY_OR_ILLEGAL", B, w.invert("ILLEGAL", legal)));
    auto error_event = w.either("ERROR_EVENT", unknown, refused);
    // A refusal while idle, or an unknown command while idle, withdraws the read.
    auto clear_valid = w.reduce("CLEAR_VALID", {accept, w.both("UNKNOWN_IDLE", unknown, idle),
                                                w.both("ILLEGAL_IDLE", w.both("ISSUED_IDLE", issued, idle), w.invert("NOT_LEGAL", legal))}, false);
    // Run signals and the analog step chains, one per command.
    auto run = [&](const std::shared_ptr<IOComponent> &command, const std::string &name) {
        return w.both("RUN_" + name, command->getOutputPin("Q"), B);
    };
    auto r_act = run(c_act, "ACTIVATE"), r_rd = run(c_rd, "READ"), r_wr = run(c_wr, "WRITE"),
         r_pre = run(c_pre, "PRECHARGE"), r_ref = run(c_ref, "REFRESH");
    // A step: its interval, then gates that copy a path it must outlast.
    struct Step { size_t interval, replica = 0; };
    auto chain = [&](const std::string &name, PinPtr go, std::vector<Step> steps) {
        std::vector<PinPtr> stages;
        auto previous = go;
        for (size_t k = 0; k < steps.size(); ++k) {
            const auto n = name + "_" + std::to_string(k + 1);
            auto line = b.addNewComponent<memory::MemoryDelayLine>("STEP_" + n, steps[k].interval, timing::leafDelay(timing::Leaf::And));
            w.connect(previous, line->getInputPin("IN"));
            previous = w.both("STAGE_" + n, line->getOutputPin("OUT"), go);
            for (size_t g = 0; g < steps[k].replica; ++g)
                previous = w.both("STAGE_" + n + "_REPLICA_" + std::to_string(g), previous, go);
            stages.push_back(previous);
        }
        return stages;
    };
    const size_t rows = config_.rows();
    // Sensing starts once the wordline has risen through the row decoder and
    // the cells' samples have reached the sense columns; precharge starts
    // once the wordline has fallen through the decoder (dramDecodeReplica).
    // The wordline follows the first ACTIVATE step and sensing the second, so
    // the second carries the decoder replica and the cell's delay; the
    // wordline falls after the close step and precharge follows the next.
    const auto sense_step = Step{memory.dramSense(rows) + timing::leafDelay(timing::Leaf::MemoryPrimitive), memory_detail::dramDecodeReplica(rows)};
    const auto precharge_step = Step{memory.dram_precharge, memory_detail::dramDecodeReplica(rows)};
    auto a = chain("ACTIVATE", r_act, {{memory.dram_wordline}, sense_step, {memory.dram_restore}});
    auto r = chain("READ", r_rd, {{memory.dram_read}});
    auto wv = chain("WRITE", r_wr, {{memory.dram_write_on}, {memory.dram_write_off}, {memory.dram_write_recovery}});
    auto p = chain("PRECHARGE", r_pre, {{memory.dram_close}, precharge_step, {memory.dram_equalize}});
    auto f = chain("REFRESH", r_ref, {{memory.dram_wordline}, sense_step, {memory.dram_close}, precharge_step,
                                      {memory.dram_equalize}});
    auto done_async = w.reduce("CHAIN_DONE", {a.back(), r.back(), wv.back(), p.back(), f.back()}, false);
    auto sampler = b.addNewComponent<memory::Synchronizer>("DONE_SAMPLER", 1);
    w.connect(getInputPin("CLK"), sampler->getInputPin("CLK"));
    w.connect(reset, sampler->getInputPin("RST"));
    w.connect(done_async, sampler->getInputPin("D"));
    const auto R = releasing->getOutputPin("Q");
    auto complete = w.reduce("COMPLETE", {B, sampler->getOutputPin("Q"), w.invert("NOT_RELEASING", R)}, true);
    auto keep = [&](const std::shared_ptr<IOComponent> &bit, PinPtr set, PinPtr clear, const std::string &name) {
        w.connect(w.either("NEXT_" + name, set, w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NO_CLEAR_" + name, clear))),
                  bit->getInputPin("D"));
    };
    keep(error, error_event, accept, "ERROR");
    w.connect(w.both("NEXT_VALID", w.invert("KEEP_VALID", clear_valid),
                     w.either("VALID_SOURCE", valid->getOutputPin("Q"), w.both("READ_DONE", complete, c_rd->getOutputPin("Q")))),
              valid->getInputPin("D"));
    keep(busy, accept, R, "BUSY");
    keep(releasing, complete, R, "RELEASING");
    for (auto [bit, decoded, name] : std::vector<std::tuple<std::shared_ptr<IOComponent>, PinPtr, std::string>>{
             {c_act, is_act, "C_ACTIVATE"}, {c_rd, is_rd, "C_READ"}, {c_wr, is_wr, "C_WRITE"}, {c_pre, is_pre, "C_PRECHARGE"}, {c_ref, is_ref, "C_REFRESH"}})
        w.connect(w.either("NEXT_" + name, w.both("LOAD_" + name, accept, decoded),
                           w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NOT_ACCEPT_" + name, accept))),
                  bit->getInputPin("D"));
    keep(open, w.both("OPENED", complete, c_act->getOutputPin("Q")), w.both("CLOSED_NOW", complete, c_pre->getOutputPin("Q")), "OPEN");
    // The refresh row counter: increments, wrapping at the row count, when a
    // refresh completes.
    size_t counter_bits = 1; while ((size_t{1} << counter_bits) < rows) ++counter_bits;
    counter_bits = memory_detail::supportedWidth(counter_bits);
    auto counter = reg("REFRESH_ROW", counter_bits);
    std::vector<PinPtr> q, next;
    if (counter_bits == 1) q.push_back(counter->getOutputPinDynamic("Q"));
    else {
        auto counter_q = bitAdapter(b, "REFRESH_ROW_BITS", counter_bits, true);
        w.connect(counter->getOutputPinDynamic("Q"), counter_q->getInputPinDynamic("IN"));
        for (size_t i = 0; i < counter_bits; ++i) q.push_back(counter_q->getOutputPin("OUT_" + std::to_string(i)));
    }
    PinPtr carry = w.constant("INCREMENT", true);
    std::vector<PinPtr> last;
    for (size_t i = 0; i < counter_bits; ++i) {
        next.push_back(w.binary<XORGate>("SUM_" + std::to_string(i), q[i], carry));
        carry = w.both("CARRY_" + std::to_string(i), q[i], carry);
        last.push_back(((rows - 1) >> i) & 1 ? q[i] : w.invert("ZERO_" + std::to_string(i), q[i]));
    }
    auto wrap = w.reduce("LAST_ROW", last, true);
    std::vector<PinPtr> next_row;
    for (size_t i = 0; i < counter_bits; ++i)
        next_row.push_back(w.both("NEXT_ROW_" + std::to_string(i), next[i], w.invert("NOT_WRAP_" + std::to_string(i), wrap)));
    if (counter_bits == 1) w.connect(next_row[0], counter->getInputPinDynamic("D"));
    else {
        auto next_bits = bitAdapter(b, "NEXT_REFRESH_ROW", counter_bits, false);
        for (size_t i = 0; i < counter_bits; ++i) w.connect(next_row[i], next_bits->getInputPin("IN_" + std::to_string(i)));
        w.connect(next_bits->getOutputPinDynamic("OUT"), counter->getInputPinDynamic("D"));
    }
    w.connect(w.both("REFRESHED", complete, c_ref->getOutputPin("Q")), counter->getInputPin("WE"));
    // Row, column and write registers.
    auto row = reg("ROW", 32), col = reg("COLUMN", 32), data = reg("DATA", width_), mask = reg("MASK", width_ / 8);
    auto refresh_row = bitAdapter(b, "REFRESH_ROW_ADDRESS", 32, false);
    for (size_t i = 0; i < 32; ++i)
        w.connect(i < counter_bits ? q[i] : w.constant("ROW_HIGH_" + std::to_string(i), false), refresh_row->getInputPin("IN_" + std::to_string(i)));
    w.connect(memory_detail::payloadMux(b, w, "ROW_SOURCE", getInputPin<32>("ADDR"), refresh_row->getOutputPinDynamic("OUT"), is_ref),
              row->getInputPin<32>("D"));
    w.connect(w.both("LOAD_ROW", accept, w.either("ROW_COMMAND", is_act, is_ref)), row->getInputPin("WE"));
    w.connect(getInputPin<32>("ADDR"), col->getInputPin<32>("D"));
    w.connect(w.both("LOAD_COLUMN", accept, w.either("COLUMN_COMMAND", is_rd, is_wr)), col->getInputPin("WE"));
    w.connect(getInputPinDynamic("WRITE_DATA"), data->getInputPinDynamic("D"));
    w.connect(getInputPinDynamic("WRITE_MASK"), mask->getInputPinDynamic("D"));
    auto load_write = w.both("LOAD_WRITE", accept, is_wr);
    w.connect(load_write, data->getInputPin("WE"));
    w.connect(load_write, mask->getInputPin("WE"));
    // Analog controls, from the open row and the running command's steps.
    auto pre_closing = w.both("PRECHARGE_CLOSING", r_pre, p[0]);
    auto open_held = w.both("OPEN_HELD", OPEN, w.invert("NOT_CLOSING", pre_closing));
    w.connect(w.reduce("WORDLINE", {open_held, w.both("ACTIVATE_WORDLINE", r_act, a[0]),
                                    w.both("REFRESH_WORDLINE", w.both("REFRESH_ROW_ON", r_ref, f[0]), w.invert("REFRESH_ROW_OFF", f[2]))}, false),
              getOutputPin("ROW_ENABLE"));
    w.connect(w.reduce("SENSING", {open_held, w.both("ACTIVATE_SENSE", r_act, a[1]),
                                   w.both("REFRESH_SENSE", w.both("REFRESH_SENSE_ON", r_ref, f[1]), w.invert("REFRESH_SENSE_OFF", f[2]))}, false),
              getOutputPin("SENSE"));
    auto closed_idle = w.reduce("CLOSED_IDLE", {w.invert("NOT_OPEN", OPEN), w.invert("NOT_ACTIVATING", r_act),
                                                w.invert("NOT_REFRESH_SENSING", w.both("REFRESH_BEFORE_PRECHARGE", r_ref, w.invert("BEFORE_PRECHARGE", f[3])))}, true);
    w.connect(w.either("PRECHARGING", closed_idle, w.both("PRECHARGE_STEP", r_pre, p[1])), getOutputPin("PRECHARGE"));
    w.connect(w.both("WRITE_PULSE", w.both("WRITE_ON", r_wr, wv[0]), w.invert("WRITE_OFF", wv[1])), getOutputPin("WRITE_ENABLE"));
    w.connect(B, getOutputPin("BUSY"));
    w.connect(error->getOutputPin("Q"), getOutputPin("ERROR"));
    w.connect(valid->getOutputPin("Q"), {getOutputPin("READ_VALID"), getOutputPin("OUTPUT_ENABLE")});
    w.connect(OPEN, getOutputPin("ROW_OPEN"));
    w.connect(row->getOutputPin<32>("Q"), getOutputPin<32>("ROW_ADDR"));
    w.connect(col->getOutputPin<32>("Q"), getOutputPin<32>("COLUMN_ADDR"));
    w.connect(data->getOutputPinDynamic("Q"), getOutputPinDynamic("DATA"));
    w.connect(mask->getOutputPinDynamic("Q"), getOutputPinDynamic("MASK"));
    w.finish();
}
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
// The DRAM device as one part: the gate-level command control cycle for
// cycle (DRAMSyncControl), and the row buffer and cells as plain storage.
// A rising wordline samples the row (destructively), rising sense restores
// it, a write pulse writes the latched lanes, a falling wordline closes the
// row; precharge clears the row buffer.
class DRAMBehavioral : public Primitive, public MemoryDeviceStorage {
  public:
    const char* getTypeName() const override { return Primitive::getTypeName(); }
    DRAMBehavioral(std::string n, size_t width, DRAMDeviceConfig c)
        : Primitive(std::move(n), "DRAM<" + std::to_string(width) + ">",
                    [width](IOComponent *s) { devicePins(s, width); }),
          MemoryDeviceStorage(c.depth_words, width, X, c.record_history, c.storage_limits),
          width_(width), config_(c), control_(width, c.rows(), c.words_per_row), buffer_(c.columns(width), X) {}
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::SelfTimed; }
    const MemoryStorage &memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t> &) override {
        throw std::logic_error("Initialize DRAM through ACT/WRITE commands");
    }
    void evaluate(size_t t, Simulator &sim) override {
        started_ = true;
        const auto clk = getInputValue("CLK");
        if (previous_clock_ == L && clk == H)
            control_.edge(t, read("CMD"), read("ADDR"), read("WRITE_DATA"), read("WRITE_MASK"));
        previous_clock_ = clk;
        const bool wl = control_.wordline(t), sense = control_.sensing(t), write = control_.writing(t),
                   precharge = control_.precharging(t);
        auto start = control_.row * config_.words_per_row * (width_ / 8);
        if (!wl_ && wl) {
            const auto it = closed_.find(control_.row);
            const size_t closed = it == closed_.end() ? 0 : it->second;
            buffer_ = t - closed >= config_.retention_ticks
                          ? Bits(buffer_.size(), X)
                          : storage_.readLogic(start, buffer_.size() / 8);
            // The read is destructive in the cells, but sensing always
            // restores the row before the wordline closes: the row buffer
            // carries the open row and storage keeps the row's logical
            // contents (an expired row becomes X when it is restored).
        }
        if (!sense_ && sense)
            storage_.writeLogic(t, start, buffer_);
        if (!write_ && write) {
            // Only selected byte lanes are logical writes. Sampling and
            // restoring the rest of the row above are internal DRAM activity.
            for (size_t byte = 0; byte < width_ / 8; ++byte) if (control_.mask[byte] == H) {
                Bits lane(8);
                for (size_t bit = 0; bit < 8; ++bit) {
                    const auto b = byte * 8 + bit;
                    lane[bit] = buffer_[control_.column * width_ + b] = stored(control_.data[b]);
                }
                storage_.writeLogic(t, start + control_.column * (width_ / 8) + byte, lane, true);
            }
        }
        if (wl_ && !wl) {
            const auto budget = config_.storage_limits.max_resident_bytes;
            if (!closed_.contains(control_.row) &&
                (closed_.size() + 1) > (budget - std::min(budget, storage_.allocatedDataBytes())) / 128)
                throw std::length_error("DRAM row metadata budget exceeded");
            closed_[control_.row] = t;
        }
        const auto budget = config_.storage_limits.max_resident_bytes;
        if (closed_.size() > (budget - std::min(budget, storage_.allocatedDataBytes())) / 128)
            throw std::length_error("DRAM row metadata budget exceeded");
        if (precharge) std::fill(buffer_.begin(), buffer_.end(), X);
        wl_ = wl; sense_ = sense; write_ = write;
        if (const auto next = control_.nextStep(t); next > t) wake(sim, next);
        publish(sim, t, "BUSY", control_.busy ? H : L);
        publish(sim, t, "ERROR", control_.error ? H : L);
        publish(sim, t, "READ_VALID", control_.valid ? H : L);
        publish(sim, t, "ROW_OPEN", control_.open ? H : L);
        publish(sim, t, "READ_DATA",
                control_.valid
                    ? Bits(buffer_.begin() + control_.column * width_, buffer_.begin() + (control_.column + 1) * width_)
                    : Bits(width_, Z));
    }

  private:
    size_t width_;
    DRAMDeviceConfig config_;
    memory_detail::DRAMSyncControl control_;
    std::map<size_t, size_t> closed_;
    Bits buffer_;
    LogicValue previous_clock_ = X;
    bool wl_ = false, sense_ = false, write_ = false;
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
    c.retention_ticks = parameter(p, "retention_ticks", timing::activeTimingProfile().memory.dram_device_retention);
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
// Sensing a row takes longer the more cells load the bitline.
DRAMSenseColumn::DRAMSenseColumn(std::string n, size_t rows)
    : Primitive(std::move(n), "DRAMSenseColumn<" + std::to_string(rows) + ">",
                [rows](IOComponent *s) { sensePins(s, rows); }, timing::activeTimingProfile().memory.dramSense(rows)),
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
        auto t = parameter(p, "retention_ticks", timing::activeTimingProfile().memory.dram_device_retention);
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
        return std::pair{parameter(p, "columns", 4), parameter(p, "retention_ticks", timing::activeTimingProfile().memory.dram_device_retention)};
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
            {}, "DRAMCommandControlTest");
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
