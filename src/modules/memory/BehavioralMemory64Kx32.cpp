#include "modules/memory/BehavioralMemory64Kx32.hpp"

#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <memory>
#include <utility>

namespace {
constexpr size_t WordWidth = 32;
constexpr size_t AddressWidth = 32;
constexpr size_t SizeWidth = 2;

std::array<LogicValue, 8> zeroByte() {
    std::array<LogicValue, 8> value{};
    value.fill(LogicValue::LOW);
    return value;
}

std::array<LogicValue, 8> unknownByte() {
    std::array<LogicValue, 8> value{};
    value.fill(LogicValue::UNKNOWN);
    return value;
}

std::vector<LogicValue> zeroWord() {
    return std::vector<LogicValue>(WordWidth, LogicValue::LOW);
}

std::vector<LogicValue> unknownWord() {
    return std::vector<LogicValue>(WordWidth, LogicValue::UNKNOWN);
}

LogicValue sanitizeBit(LogicValue value) {
    return (value == LogicValue::HIGH || value == LogicValue::LOW) ? value : LogicValue::UNKNOWN;
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

bool readUInt(const std::shared_ptr<Pin<AddressWidth>>& pin, uint32_t& value) {
    if (!pin) {
        return false;
    }

    value = 0;
    for (size_t bit = 0; bit < AddressWidth; ++bit) {
        const auto bit_value = sanitizeBit(pin->getBit(bit));
        if (bit_value == LogicValue::UNKNOWN) {
            return false;
        }
        if (bit_value == LogicValue::HIGH) {
            value |= (uint32_t{1} << bit);
        }
    }
    return true;
}

bool readSize(const std::shared_ptr<Pin<SizeWidth>>& pin, uint8_t& value) {
    if (!pin) {
        return false;
    }

    value = 0;
    for (size_t bit = 0; bit < SizeWidth; ++bit) {
        const auto bit_value = sanitizeBit(pin->getBit(bit));
        if (bit_value == LogicValue::UNKNOWN) {
            return false;
        }
        if (bit_value == LogicValue::HIGH) {
            value |= static_cast<uint8_t>(uint8_t{1} << bit);
        }
    }
    return true;
}

bool isAccess(LogicValue read_en, LogicValue write_en) {
    return read_en == LogicValue::HIGH || write_en == LogicValue::HIGH;
}

bool accessHasUnknownEnable(LogicValue read_en, LogicValue write_en) {
    return read_en == LogicValue::UNKNOWN || write_en == LogicValue::UNKNOWN;
}

size_t accessWidthBytes(uint8_t size) {
    switch (size) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        default: return 0;
    }
}

bool isMisaligned(uint32_t address, uint8_t size) {
    switch (size) {
        case 0: return false;
        case 1: return (address & 0x1U) != 0;
        case 2: return (address & 0x3U) != 0;
        default: return true;
    }
}

bool isOutOfRange(uint32_t address, size_t width, size_t byte_count) {
    if (width == 0) {
        return false;
    }
    return address >= byte_count || width > byte_count - static_cast<size_t>(address);
}

void copyWriteByte(
    std::vector<std::array<LogicValue, 8>>& bytes,
    size_t address,
    size_t byte_index,
    const std::vector<LogicValue>& write_data
) {
    for (size_t bit = 0; bit < 8; ++bit) {
        bytes[address + byte_index][bit] = write_data[byte_index * 8 + bit];
    }
}

std::vector<LogicValue> readValue(
    const std::vector<std::array<LogicValue, 8>>& bytes,
    uint32_t address,
    uint8_t size,
    LogicValue sign_extend
) {
    const auto width = accessWidthBytes(size);
    if (width == 0) {
        return unknownWord();
    }

    auto result = zeroWord();
    for (size_t byte = 0; byte < width; ++byte) {
        for (size_t bit = 0; bit < 8; ++bit) {
            result[byte * 8 + bit] = bytes[static_cast<size_t>(address) + byte][bit];
        }
    }

    if (width == 4) {
        return result;
    }

    const auto sign_bit = result[width * 8 - 1];
    LogicValue extend_bit = LogicValue::LOW;
    if (sign_extend == LogicValue::HIGH) {
        extend_bit = sign_bit;
    } else if (sign_extend == LogicValue::UNKNOWN) {
        extend_bit = LogicValue::UNKNOWN;
    }

    for (size_t bit = width * 8; bit < WordWidth; ++bit) {
        result[bit] = extend_bit;
    }
    return result;
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
    }
}
}

BehavioralMemory64Kx32::BehavioralMemory64Kx32(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
          self->addPin<32>("ADDR", PinType::INPUT);
          self->addPin<32>("WRITE_DATA", PinType::INPUT);
          self->addPin("READ_EN", PinType::INPUT);
          self->addPin("WRITE_EN", PinType::INPUT);
          self->addPin<2>("SIZE", PinType::INPUT);
          self->addPin("SIGN_EXTEND", PinType::INPUT);
          self->addPin("CLK", PinType::INPUT);
          self->addPin("RST", PinType::INPUT);
          self->addPin<32>("READ_DATA", PinType::OUTPUT);
          self->addPin("READY", PinType::OUTPUT);
          self->addPin("FAULT", PinType::OUTPUT);
      }),
      bytes(ByteCount, zeroByte()),
      previous_clk(LogicValue::UNKNOWN) {}

void BehavioralMemory64Kx32::evaluate(size_t current_time, Simulator& simulator) {
    const auto write_data = inputWord(getInputPin<WordWidth>("WRITE_DATA"));
    const auto read_en = sanitizeBit(getInputValue("READ_EN"));
    const auto write_en = sanitizeBit(getInputValue("WRITE_EN"));
    const auto sign_extend = sanitizeBit(getInputValue("SIGN_EXTEND"));
    const auto clk = sanitizeBit(getInputValue("CLK"));
    const auto rst = sanitizeBit(getInputValue("RST"));

    uint32_t address = 0;
    uint8_t size = 0;
    const bool address_known = readUInt(getInputPin<AddressWidth>("ADDR"), address);
    const bool size_known = readSize(getInputPin<SizeWidth>("SIZE"), size);
    const size_t width = size_known ? accessWidthBytes(size) : 0;

    LogicValue fault = LogicValue::LOW;
    if (accessHasUnknownEnable(read_en, write_en)) {
        fault = LogicValue::UNKNOWN;
    }
    if (isAccess(read_en, write_en)) {
        if (!address_known || !size_known) {
            fault = LogicValue::UNKNOWN;
        } else if (width == 0 || isMisaligned(address, size) || isOutOfRange(address, width, ByteCount)) {
            fault = LogicValue::HIGH;
        } else {
            fault = LogicValue::LOW;
        }
    }

    if (rst == LogicValue::HIGH) {
        const auto zero = zeroByte();
        for (auto& byte : bytes) {
            byte = zero;
        }
    } else if (rst == LogicValue::UNKNOWN) {
        const auto unknown = unknownByte();
        for (auto& byte : bytes) {
            byte = unknown;
        }
    } else if (previous_clk == LogicValue::LOW && clk == LogicValue::HIGH) {
        if (write_en == LogicValue::HIGH) {
            if (fault == LogicValue::LOW && address_known && size_known && width > 0) {
                for (size_t byte = 0; byte < width; ++byte) {
                    copyWriteByte(bytes, address, byte, write_data);
                }
            }
        } else if (write_en == LogicValue::UNKNOWN && address_known && size_known && width > 0
                   && !isMisaligned(address, size) && !isOutOfRange(address, width, ByteCount)) {
            const auto unknown = unknownByte();
            for (size_t byte = 0; byte < width; ++byte) {
                bytes[static_cast<size_t>(address) + byte] = unknown;
            }
        }
    }

    previous_clk = clk;

    std::vector<LogicValue> read_data = unknownWord();
    if (address_known && size_known && width > 0 && !isMisaligned(address, size) && !isOutOfRange(address, width, ByteCount)) {
        read_data = readValue(bytes, address, size, sign_extend);
    }

    updateOutputWord(*this, simulator, "READ_DATA", read_data, current_time + delay);
    _updateOutputWire(simulator, "READY", LogicValue::HIGH, current_time);
    _updateOutputWire(simulator, "FAULT", fault, current_time);
}
