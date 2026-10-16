#pragma once
#include "modules/memory/SRAM.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"
#include "modules/memory/MemoryTimingParts.hpp"
#include "modules/memory/MemoryRangeCheck.hpp"
#include "components/TimingProfile.hpp"

template<size_t W> SRAMAccessControl<W>::SRAMAccessControl(std::string name, SRAMDeviceConfig config)
    : IOComponent(std::move(name), [](IOComponent* s) {
        s->addPin<32>("WORD_ADDR", PinType::INPUT);
        s->addPin<W>("WRITE_DATA", PinType::INPUT); s->addPin<W / 8>("WRITE_MASK", PinType::INPUT);
        for (const auto* p : {"CE_N", "WE_N", "OE_N"}) s->addPin(p, PinType::INPUT);
        s->addPin<32>("ADDRESS", PinType::OUTPUT); s->addPin<W>("DATA", PinType::OUTPUT);
        s->addPin<W / 8>("MASK", PinType::OUTPUT);
        for (const auto* p : {"PRECHARGE", "ROW_ENABLE", "WRITE_ENABLE", "SENSE_ENABLE", "OUTPUT_ENABLE", "READY"}) s->addPin(p, PinType::OUTPUT);
    }), config_(config) { config.validate(W); }
template<size_t W> void SRAMAccessControl<W>::buildInternals(ComponentBuilder& b) {
    using circuit::memory_detail::StructuralWiring;
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
            {timing.sram_precharge, 1}, {0, std::max<size_t>(decode_depth, 3) + 1}, {timing.sramBitline(config_.rows()), 1},
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
    : IOComponent(std::move(name), defineSRAMPins<W>), config_(config) { config.validate(W, true); }
// A large array: subarrays of SRAM_SUBARRAY_ROWS rows, each a whole SRAM
// device with its own self-timing. The high word-address bits select one
// subarray (its CE_N); the others stay idle. Requests reach the subarrays
// over a global wire and the selected subarray's data and READY come back
// over another, through a multiplexer tree on the same select bits. The wire
// delay grows with the side of the subarray grid. An address beyond the
// depth goes to subarray 0 with its top bit set, so that subarray answers
// with X and READY exactly as a single bank does.
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
    auto bits = b.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
    w.connect(address, bits->template getInputPin<32>("IN"));
    std::vector<PinPtr> word;
    for (size_t i = 0; i < 32; ++i) word.push_back(bits->getOutputPin("OUT_" + std::to_string(i)));
    const auto valid = circuit::memory_detail::addressBelow(w, word, config_.depth_words, "IN_RANGE");
    const auto invalid = w.invert("OUT_OF_RANGE", valid);
    // The select field, forced to 0 for an address beyond the depth.
    std::vector<PinPtr> select, select_bar;
    for (size_t i = 0; i < select_bits; ++i) {
        select.push_back(w.both("SELECT_" + std::to_string(i), word[low_bits + i], valid));
        select_bar.push_back(w.invert("NOT_SELECT_" + std::to_string(i), select.back()));
    }
    const auto low = w.constant("LOW", false);
    std::vector<PinPtr> data_out, ready_out;
    std::vector<std::vector<PinPtr>> data_bits(W);
    const auto suffix = "." + std::to_string(W);
    for (size_t k = 0; k < count; ++k) {
        const auto n = std::to_string(k);
        SRAMDeviceConfig sub = config_;
        sub.depth_words = std::min(sub_words, config_.depth_words - k * sub_words);
        auto part = add<SRAM<W>>(b, "memory.sram.device" + suffix, "SUBARRAY_" + n, sramParameters(sub), sub);
        auto local = b.addNewComponent<BitJoiner<32>>("LOCAL_ADDRESS_" + n);
        for (size_t i = 0; i < 32; ++i)
            w.connect(i < low_bits ? word[i] : (k == 0 && i == 31 ? invalid : low), local->getInputPin("IN_" + std::to_string(i)));
        w.connect(local->template getOutputPin<32>("OUT"), part->template getInputPin<32>("WORD_ADDR"));
        const auto chosen = circuit::memory_detail::decodeMatch(w, "CHOSEN_" + n, select, select_bar, k);
        w.connect(w.either("CE_N_" + n, ce_n, w.invert("NOT_CHOSEN_" + n, chosen)), part->getInputPin("CE_N"));
        w.connect(we_n, part->getInputPin("WE_N"));
        w.connect(oe_n, part->getInputPin("OE_N"));
        w.connect(data, part->template getInputPin<W>("WRITE_DATA"));
        w.connect(mask, part->template getInputPin<W / 8>("WRITE_MASK"));
        auto out = b.addNewComponent<BitSplitter<W>>("READ_BITS_" + n);
        w.connect(part->template getOutputPin<W>("READ_DATA"), out->template getInputPin<W>("IN"));
        for (size_t bit = 0; bit < W; ++bit) data_bits[bit].push_back(out->getOutputPin("OUT_" + std::to_string(bit)));
        ready_out.push_back(part->getOutputPin("READY"));
    }
    auto joined = b.addNewComponent<BitJoiner<W>>("READ_DATA_BITS");
    for (size_t bit = 0; bit < W; ++bit)
        w.connect(circuit::memory_detail::selectMuxTree(w, b, "RETURN_" + std::to_string(bit), data_bits[bit], select, data_bits[bit].front()),
                  joined->getInputPin("IN_" + std::to_string(bit)));
    const auto ready = circuit::memory_detail::selectMuxTree(w, b, "READY_RETURN", ready_out, select, ready_out.front());
    // The multiplexers pass no high impedance: a global output buffer drives
    // the data while the selected subarray is ready from a read with the chip
    // and its output selected, as a single bank's buffer does once settled.
    auto output = add<SRAMOutputBuffer<W>>(b, "memory.sram.output-buffer" + suffix, "OUTPUT", {});
    w.connect(joined->template getOutputPin<W>("OUT"), output->template getInputPin<W>("DATA"));
    w.connect(w.reduce("DRIVE_OUTPUT", {ready, w.invert("CHIP_ON", ce_n), w.invert("OUTPUT_ON", oe_n), we_n}, true), output->getInputPin("ENABLE"));
    w.connect(global("READ_DATA_WIRE", output->template getOutputPin<W>("OUT")), this->template getOutputPin<W>("READ_DATA"));
    w.connect(global("READY_WIRE", ready), this->getOutputPin("READY"));
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
