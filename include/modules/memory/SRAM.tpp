#pragma once
#include "modules/memory/SRAM.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/memory/MemoryRangeCheck.hpp"
#include "components/TimingProfile.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/storage/Register.hpp"
#include "modules/basic/Gate.hpp"

template<size_t W> SRAMAccessControl<W>::SRAMAccessControl(std::string name, SRAMDeviceConfig config)
    : IOComponent(std::move(name), [clocked = config.clocked](IOComponent* s) {
        if (clocked) s->addPin("CLK", PinType::INPUT);
        s->addPin<32>("WORD_ADDR", PinType::INPUT);
        s->addPin<W>("WRITE_DATA", PinType::INPUT); s->addPin<W / 8>("WRITE_MASK", PinType::INPUT);
        for (const auto* p : {"CE_N", "WE_N", "OE_N"}) s->addPin(p, PinType::INPUT);
        s->addPin<32>("ADDRESS", PinType::OUTPUT); s->addPin<W>("DATA", PinType::OUTPUT);
        s->addPin<W / 8>("MASK", PinType::OUTPUT);
        for (const auto* p : {"PRECHARGE", "ROW_ENABLE", "WRITE_ENABLE", "SENSE_ENABLE", "OUTPUT_ENABLE", "READY"}) s->addPin(p, PinType::OUTPUT);
    }), config_(config) { config.validate(W); }
namespace circuit::sram {
// Replica gates for the sense stage after the wordline path (the row
// decoder's enable AND and its balanced tree, `decode_depth` gates, added by
// the caller): one gate, plus the column multiplexer the selected bitline
// pair crosses before the sense amplifier (an AND and an OR per level of the
// 2:1 tree; one pass gate for one word per row), so the sense enable and
// READY never come before the selected column is valid.
inline size_t senseReplicaGates(size_t words_per_row) {
    using circuit::timing::Leaf;
    using circuit::timing::leafDelay;
    size_t levels = 0; while ((size_t{1} << levels) < words_per_row) ++levels;
    const size_t gate = std::max<size_t>(1, leafDelay(Leaf::And));
    const size_t column = levels ? levels * (leafDelay(Leaf::And) + leafDelay(Leaf::Or)) : leafDelay(Leaf::And);
    return 1 + (column + gate - 1) / gate;
}
}

template<size_t W> void SRAMAccessControl<W>::buildInternals(ComponentBuilder& b) {
    using circuit::memory_detail::StructuralWiring;
    if (config_.clocked) { buildClocked(b); return; }
    StructuralWiring w(b);
    const auto& timing = circuit::timing::activeTimingProfile().memory;
    // Inputs in timer order: address, CE_N, WE_N, then data and mask, which
    // restart the access only during a write (a read ignores them).
    constexpr size_t inputs = 32 + 2 + W + W / 8;
    size_t row_bits = 0; while ((size_t{1} << row_bits) < config_.rows()) ++row_bits;
    // The row decoder ANDs the row bits and the enable in a balanced tree.
    // The row may open only once both that path and the release of
    // precharge (inverter, phase AND, distribution AND) have reached the
    // bitlines: the replica covers the longer of the two plus one gate.
    size_t decode_depth = 1; while ((size_t{1} << (decode_depth - 1)) < row_bits + 1) ++decode_depth;
    auto timer = b.addNewComponent<circuit::memory::MemoryTimer>("ACCESS_TIMER", inputs, timing.sram_settle,
        std::vector<circuit::memory::MemoryTimerStage>{
            {timing.sram_precharge, 1}, {0, std::max<size_t>(decode_depth, 3) + 1},
            {timing.sramBitline(config_.rows()), decode_depth + circuit::sram::senseReplicaGates(config_.words_per_row)},
            // READY after the data: the sense enable gate, the sense
            // amplifier and the output buffer, plus one gate.
            {2 * circuit::timing::leafDelay(circuit::timing::Leaf::MemoryPrimitive), 2}}, 34);
    auto address = b.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
    auto data = b.addNewComponent<BitSplitter<W>>("DATA_BITS");
    auto held_address = b.addNewComponent<BitJoiner<32>>("HELD_ADDRESS");
    auto held_data = b.addNewComponent<BitJoiner<W>>("HELD_DATA");
    w.connect(this->template getInputPin<32>("WORD_ADDR"), address->template getInputPin<32>("IN"));
    w.connect(this->template getInputPin<W>("WRITE_DATA"), data->template getInputPin<W>("IN"));
    size_t index = 0;
    const auto feed = [&](StructuralWiring::PinPtr source) -> StructuralWiring::PinPtr {
        const auto n = std::to_string(index++);
        w.connect(source, timer->getInputPin("IN_" + n));
        return timer->getOutputPin("HELD_" + n);
    };
    for (size_t bit = 0; bit < 32; ++bit)
        w.connect(feed(address->getOutputPin("OUT_" + std::to_string(bit))), held_address->getInputPin("IN_" + std::to_string(bit)));
    const auto held_ce_n = feed(this->getInputPin("CE_N"));
    const auto held_we_n = feed(this->getInputPin("WE_N"));
    for (size_t bit = 0; bit < W; ++bit)
        w.connect(feed(data->getOutputPin("OUT_" + std::to_string(bit))), held_data->getInputPin("IN_" + std::to_string(bit)));
    if constexpr (W == 8) {
        w.connect(feed(this->getInputPin("WRITE_MASK")), this->getOutputPin("MASK"));
    } else {
        auto mask = b.addNewComponent<BitSplitter<W / 8>>("MASK_BITS");
        auto held_mask = b.addNewComponent<BitJoiner<W / 8>>("HELD_MASK");
        w.connect(this->template getInputPin<W / 8>("WRITE_MASK"), mask->template getInputPin<W / 8>("IN"));
        for (size_t bit = 0; bit < W / 8; ++bit)
            w.connect(feed(mask->getOutputPin("OUT_" + std::to_string(bit))), held_mask->getInputPin("IN_" + std::to_string(bit)));
        w.connect(held_mask->template getOutputPin<W / 8>("OUT"), this->template getOutputPin<W / 8>("MASK"));
    }
    w.connect(held_address->template getOutputPin<32>("OUT"), this->template getOutputPin<32>("ADDRESS"));
    w.connect(held_data->template getOutputPin<W>("OUT"), this->template getOutputPin<W>("DATA"));
    w.connect(w.invert("SELECTED", held_ce_n), timer->getInputPin("ENABLE"));
    const auto writing = w.invert("WRITING", held_we_n);
    w.connect(writing, timer->getInputPin("CONDITION"));
    const auto stage = [&](size_t k) { return timer->getOutputPin("STAGE_" + std::to_string(k)); };
    w.connect(w.both("PRECHARGE_PHASE", stage(0), w.invert("PRECHARGED", stage(1))), this->getOutputPin("PRECHARGE"));
    w.connect(w.both("WRITE_PHASE", stage(1), writing), this->getOutputPin("WRITE_ENABLE"));
    w.connect(stage(2), this->getOutputPin("ROW_ENABLE"));
    w.connect(w.both("SENSE_PHASE", stage(3), held_we_n), this->getOutputPin("SENSE_ENABLE"));
    w.connect(stage(4), this->getOutputPin("READY"));
    w.connect(w.reduce("OUTPUT_PHASE", {stage(3), held_we_n, w.invert("OUTPUT_ON", this->getInputPin("OE_N"))}, true),
              this->getOutputPin("OUTPUT_ENABLE"));
    w.finish();
}

template<size_t W> void SRAMAccessControl<W>::buildClocked(ComponentBuilder& b) {
    using circuit::memory_detail::StructuralWiring;
    using PinPtr = StructuralWiring::PinPtr;
    using circuit::timing::Leaf;
    using circuit::timing::leafDelay;
    StructuralWiring w(b);
    const auto& timing = circuit::timing::activeTimingProfile().memory;
    size_t row_bits = 0; while ((size_t{1} << row_bits) < config_.rows()) ++row_bits;
    size_t decode_depth = 1; while ((size_t{1} << (decode_depth - 1)) < row_bits + 1) ++decode_depth;
    // The macro has no reset pin: its registers clear at power-on.
    const size_t slowest = std::max({leafDelay(Leaf::Not), leafDelay(Leaf::Nand), leafDelay(Leaf::And), leafDelay(Leaf::Or)});
    auto por = b.addNewComponent<circuit::memory::PowerOnReset>("POWER_ON_RESET", 16 * slowest);
    const auto reset = por->getOutputPin("OUT");
    const auto clock = this->getInputPin("CLK");
    const auto selected = w.invert("SELECTED", this->getInputPin("CE_N"));
    // A state bit loaded at an edge with CE_N low and held otherwise.
    const auto bit = [&](const std::string& name, PinPtr d) {
        auto state = b.add(circuit::families::ClockedStateBit, name);
        w.connect(clock, state->getInputPin("CLK"));
        w.connect(reset, state->getInputPin("RST"));
        const auto q = state->getOutputPin("Q");
        w.connect(w.either(name + "_NEXT", w.both(name + "_LOAD", selected, d), w.both(name + "_HOLD", w.invert(name + "_IDLE", selected), q)),
                  state->getInputPin("D"));
        return q;
    };
    // A register word loaded at an edge with CE_N low, as single bits.
    const auto word = [&](const std::string& name, PinPtr source, size_t bits) {
        std::vector<PinPtr> out;
        if (bits == 1) { out.push_back(bit(name, source)); return out; }
        auto r = b.add(circuit::families::registerWord(bits), name);
        w.connect(clock, r->getInputPin("CLK"));
        w.connect(reset, r->getInputPin("RST"));
        w.connect(selected, r->getInputPin("WE"));
        w.connect(source, r->getInputPinDynamic("D"));
        auto split = circuit::memory_device::bitAdapter(b, name + "_BITS", bits, true);
        w.connect(r->getOutputPinDynamic("Q"), split->getInputPinDynamic("IN"));
        for (size_t i = 0; i < bits; ++i) out.push_back(split->getOutputPin("OUT_" + std::to_string(i)));
        return out;
    };
    // The access token toggles at every access; the timer watches it alone.
    auto token = b.add(circuit::families::ClockedStateBit, "ACCESS_TOKEN");
    w.connect(clock, token->getInputPin("CLK"));
    w.connect(reset, token->getInputPin("RST"));
    w.connect(w.binary<XORGate>("TOKEN_NEXT", token->getOutputPin("Q"), selected), token->getInputPin("D"));
    // Accessed at least once since power-on.
    auto started = b.add(circuit::families::ClockedStateBit, "STARTED");
    w.connect(clock, started->getInputPin("CLK"));
    w.connect(reset, started->getInputPin("RST"));
    w.connect(w.either("STARTED_NEXT", started->getOutputPin("Q"), selected), started->getInputPin("D"));

    const auto address = word("ADDRESS_REGISTER", this->template getInputPin<32>("WORD_ADDR"), 32);
    const auto we_n = bit("WE_N_REGISTER", this->getInputPin("WE_N"));
    const auto data = word("DATA_REGISTER", this->template getInputPin<W>("WRITE_DATA"), W);
    const auto mask = word("MASK_REGISTER", this->template getInputPin<W / 8>("WRITE_MASK"), W / 8);
    constexpr size_t inputs = 1 + 32 + 1 + W + W / 8;
    // The same stages as the asynchronous access, started one register
    // delay after the edge; the settle window clears the previous access
    // before the registered inputs reach the array. With no settle window
    // in front, the precharge itself must cover a bitline the last access
    // swung fully (a write): restoring it takes the bitline's swing time,
    // and until then the write drive still reaches the cells.
    const size_t bitline = timing.sramBitline(config_.rows());
    auto timer = b.addNewComponent<circuit::memory::MemoryTimer>("ACCESS_TIMER", inputs, 1,
        std::vector<circuit::memory::MemoryTimerStage>{
            {std::max(timing.sram_precharge, bitline), 1}, {0, std::max<size_t>(decode_depth, 3) + 1},
            {bitline, decode_depth + circuit::sram::senseReplicaGates(config_.words_per_row)},
            {2 * leafDelay(Leaf::MemoryPrimitive), 2}}, static_cast<size_t>(-1), 1);
    size_t index = 0;
    const auto feed = [&](PinPtr source) -> PinPtr {
        const auto n = std::to_string(index++);
        w.connect(source, timer->getInputPin("IN_" + n));
        return timer->getOutputPin("HELD_" + n);
    };
    feed(token->getOutputPin("Q"));
    const auto join = [&](const std::string& name, const std::vector<PinPtr>& bits, std::shared_ptr<PinBase> target) {
        if (bits.size() == 1) { w.connect(feed(bits.front()), target); return; }
        auto joined = circuit::memory_device::bitAdapter(b, name, bits.size(), false);
        for (size_t i = 0; i < bits.size(); ++i) w.connect(feed(bits[i]), joined->getInputPin("IN_" + std::to_string(i)));
        w.connect(joined->getOutputPinDynamic("OUT"), target);
    };
    join("HELD_ADDRESS", address, this->template getOutputPin<32>("ADDRESS"));
    const auto held_we_n = feed(we_n);
    join("HELD_DATA", data, this->template getOutputPin<W>("DATA"));
    join("HELD_MASK", mask, this->template getOutputPin<W / 8>("MASK"));
    w.connect(started->getOutputPin("Q"), timer->getInputPin("ENABLE"));
    const auto writing = w.invert("WRITING", held_we_n);
    const auto stage = [&](size_t k) { return timer->getOutputPin("STAGE_" + std::to_string(k)); };
    w.connect(w.both("PRECHARGE_PHASE", stage(0), w.invert("PRECHARGED", stage(1))), this->getOutputPin("PRECHARGE"));
    w.connect(w.both("WRITE_PHASE", stage(1), writing), this->getOutputPin("WRITE_ENABLE"));
    w.connect(stage(2), this->getOutputPin("ROW_ENABLE"));
    w.connect(w.both("SENSE_PHASE", stage(3), held_we_n), this->getOutputPin("SENSE_ENABLE"));
    w.connect(stage(4), this->getOutputPin("READY"));
    w.connect(w.reduce("OUTPUT_PHASE", {stage(3), held_we_n, w.invert("OUTPUT_ON", this->getInputPin("OE_N"))}, true),
              this->getOutputPin("OUTPUT_ENABLE"));
    w.finish();
}

template<size_t W> SRAMColumnSelector<W>::SRAMColumnSelector(std::string name, size_t k)
    : IOComponent(std::move(name), [k](IOComponent* s) {
        SRAMDeviceConfig{k, k}.validate(W, true);
        s->addPin<32>("WORD_ADDR", PinType::INPUT);
        for (size_t bit = 0; bit < W; ++bit) {
            const auto n = std::to_string(bit);
            for (const auto* p : {"WRITE_BL_", "WRITE_BL_BAR_", "WRITE_DRIVE_"}) s->addPin(p + n, PinType::INPUT);
            for (const auto* p : {"READ_BL_", "READ_BL_BAR_", "READ_VALID_"}) s->addPin(p + n, PinType::OUTPUT);
        }
        for (size_t c = 0; c < W * k; ++c) {
            const auto n = std::to_string(c);
            for (const auto* p : {"BL_", "BL_BAR_", "VALID_"}) s->addPin(p + n, PinType::INPUT);
            for (const auto* p : {"DRIVE_", "DRIVE_BL_", "DRIVE_BL_BAR_"}) s->addPin(p + n, PinType::OUTPUT);
        }
    }), words_per_row_(k), type_("SRAMColumnSelector<" + std::to_string(W) + "," + std::to_string(k) + ">") { SRAMDeviceConfig{k, k}.validate(W, true); }
template<size_t W> void SRAMColumnSelector<W>::buildInternals(ComponentBuilder& b) {
    using namespace circuit::memory_detail;
    StructuralWiring w(b);
    size_t selectBits = 0; while ((size_t{1} << selectBits) < words_per_row_) ++selectBits;
    auto address = b.addNewComponent<BitSplitter<32>>("ADDRESS");
    w.connect(this->template getInputPin<32>("WORD_ADDR"), address->template getInputPin<32>("IN"));
    std::vector<StructuralWiring::PinPtr> selectPins;
    std::vector<StructuralWiring::PinPtr> invertedSelectPins;
    for (size_t bit = 0; bit < selectBits; ++bit)
        selectPins.push_back(address->getOutputPin("OUT_" + std::to_string(bit)));
    for (size_t bit = 0; bit < selectBits; ++bit)
        invertedSelectPins.push_back(w.invert("NOT_SELECT_" + std::to_string(bit), selectPins[bit]));
    const auto one = w.constant("ONE", true);
    std::vector<StructuralWiring::PinPtr> selects;
    for (size_t slot = 0; slot < words_per_row_; ++slot) {
        std::vector<StructuralWiring::PinPtr> terms;
        for (size_t bit = 0; bit < selectBits; ++bit) {
            terms.push_back(slot & (size_t{1} << bit) ? selectPins[bit] : invertedSelectPins[bit]);
        }
        selects.push_back(w.reduce("SELECT_" + std::to_string(slot), terms, true));
    }
    for (size_t bit = 0; bit < W; ++bit) {
        const auto lane = std::to_string(bit);
        auto pass_bl = w.both("PASS_BL_" + lane, this->getInputPin("WRITE_BL_" + lane), one);
        auto pass_bar = w.both("PASS_BAR_" + lane, this->getInputPin("WRITE_BL_BAR_" + lane), one);
        for (size_t slot = 0; slot < words_per_row_; ++slot) {
            const auto col = std::to_string(slot * W + bit);
            w.connect(w.both("DRIVE_" + col, selects[slot], this->getInputPin("WRITE_DRIVE_" + lane)), this->getOutputPin("DRIVE_" + col));
            w.connect(pass_bl, this->getOutputPin("DRIVE_BL_" + col));
            w.connect(pass_bar, this->getOutputPin("DRIVE_BL_BAR_" + col));
        }
        for (const std::string signal : {"BL", "BL_BAR", "VALID"}) {
            std::vector<StructuralWiring::PinPtr> inputs;
            for (size_t slot = 0; slot < words_per_row_; ++slot) inputs.push_back(this->getInputPin(signal + "_" + std::to_string(slot * W + bit)));
            auto selected = selectMuxTree(w, b, signal + "_" + lane, std::move(inputs), selectPins);
            // Current hierarchy pins need an internal driver for a boundary
            // input-to-output pass-through. No width-one splitter is needed.
            if (words_per_row_ == 1) selected = w.both("PASS_READ_" + signal + lane, selected, one);
            w.connect(selected, this->getOutputPin("READ_" + signal + "_" + lane));
        }
    }
    w.finish();
}

template<size_t W> SRAMOutputBuffer<W>::SRAMOutputBuffer(std::string name)
    : circuit::memory_device::MemoryOutputBuffer(std::move(name), W) {}

template<size_t W> void defineSRAMBankPins(IOComponent* s, SRAMDeviceConfig config) {
    config.validate(W, true);
    for (size_t r = 0; r < config.rows(); ++r) s->addPin("WL_" + std::to_string(r), PinType::INPUT);
    for (size_t c = 0; c < config.columns(W); ++c) {
        const auto n = std::to_string(c);
        for (const auto* p : {"PRECHARGE_", "DRIVE_", "WRITE_BL_", "WRITE_BL_BAR_"}) s->addPin(p + n, PinType::INPUT);
        for (const auto* p : {"BL_", "BL_BAR_", "VALID_"}) s->addPin(p + n, PinType::OUTPUT);
    }
}
template<size_t W> SRAMStorageBank<W>::SRAMStorageBank(std::string name, SRAMDeviceConfig config)
    : IOComponent(std::move(name), [config](IOComponent* s) { defineSRAMBankPins<W>(s, config); }), config_(config),
      type_("SRAMStorageBank<" + std::to_string(W) + "," + std::to_string(config.depth_words) + "," + std::to_string(config.words_per_row) + ">") { config.validate(W, true); }
template<size_t W> void SRAMStorageBank<W>::buildInternals(ComponentBuilder& b) {
    using namespace circuit::sram;
    circuit::memory_detail::StructuralWiring w(b);
    std::vector<std::shared_ptr<IOComponent>> rows;
    for (size_t r = 0; r < config_.rows(); ++r) {
        auto row = add<SRAMRow>(b, "memory.sram.row", "ROW_" + std::to_string(r), {{"columns", std::to_string(config_.columns(W))}}, config_.columns(W));
        w.connect(this->getInputPin("WL_" + std::to_string(r)), row->getInputPin("WL")); rows.push_back(row);
    }
    for (size_t c = 0; c < config_.columns(W); ++c) {
        const auto n = std::to_string(c);
        auto pair = add<SRAMBitlinePair>(b, "memory.sram.bitline-pair", "BITLINE_" + n, {{"rows", std::to_string(config_.rows())}}, config_.rows());
        for (const auto* pin : {"PRECHARGE", "DRIVE", "WRITE_BL", "WRITE_BL_BAR"}) w.connect(this->getInputPin(std::string(pin) + "_" + n), pair->getInputPin(pin));
        for (const auto* pin : {"BL", "BL_BAR", "VALID"}) w.connect(pair->getOutputPin(pin), this->getOutputPin(std::string(pin) + "_" + n));
        for (size_t r = 0; r < rows.size(); ++r) {
            w.connect(this->getInputPin("WL_" + std::to_string(r)), pair->getInputPin("WL_" + std::to_string(r)));
            w.connect(rows[r]->getOutputPin("Q_" + n), pair->getInputPin("Q_" + std::to_string(r)));
            w.connect(pair->getOutputPin("CELL_DRIVE"), rows[r]->getInputPin("DRIVE_" + n));
            w.connect(pair->getOutputPin("CELL_BL"), rows[r]->getInputPin("BL_" + n));
            w.connect(pair->getOutputPin("CELL_BL_BAR"), rows[r]->getInputPin("BL_BAR_" + n));
        }
    }
    w.finish();
}

template<size_t W> SRAM<W>::SRAM(std::string name, SRAMDeviceConfig config)
    : IOComponent(std::move(name), [clocked = config.clocked](IOComponent* s) { defineSRAMPins<W>(s, clocked); }), config_(config) { config.validate(W, true); }
// A large array: subarrays of SRAM_SUBARRAY_ROWS rows, each a whole SRAM
// device with its own self-timing. Requests reach the subarrays over a
// global wire, and each subarray drives a shared return bus for data and
// READY through its own tri-state buffers. The wire delay grows with the
// side of the subarray grid.
//
// Asynchronous: the high word-address bits select one subarray (its CE_N),
// the bus is enabled by the live select, and an address beyond the depth
// goes to subarray 0 with its top bit set, so that subarray answers with X
// and READY exactly as a single bank does.
//
// Clocked macro: every subarray takes each access (in parallel, so no
// slower), with CE_N and WE_N ungated. Subarray k sees the address with its
// select bits inverted where k has a one, so its own range check accepts
// only addresses meant for it (select field k, upper bits zero): an
// out-of-range access writes nothing and reads X in every subarray. The bus
// enables are decoded after the edge from a periphery register holding the
// raw address (an out-of-range address enables subarray 0). Nothing but the
// global wire lies between the macro's inputs and its registers, so the
// setup time is the registers' own; the clock is ideal (the timing analysis
// takes one undelayed clock).
template<size_t W> void SRAM<W>::buildSubarrays(ComponentBuilder& b) {
    using namespace circuit::sram;
    using circuit::memory_detail::StructuralWiring;
    using PinPtr = StructuralWiring::PinPtr;
    StructuralWiring w(b);
    const auto& timing = circuit::timing::activeTimingProfile().memory;
    const size_t sub_words = SRAM_SUBARRAY_ROWS * config_.words_per_row, count = sramSubarrays(config_);
    size_t low_bits = 0; while ((size_t{1} << low_bits) < sub_words) ++low_bits;
    size_t select_bits = 0; while ((size_t{1} << select_bits) < count) ++select_bits;
    size_t side = 1; while (side * side < count) ++side;
    const size_t wire = timing.sram_wire_per_side * side;
    const auto global = [&](const std::string& name, PinPtr source) {
        auto line = b.addNewComponent<circuit::memory::MemoryWire>(name, source->getWidth(), wire);
        w.connect(source, line->getInputPinDynamic("IN"));
        return line->getOutputPinDynamic("OUT");
    };
    const auto address = global("ADDRESS_WIRE", this->template getInputPin<32>("WORD_ADDR"));
    const auto data = global("DATA_WIRE", this->template getInputPin<W>("WRITE_DATA"));
    const auto mask = global("MASK_WIRE", this->template getInputPin<W / 8>("WRITE_MASK"));
    const auto ce_n = global("CE_WIRE", this->getInputPin("CE_N"));
    const auto we_n = global("WE_WIRE", this->getInputPin("WE_N"));
    const auto oe_n = global("OE_WIRE", this->getInputPin("OE_N"));
    const auto clock = config_.clocked ? PinPtr(this->getInputPin("CLK")) : PinPtr{};
    // The select field, forced to 0 for an address beyond the depth.
    const auto decode = [&](const std::string& prefix, PinPtr source, std::vector<PinPtr>* word_out) {
        auto bits = b.addNewComponent<BitSplitter<32>>(prefix + "ADDRESS_BITS");
        w.connect(source, bits->template getInputPin<32>("IN"));
        std::vector<PinPtr> word;
        for (size_t i = 0; i < 32; ++i) word.push_back(bits->getOutputPin("OUT_" + std::to_string(i)));
        const auto valid = circuit::memory_detail::addressBelow(w, word, config_.depth_words, prefix + "IN_RANGE");
        std::vector<PinPtr> select, select_bar;
        for (size_t i = 0; i < select_bits; ++i) {
            select.push_back(w.both(prefix + "SELECT_" + std::to_string(i), word[low_bits + i], valid));
            select_bar.push_back(w.invert(prefix + "NOT_SELECT_" + std::to_string(i), select.back()));
        }
        std::vector<PinPtr> chosen;
        for (size_t k = 0; k < count; ++k)
            chosen.push_back(circuit::memory_detail::decodeMatch(w, prefix + "CHOSEN_" + std::to_string(k), select, select_bar, k));
        if (word_out) { *word_out = word; word_out->push_back(w.invert(prefix + "OUT_OF_RANGE", valid)); }
        return chosen;
    };
    std::vector<PinPtr> word, chosen, enables, inverted;
    PinPtr invalid;
    if (!config_.clocked) {
        chosen = decode("", address, &word);
        invalid = word.back(); word.pop_back();
        const auto enabled = w.invert("ENABLED", ce_n);
        for (size_t k = 0; k < count; ++k) enables.push_back(w.both("BUS_ENABLE_" + std::to_string(k), chosen[k], enabled));
    } else {
        auto bits = b.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(address, bits->template getInputPin<32>("IN"));
        for (size_t i = 0; i < 32; ++i) word.push_back(bits->getOutputPin("OUT_" + std::to_string(i)));
        for (size_t i = 0; i < select_bits; ++i) inverted.push_back(w.invert("FLIPPED_SELECT_" + std::to_string(i), word[low_bits + i]));
        // The periphery's copy of the address, registered at each access
        // (CE_N low at the edge), decoded after the edge into the enables.
        using circuit::timing::Leaf;
        using circuit::timing::leafDelay;
        const size_t slowest = std::max({leafDelay(Leaf::Not), leafDelay(Leaf::Nand), leafDelay(Leaf::And), leafDelay(Leaf::Or)});
        auto por = b.addNewComponent<circuit::memory::PowerOnReset>("POWER_ON_RESET", 16 * slowest);
        auto held = b.add(circuit::families::registerWord(32), "PERIPHERY_ADDRESS");
        w.connect(this->getInputPin("CLK"), held->getInputPin("CLK"));
        w.connect(por->getOutputPin("OUT"), held->getInputPin("RST"));
        w.connect(w.invert("ACCESS", this->getInputPin("CE_N")), held->getInputPin("WE"));
        w.connect(this->template getInputPin<32>("WORD_ADDR"), held->getInputPinDynamic("D"));
        enables = decode("PERIPHERY_", held->getOutputPinDynamic("Q"), nullptr);
    }
    const auto low = config_.clocked ? PinPtr{} : w.constant("LOW", false);
    auto data_bus = b.addNewComponent<circuit::memory::MemoryBus>("READ_DATA_BUS", count, W, wire);
    // READY has a pull-down: low while no subarray drives it (deselected,
    // or before a clocked macro's first access), as in a single bank.
    auto ready_bus = b.addNewComponent<circuit::memory::MemoryBus>("READY_BUS", count, 1, wire, true);
    const auto suffix = "." + std::to_string(W);
    for (size_t k = 0; k < count; ++k) {
        const auto n = std::to_string(k);
        SRAMDeviceConfig sub = config_;
        sub.depth_words = std::min(sub_words, config_.depth_words - k * sub_words);
        auto part = add<SRAM<W>>(b, "memory.sram.device" + suffix, "SUBARRAY_" + n, sramParameters(sub), sub);
        auto local = b.addNewComponent<BitJoiner<32>>("LOCAL_ADDRESS_" + n);
        for (size_t i = 0; i < 32; ++i) {
            PinPtr bit;
            if (i < low_bits) bit = word[i];
            else if (config_.clocked) bit = i < low_bits + select_bits && (k >> (i - low_bits)) & 1 ? inverted[i - low_bits] : word[i];
            else bit = k == 0 && i == 31 ? invalid : low;
            w.connect(bit, local->getInputPin("IN_" + std::to_string(i)));
        }
        w.connect(local->template getOutputPin<32>("OUT"), part->template getInputPin<32>("WORD_ADDR"));
        if (config_.clocked) {
            w.connect(ce_n, part->getInputPin("CE_N"));
            w.connect(clock, part->getInputPin("CLK"));
        } else w.connect(w.either("CE_N_" + n, ce_n, w.invert("NOT_CHOSEN_" + n, chosen[k])), part->getInputPin("CE_N"));
        w.connect(we_n, part->getInputPin("WE_N"));
        w.connect(oe_n, part->getInputPin("OE_N"));
        w.connect(data, part->template getInputPin<W>("WRITE_DATA"));
        w.connect(mask, part->template getInputPin<W / 8>("WRITE_MASK"));
        auto drive = add<SRAMOutputBuffer<W>>(b, "memory.sram.output-buffer" + suffix, "BUS_DRIVER_" + n, {});
        w.connect(part->template getOutputPin<W>("READ_DATA"), drive->template getInputPin<W>("DATA"));
        w.connect(enables[k], drive->getInputPin("ENABLE"));
        w.connect(drive->template getOutputPin<W>("OUT"), data_bus->getInputPinDynamic("IN_" + n));
        auto ready_drive = b.addNewComponent<circuit::memory_device::MemoryOutputBuffer>("READY_DRIVER_" + n, 1);
        w.connect(part->getOutputPin("READY"), ready_drive->getInputPinDynamic("DATA"));
        w.connect(enables[k], ready_drive->getInputPin("ENABLE"));
        w.connect(ready_drive->getOutputPinDynamic("OUT"), ready_bus->getInputPinDynamic("IN_" + n));
    }
    w.connect(data_bus->getOutputPinDynamic("OUT"), this->template getOutputPin<W>("READ_DATA"));
    w.connect(ready_bus->getOutputPinDynamic("OUT"), this->getOutputPin("READY"));
    w.finish();
}

template<size_t W> void SRAM<W>::buildInternals(ComponentBuilder& b) {
    using namespace circuit::sram;
    using circuit::memory_detail::StructuralWiring;
    if (config_.rows() > SRAM_SUBARRAY_ROWS) { buildSubarrays(b); return; }
    StructuralWiring w(b);
    const auto params = sramParameters(config_);
    const auto suffix = "." + std::to_string(W);
    auto control = add<SRAMAccessControl<W>>(b, "memory.sram.access-control" + suffix, "ACCESS_CONTROL", params, config_);
    auto decode = add<SRAMRowDecoder>(b, "memory.sram.row-decoder", "ROW_DECODE", params, config_);
    auto storage = add<SRAMStorageBank<W>>(b, "memory.sram.bank" + suffix, "STORAGE", params, config_);
    auto precharge = add<SRAMPrecharge>(b, "memory.sram.precharge", "PRECHARGE", {{"columns", std::to_string(config_.columns(W))}}, config_.columns(W));
    auto columns = add<SRAMColumnSelector<W>>(b, "memory.sram.column-selector" + suffix, "COLUMN_SELECT", {{"words_per_row", std::to_string(config_.words_per_row)}}, config_.words_per_row);
    auto output = add<SRAMOutputBuffer<W>>(b, "memory.sram.output-buffer" + suffix, "OUTPUT", {});
    auto data = b.addNewComponent<BitSplitter<W>>("WRITE_DATA_BITS");
    std::shared_ptr<IOComponent> mask;
    if constexpr (W > 8) mask = b.addNewComponent<BitSplitter<W / 8>>("WRITE_MASK_BITS");
    auto read = b.addNewComponent<BitJoiner<W>>("READ_DATA_BITS");
    for (const auto& [pin, unused] : this->getAllInputPins()) w.connect(this->getInputPinDynamic(pin), control->getInputPinDynamic(pin));
    w.connect(control->template getOutputPin<32>("ADDRESS"), decode->getInputPin<32>("WORD_ADDR"));
    w.connect(control->template getOutputPin<32>("ADDRESS"), columns->template getInputPin<32>("WORD_ADDR"));
    w.connect(control->getOutputPin("ROW_ENABLE"), decode->getInputPin("ENABLE"));
    w.connect(control->getOutputPin("PRECHARGE"), precharge->getInputPin("ENABLE"));
    w.connect(control->template getOutputPin<W>("DATA"), data->template getInputPin<W>("IN"));
    if constexpr (W > 8) w.connect(control->template getOutputPin<W / 8>("MASK"), mask->template getInputPin<W / 8>("IN"));
    w.connect(control->getOutputPin("OUTPUT_ENABLE"), output->getInputPin("ENABLE"));
    w.connect(control->getOutputPin("READY"), this->getOutputPin("READY"));
    w.connect(read->template getOutputPin<W>("OUT"), output->template getInputPin<W>("DATA"));
    w.connect(output->template getOutputPin<W>("OUT"), this->template getOutputPin<W>("READ_DATA"));
    for (size_t r = 0; r < config_.rows(); ++r) w.connect(decode->getOutputPin("WL_" + std::to_string(r)), storage->getInputPin("WL_" + std::to_string(r)));
    for (size_t c = 0; c < config_.columns(W); ++c) {
        const auto n = std::to_string(c);
        w.connect(precharge->getOutputPin("PRECHARGE_" + n), storage->getInputPin("PRECHARGE_" + n));
        w.connect(columns->getOutputPin("DRIVE_" + n), storage->getInputPin("DRIVE_" + n));
        w.connect(columns->getOutputPin("DRIVE_BL_" + n), storage->getInputPin("WRITE_BL_" + n));
        w.connect(columns->getOutputPin("DRIVE_BL_BAR_" + n), storage->getInputPin("WRITE_BL_BAR_" + n));
        for (const auto* signal : {"BL_", "BL_BAR_", "VALID_"}) w.connect(storage->getOutputPin(signal + n), columns->getInputPin(signal + n));
    }
    for (size_t bit = 0; bit < W; ++bit) {
        const auto n = std::to_string(bit);
        auto driver = add<SRAMWriteDriver>(b, "memory.sram.write-driver", "WRITE_DRIVER_" + n, {});
        auto sense = add<SRAMSenseAmplifier>(b, "memory.sram.sense-amplifier", "SENSE_" + n, {});
        w.connect(data->getOutputPin("OUT_" + n), driver->getInputPin("D"));
        if constexpr (W == 8) w.connect(control->getOutputPin("MASK"), driver->getInputPin("MASK"));
        else w.connect(mask->getOutputPin("OUT_" + std::to_string(bit / 8)), driver->getInputPin("MASK"));
        w.connect(control->getOutputPin("WRITE_ENABLE"), driver->getInputPin("ENABLE"));
        for (const auto* signal : {"BL", "BL_BAR", "DRIVE"}) w.connect(driver->getOutputPin(signal), columns->getInputPin(std::string("WRITE_") + signal + "_" + n));
        for (const auto* signal : {"BL", "BL_BAR", "VALID"}) w.connect(columns->getOutputPin(std::string("READ_") + signal + "_" + n), sense->getInputPin(signal));
        w.connect(control->getOutputPin("SENSE_ENABLE"), sense->getInputPin("ENABLE"));
        w.connect(sense->getOutputPin("Q"), read->getInputPin("IN_" + n));
    }
    w.finish();
}
