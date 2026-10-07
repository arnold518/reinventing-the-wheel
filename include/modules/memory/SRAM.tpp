#pragma once
#include "modules/memory/SRAM.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/memory/MemoryPeripheralWiring.hpp"

template<size_t W> SRAMAccessControl<W>::SRAMAccessControl(std::string name, SRAMDeviceConfig config)
    : BasicComponent(std::move(name), 0, [](IOComponent* s) {
        s->addPin<32>("WORD_ADDR", PinType::INPUT);
        s->addPin<W>("WRITE_DATA", PinType::INPUT); s->addPin<W / 8>("WRITE_MASK", PinType::INPUT);
        for (const auto* p : {"CE_N", "WE_N", "OE_N"}) s->addPin(p, PinType::INPUT);
        s->addPin<32>("ADDRESS", PinType::OUTPUT); s->addPin<W>("DATA", PinType::OUTPUT);
        s->addPin<W / 8>("MASK", PinType::OUTPUT);
        for (const auto* p : {"PRECHARGE", "ROW_ENABLE", "WRITE_ENABLE", "SENSE_ENABLE", "OUTPUT_ENABLE"}) s->addPin(p, PinType::OUTPUT);
    }), config_(config) { config.validate(W); }
template<size_t W> void SRAMAccessControl<W>::evaluate(size_t t, Simulator& sim) {
    if (auto due = sequence_.update(*this, t, config_.phase_ticks)) sim.scheduleEvent(std::make_shared<ComponentEvalEvent>(*due, shared_from_this()));
    this->template _updateOutputWire<32>(sim, "ADDRESS", sequence_.address, t);
    this->template _updateOutputWire<W>(sim, "DATA", sequence_.data, t);
    this->template _updateOutputWire<W / 8>(sim, "MASK", sequence_.mask, t);
    for (const auto& [pin, value] : std::vector<std::pair<std::string, bool>>{
        {"PRECHARGE", sequence_.precharge}, {"ROW_ENABLE", sequence_.row_enable},
        {"WRITE_ENABLE", sequence_.write_enable}, {"SENSE_ENABLE", sequence_.sense_enable}, {"OUTPUT_ENABLE", sequence_.output_enable}})
        this->_updateOutputWire(sim, pin, value ? LogicValue::HIGH : LogicValue::LOW, t);
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
template<size_t W> void SRAM<W>::buildInternals(ComponentBuilder& b) {
    using namespace circuit::sram;
    using circuit::memory_detail::StructuralWiring;
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
