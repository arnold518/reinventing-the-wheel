#include "modules/memory/SRAM.hpp"
#include "modules/memory/SRAMBehavioral.hpp"
#include <charconv>

namespace {
using namespace circuit;
using Pins = std::vector<PinDescriptor>;
using Schema = std::function<Pins(const ParameterMap&)>;
size_t dimension(const ParameterMap& p, const std::string& key, size_t defaultValue, size_t maximum) {
    size_t n = defaultValue;
    for (const auto& [k, v] : p) {
        if (k != key) throw std::invalid_argument("Unknown SRAM primitive parameter: " + k);
        auto result = std::from_chars(v.data(), v.data() + v.size(), n);
        if (result.ec != std::errc{} || result.ptr != v.data() + v.size()) throw std::invalid_argument("Invalid SRAM dimension");
    }
    if (!n || n > maximum) throw std::invalid_argument("Invalid SRAM dimension");
    return n;
}
void empty(const ParameterMap& p) { if (!p.empty()) throw std::invalid_argument("This SRAM component takes no parameters"); }
void in(Pins& p, std::string name, size_t width = 1) { p.push_back({std::move(name), width, PinType::INPUT}); }
void out(Pins& p, std::string name, size_t width = 1) { p.push_back({std::move(name), width, PinType::OUTPUT}); }
void registerBlock(ComponentCatalog& catalog, std::string kind, std::string type, Schema schema,
    ComponentFactory structural, ComponentFactory behavioral, std::string test, bool terminal = false,
    ParameterPredicate structuralSupport = {}) {
    const auto id = "memory.sram." + kind;
    catalog.registerContract({id, 1, type, schema({}), {}, "four-state", "stable-after-settle", schema});
    for (const auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        if (fidelity == Fidelity::Behavioral && !behavioral) continue;
        ImplementationDescriptor impl;
        impl.id = id + (fidelity == Fidelity::Structural ? ".structural" : ".behavioral");
        impl.contract_id = id; impl.fidelity = fidelity; impl.concrete_type_name = type;
        impl.terminal_primitive = fidelity == Fidelity::Structural && terminal;
        impl.supports = [schema, fidelity, structuralSupport](const ParameterMap& p) {
            try { schema(p); return fidelity != Fidelity::Structural || !structuralSupport || structuralSupport(p); }
            catch (const std::exception&) { return false; }
        };
        impl.factory = fidelity == Fidelity::Structural ? structural : behavioral;
        impl.evidence = {VerificationStatus::Verified,
            {terminal ? "explicit digital SRAM primitive, not transistor simulation" : "SRAM cell/row/column structural composition"},
            {test}, behavioral ? std::vector<std::string>{"SRAMContractTest"} : std::vector<std::string>{},
            {"SRAMTimingTest"}, "four-state", "stable-after-settle"};
        if (kind.starts_with("device.") && fidelity == Fidelity::Behavioral)
            impl.capabilities = {"memory-state-view", "memory-image-load"};
        catalog.registerImplementation(std::move(impl));
    }
}
template<class T> ComponentFactory simple() {
    return [](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& c) {
        empty(p); return Component::createWithContext<T>(c, name);
    };
}
template<class T> ComponentFactory sized(std::string key, size_t def, size_t max) {
    return [key, def, max](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& c) {
        return Component::createWithContext<T>(c, name, dimension(p, key, def, max));
    };
}
template<class T> ComponentFactory configured() {
    return [](const std::string& name, const ParameterMap& p, const std::shared_ptr<BuildContext>& c) {
        return Component::createWithContext<T>(c, name, sramConfigFromParameters(p));
    };
}
template<size_t W> Pins devicePins() {
    Pins p; in(p, "WORD_ADDR", 32); in(p, "WRITE_DATA", W); in(p, "WRITE_MASK", W / 8);
    for (const auto* n : {"CE_N", "WE_N", "OE_N"}) in(p, n);
    out(p, "READ_DATA", W); return p;
}
template<size_t W> void registerWidth(ComponentCatalog& catalog) {
    const auto suffix = "." + std::to_string(W), typeSuffix = "<" + std::to_string(W) + ">";
    registerBlock(catalog, "device" + suffix, "SRAM" + typeSuffix, [](const ParameterMap& p) {
        sramConfigFromParameters(p).validate(W); return devicePins<W>();
    }, configured<SRAM<W>>(), configured<sram::DeviceBehavioral<W>>(), "SRAMTest", false,
    [](const ParameterMap& p) { sramConfigFromParameters(p).validate(W, true); return true; });
    registerBlock(catalog, "bank" + suffix, "SRAMStorageBank" + typeSuffix, [](const ParameterMap& params) {
        const auto c = sramConfigFromParameters(params); c.validate(W, true);
        Pins p;
        for (size_t r = 0; r < c.rows(); ++r) in(p, "WL_" + std::to_string(r));
        for (size_t col = 0; col < c.columns(W); ++col) {
            const auto n = std::to_string(col);
            for (const auto* prefix : {"PRECHARGE_", "DRIVE_", "WRITE_BL_", "WRITE_BL_BAR_"}) in(p, prefix + n);
            for (const auto* prefix : {"BL_", "BL_BAR_", "VALID_"}) out(p, prefix + n);
        }
        return p;
    }, configured<SRAMStorageBank<W>>(), configured<sram::BankBehavioral<W>>(), "SRAMStorageBankTest");
    registerBlock(catalog, "access-control" + suffix, "SRAMAccessControl" + typeSuffix, [](const ParameterMap& params) {
        sramConfigFromParameters(params).validate(W);
        auto p = devicePins<W>(); p.pop_back();
        out(p, "ADDRESS", 32); out(p, "DATA", W); out(p, "MASK", W / 8);
        for (const auto* n : {"PRECHARGE", "ROW_ENABLE", "WRITE_ENABLE", "SENSE_ENABLE", "OUTPUT_ENABLE"}) out(p, n);
        return p;
    }, configured<SRAMAccessControl<W>>(), {}, "SRAMAccessControlTest", true);
    registerBlock(catalog, "output-buffer" + suffix, "SRAMOutputBuffer" + typeSuffix, [](const ParameterMap& params) {
        empty(params); Pins p; in(p, "DATA", W); in(p, "ENABLE"); out(p, "OUT", W); return p;
    }, simple<SRAMOutputBuffer<W>>(), {}, "SRAMOutputBufferTest", true);
    registerBlock(catalog, "column-selector" + suffix, "SRAMColumnSelector" + typeSuffix, [](const ParameterMap& params) {
        const auto k = dimension(params, "words_per_row", 2, 64); SRAMDeviceConfig{k,k}.validate(W, true);
        Pins p; in(p, "WORD_ADDR", 32);
        for (size_t bit = 0; bit < W; ++bit) {
            const auto n = std::to_string(bit);
            for (const auto* prefix : {"WRITE_BL_", "WRITE_BL_BAR_", "WRITE_DRIVE_"}) in(p, prefix + n);
            for (const auto* prefix : {"READ_BL_", "READ_BL_BAR_", "READ_VALID_"}) out(p, prefix + n);
        }
        for (size_t c = 0; c < W * k; ++c) {
            const auto n = std::to_string(c);
            for (const auto* prefix : {"BL_", "BL_BAR_", "VALID_"}) in(p, prefix + n);
            for (const auto* prefix : {"DRIVE_", "DRIVE_BL_", "DRIVE_BL_BAR_"}) out(p, prefix + n);
        }
        return p;
    }, sized<SRAMColumnSelector<W>>("words_per_row", 2, 64), {}, "SRAMColumnSelectorTest");
}
}

void registerSRAMComponents(circuit::ComponentCatalog& catalog) {
    registerBlock(catalog, "cell", "SRAMCell", [](const ParameterMap& params) {
        empty(params); Pins p; for (const auto* n : {"WL", "DRIVE", "BL", "BL_BAR"}) in(p, n);
        out(p, "Q"); out(p, "Q_BAR"); return p;
    }, simple<SRAMCell>(), simple<circuit::sram::CellBehavioral>(), "SRAMCellTest");
    registerBlock(catalog, "row", "SRAMRow", [](const ParameterMap& params) {
        auto columns = dimension(params, "columns", 2, 4096); Pins p; in(p, "WL");
        for (size_t c = 0; c < columns; ++c) {
            const auto n = std::to_string(c);
            for (const auto* prefix : {"DRIVE_", "BL_", "BL_BAR_"}) in(p, prefix + n);
            out(p, "Q_" + n);
        }
        return p;
    }, sized<SRAMRow>("columns", 2, 4096), sized<circuit::sram::RowBehavioral>("columns", 2, 4096), "SRAMRowTest");
    registerBlock(catalog, "bitline-pair", "SRAMBitlinePair", [](const ParameterMap& params) {
        auto rows = dimension(params, "rows", 2, 512); Pins p;
        for (const auto* n : {"PRECHARGE", "DRIVE", "WRITE_BL", "WRITE_BL_BAR"}) in(p, n);
        for (size_t r = 0; r < rows; ++r) { in(p, "WL_" + std::to_string(r)); in(p, "Q_" + std::to_string(r)); }
        for (const auto* n : {"BL", "BL_BAR", "VALID", "CELL_DRIVE", "CELL_BL", "CELL_BL_BAR"}) out(p, n);
        return p;
    }, sized<SRAMBitlinePair>("rows", 2, 512), {}, "SRAMBitlinePairTest", true);
    registerBlock(catalog, "row-decoder", "SRAMRowDecoder", [](const ParameterMap& params) {
        auto c = sramConfigFromParameters(params); c.validate(8, true); Pins p; in(p, "WORD_ADDR", 32); in(p, "ENABLE");
        for (size_t r = 0; r < c.rows(); ++r) out(p, "WL_" + std::to_string(r));
        return p;
    }, configured<SRAMRowDecoder>(), {}, "SRAMRowDecoderTest");
    registerBlock(catalog, "precharge", "SRAMPrecharge", [](const ParameterMap& params) {
        auto columns = dimension(params, "columns", 2, 4096); Pins p; in(p, "ENABLE");
        for (size_t c = 0; c < columns; ++c) out(p, "PRECHARGE_" + std::to_string(c));
        return p;
    }, sized<SRAMPrecharge>("columns", 2, 4096), {}, "SRAMPrechargeTest");
    registerBlock(catalog, "write-driver", "SRAMWriteDriver", [](const ParameterMap& params) {
        empty(params); Pins p; for (const auto* n : {"D", "ENABLE", "MASK"}) in(p, n);
        for (const auto* n : {"BL", "BL_BAR", "DRIVE"}) out(p, n);
        return p;
    }, simple<SRAMWriteDriver>(), {}, "SRAMWriteDriverTest");
    registerBlock(catalog, "sense-amplifier", "SRAMSenseAmplifier", [](const ParameterMap& params) {
        empty(params); Pins p; for (const auto* n : {"BL", "BL_BAR", "VALID", "ENABLE"}) in(p, n); out(p, "Q"); return p;
    }, simple<SRAMSenseAmplifier>(), {}, "SRAMSenseAmplifierTest", true);
    registerWidth<8>(catalog); registerWidth<16>(catalog); registerWidth<32>(catalog);
}
