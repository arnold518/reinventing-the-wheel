#include "modules/memory/Memory64Kx32.hpp"
#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
constexpr size_t WordWidth = 32;
constexpr size_t AddressWidth = 32;
constexpr size_t SizeWidth = 2;
std::vector<LogicValue> unknownWord() { return std::vector<LogicValue>(32, LogicValue::UNKNOWN); }
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


std::vector<LogicValue> numericBits(size_t bytes, uint32_t value) {
    std::vector<LogicValue> result(bytes * 8, LogicValue::LOW);
    for (size_t bit = 0; bit < result.size(); ++bit)
        if ((value >> bit) & 1u) result[bit] = LogicValue::HIGH;
    return result;
}
void defineMemory64Kx32Pins(IOComponent* self) {
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
}
}

namespace circuit::families {
const ComponentFamily Memory64Kx32{
    "rv32i.memory.64k-x32",
    "Memory64Kx32",
    defineMemory64Kx32Pins,
    nullptr,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Memory64Kx32>(
            context, name);
    }};
}


Memory64Kx32::Memory64Kx32(std::string name)
    : BasicComponent(std::move(name), 1, circuit::families::Memory64Kx32.pinInitializer()),
      storage_(WordCount, 32, LogicValue::LOW), previous_clk(LogicValue::UNKNOWN) {}

bool Memory64Kx32::canAccess(uint32_t address, size_t count) const { return storage_.canAccess(address, count); }
void Memory64Kx32::clearContents() { storage_.clearContents(); }
void Memory64Kx32::loadBytes(uint32_t address, const std::vector<uint8_t>& data) { storage_.loadBytes(address, data); }
void Memory64Kx32::loadWords(uint32_t address, const std::vector<uint32_t>& words) {
    if (address % 4) throw std::invalid_argument("Memory64Kx32 word load address must be 4-byte aligned");
    if (words.size() > ByteCount / 4 || !canAccess(address, words.size() * 4))
        throw std::out_of_range("Memory64Kx32 load outside capacity");
    std::vector<uint8_t> data;
    data.reserve(words.size() * 4);
    for (auto word : words) for (size_t b = 0; b < 4; ++b) data.push_back(word >> (b * 8));
    loadBytes(address, data);
}
std::vector<uint8_t> Memory64Kx32::readBytes(uint32_t address, size_t count) const { return storage_.readBytes(address, count); }
uint8_t Memory64Kx32::readByte(uint32_t address) const { return readBytes(address, 1)[0]; }
uint32_t Memory64Kx32::readWord(uint32_t address) const {
    if (address % 4) throw std::invalid_argument("Memory64Kx32 word read address must be 4-byte aligned");
    const auto bytes = readBytes(address, 4);
    uint32_t value = 0;
    for (size_t b = 0; b < 4; ++b) value |= uint32_t{bytes[b]} << (8 * b);
    return value;
}
uint8_t Memory64Kx32::readU8(uint32_t address) const { return readByte(address); }
uint16_t Memory64Kx32::readU16(uint32_t address) const {
    const auto bytes = readBytes(address, 2);
    return uint16_t{bytes[0]} | (uint16_t{bytes[1]} << 8);
}
uint32_t Memory64Kx32::readU32(uint32_t address) const { return readWord(address); }
void Memory64Kx32::setHistoryRecordingEnabled(bool enabled) { storage_.setHistoryRecordingEnabled(enabled); }
bool Memory64Kx32::isHistoryRecordingEnabled() const noexcept { return storage_.isHistoryRecordingEnabled(); }
void Memory64Kx32::writeU8(uint32_t a, uint8_t v) { writeU8AtTime(0, a, v); }
void Memory64Kx32::writeU16(uint32_t a, uint16_t v) { writeU16AtTime(0, a, v); }
void Memory64Kx32::writeU32(uint32_t a, uint32_t v) { writeU32AtTime(0, a, v); }
void Memory64Kx32::writeU8AtTime(size_t t, uint32_t a, uint8_t v) { storage_.writeLogic(t, a, numericBits(1, v)); }
void Memory64Kx32::writeU16AtTime(size_t t, uint32_t a, uint16_t v) { storage_.writeLogic(t, a, numericBits(2, v)); }
void Memory64Kx32::writeU32AtTime(size_t t, uint32_t a, uint32_t v) {
    if (a % 4) throw std::invalid_argument("Memory64Kx32 word write address must be 4-byte aligned");
    storage_.writeLogic(t, a, numericBits(4, v));
}

void Memory64Kx32::evaluate(size_t time, Simulator& simulator) {
    const auto write_data = inputWord(getInputPin<32>("WRITE_DATA"));
    const auto read_en = sanitizeBit(getInputValue("READ_EN"));
    const auto write_en = sanitizeBit(getInputValue("WRITE_EN"));
    const auto sign = sanitizeBit(getInputValue("SIGN_EXTEND"));
    const auto clk = sanitizeBit(getInputValue("CLK"));
    const auto rst = sanitizeBit(getInputValue("RST"));
    uint32_t address = 0;
    uint8_t size = 0;
    const bool address_known = readUInt(getInputPin<32>("ADDR"), address);
    const bool size_known = readSize(getInputPin<2>("SIZE"), size);
    const auto width = size_known ? accessWidthBytes(size) : 0;
    LogicValue fault = accessHasUnknownEnable(read_en, write_en) ? LogicValue::UNKNOWN : LogicValue::LOW;
    if (isAccess(read_en, write_en)) {
        if (!address_known || !size_known) fault = LogicValue::UNKNOWN;
        else fault = width == 0 || isMisaligned(address, size) || isOutOfRange(address, width, ByteCount)
                     ? LogicValue::HIGH : LogicValue::LOW;
    }
    if (rst == LogicValue::HIGH) storage_.fill(time, LogicValue::LOW);
    else if (rst == LogicValue::UNKNOWN) storage_.fill(time, LogicValue::UNKNOWN);
    else if (previous_clk == LogicValue::LOW && clk == LogicValue::HIGH) {
        if (write_en == LogicValue::HIGH && fault == LogicValue::LOW && address_known && size_known && width > 0) {
            storage_.writeLogic(time, address, {write_data.begin(), write_data.begin() + width * 8}, true);
        } else if (write_en == LogicValue::UNKNOWN && address_known && size_known && width > 0 &&
                   !isMisaligned(address, size) && !isOutOfRange(address, width, ByteCount)) {
            storage_.writeLogic(time, address, std::vector<LogicValue>(width * 8, LogicValue::UNKNOWN));
        }
    }
    previous_clk = clk;
    auto data = unknownWord();
    if (address_known && size_known && width > 0 && !isMisaligned(address, size) && !isOutOfRange(address, width, ByteCount)) {
        data = storage_.readLogic(address, width);
        const auto extension = sign == LogicValue::HIGH ? data.back() :
                               sign == LogicValue::UNKNOWN ? LogicValue::UNKNOWN : LogicValue::LOW;
        data.resize(32, extension);
    }
    // Keep the historical legacy contract, including always-ready and ungated reads.
    updateOutputWord(*this, simulator, "READ_DATA", data, time + delay);
    _updateOutputWire(simulator, "READY", LogicValue::HIGH, time);
    _updateOutputWire(simulator, "FAULT", fault, time);
}

std::vector<Memory64Kx32::MemoryWordState> Memory64Kx32::getTouchedWordsAtTime(size_t t, size_t n) const { return storage_.getTouchedWordsAtTime(t, n); }
size_t Memory64Kx32::getTouchedWordCountAtTime(size_t t) const { return storage_.getTouchedWordCountAtTime(t); }
std::vector<Memory64Kx32::MemoryWordState> Memory64Kx32::getOccupiedWordsAtTime(size_t t, size_t n) const { return getTouchedWordsAtTime(t, n); }
size_t Memory64Kx32::getOccupiedWordCountAtTime(size_t t) const { return getTouchedWordCountAtTime(t); }
std::vector<Memory64Kx32::MemoryWordState> Memory64Kx32::getWordsAtTime(size_t t, uint32_t a, size_t n) const { return storage_.getWordsAtTime(t, a, n); }
std::map<uint32_t, uint8_t> Memory64Kx32::getByteWritesInTimeRange(size_t s, size_t e) const { return storage_.getByteWritesInTimeRange(s, e); }
