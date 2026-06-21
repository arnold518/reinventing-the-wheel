#include "modules/memory/BehavioralRegisterFile32x32.hpp"

#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <memory>
#include <utility>

namespace {
constexpr size_t RegisterCount = 32;
constexpr size_t WordWidth = 32;
constexpr size_t AddressWidth = 5;

std::vector<LogicValue> zeroWord() {
    return std::vector<LogicValue>(WordWidth, LogicValue::LOW);
}

std::vector<LogicValue> unknownWord() {
    return std::vector<LogicValue>(WordWidth, LogicValue::UNKNOWN);
}

LogicValue sanitizeBit(LogicValue value) {
    return (value == LogicValue::HIGH || value == LogicValue::LOW) ? value : LogicValue::UNKNOWN;
}

LogicValue andValue(LogicValue left, LogicValue right) {
    if (left == LogicValue::LOW || right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    if (left == LogicValue::HIGH && right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    return LogicValue::UNKNOWN;
}

std::vector<LogicValue> inputWord(const std::shared_ptr<Pin<WordWidth>>& pin) {
    std::vector<LogicValue> values(WordWidth, LogicValue::UNKNOWN);
    if (!pin) {
        return values;
    }
    for (size_t bit = 0; bit < WordWidth; ++bit) {
        values[bit] = sanitizeBit(pin->getBit(bit));
    }
    return values;
}

std::array<LogicValue, AddressWidth> inputAddress(const std::shared_ptr<Pin<AddressWidth>>& pin) {
    std::array<LogicValue, AddressWidth> bits{};
    bits.fill(LogicValue::UNKNOWN);
    if (!pin) {
        return bits;
    }
    for (size_t bit = 0; bit < AddressWidth; ++bit) {
        bits[bit] = sanitizeBit(pin->getBit(bit));
    }
    return bits;
}

LogicValue addressMatch(const std::array<LogicValue, AddressWidth>& address, size_t index) {
    bool has_unknown = false;
    for (size_t bit = 0; bit < AddressWidth; ++bit) {
        const bool expected_high = ((index >> bit) & 1U) != 0;
        if (address[bit] == LogicValue::UNKNOWN) {
            has_unknown = true;
            continue;
        }
        if ((address[bit] == LogicValue::HIGH) != expected_high) {
            return LogicValue::LOW;
        }
    }
    return has_unknown ? LogicValue::UNKNOWN : LogicValue::HIGH;
}

bool addressMayMatch(const std::array<LogicValue, AddressWidth>& address, size_t index) {
    return addressMatch(address, index) != LogicValue::LOW;
}

std::vector<LogicValue> readSelectedWord(
    const std::array<std::vector<LogicValue>, RegisterCount>& registers,
    const std::array<LogicValue, AddressWidth>& address
) {
    for (size_t index = 0; index < RegisterCount; ++index) {
        if (addressMatch(address, index) == LogicValue::HIGH) {
            return registers[index];
        }
    }

    std::vector<LogicValue> result(WordWidth, LogicValue::UNKNOWN);
    bool first_possible = true;
    for (size_t index = 0; index < RegisterCount; ++index) {
        if (!addressMayMatch(address, index)) {
            continue;
        }
        if (first_possible) {
            result = registers[index];
            first_possible = false;
            continue;
        }
        for (size_t bit = 0; bit < WordWidth; ++bit) {
            if (result[bit] != registers[index][bit]) {
                result[bit] = LogicValue::UNKNOWN;
            }
        }
    }
    return first_possible ? unknownWord() : result;
}

void updateOutputWord(
    BasicComponent& component,
    Simulator& simulator,
    const std::string& pin_name,
    const std::vector<LogicValue>& values,
    size_t event_time
) {
    auto pin = component.getOutputPin<WordWidth>(pin_name);
    if (!pin) {
        return;
    }
    pin->setValueFromVector(values);
    if (auto wire = pin->getExternalWire()) {
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<WordWidth>>(event_time, wire, values));
    } else {
        simulator.recordPinChange(event_time, pin, values);
    }
}
}

BehavioralRegisterFile32x32::BehavioralRegisterFile32x32(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
          self->addPin<5>("RS1_ADDR", PinType::INPUT);
          self->addPin<5>("RS2_ADDR", PinType::INPUT);
          self->addPin<5>("RD_ADDR", PinType::INPUT);
          self->addPin<32>("WRITE_DATA", PinType::INPUT);
          self->addPin("REG_WRITE", PinType::INPUT);
          self->addPin("CLK", PinType::INPUT);
          self->addPin("RST", PinType::INPUT);
          self->addPin<32>("RS1_DATA", PinType::OUTPUT);
          self->addPin<32>("RS2_DATA", PinType::OUTPUT);
      }),
      previous_clk(LogicValue::UNKNOWN) {
    for (auto& word : registers) {
        word = unknownWord();
    }
    registers[0] = zeroWord();
    recordRegisterHistory(0);
}

void BehavioralRegisterFile32x32::evaluate(size_t current_time, Simulator& simulator) {
    const auto rs1_addr = inputAddress(getInputPin<AddressWidth>("RS1_ADDR"));
    const auto rs2_addr = inputAddress(getInputPin<AddressWidth>("RS2_ADDR"));
    const auto rd_addr = inputAddress(getInputPin<AddressWidth>("RD_ADDR"));
    const auto write_data = inputWord(getInputPin<WordWidth>("WRITE_DATA"));
    const auto reg_write = sanitizeBit(getInputValue("REG_WRITE"));
    const auto clk = sanitizeBit(getInputValue("CLK"));
    const auto rst = sanitizeBit(getInputValue("RST"));
    const auto before_registers = registers;

    if (rst == LogicValue::HIGH) {
        for (size_t reg = 1; reg < RegisterCount; ++reg) {
            registers[reg] = zeroWord();
        }
    } else if (rst != LogicValue::LOW) {
        for (size_t reg = 1; reg < RegisterCount; ++reg) {
            registers[reg] = unknownWord();
        }
    } else if (previous_clk == LogicValue::LOW && clk == LogicValue::HIGH) {
        for (size_t reg = 1; reg < RegisterCount; ++reg) {
            const auto local_write_enable = andValue(reg_write, addressMatch(rd_addr, reg));
            if (local_write_enable == LogicValue::HIGH) {
                registers[reg] = write_data;
            } else if (local_write_enable == LogicValue::UNKNOWN) {
                registers[reg] = unknownWord();
            }
        }
    }

    registers[0] = zeroWord();
    if (registers != before_registers) {
        recordRegisterHistory(current_time);
    }

    previous_clk = clk;

    updateOutputWord(*this, simulator, "RS1_DATA", readSelectedWord(registers, rs1_addr), current_time + delay);
    updateOutputWord(*this, simulator, "RS2_DATA", readSelectedWord(registers, rs2_addr), current_time + delay);
}

std::vector<std::vector<LogicValue>> BehavioralRegisterFile32x32::getRegisterStateAtTime(size_t target_time) const {
    const RegisterSnapshot* snapshot = &registers;
    if (!register_history.empty()) {
        auto upper = std::upper_bound(
            register_history.begin(),
            register_history.end(),
            target_time,
            [](size_t time, const auto& entry) {
                return time < entry.first;
            });
        if (upper == register_history.begin()) {
            snapshot = &register_history.front().second;
        } else {
            snapshot = &(--upper)->second;
        }
    }

    std::vector<std::vector<LogicValue>> result;
    result.reserve(RegisterCount);
    for (const auto& word : *snapshot) {
        result.push_back(word);
    }
    return result;
}

void BehavioralRegisterFile32x32::recordRegisterHistory(size_t time) {
    if (!register_history.empty() && time < register_history.back().first) {
        register_history.clear();
    }

    if (!register_history.empty() && register_history.back().first == time) {
        register_history.back().second = registers;
        return;
    }

    if (!register_history.empty() && register_history.back().second == registers) {
        return;
    }

    register_history.emplace_back(time, registers);
}
