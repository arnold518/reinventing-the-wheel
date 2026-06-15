#include "modules/memory/BehavioralMemory64Kx32.hpp"

#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <stdexcept>
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

std::array<LogicValue, 8> byteFromUInt8(uint8_t value) {
    std::array<LogicValue, 8> result{};
    for (size_t bit = 0; bit < 8; ++bit) {
        result[bit] = ((value >> bit) & 0x1U) ? LogicValue::HIGH : LogicValue::LOW;
    }
    return result;
}

uint8_t byteToUInt8(const std::array<LogicValue, 8>& value) {
    uint8_t result = 0;
    for (size_t bit = 0; bit < 8; ++bit) {
        if (value[bit] == LogicValue::HIGH) {
            result |= static_cast<uint8_t>(uint8_t{1} << bit);
        } else if (value[bit] != LogicValue::LOW) {
            throw std::logic_error("Cannot read unknown memory byte as uint8_t");
        }
    }
    return result;
}

void requireRange(uint32_t base_address, size_t count, size_t byte_count) {
    if (count > byte_count || base_address > byte_count - count) {
        throw std::out_of_range("BehavioralMemory64Kx32 address range is outside memory capacity");
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

bool BehavioralMemory64Kx32::canAccess(uint32_t address, size_t count) const {
    return count <= ByteCount && address <= ByteCount - count;
}

void BehavioralMemory64Kx32::clearContents() {
    const auto zero = zeroByte();
    for (auto& byte : bytes) {
        byte = zero;
    }
}

void BehavioralMemory64Kx32::loadBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    requireRange(base_address, data.size(), ByteCount);
    for (size_t index = 0; index < data.size(); ++index) {
        bytes[static_cast<size_t>(base_address) + index] = byteFromUInt8(data[index]);
    }
}

void BehavioralMemory64Kx32::loadWords(uint32_t base_address, const std::vector<uint32_t>& words) {
    if ((base_address & 0x3U) != 0) {
        throw std::invalid_argument("BehavioralMemory64Kx32 word load address must be 4-byte aligned");
    }

    requireRange(base_address, words.size() * BytesPerWord, ByteCount);
    std::vector<uint8_t> data;
    data.reserve(words.size() * BytesPerWord);
    for (const auto word : words) {
        data.push_back(static_cast<uint8_t>(word & 0xFFU));
        data.push_back(static_cast<uint8_t>((word >> 8) & 0xFFU));
        data.push_back(static_cast<uint8_t>((word >> 16) & 0xFFU));
        data.push_back(static_cast<uint8_t>((word >> 24) & 0xFFU));
    }
    loadBytes(base_address, data);
}

std::vector<uint8_t> BehavioralMemory64Kx32::readBytes(uint32_t base_address, size_t count) const {
    requireRange(base_address, count, ByteCount);
    std::vector<uint8_t> result;
    result.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        result.push_back(byteToUInt8(bytes[static_cast<size_t>(base_address) + index]));
    }
    return result;
}

uint8_t BehavioralMemory64Kx32::readByte(uint32_t address) const {
    requireRange(address, 1, ByteCount);
    return byteToUInt8(bytes[static_cast<size_t>(address)]);
}

uint32_t BehavioralMemory64Kx32::readWord(uint32_t address) const {
    if ((address & 0x3U) != 0) {
        throw std::invalid_argument("BehavioralMemory64Kx32 word read address must be 4-byte aligned");
    }

    const auto data = readBytes(address, BytesPerWord);
    return static_cast<uint32_t>(data[0])
         | (static_cast<uint32_t>(data[1]) << 8)
         | (static_cast<uint32_t>(data[2]) << 16)
         | (static_cast<uint32_t>(data[3]) << 24);
}

uint8_t BehavioralMemory64Kx32::readU8(uint32_t address) const {
    return readByte(address);
}

uint16_t BehavioralMemory64Kx32::readU16(uint32_t address) const {
    const auto data = readBytes(address, 2);
    return static_cast<uint16_t>(data[0])
         | static_cast<uint16_t>(static_cast<uint16_t>(data[1]) << 8);
}

uint32_t BehavioralMemory64Kx32::readU32(uint32_t address) const {
    return readWord(address);
}

void BehavioralMemory64Kx32::writeU8(uint32_t address, uint8_t value) {
    requireRange(address, 1, ByteCount);
    bytes[static_cast<size_t>(address)] = byteFromUInt8(value);
}

void BehavioralMemory64Kx32::writeU16(uint32_t address, uint16_t value) {
    requireRange(address, 2, ByteCount);
    bytes[static_cast<size_t>(address)] = byteFromUInt8(static_cast<uint8_t>(value & 0xFFU));
    bytes[static_cast<size_t>(address) + 1] = byteFromUInt8(static_cast<uint8_t>((value >> 8) & 0xFFU));
}

void BehavioralMemory64Kx32::writeU32(uint32_t address, uint32_t value) {
    if ((address & 0x3U) != 0) {
        throw std::invalid_argument("BehavioralMemory64Kx32 word write address must be 4-byte aligned");
    }

    requireRange(address, BytesPerWord, ByteCount);
    bytes[static_cast<size_t>(address)] = byteFromUInt8(static_cast<uint8_t>(value & 0xFFU));
    bytes[static_cast<size_t>(address) + 1] = byteFromUInt8(static_cast<uint8_t>((value >> 8) & 0xFFU));
    bytes[static_cast<size_t>(address) + 2] = byteFromUInt8(static_cast<uint8_t>((value >> 16) & 0xFFU));
    bytes[static_cast<size_t>(address) + 3] = byteFromUInt8(static_cast<uint8_t>((value >> 24) & 0xFFU));
}

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
