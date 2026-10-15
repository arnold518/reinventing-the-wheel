#include "modules/memory/ROM.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/memory/MemoryRangeCheck.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/TimingProfile.hpp"
#include "MemoryHandshakeWiring.hpp"
#include "MemoryRestartTiming.hpp"
#include <algorithm>
#include <optional>

using namespace circuit;
using namespace circuit::memory_device;
namespace {
unsigned hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    throw std::invalid_argument("ROM image must contain only hexadecimal byte pairs");
}
void cellPins(IOComponent *s) {
    s->addPin("Q", PinType::OUTPUT);
}
void rowPins(IOComponent *s, const std::string &contents) {
    if (contents.empty() || contents.size() > 512 || contents.find_first_not_of("01") != std::string::npos)
        throw std::invalid_argument("ROM row requires 1..512 binary bits");
    for (size_t c = 0; c < contents.size(); ++c)
        s->addPin("Q_" + std::to_string(c), PinType::OUTPUT);
}
std::string rowContents(const ParameterMap &p) {
    keys(p, {"contents"});
    auto it = p.find("contents");
    return it == p.end() ? "0000" : it->second;
}
void bankPins(IOComponent *s, size_t width, const ROMDeviceConfig &c) {
    c.validate(width, true);
    for (size_t r = 0; r < c.rows(); ++r)
        s->addPin("WL_" + std::to_string(r), PinType::INPUT);
    for (size_t col = 0; col < c.columns(width); ++col)
        s->addPin("COL_" + std::to_string(col), PinType::OUTPUT);
}
void controlPins(IOComponent *s) {
    s->addPin<32>("WORD_ADDR", PinType::INPUT);
    s->addPin("CE_N", PinType::INPUT);
    s->addPin("OE_N", PinType::INPUT);
    s->addPin<32>("ROW_ADDR", PinType::OUTPUT);
    s->addPin<32>("COLUMN_ADDR", PinType::OUTPUT);
    s->addPin("ROW_ENABLE", PinType::OUTPUT);
    s->addPin("OUTPUT_ENABLE", PinType::OUTPUT);
    s->addPin("READY", PinType::OUTPUT);
}
void devicePins(IOComponent *s, size_t width) {
    s->addPin<32>("WORD_ADDR", PinType::INPUT);
    s->addPin("CE_N", PinType::INPUT);
    s->addPin("OE_N", PinType::INPUT);
    s->addPinDynamic("READ_DATA", PinType::OUTPUT, width);
    // The selected word has been sensed: READ_DATA is valid.
    s->addPin("READY", PinType::OUTPUT);
}
// Address decode and access timing as gates. A MemoryTimer holds the address
// and CE_N; the held address is split into row and column fields (wiring),
// checked against the depth (gates), and drives the decoder and column
// multiplexer. READY rises once a replica of the decoder depth and a replica
// read column have settled after the inputs became steady. OE_N only gates
// the output buffer.
class ROMAddressControl : public IOComponent {
  public:
    ROMAddressControl(std::string n, ROMDeviceConfig c)
        : IOComponent(std::move(n), controlPins), config_(std::move(c)) {
        config_.validate(8);
    }
    const char *getTypeName() const override { return "ROMAddressControl"; }
    void buildInternals(ComponentBuilder &b) override {
        memory_detail::StructuralWiring w(b);
        const auto &timing = timing::activeTimingProfile().memory;
        size_t row_bits = 0; while ((size_t{1} << row_bits) < config_.rows()) ++row_bits;
        size_t column_bits = 0; while ((size_t{1} << column_bits) < config_.words_per_row) ++column_bits;
        size_t decode_depth = 1; while ((size_t{1} << (decode_depth - 1)) < row_bits + 1) ++decode_depth;
        auto timer = b.addNewComponent<memory::MemoryTimer>("ACCESS_TIMER", 33, timing.rom_settle,
            std::vector<memory::MemoryTimerStage>{{0, decode_depth}, {timing.romColumn(config_.rows()), 1}});
        auto address = b.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(getInputPin<32>("WORD_ADDR"), address->getInputPin<32>("IN"));
        std::vector<memory_detail::StructuralWiring::PinPtr> held;
        for (size_t bit = 0; bit < 32; ++bit) {
            w.connect(address->getOutputPin("OUT_" + std::to_string(bit)), timer->getInputPin("IN_" + std::to_string(bit)));
            held.push_back(timer->getOutputPin("HELD_" + std::to_string(bit)));
        }
        w.connect(getInputPin("CE_N"), timer->getInputPin("IN_32"));
        const auto held_ce_n = timer->getOutputPin("HELD_32");
        const auto selected = w.invert("SELECTED", held_ce_n);
        w.connect(selected, timer->getInputPin("ENABLE"));
        const auto low = w.constant("LOW", false);
        auto row = b.addNewComponent<BitJoiner<32>>("ROW_FIELD");
        auto column = b.addNewComponent<BitJoiner<32>>("COLUMN_FIELD");
        for (size_t bit = 0; bit < 32; ++bit) {
            w.connect(bit + column_bits < 32 ? held[bit + column_bits] : low, row->getInputPin("IN_" + std::to_string(bit)));
            w.connect(bit < column_bits ? held[bit] : low, column->getInputPin("IN_" + std::to_string(bit)));
        }
        w.connect(row->getOutputPin<32>("OUT"), getOutputPin<32>("ROW_ADDR"));
        w.connect(column->getOutputPin<32>("OUT"), getOutputPin<32>("COLUMN_ADDR"));
        const auto valid = memory_detail::addressBelow(w, held, config_.depth_words);
        w.connect(w.both("ROW_ON", selected, valid), getOutputPin("ROW_ENABLE"));
        // An address beyond the depth selects no row, so the read columns
        // give X: the output is enabled by the chip and output selects only.
        w.connect(w.both("OUTPUT_ON", selected, w.invert("OUTPUT_SELECTED", getInputPin("OE_N"))),
                  getOutputPin("OUTPUT_ENABLE"));
        w.connect(timer->getOutputPin("STAGE_2"), getOutputPin("READY"));
        w.finish();
    }

  private:
    ROMDeviceConfig config_;
};
class ROMCellBehavioral : public Primitive {
  public:
    ROMCellBehavioral(std::string n, bool v) : Primitive(std::move(n), "ROMCell", cellPins), value_(v ? H : L) {}
    void evaluate(size_t t, Simulator &sim) override { publish(sim, t, "Q", value_); }

  private:
    LogicValue value_;
};
class ROMRowBehavioral : public Primitive {
  public:
    ROMRowBehavioral(std::string n, std::string contents)
        : Primitive(std::move(n), "ROMRow<" + std::to_string(contents.size()) + ">",
                    [contents](IOComponent *s) { rowPins(s, contents); }),
          contents_(std::move(contents)) {}
    void evaluate(size_t t, Simulator &sim) override {
        for (size_t c = 0; c < contents_.size(); ++c)
            publish(sim, t, "Q_" + std::to_string(c), contents_[c] == '1' ? H : L);
    }

  private:
    std::string contents_;
};
class ROMBankBehavioral : public Primitive {
  public:
    ROMBankBehavioral(std::string n, size_t width, ROMDeviceConfig c)
        : Primitive(std::move(n),
                    "ROMStorageBank<" + std::to_string(width) + "," + std::to_string(c.depth_words) + "," +
                        std::to_string(c.words_per_row) + ">",
                    [=](IOComponent *s) { bankPins(s, width, c); }),
          width_(width), config_(c), image_(c.image(width)) {}
    void evaluate(size_t t, Simulator &sim) override {
        size_t count = 0, row = 0;
        bool valid = true;
        for (size_t r = 0; r < config_.rows(); ++r) {
            auto v = getInputValue("WL_" + std::to_string(r));
            valid &= binary(v);
            if (v == H) {
                ++count;
                row = r;
            }
        }
        for (size_t c = 0; c < config_.columns(width_); ++c) {
            size_t bit = row * config_.columns(width_) + c;
            publish(sim, t, "COL_" + std::to_string(c),
                    valid && count == 1 ? ((image_[bit / 8] >> (bit % 8)) & 1 ? H : L) : X);
        }
    }

  private:
    size_t width_;
    ROMDeviceConfig config_;
    std::vector<uint8_t> image_;
};
class ROMBehavioral : public Primitive, public MemoryDeviceStorage {
  public:
    const char* getTypeName() const override { return Primitive::getTypeName(); }
    ROMBehavioral(std::string n, size_t width, ROMDeviceConfig c)
        : Primitive(std::move(n), "ROM<" + std::to_string(width) + ">",
                    [width](IOComponent *s) { devicePins(s, width); }),
          MemoryDeviceStorage(c.depth_words, width, L, c.record_history, c.storage_limits),
          width_(width), config_(checked(c, width)) {
        if (!c.image_hex.empty()) {
            std::vector<uint8_t> image(c.image_hex.size() / 2);
            for (size_t i = 0; i < image.size(); ++i)
                image[i] = (hex(c.image_hex[2 * i]) << 4) | hex(c.image_hex[2 * i + 1]);
            storage_.loadBytes(0, image);
        }
    }
    const MemoryStorage &memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t> &) override {
        throw std::logic_error("ROM contents are immutable; supply image_hex at construction");
    }
    PrimitiveTiming primitiveTiming() const override { return PrimitiveTiming::SelfTimed; }
    void evaluate(size_t t, Simulator &sim) override {
        auto ce = getInputValue("CE_N"), oe = getInputValue("OE_N");
        auto a = read("WORD_ADDR");
        // READY drops on a change of address or CE_N and rises once the access
        // has settled; the word follows its own table delays. Both are applied
        // at their times (TimedOutput), as the gate-level ROM shows them.
        std::vector<std::string> changed;
        if (!started_ || a != address_) changed.push_back("WORD_ADDR");
        if (!started_ || ce != ce_) changed.push_back("CE_N");
        if (!started_ || oe != oe_) changed.push_back("OE_N");
        if (!changed.empty()) {
            const bool restart = !started_ || a != address_ || ce != ce_;
            address_ = a; ce_ = ce; oe_ = oe;
            const auto &timing = timing::activeTimingProfile().memory;
            const size_t gate = timing::leafDelay(timing::Leaf::And);
            const size_t full = timing.rom_settle + timing.romColumn(config_.rows()) + 8 * gate;
            if (restart) {
                auto bounds = memory_detail::arcBounds(getDelays(), changed, "READY", {gate, full});
                bounds.minimum = memory_detail::safeEarliest(getDelays(), changed, "READY", gate);
                ready_.clear();
                ready_.at(t + bounds.minimum, {L});
                ready_.at(t + bounds.maximum, {ce == H ? L : ce == L ? H : X});
            }
            Bits out(width_, Z);
            if (ce != H && oe != H)
                out = ce == L && oe == L && known(a) && number(a) < config_.depth_words
                          ? storage_.readLogic(number(a) * (width_ / 8), width_ / 8)
                          : Bits(width_, X);
            const auto data = memory_detail::arcBounds(getDelays(), changed, "READ_DATA", {1, full});
            data_.clear();
            data_.at(t + data.maximum, out);
        }
        started_ = true;
        ready_.apply(sim, getOutputPin("READY"), t);
        applyData(sim, t);
        for (const auto when : {ready_.next(), data_.next()})
            if (when && *when > t) wake(sim, *when);
    }

  private:
    static ROMDeviceConfig checked(ROMDeviceConfig c, size_t width) {
        c.validate(width);
        return c;
    }
    size_t width_;
    ROMDeviceConfig config_;
    Bits address_;
    LogicValue ce_ = X, oe_ = X;
    memory_detail::TimedOutput<1> ready_{{L}};
    struct DynamicData {
        std::vector<std::pair<size_t, Bits>> future;
        Bits current;
        bool published = false;
        void clear() { future.clear(); }
        void at(size_t time, Bits value) { future.push_back({time, std::move(value)}); }
        std::optional<size_t> next() const {
            std::optional<size_t> e;
            for (const auto &[time, v] : future) if (!e || time < *e) e = time;
            return e;
        }
    } data_;
    void applyData(Simulator &sim, size_t t) {
        std::stable_sort(data_.future.begin(), data_.future.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        auto value = data_.current;
        size_t taken = 0;
        while (taken < data_.future.size() && data_.future[taken].first <= t) value = data_.future[taken++].second;
        data_.future.erase(data_.future.begin(), data_.future.begin() + taken);
        if ((value == data_.current && data_.published) || value.empty()) return;
        data_.current = value; data_.published = true;
        // The word goes out through the ordinary path with no further delay.
        auto pin = getOutputPinDynamic("READ_DATA");
        pin->setValueFromVector(value);
        if (auto wire = pin->getExternalWireBase()) sim.scheduleEvent(makeWireUpdateEvent(t, wire, value));
        else sim.recordPinChange(t, pin, value);
    }
};
} // namespace
void ROMDeviceConfig::validate(size_t width, bool expanded) const {
    Geometry::validate(width, expanded);
    if (image_hex.size() % 2 || image_hex.size() > depth_words * (width / 8) * 2)
        throw std::invalid_argument("ROM image exceeds capacity or has an incomplete byte");
    for (char c : image_hex)
        (void)hex(c);
}
std::vector<uint8_t> ROMDeviceConfig::image(size_t width) const {
    validate(width);
    std::vector<uint8_t> out(depth_words * (width / 8), 0);
    for (size_t i = 0; i < image_hex.size() / 2; ++i)
        out[i] = (hex(image_hex[2 * i]) << 4) | hex(image_hex[2 * i + 1]);
    return out;
}
ROMDeviceConfig romConfigFromParameters(const ParameterMap &p) {
    keys(p, {"depth_words", "words_per_row", "image_hex", "record_history", "paged", "max_resident_bytes", "max_history_bytes"});
    ROMDeviceConfig c;
    readGeometry(p, c);
    if (auto it = p.find("image_hex"); it != p.end())
        c.image_hex = it->second;
    return c;
}
ParameterMap romParameters(const ROMDeviceConfig &c) {
    auto p = geometryParameters(c);
    p.insert({{"image_hex", c.image_hex}});
    return p;
}
ROMCell::ROMCell(std::string n, bool v) : IOComponent(std::move(n), cellPins), value_(v) {
}
void ROMCell::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    w.connect(w.constant("MASK_BIT", value_), getOutputPin("Q"));
    w.finish();
}
ROMRow::ROMRow(std::string n, std::string contents)
    : IOComponent(std::move(n), [contents](IOComponent *s) { rowPins(s, contents); }),
      contents_(std::move(contents)), type_("ROMRow<" + std::to_string(contents_.size()) + ">") {
}
void ROMRow::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    for (size_t c = 0; c < contents_.size(); ++c) {
        auto n = std::to_string(c);
        auto cell =
            child(b, *this, "memory.rom.cell", "CELL_" + n, {{"value", contents_[c] == '1' ? "1" : "0"}});
        w.connect(cell->getOutputPin("Q"), getOutputPin("Q_" + n));
    }
    w.finish();
}
ROMBank::ROMBank(std::string n, size_t width, ROMDeviceConfig c)
    : IOComponent(std::move(n), [=](IOComponent *s) { bankPins(s, width, c); }), width_(width), config_(c),
      type_("ROMStorageBank<" + std::to_string(width) + "," + std::to_string(c.depth_words) + "," +
            std::to_string(c.words_per_row) + ">") {
    c.validate(width, true);
}
void ROMBank::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto image = config_.image(width_);
    std::vector<std::shared_ptr<IOComponent>> rows;
    for (size_t r = 0; r < config_.rows(); ++r) {
        std::string contents;
        for (size_t c = 0; c < config_.columns(width_); ++c) {
            auto bit = r * config_.columns(width_) + c;
            contents += ((image[bit / 8] >> (bit % 8)) & 1) ? '1' : '0';
        }
        rows.push_back(
            child(b, *this, "memory.rom.row", "ROW_" + std::to_string(r), {{"contents", contents}}));
    }
    for (size_t c = 0; c < config_.columns(width_); ++c) {
        auto n = std::to_string(c);
        auto line = child(b, *this, "memory.shared.read-column", "COLUMN_" + n,
                          {{"rows", std::to_string(config_.rows())}});
        for (size_t r = 0; r < rows.size(); ++r) {
            auto rn = std::to_string(r);
            w.connect(getInputPin("WL_" + rn), line->getInputPin("WL_" + rn));
            w.connect(rows[r]->getOutputPin("Q_" + n), line->getInputPin("Q_" + rn));
        }
        w.connect(line->getOutputPin("OUT"), getOutputPin("COL_" + n));
    }
    w.finish();
}
ROMDevice::ROMDevice(std::string n, size_t width, ROMDeviceConfig c)
    : IOComponent(std::move(n), [width](IOComponent *s) { devicePins(s, width); }), width_(width), config_(c),
      type_("ROM<" + std::to_string(width) + ">") {
    c.validate(width, true);
}
void ROMDevice::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto suffix = std::to_string(width_);
    auto control = child(b, *this, "memory.rom.address-control", "ACCESS_CONTROL",
                         {{"depth_words", std::to_string(config_.depth_words)},
                          {"words_per_row", std::to_string(config_.words_per_row)}});
    auto decoder = child(b, *this, "memory.shared.row-decoder", "ROW_DECODE",
                         {{"rows", std::to_string(config_.rows())}});
    auto bank = child(b, *this, "memory.rom.bank." + suffix, "STORAGE", romParameters(config_));
    auto mux = child(b, *this, "memory.shared.column-mux." + suffix, "COLUMN_SELECT",
                     {{"words_per_row", std::to_string(config_.words_per_row)}});
    auto output = child(b, *this, "memory.shared.output-buffer." + suffix, "OUTPUT");
    for (const auto &[pin, p] : getAllInputPins())
        w.connect(p, control->getInputPinDynamic(pin));
    w.connect(control->getOutputPin<32>("ROW_ADDR"), decoder->getInputPin<32>("ROW_ADDR"));
    w.connect(control->getOutputPin("ROW_ENABLE"), decoder->getInputPin("ENABLE"));
    w.connect(control->getOutputPin<32>("COLUMN_ADDR"), mux->getInputPin<32>("COLUMN_ADDR"));
    for (size_t r = 0; r < config_.rows(); ++r) {
        auto n = "WL_" + std::to_string(r);
        w.connect(decoder->getOutputPin(n), bank->getInputPin(n));
    }
    for (size_t c = 0; c < config_.columns(width_); ++c) {
        auto n = "COL_" + std::to_string(c);
        w.connect(bank->getOutputPin(n), mux->getInputPin(n));
    }
    w.connect(mux->getOutputPinDynamic("DATA"), output->getInputPinDynamic("DATA"));
    w.connect(control->getOutputPin("OUTPUT_ENABLE"), output->getInputPin("ENABLE"));
    w.connect(output->getOutputPinDynamic("OUT"), getOutputPinDynamic("READ_DATA"));
    w.connect(control->getOutputPin("READY"), getOutputPin("READY"));
    w.finish();
}
void registerROMComponents(ComponentCatalog &c) {
    auto value = [](const ParameterMap &p) {
        keys(p, {"value"});
        auto v = parameter(p, "value", 0);
        if (v > 1)
            throw std::invalid_argument("ROM cell value must be 0 or 1");
        return bool(v);
    };
    registerBlock(
        c, "memory.rom.cell", "ROMCell",
        [value](const ParameterMap &p) {
            value(p);
            return describe(cellPins);
        },
        [value](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<ROMCell>(ctx, n, value(p));
        },
        [value](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<ROMCellBehavioral>(ctx, n, value(p));
        },
        "ROMCellTest");
    registerBlock(
        c, "memory.rom.row", "ROMRow",
        [](const ParameterMap &p) {
            return describe([s = rowContents(p)](IOComponent *io) { rowPins(io, s); });
        },
        [](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<ROMRow>(ctx, n, rowContents(p));
        },
        [](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<ROMRowBehavioral>(ctx, n, rowContents(p));
        },
        "ROMRowTest");
    registerBlock(
        c, "memory.rom.address-control", "ROMAddressControl",
        [](const ParameterMap &p) {
            keys(p, {"depth_words", "words_per_row"});
            auto cfg = romConfigFromParameters(p);
            cfg.validate(8);
            return describe(controlPins);
        },
        [](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<ROMAddressControl>(ctx, n, romConfigFromParameters(p));
        },
        {}, "ROMAddressControlTest");
    for (size_t width : {8, 16, 32}) {
        auto suffix = std::to_string(width);
        registerBlock(
            c, "memory.rom.bank." + suffix, "ROMStorageBank<" + suffix + ">",
            [width](const ParameterMap &p) {
                return describe(
                    [width, cfg = romConfigFromParameters(p)](IOComponent *s) { bankPins(s, width, cfg); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<ROMBank>(ctx, n, width, romConfigFromParameters(p));
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<ROMBankBehavioral>(ctx, n, width, romConfigFromParameters(p));
            },
            "ROMStorageBankTest");
        registerBlock(
            c, "memory.rom.device." + suffix, "ROM<" + suffix + ">",
            [width](const ParameterMap &p) {
                romConfigFromParameters(p).validate(width);
                return describe([width](IOComponent *s) { devicePins(s, width); });
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<ROMDevice>(ctx, n, width, romConfigFromParameters(p));
            },
            [width](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<ROMBehavioral>(ctx, n, width, romConfigFromParameters(p));
            },
            "ROMTest", false,
            [width](const ParameterMap &p) {
                romConfigFromParameters(p).validate(width, true);
                return true;
            });
    }
}
