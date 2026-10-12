#pragma once
// Private factory implementation; callers use families::RegisterFile32x32.

#include "basic/LogicValueHelpers.hpp"
#include "components/BasicComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

class RegisterFile32x32Behavioral : public BasicComponent,
                                             public RegisterStateView {
public:
    RegisterFile32x32Behavioral(std::string name);
    static constexpr const char* TypeName = "RegisterFile32x32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(
        size_t target_time) const override;

private:
    using RegisterSnapshot = std::array<std::vector<LogicValue>, 32>;

    void recordRegisterHistory(size_t time);

    RegisterSnapshot registers;
    std::vector<std::pair<size_t, RegisterSnapshot>> register_history;
    LogicValue previous_clk;
};

// The register file's write rule for one evaluation, shared by every compact
// part that holds the 32 registers: an unknown reset, write enable or address
// leaves X exactly where the gate-level register file does. `Registers` is 32
// words of 32 four-state bits.
namespace rv32i::register_file {
template<class Registers, class Address, class Word>
void step(Registers& registers, LogicValue& previous_clock, LogicValue reset, LogicValue clock,
          LogicValue write, const Address& address, const Word& data) {
    using namespace circuit::logic_values;
    reset = sanitizeBit(reset); clock = sanitizeBit(clock);
    if (reset == LogicValue::HIGH) {
        for (auto& word : registers) for (auto& bit : word) bit = LogicValue::LOW;
    } else if (reset != LogicValue::LOW) {
        for (auto& word : registers) for (auto& bit : word) if (bit != LogicValue::LOW) bit = LogicValue::UNKNOWN;
    } else if (previous_clock == LogicValue::LOW && clock == LogicValue::HIGH) {
        for (size_t index = 1; index < 32; ++index) {
            auto enable = sanitizeBit(write);
            for (size_t bit = 0; bit < 5; ++bit)
                enable = andGate(enable, (index >> bit) & 1U ? sanitizeBit(address[bit]) : notGate(address[bit]));
            if (enable == LogicValue::LOW) continue;
            for (size_t bit = 0; bit < 32; ++bit)
                registers[index][bit] = enable == LogicValue::HIGH
                    ? sanitizeBit(data[bit]) : mux2(registers[index][bit], sanitizeBit(data[bit]), enable);
        }
    }
    for (auto& bit : registers[0]) bit = LogicValue::LOW;
    previous_clock = clock;
}
}
