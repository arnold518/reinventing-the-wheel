#pragma once

#include "components/BasicComponent.hpp"
#include "modules/memory/FFStorageBank.hpp"
#include <array>

// Compact abstraction of parallel FFWords. No decoder, read mux or child topology.
template<size_t Width>
class FFStorageBankBehavioral : public BasicComponent {
public:
    FFStorageBankBehavioral(std::string name, size_t depth)
        : BasicComponent(std::move(name), 1, [depth](IOComponent* self) {
            defineFFStorageBankPins<Width>(self, depth);
        }), words_(validateFFStorageBankDepth<Width>(depth)), type_(ffStorageBankTypeName<Width>(depth)) {
        for (auto& word : words_) word.fill(LogicValue::UNKNOWN);
    }
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(size_t time, Simulator& sim) override {
        const auto reset = getInputValue("RST"), clock = getInputValue("CLK");
        const auto data = this->template getInputPin<Width>("WRITE_DATA")->getValueAsVector();
        const auto mask = this->template getInputPin<Width / 8>("WRITE_MASK")->getValueAsVector();
        for (size_t n = 0; n < words_.size(); ++n) {
            auto& word = words_[n];
            if (reset == LogicValue::HIGH) word.fill(LogicValue::LOW);
            else if (reset != LogicValue::LOW) {
                for (auto& bit : word) bit = merge(bit, LogicValue::LOW);
            } else if (previous_clock_ == LogicValue::LOW && clock == LogicValue::HIGH) {
                const auto we = getInputValue("WRITE_" + std::to_string(n));
                for (size_t bit = 0; bit < Width; ++bit) {
                    if (we == LogicValue::LOW || mask[bit / 8] == LogicValue::LOW) continue;
                    const auto next = known(data[bit]);
                    word[bit] = we == LogicValue::HIGH && mask[bit / 8] == LogicValue::HIGH
                        ? next : (word[bit] == LogicValue::LOW && next == LogicValue::LOW
                            ? LogicValue::LOW : LogicValue::UNKNOWN);
                }
            }
            this->template _updateOutputWire<Width>(sim, "WORD_" + std::to_string(n),
                std::vector<LogicValue>(word.begin(), word.end()), time);
        }
        previous_clock_ = clock;
    }
private:
    static LogicValue known(LogicValue bit) {
        return bit == LogicValue::LOW || bit == LogicValue::HIGH ? bit : LogicValue::UNKNOWN;
    }
    static LogicValue merge(LogicValue a, LogicValue b) {
        return known(a) == known(b) ? known(a) : LogicValue::UNKNOWN;
    }
    std::vector<std::array<LogicValue, Width>> words_;
    std::string type_;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};
