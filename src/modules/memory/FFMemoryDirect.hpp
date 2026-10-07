#pragma once

#include "components/BasicComponent.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "modules/memory/MemoryArrayInterface.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "modules/memory/MemoryDeviceStorage.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

// Private compact abstraction of the tested FFMemory structure.
// Keep edge, reset, mask, history and output-delay behavior identical.
template<size_t DataWidth>
class FFMemoryDirect : public BasicComponent, public MemoryDeviceStorage {
public:
    inline static const std::string TypeName = std::string("FFMemory") + std::to_string(DataWidth);
    const char* getTypeName() const override { return memoryDisplayType(TypeName.c_str()); }
    FFMemoryDirect(std::string name, MemoryConfig config = MemoryConfig{
                        .initial_value = LogicValue::UNKNOWN})
        : BasicComponent(std::move(name), config.read_delay_ticks,
                         defineMemoryArrayPins<DataWidth>),
          MemoryDeviceStorage(config.depth_words, DataWidth, config.initial_value, config.record_history,
                              config.storage_limits), config_(config) {
        if (config.read_delay_ticks == 0 || config.read_delay_ticks > 1'000'000)
            throw std::invalid_argument("Memory read delay must be 1..1000000 simulation ticks");
    }
    const MemoryStorage& memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t address, const std::vector<uint8_t>& data) override {
        if (started_) throw std::logic_error("Load memory before starting simulation");
        storage_.loadBytes(address, data);
    }
    const MemoryConfig& configuration() const noexcept { return config_; }
    void evaluate(size_t time, Simulator& sim) override {
        if (time > std::numeric_limits<size_t>::max() - delay)
            throw std::overflow_error("Memory output timestamp overflow");
        started_ = true;
        const auto clk = getInputValue("CLK"), rst = getInputValue("RST");
        const auto we = getInputValue("WE"), re = getInputValue("READ_EN");
        uint32_t word = 0, mask = 0;
        const bool address_known = readKnown("WORD_ADDR", word);
        const bool mask_known = readKnown("WRITE_MASK", mask);
        const bool selected = address_known && word < config_.depth_words;
        const bool active = we != LogicValue::LOW || re != LogicValue::LOW;
        LogicValue fault = LogicValue::LOW;
        if (active) {
            if (!known(we) || !known(re) || !address_known ||
                (we == LogicValue::HIGH && !mask_known)) fault = LogicValue::UNKNOWN;
            else if (!selected) fault = LogicValue::HIGH;
        }
        const bool reset_active = rst != LogicValue::LOW;
        if (rst == LogicValue::HIGH) storage_.fill(time, LogicValue::LOW);
        else if (!known(rst)) storage_.fill(time, LogicValue::UNKNOWN);
        const bool write_time = (previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH);
        {
            if (!reset_active && write_time && we == LogicValue::HIGH &&
                selected && mask_known && fault == LogicValue::LOW) {
                const auto bits = this->template getInputPin<DataWidth>("WRITE_DATA")->getValueAsVector();
                for (size_t byte = 0; byte < DataWidth / 8; ++byte) {
                    if (mask & (1u << byte)) storage_.writeLogic(time,
                        static_cast<uint32_t>(word * (DataWidth / 8) + byte),
                        {bits.begin() + byte * 8, bits.begin() + (byte + 1) * 8}, true);
                }
            }
        }
        previous_clk_ = clk;
        std::vector<LogicValue> data(DataWidth, LogicValue::LOW);
        if (!known(re) || (re == LogicValue::HIGH && !address_known)) data.assign(DataWidth, LogicValue::UNKNOWN);
        else if (re == LogicValue::HIGH && selected) data = storage_.readLogic(
            static_cast<uint32_t>(word * (DataWidth / 8)), DataWidth / 8);
        this->template _updateOutputWire<DataWidth>(sim, "READ_DATA", data, time);
        _updateOutputWire(sim, "FAULT", fault, time);
    }
private:
    static bool known(LogicValue bit) { return bit == LogicValue::LOW || bit == LogicValue::HIGH; }
    bool readKnown(const std::string& name, uint32_t& value) const {
        value = 0;
        const auto pin = getInputPinDynamic(name);
        for (size_t b = 0; b < pin->getWidth(); ++b) {
            const auto bit = pin->getBit(b);
            if (!known(bit)) return false;
            if (bit == LogicValue::HIGH) value |= uint32_t{1} << b;
        }
        return true;
    }
    MemoryConfig config_;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
