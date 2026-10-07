#include "modules/memory/SRAM.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <algorithm>
#include <charconv>

namespace {
constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH, X = LogicValue::UNKNOWN;
void countValid(size_t count, size_t maximum) {
    if (!count || count > maximum) throw std::invalid_argument("Invalid SRAM primitive geometry");
}
size_t addressBits(size_t count) {
    size_t result = 0;
    while ((size_t{1} << result) < count) ++result;
    return result;
}
// Converts symbolic strong write excitation to latch controls. It deliberately
// distinguishes write drive from precharge/read differential; not a transistor.
class SRAMCellAccess : public BasicComponent {
public:
    explicit SRAMCellAccess(std::string name) : BasicComponent(std::move(name), 1, [](IOComponent* s) {
        for (const auto* p : {"WL", "DRIVE", "BL", "BL_BAR"}) s->addPin(p, PinType::INPUT);
        for (const auto* p : {"S_BAR", "R_BAR"}) s->addPin(p, PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override { return "SRAMCellAccess"; }
    void evaluate(size_t t, Simulator& sim) override {
        const auto wl = getInputValue("WL"), drive = getInputValue("DRIVE");
        auto set = H, reset = H;
        if (wl != L && drive != L) {
            const auto bl = getInputValue("BL"), bar = getInputValue("BL_BAR");
            if (wl == H && drive == H && circuit::sram::binary(bl) && bar == circuit::sram::invert(bl)) {
                set = bl == H ? L : H;
                reset = bl == L ? L : H;
            } else set = reset = X;
        }
        _updateOutputWire(sim, "S_BAR", set, t);
        _updateOutputWire(sim, "R_BAR", reset, t);
    }
};
}

void SRAMDeviceConfig::validate(size_t width, bool structural) const {
    if ((width != 8 && width != 16 && width != 32) || !depth_words || depth_words > UINT32_MAX / (width / 8) ||
        !words_per_row || words_per_row > 64 || (words_per_row & (words_per_row - 1)) ||
        depth_words % words_per_row || phase_ticks < 32 || phase_ticks > 10000)
        throw std::invalid_argument("SRAM requires W=8/16/32, 32-bit byte capacity, power-of-two K<=64 dividing N, phase_ticks=32..10000");
    if (structural && depth_words > 4096 / width)
        throw std::invalid_argument("Structural SRAM and pin-expanded banks are limited to 4096 cells");
}
SRAMDeviceConfig sramConfigFromParameters(const circuit::ParameterMap& p) {
    SRAMDeviceConfig c;
    c.record_history = circuit::memory_device::historyParameter(p);
    c.storage_limits = circuit::memory_device::storageLimits(p);
    for (const auto& [key, value] : p) {
        if (key == "record_history" || key == "paged" || key == "max_resident_bytes" || key == "max_history_bytes") continue;
        size_t n = 0;
        auto result = std::from_chars(value.data(), value.data() + value.size(), n);
        if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
            throw std::invalid_argument("Invalid SRAM parameter: " + key);
        if (key == "depth_words") c.depth_words = n;
        else if (key == "words_per_row") c.words_per_row = n;
        else if (key == "phase_ticks") c.phase_ticks = n;
        else throw std::invalid_argument("Unknown SRAM parameter: " + key);
    }
    return c;
}
circuit::ParameterMap sramParameters(const SRAMDeviceConfig& c) {
    auto p = circuit::memory_device::storageParameters(c.storage_limits);
    p.insert({{"depth_words", std::to_string(c.depth_words)}, {"words_per_row", std::to_string(c.words_per_row)},
              {"phase_ticks", std::to_string(c.phase_ticks)}, {"record_history", c.record_history ? "1" : "0"}});
    return p;
}

namespace circuit::sram {
bool binary(LogicValue v) { return v == L || v == H; }
LogicValue invert(LogicValue v) { return v == L ? H : v == H ? L : X; }
bool known(const Bits& v) { return std::all_of(v.begin(), v.end(), binary); }
uint32_t number(const Bits& v) {
    uint32_t result = 0;
    for (size_t bit = 0; bit < v.size(); ++bit) if (v[bit] == H) result |= uint32_t{1} << bit;
    return result;
}
Bits bits(size_t width, uint32_t value) {
    Bits result(width);
    for (size_t bit = 0; bit < width; ++bit) result[bit] = (value & (uint32_t{1} << bit)) ? H : L;
    return result;
}
void BitlineState::update(LogicValue precharge, LogicValue drive, LogicValue write_bl,
    LogicValue write_bar, const Bits& wordlines, const Bits& cells) {
    valid = cell_drive = L;
    cell_bl = cell_bar = X;
    size_t count = 0, selected = 0;
    for (size_t r = 0; r < wordlines.size(); ++r) if (wordlines[r] == H) { ++count; selected = r; }
    auto invalid = [&] { bl = bar = X; prepared = false; reading_row.reset(); };
    if (!binary(precharge) || !binary(drive) || !known(wordlines) || count > 1 ||
        (precharge == H && (drive == H || count))) { invalid(); return; }
    if (precharge == H) {
        bl = bar = H; prepared = true; reading_row.reset(); return;
    }
    if (drive == H) {
        // Unknown write data still reaches the cell and poisons only enabled bits.
        bl = cell_bl = binary(write_bl) ? write_bl : X;
        bar = cell_bar = binary(write_bar) ? write_bar : X;
        cell_drive = H; prepared = false; reading_row.reset(); return;
    }
    if (count) {
        if (!prepared || (reading_row && *reading_row != selected)) { invalid(); return; }
        reading_row = selected;
        bl = binary(cells[selected]) ? cells[selected] : X;
        bar = invert(bl);
        valid = binary(bl) ? H : L;
    } else if (reading_row) {
        // A used read pair needs a new precharge before another row access.
        prepared = false; reading_row.reset();
    }
}

AccessSequence::AccessSequence(size_t width)
    : address(32, X), data(width, X), mask(width / 8, L) {}
std::optional<size_t> AccessSequence::update(const IOComponent& io, size_t time, size_t ticks) {
    auto read = [&](const char* pin) { return io.getInputPinDynamic(pin)->getValueAsVector(); };
    const auto ce = io.getInputValue("CE_N"), we = io.getInputValue("WE_N");
    Bits next = read("WORD_ADDR");
    next.push_back(ce); next.push_back(we);
    // Data/mask are don't-care during reads; OE never starts/restarts an access.
    if (we == L) {
        auto d = read("WRITE_DATA"), m = read("WRITE_MASK");
        next.insert(next.end(), d.begin(), d.end()); next.insert(next.end(), m.begin(), m.end());
    }
    std::optional<size_t> schedule;
    if (next != signature) {
        signature = std::move(next);
        precharge = row_enable = write_enable = sense_enable = false;
        pending_address = read("WORD_ADDR"); pending_data = read("WRITE_DATA"); pending_mask = read("WRITE_MASK");
        writing = we == L;
        phase = ce == L && binary(we) ? Phase::Quiet : Phase::Idle;
        due = phase == Phase::Quiet ? time + ticks : 0;
        if (due) schedule = due;
    } else if (due && time >= due) {
        switch (phase) {
        case Phase::Quiet:
            address = pending_address; data = pending_data; mask = pending_mask;
            precharge = true; phase = Phase::Precharge; break;
        case Phase::Precharge:
            precharge = false; write_enable = writing; phase = Phase::Release; break;
        case Phase::Release:
            row_enable = true; phase = Phase::Access; break;
        case Phase::Access:
            sense_enable = !writing; phase = Phase::Ready; break;
        default: break;
        }
        due = phase == Phase::Ready ? 0 : time + ticks;
        if (due) schedule = due;
    }
    output_enable = phase == Phase::Ready && ce == L && we == H && io.getInputValue("OE_N") == L;
    return schedule;
}
}

SRAMCell::SRAMCell(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
    for (const auto* p : {"WL", "DRIVE", "BL", "BL_BAR"}) s->addPin(p, PinType::INPUT);
    for (const auto* p : {"Q", "Q_BAR"}) s->addPin(p, PinType::OUTPUT);
}) {}
void SRAMCell::buildInternals(ComponentBuilder& b) {
    b.addNewComponent<SRAMCellAccess>("ACCESS");
    b.addNewComponent<SRLatch>("STATE");
    circuit::memory_detail::StructuralWiring w(b);
    for (const auto* pin : {"WL", "DRIVE", "BL", "BL_BAR"})
        w.connect(getInputPin(pin), b.getInputPin<SRAMCellAccess>("ACCESS", pin));
    for (const auto* pin : {"S_BAR", "R_BAR"})
        w.connect(b.getOutputPin<SRAMCellAccess>("ACCESS", pin), b.getInputPin<SRLatch>("STATE", pin));
    for (const auto* pin : {"Q", "Q_BAR"}) w.connect(b.getOutputPin<SRLatch>("STATE", pin), getOutputPin(pin));
    w.finish();
}
SRAMRow::SRAMRow(std::string name, size_t columns)
    : IOComponent(std::move(name), [columns](IOComponent* s) {
        countValid(columns, 4096);
        s->addPin("WL", PinType::INPUT);
        for (size_t c = 0; c < columns; ++c) {
            for (const auto* p : {"DRIVE_", "BL_", "BL_BAR_"}) s->addPin(p + std::to_string(c), PinType::INPUT);
            s->addPin("Q_" + std::to_string(c), PinType::OUTPUT);
        }
    }), columns_(columns), type_("SRAMRow<" + std::to_string(columns) + ">") { countValid(columns, 4096); }
void SRAMRow::buildInternals(ComponentBuilder& b) {
    circuit::memory_detail::StructuralWiring w(b);
    for (size_t c = 0; c < columns_; ++c) {
        const auto n = std::to_string(c);
        auto cell = circuit::sram::add<SRAMCell>(b, "memory.sram.cell", "CELL_" + n, {});
        w.connect(getInputPin("WL"), cell->getInputPin("WL"));
        for (const auto* p : {"DRIVE", "BL", "BL_BAR"}) w.connect(getInputPin(std::string(p) + "_" + n), cell->getInputPin(p));
        w.connect(cell->getOutputPin("Q"), getOutputPin("Q_" + n));
    }
    w.finish();
}
SRAMBitlinePair::SRAMBitlinePair(std::string name, size_t rows)
    : BasicComponent(std::move(name), 1, [rows](IOComponent* s) {
        countValid(rows, 512);
        for (const auto* p : {"PRECHARGE", "DRIVE", "WRITE_BL", "WRITE_BL_BAR"}) s->addPin(p, PinType::INPUT);
        for (size_t r = 0; r < rows; ++r) {
            s->addPin("WL_" + std::to_string(r), PinType::INPUT);
            s->addPin("Q_" + std::to_string(r), PinType::INPUT);
        }
        for (const auto* p : {"BL", "BL_BAR", "VALID", "CELL_DRIVE", "CELL_BL", "CELL_BL_BAR"}) s->addPin(p, PinType::OUTPUT);
    }), rows_(rows), type_("SRAMBitlinePair<" + std::to_string(rows) + ">") { countValid(rows, 512); }
void SRAMBitlinePair::evaluate(size_t t, Simulator& sim) {
    circuit::sram::Bits wl, cells;
    for (size_t r = 0; r < rows_; ++r) {
        wl.push_back(getInputValue("WL_" + std::to_string(r)));
        cells.push_back(getInputValue("Q_" + std::to_string(r)));
    }
    state_.update(getInputValue("PRECHARGE"), getInputValue("DRIVE"), getInputValue("WRITE_BL"), getInputValue("WRITE_BL_BAR"), wl, cells);
    for (const auto& [pin, value] : std::vector<std::pair<std::string, LogicValue>>{
        {"BL", state_.bl}, {"BL_BAR", state_.bar}, {"VALID", state_.valid},
        {"CELL_DRIVE", state_.cell_drive}, {"CELL_BL", state_.cell_bl}, {"CELL_BL_BAR", state_.cell_bar}})
        _updateOutputWire(sim, pin, value, t);
}
SRAMRowDecoder::SRAMRowDecoder(std::string name, SRAMDeviceConfig config)
    : IOComponent(std::move(name), [config](IOComponent* s) {
        config.validate(8, true);
        s->addPin<32>("WORD_ADDR", PinType::INPUT); s->addPin("ENABLE", PinType::INPUT);
        for (size_t r = 0; r < config.rows(); ++r) s->addPin("WL_" + std::to_string(r), PinType::OUTPUT);
    }), config_(config), type_("SRAMRowDecoder<" + std::to_string(config.depth_words) + "," + std::to_string(config.words_per_row) + ">") { config.validate(8, true); }
void SRAMRowDecoder::buildInternals(ComponentBuilder& b) {
    using namespace circuit::memory_detail;
    StructuralWiring w(b);
    auto split = b.addNewComponent<BitSplitter<32>>("ADDRESS");
    w.connect(getInputPin<32>("WORD_ADDR"), split->getInputPin<32>("IN"));
    const auto low = addressBits(config_.words_per_row);
    const auto row_bits = addressBits(config_.rows());
    std::vector<StructuralWiring::PinPtr> pos, neg;
    for (size_t bit = low; bit < low + row_bits; ++bit) {
        auto a = split->getOutputPin("OUT_" + std::to_string(bit));
        pos.push_back(a); neg.push_back(w.invert("NOT_" + std::to_string(bit), a));
    }
    std::vector<StructuralWiring::PinPtr> high_zero;
    for (size_t bit = low + row_bits; bit < 32; ++bit)
        high_zero.push_back(w.invert("NOT_" + std::to_string(bit),
            split->getOutputPin("OUT_" + std::to_string(bit))));
    auto valid_high = w.reduce("HIGH_ADDRESS_VALID", std::move(high_zero), true);
    auto enabled = w.both("VALID_AND_ENABLED", valid_high, getInputPin("ENABLE"));
    for (size_t r = 0; r < config_.rows(); ++r) {
        w.connect(decodeMatch(w, "ROW_" + std::to_string(r), pos, neg, r, enabled),
                  getOutputPin("WL_" + std::to_string(r)));
    }
    w.finish();
}
SRAMPrecharge::SRAMPrecharge(std::string name, size_t columns)
    : IOComponent(std::move(name), [columns](IOComponent* s) {
        countValid(columns, 4096); s->addPin("ENABLE", PinType::INPUT);
        for (size_t c = 0; c < columns; ++c) s->addPin("PRECHARGE_" + std::to_string(c), PinType::OUTPUT);
    }), columns_(columns), type_("SRAMPrecharge<" + std::to_string(columns) + ">") { countValid(columns, 4096); }
void SRAMPrecharge::buildInternals(ComponentBuilder& b) {
    circuit::memory_detail::StructuralWiring w(b);
    auto one = w.constant("ONE", true);
    auto precharge = w.both("PRECHARGE", getInputPin("ENABLE"), one);
    for (size_t c = 0; c < columns_; ++c)
        w.connect(precharge, getOutputPin("PRECHARGE_" + std::to_string(c)));
    w.finish();
}
SRAMWriteDriver::SRAMWriteDriver(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
    for (const auto* p : {"D", "ENABLE", "MASK"}) s->addPin(p, PinType::INPUT);
    for (const auto* p : {"BL", "BL_BAR", "DRIVE"}) s->addPin(p, PinType::OUTPUT);
}) {}
void SRAMWriteDriver::buildInternals(ComponentBuilder& b) {
    circuit::memory_detail::StructuralWiring w(b);
    auto drive = w.both("ENABLED_BYTE", getInputPin("ENABLE"), getInputPin("MASK"));
    auto d = w.both("DATA", getInputPin("D"), w.constant("ONE", true));
    w.connect(d, getOutputPin("BL"));
    w.connect(w.invert("COMPLEMENT", d), getOutputPin("BL_BAR"));
    w.connect(drive, getOutputPin("DRIVE")); w.finish();
}
SRAMSenseAmplifier::SRAMSenseAmplifier(std::string name) : BasicComponent(std::move(name), 1, [](IOComponent* s) {
    for (const auto* p : {"BL", "BL_BAR", "VALID", "ENABLE"}) s->addPin(p, PinType::INPUT);
    s->addPin("Q", PinType::OUTPUT);
}) {}
void SRAMSenseAmplifier::evaluate(size_t t, Simulator& sim) {
    const auto bl = getInputValue("BL");
    const auto q = getInputValue("ENABLE") == H && getInputValue("VALID") == H &&
        circuit::sram::binary(bl) && getInputValue("BL_BAR") == circuit::sram::invert(bl) ? bl : X;
    _updateOutputWire(sim, "Q", q, t);
}

#include "modules/memory/SRAM.tpp"
template class SRAMAccessControl<8>;
template class SRAMAccessControl<16>;
template class SRAMAccessControl<32>;
template class SRAMColumnSelector<8>;
template class SRAMColumnSelector<16>;
template class SRAMColumnSelector<32>;
template class SRAMOutputBuffer<8>;
template class SRAMOutputBuffer<16>;
template class SRAMOutputBuffer<32>;
template class SRAMStorageBank<8>;
template class SRAMStorageBank<16>;
template class SRAMStorageBank<32>;
template class SRAM<8>;
template class SRAM<16>;
template class SRAM<32>;
