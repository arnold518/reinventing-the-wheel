#include "modules/memory/ROM.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include <algorithm>

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
}
void devicePins(IOComponent *s, size_t width) {
    s->addPin<32>("WORD_ADDR", PinType::INPUT);
    s->addPin("CE_N", PinType::INPUT);
    s->addPin("OE_N", PinType::INPUT);
    s->addPinDynamic("READ_DATA", PinType::OUTPUT, width);
}
class ROMAddressControl : public Primitive {
  public:
    ROMAddressControl(std::string n, ROMDeviceConfig c)
        : Primitive(std::move(n), "ROMAddressControl", controlPins), config_(std::move(c)) {
        config_.validate(8);
    }
    bool respondsOnlyToInputs() const override { return true; }
    // The enables do not enter the row and column numbers, and the output
    // enable does not select a row.
    bool reaches(const std::string &input, const std::string &output) const override {
        if (input == "WORD_ADDR") return true;
        return output == "OUTPUT_ENABLE" || (input == "CE_N" && output == "ROW_ENABLE");
    }
    void evaluate(size_t t, Simulator &sim) override {
        auto a = read("WORD_ADDR");
        bool valid = known(a) && number(a) < config_.depth_words;
        auto ce = getInputValue("CE_N"), oe = getInputValue("OE_N");
        publish(sim, t, "ROW_ADDR", valid ? bits(32, number(a) / config_.words_per_row) : Bits(32, X));
        publish(sim, t, "COLUMN_ADDR", valid ? bits(32, number(a) % config_.words_per_row) : Bits(32, X));
        publish(sim, t, "ROW_ENABLE", ce == L && valid ? H : L);
        publish(sim, t, "OUTPUT_ENABLE", ce == H || oe == H ? L : ce == L && oe == L && valid ? H : X);
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
    const char* getTypeName() const override { return memoryDisplayType(Primitive::getTypeName()); }
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
    void evaluate(size_t t, Simulator &sim) override {
        started_ = true;
        auto ce = getInputValue("CE_N"), oe = getInputValue("OE_N");
        auto a = read("WORD_ADDR");
        Bits out(width_, Z);
        if (ce != H && oe != H)
            out = ce == L && oe == L && known(a) && number(a) < config_.depth_words
                      ? storage_.readLogic(number(a) * (width_ / 8), width_ / 8)
                      : Bits(width_, X);
        publish(sim, t, "READ_DATA", out);
    }

  private:
    static ROMDeviceConfig checked(ROMDeviceConfig c, size_t width) {
        c.validate(width);
        return c;
    }
    size_t width_;
    ROMDeviceConfig config_;
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
        {}, "ROMAddressControlTest", true);
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
