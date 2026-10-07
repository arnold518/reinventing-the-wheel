#pragma once

#include "components/BasicComponent.hpp"
#include "modules/memory/FFWord.hpp"
#include <array>

// Private compact implementation. Equivalence is specified at settled binary
// checkpoints, not every transient of the structural splitter/gate network.
template<size_t Width>
class FFWordBehavioral : public BasicComponent {
public:
    explicit FFWordBehavioral(std::string name)
        : BasicComponent(std::move(name), 1, defineFFWordPins<Width>) {
        bits_.fill(LogicValue::UNKNOWN);
    }
    const char* getTypeName() const override { return FFWord<Width>::TypeName.c_str(); }
    void evaluate(size_t time, Simulator& sim) override {
        const auto reset = getInputValue("RST");
        const auto clock = getInputValue("CLK");
        if (reset == LogicValue::HIGH) bits_.fill(LogicValue::LOW);
        else if (reset != LogicValue::LOW) {
            for (auto& bit : bits_) bit = merge(bit, LogicValue::LOW);
        } else if (previous_clock_ == LogicValue::LOW && clock == LogicValue::HIGH) {
            const auto we = getInputValue("WE");
            const auto data = this->template getInputPin<Width>("WRITE_DATA")->getValueAsVector();
            const auto mask = this->template getInputPin<Width / 8>("WRITE_MASK")->getValueAsVector();
            for (size_t bit = 0; bit < Width; ++bit) {
                if (we == LogicValue::LOW || mask[bit / 8] == LogicValue::LOW) continue;
                const auto next = known(data[bit]);
                bits_[bit] = we == LogicValue::HIGH && mask[bit / 8] == LogicValue::HIGH
                    ? next : (bits_[bit] == LogicValue::LOW && next == LogicValue::LOW
                        ? LogicValue::LOW : LogicValue::UNKNOWN);
            }
        }
        previous_clock_ = clock;
        this->template _updateOutputWire<Width>(sim, "Q",
            std::vector<LogicValue>(bits_.begin(), bits_.end()), time);
    }
private:
    static LogicValue known(LogicValue bit) {
        return bit == LogicValue::LOW || bit == LogicValue::HIGH ? bit : LogicValue::UNKNOWN;
    }
    static LogicValue merge(LogicValue a, LogicValue b) {
        return known(a) == known(b) ? known(a) : LogicValue::UNKNOWN;
    }
    std::array<LogicValue, Width> bits_;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};
