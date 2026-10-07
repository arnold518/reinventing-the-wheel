#include "modules/memory/DigitalMemory.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <algorithm>
#include <charconv>

namespace circuit::memory_device {
bool binary(LogicValue v) {
    return v == L || v == H;
}
bool known(const Bits &v) {
    return std::all_of(v.begin(), v.end(), binary);
}
LogicValue stored(LogicValue v) {
    return binary(v) ? v : X;
}
uint32_t number(const Bits &v) {
    uint32_t n = 0;
    for (size_t i = 0; i < v.size() && i < 32; ++i)
        if (v[i] == H)
            n |= uint32_t{1} << i;
    return n;
}
Bits bits(size_t width, uint32_t n) {
    Bits v(width, L);
    for (size_t i = 0; i < width; ++i)
        if ((n >> i) & 1u)
            v[i] = H;
    return v;
}
size_t parameter(const ParameterMap &p, const std::string &key, size_t fallback) {
    auto it = p.find(key);
    if (it == p.end())
        return fallback;
    size_t value = 0;
    auto r = std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
    if (r.ec != std::errc{} || r.ptr != it->second.data() + it->second.size())
        throw std::invalid_argument("Invalid memory parameter: " + key);
    return value;
}
void keys(const ParameterMap &p, std::initializer_list<const char *> allowed) {
    for (const auto &[key, value] : p)
        if (std::none_of(allowed.begin(), allowed.end(), [&](const char *s) { return key == s; }))
            throw std::invalid_argument("Unknown memory parameter: " + key);
}
void Geometry::validate(size_t width, bool expanded) const {
    if ((width != 8 && width != 16 && width != 32) || !depth_words || depth_words > UINT32_MAX / (width / 8) ||
        !words_per_row || words_per_row > 16 || (words_per_row & (words_per_row - 1)) ||
        depth_words % words_per_row)
        throw std::invalid_argument("Memory requires W=8/16/32, 32-bit byte capacity, power-of-two K<=16 dividing N");
    if (expanded && depth_words > 4096 / width)
        throw std::invalid_argument("Expanded memory is limited to 4096 cells");
}
bool historyParameter(const ParameterMap &p) {
    auto v = parameter(p, "record_history", 1);
    if (v > 1)
        throw std::invalid_argument("record_history must be 0 or 1");
    return v != 0;
}
MemoryStorage::Limits storageLimits(const ParameterMap &p) {
    const auto paged = parameter(p, "paged", 1);
    const auto resident = parameter(p, "max_resident_bytes", 64 * 1024 * 1024);
    const auto history = parameter(p, "max_history_bytes", 16 * 1024 * 1024);
    if (paged > 1 || !resident || (!history && historyParameter(p)))
        throw std::invalid_argument("Invalid memory storage limits");
    return {paged != 0, resident, history};
}
ParameterMap storageParameters(const MemoryStorage::Limits &limits) {
    return {{"paged", limits.paged ? "1" : "0"},
            {"max_resident_bytes", std::to_string(limits.max_resident_bytes)},
            {"max_history_bytes", std::to_string(limits.max_history_bytes)}};
}
void readGeometry(const ParameterMap &p, Geometry &g) {
    g.depth_words = parameter(p, "depth_words", 4);
    g.words_per_row = parameter(p, "words_per_row", 2);
    g.record_history = historyParameter(p);
    g.storage_limits = storageLimits(p);
}
ParameterMap geometryParameters(const Geometry &g) {
    auto p = storageParameters(g.storage_limits);
    p.insert({{"record_history", g.record_history ? "1" : "0"},
              {"depth_words", std::to_string(g.depth_words)},
              {"words_per_row", std::to_string(g.words_per_row)}});
    return p;
}
void Primitive::publish(Simulator &sim, size_t t, const std::string &pin, const Bits &value) {
    auto it = published_.find(pin);
    if (it != published_.end() && it->second == value)
        return;
    published_[pin] = value;
    _updateOutputWireDynamic(sim, pin, value, t);
}
void Primitive::wake(Simulator &sim, size_t t) {
    sim.scheduleEvent(std::make_shared<ComponentEvalEvent>(t, shared_from_this()));
}
std::vector<PinDescriptor> describe(IOComponent::PinInitFunction pins) {
    auto io = Component::create<IOComponent>("SCHEMA", std::move(pins));
    std::vector<PinDescriptor> result;
    for (const auto &[name, pin] : io->getAllInputPins())
        result.push_back({name, pin->getWidth(), PinType::INPUT});
    for (const auto &[name, pin] : io->getAllOutputPins())
        result.push_back({name, pin->getWidth(), PinType::OUTPUT});
    return result;
}
void registerBlock(ComponentCatalog &c, const std::string &id, const std::string &type, Schema schema,
                   ComponentFactory structural, ComponentFactory behavioral, const std::string &test,
                   bool terminal, ParameterPredicate support) {
    c.registerContract({id, 1, type, schema({}), {}, "four-state", "stable-after-settle", schema});
    for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        if (fidelity == Fidelity::Structural && !structural)
            continue;
        if (fidelity == Fidelity::Behavioral && !behavioral)
            continue;
        ImplementationDescriptor i;
        i.id = id + (fidelity == Fidelity::Structural ? ".structural" : ".behavioral");
        i.contract_id = id;
        i.concrete_type_name = type;
        i.fidelity = fidelity;
        i.terminal_primitive = terminal && fidelity == Fidelity::Structural;
        i.supports = [schema, support, fidelity](const ParameterMap &p) {
            try {
                schema(p);
                return fidelity != Fidelity::Structural || !support || support(p);
            } catch (const std::exception &) {
                return false;
            }
        };
        i.factory = fidelity == Fidelity::Structural ? structural : behavioral;
        if (fidelity == Fidelity::Behavioral && id.find(".device.") != std::string::npos)
            i.capabilities = {"memory-state-view"};
        i.evidence = {VerificationStatus::Verified,
                      {terminal ? "explicit digital primitive, not transistor physics"
                                : "tested cells, rows and peripheral composition"},
                      {test},
                      behavioral ? std::vector<std::string>{"MemoryDeviceContractTest"}
                                 : std::vector<std::string>{},
                      {"MemoryDeviceTimingTest"},
                      "four-state",
                      "stable-after-settle"};
        if (id.starts_with("memory.device."))
            i.evidence.equivalence_tests.push_back("NativeMemoryDeviceContractTest");
        c.registerImplementation(std::move(i));
    }
}
std::shared_ptr<IOComponent> child(ComponentBuilder &b, IOComponent &parent, const std::string &id,
                                   const std::string &name, ParameterMap params) {
    if (b.getBuildContext())
        return b.addByContract({id, name, std::move(params), {}, {}, {}});
    auto built = builtinComponentCatalog().createRoot({id, name, std::move(params), {}, {}, {}},
                                                      canonicalDefaultProfile());
    auto io = std::dynamic_pointer_cast<IOComponent>(built.root);
    parent.addChild(io);
    return io;
}
std::shared_ptr<IOComponent> bitAdapter(ComponentBuilder &b, const std::string &n, size_t width, bool split) {
    if (width == 1)
        throw std::invalid_argument("One-bit adapters are unnecessary; connect directly");
    if (split) {
        if (width == 3)
            return b.addNewComponent<BitSplitter<3>>(n);
        if (width == 2)
            return b.addNewComponent<BitSplitter<2>>(n);
        if (width == 4)
            return b.addNewComponent<BitSplitter<4>>(n);
        if (width == 8)
            return b.addNewComponent<BitSplitter<8>>(n);
        if (width == 16)
            return b.addNewComponent<BitSplitter<16>>(n);
        if (width == 32)
            return b.addNewComponent<BitSplitter<32>>(n);
    } else {
        if (width == 2)
            return b.addNewComponent<BitJoiner<2>>(n);
        if (width == 3)
            return b.addNewComponent<BitJoiner<3>>(n);
        if (width == 4)
            return b.addNewComponent<BitJoiner<4>>(n);
        if (width == 8)
            return b.addNewComponent<BitJoiner<8>>(n);
        if (width == 16)
            return b.addNewComponent<BitJoiner<16>>(n);
        if (width == 32)
            return b.addNewComponent<BitJoiner<32>>(n);
    }
    throw std::invalid_argument("Unsupported bit adapter width");
}
static void decoderPins(IOComponent *s, size_t rows) {
    if (!rows || rows > 512)
        throw std::invalid_argument("Decoder rows must be 1..512");
    s->addPin<32>("ROW_ADDR", PinType::INPUT);
    s->addPin("ENABLE", PinType::INPUT);
    for (size_t r = 0; r < rows; ++r)
        s->addPin("WL_" + std::to_string(r), PinType::OUTPUT);
}
MatrixRowDecoder::MatrixRowDecoder(std::string n, size_t rows)
    : IOComponent(std::move(n), [rows](IOComponent *s) { decoderPins(s, rows); }), rows_(rows),
      type_("MatrixRowDecoder<" + std::to_string(rows) + ">") {
}
void MatrixRowDecoder::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto a = bitAdapter(b, "ADDRESS_BITS", 32, true);
    w.connect(getInputPin<32>("ROW_ADDR"), a->getInputPin<32>("IN"));
    std::vector<memory_detail::StructuralWiring::PinPtr> positive, negative;
    size_t row_bits = 0;
    while ((size_t{1} << row_bits) < rows_) ++row_bits;
    for (size_t bit = 0; bit < row_bits; ++bit) {
        positive.push_back(a->getOutputPin("OUT_" + std::to_string(bit)));
        negative.push_back(w.invert("NOT_" + std::to_string(bit), positive.back()));
    }
    std::vector<memory_detail::StructuralWiring::PinPtr> high_zero;
    for (size_t bit = row_bits; bit < 32; ++bit)
        high_zero.push_back(w.invert("NOT_" + std::to_string(bit),
                                    a->getOutputPin("OUT_" + std::to_string(bit))));
    auto valid_high = w.reduce("HIGH_ADDRESS_VALID", std::move(high_zero), true);
    auto enabled = w.both("VALID_AND_ENABLED", valid_high, getInputPin("ENABLE"));
    for (size_t r = 0; r < rows_; ++r) {
        w.connect(memory_detail::decodeMatch(w, "ROW_" + std::to_string(r), positive, negative,
                                            r, enabled),
                  getOutputPin("WL_" + std::to_string(r)));
    }
    w.finish();
}
static void muxPins(IOComponent *s, size_t width, size_t k) {
    Geometry{k, k}.validate(width, true);
    s->addPin<32>("COLUMN_ADDR", PinType::INPUT);
    for (size_t c = 0; c < width * k; ++c)
        s->addPin("COL_" + std::to_string(c), PinType::INPUT);
    s->addPinDynamic("DATA", PinType::OUTPUT, width);
}
MatrixColumnMux::MatrixColumnMux(std::string n, size_t width, size_t k)
    : IOComponent(std::move(n), [=](IOComponent *s) { muxPins(s, width, k); }), width_(width), k_(k),
      type_("MatrixColumnMux<" + std::to_string(width) + "," + std::to_string(k) + ">") {
}
void MatrixColumnMux::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto decode = b.addNewComponent<MatrixRowDecoder>("SELECT", k_);
    auto pack = bitAdapter(b, "DATA_BITS", width_, false);
    w.connect(getInputPin<32>("COLUMN_ADDR"), decode->getInputPin<32>("ROW_ADDR"));
    w.connect(w.constant("ENABLED", true), decode->getInputPin("ENABLE"));
    for (size_t bit = 0; bit < width_; ++bit) {
        std::vector<memory_detail::StructuralWiring::PinPtr> terms;
        for (size_t slot = 0; slot < k_; ++slot)
            terms.push_back(w.both("SELECT_" + std::to_string(slot) + "_" + std::to_string(bit),
                                   decode->getOutputPin("WL_" + std::to_string(slot)),
                                   getInputPin("COL_" + std::to_string(slot * width_ + bit))));
        w.connect(w.reduce("BIT_" + std::to_string(bit), terms, false),
                  pack->getInputPin("IN_" + std::to_string(bit)));
    }
    w.connect(pack->getOutputPinDynamic("OUT"), getOutputPinDynamic("DATA"));
    w.finish();
}
static void readColumnPins(IOComponent *s, size_t rows) {
    if (!rows || rows > 512)
        throw std::invalid_argument("Invalid read column rows");
    for (size_t r = 0; r < rows; ++r)
        for (const auto *p : {"WL_", "Q_"})
            s->addPin(p + std::to_string(r), PinType::INPUT);
    s->addPin("OUT", PinType::OUTPUT);
}
static void writePins(IOComponent *s, size_t width, size_t k) {
    Geometry{k, k}.validate(width, true);
    s->addPin<32>("COLUMN_ADDR", PinType::INPUT);
    s->addPin("ENABLE", PinType::INPUT);
    s->addPinDynamic("DATA", PinType::INPUT, width);
    s->addPinDynamic("MASK", PinType::INPUT, width / 8);
    for (size_t c = 0; c < width * k; ++c)
        for (const auto *p : {"DRIVE_", "DATA_"})
            s->addPin(p + std::to_string(c), PinType::OUTPUT);
}
MatrixWriteDistributor::MatrixWriteDistributor(std::string n, size_t width, size_t k)
    : IOComponent(std::move(n), [=](IOComponent *s) { writePins(s, width, k); }), width_(width), k_(k),
      type_("MatrixWriteDistributor<" + std::to_string(width) + "," + std::to_string(k) + ">") {
}
void MatrixWriteDistributor::buildInternals(ComponentBuilder &b) {
    memory_detail::StructuralWiring w(b);
    auto decode = b.addNewComponent<MatrixRowDecoder>("SELECT", k_);
    auto data = bitAdapter(b, "DATA_BITS", width_, true);
    std::shared_ptr<IOComponent> mask;
    if (width_ > 8)
        mask = bitAdapter(b, "MASK_BITS", width_ / 8, true);
    w.connect(getInputPin<32>("COLUMN_ADDR"), decode->getInputPin<32>("ROW_ADDR"));
    w.connect(getInputPin("ENABLE"), decode->getInputPin("ENABLE"));
    w.connect(getInputPinDynamic("DATA"), data->getInputPinDynamic("IN"));
    if (mask)
        w.connect(getInputPinDynamic("MASK"), mask->getInputPinDynamic("IN"));
    for (size_t slot = 0; slot < k_; ++slot) {
        for (size_t byte = 0; byte < width_ / 8; ++byte) {
            auto m = mask ? mask->getOutputPin("OUT_" + std::to_string(byte)) : getInputPin("MASK");
            auto enabled = w.both("WRITE_SLOT_" + std::to_string(slot) + "_BYTE_" + std::to_string(byte),
                                  decode->getOutputPin("WL_" + std::to_string(slot)), m);
            for (size_t bit = byte * 8; bit < (byte + 1) * 8; ++bit) {
                auto n = std::to_string(slot * width_ + bit);
                w.connect(enabled, getOutputPin("DRIVE_" + n));
                w.connect(data->getOutputPin("OUT_" + std::to_string(bit)), getOutputPin("DATA_" + n));
            }
        }
    }
    w.finish();
}
StaticReadColumn::StaticReadColumn(std::string n, size_t rows)
    : Primitive(std::move(n), "StaticReadColumn<" + std::to_string(rows) + ">",
                [rows](IOComponent *s) { readColumnPins(s, rows); }),
      rows_(rows) {
}
void StaticReadColumn::evaluate(size_t t, Simulator &sim) {
    size_t count = 0;
    auto out = X;
    for (size_t r = 0; r < rows_; ++r) {
        auto wl = getInputValue("WL_" + std::to_string(r));
        if (!binary(wl)) {
            publish(sim, t, "OUT", X);
            return;
        }
        if (wl == H) {
            ++count;
            out = stored(getInputValue("Q_" + std::to_string(r)));
        }
    }
    publish(sim, t, "OUT", count == 1 ? out : X);
}
static void outputPins(IOComponent *s, size_t width) {
    Geometry{}.validate(width);
    s->addPinDynamic("DATA", PinType::INPUT, width);
    s->addPin("ENABLE", PinType::INPUT);
    s->addPinDynamic("OUT", PinType::OUTPUT, width);
}
MemoryOutputBuffer::MemoryOutputBuffer(std::string n, size_t width)
    : Primitive(std::move(n), "MemoryOutputBuffer<" + std::to_string(width) + ">",
                [width](IOComponent *s) { outputPins(s, width); }),
      width_(width) {
}
void MemoryOutputBuffer::evaluate(size_t t, Simulator &sim) {
    auto en = getInputValue("ENABLE");
    publish(sim, t, "OUT", en == H ? read("DATA") : Bits(width_, en == L ? Z : X));
}
void registerMatrixComponents(ComponentCatalog &c) {
    auto count = [](const ParameterMap &p) {
        keys(p, {"rows"});
        auto r = parameter(p, "rows", 2);
        if (!r || r > 512)
            throw std::invalid_argument("Invalid rows");
        return r;
    };
    registerBlock(
        c, "memory.matrix.row-decoder", "MatrixRowDecoder",
        [count](const ParameterMap &p) {
            return describe([r = count(p)](IOComponent *s) { decoderPins(s, r); });
        },
        [count](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<MatrixRowDecoder>(ctx, n, count(p));
        },
        {}, "MatrixRowDecoderTest");
    registerBlock(
        c, "memory.matrix.read-column", "StaticReadColumn",
        [count](const ParameterMap &p) {
            return describe([r = count(p)](IOComponent *s) { readColumnPins(s, r); });
        },
        [count](const std::string &n, const ParameterMap &p, const std::shared_ptr<BuildContext> &ctx) {
            return Component::createWithContext<StaticReadColumn>(ctx, n, count(p));
        },
        {}, "StaticReadColumnTest", true);
    for (size_t width : {8, 16, 32}) {
        const auto suffix = std::to_string(width);
        auto k = [](const ParameterMap &p) {
            keys(p, {"words_per_row"});
            return parameter(p, "words_per_row", 2);
        };
        registerBlock(
            c, "memory.matrix.column-mux." + suffix, "MatrixColumnMux<" + suffix + ">",
            [width, k](const ParameterMap &p) {
                return describe([=](IOComponent *s) { muxPins(s, width, k(p)); });
            },
            [width, k](const std::string &n, const ParameterMap &p,
                       const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<MatrixColumnMux>(ctx, n, width, k(p));
            },
            {}, "MatrixColumnMuxTest");
        registerBlock(
            c, "memory.matrix.write-distributor." + suffix, "MatrixWriteDistributor<" + suffix + ">",
            [width, k](const ParameterMap &p) {
                return describe([=](IOComponent *s) { writePins(s, width, k(p)); });
            },
            [width, k](const std::string &n, const ParameterMap &p,
                       const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<MatrixWriteDistributor>(ctx, n, width, k(p));
            },
            {}, "MatrixWriteDistributorTest");
        registerBlock(
            c, "memory.matrix.output-buffer." + suffix, "MemoryOutputBuffer<" + suffix + ">",
            [width](const ParameterMap &p) {
                keys(p, {});
                return describe([=](IOComponent *s) { outputPins(s, width); });
            },
            [width](const std::string &n, const ParameterMap &, const std::shared_ptr<BuildContext> &ctx) {
                return Component::createWithContext<MemoryOutputBuffer>(ctx, n, width);
            },
            {}, "MemoryOutputBufferTest", true);
    }
}
} // namespace circuit::memory_device
