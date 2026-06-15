#pragma once

#include "components/BasicComponent.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

class BehavioralMemory64Kx32 : public BasicComponent {
public:
    BehavioralMemory64Kx32(std::string name);
    static constexpr const char* TypeName = "BehavioralMemory64Kx32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

    static constexpr size_t capacityBytes() { return ByteCount; }
    static constexpr size_t capacityWords() { return WordCount; }

    bool canAccess(uint32_t address, size_t count) const;

    void clearContents();
    void loadBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadWords(uint32_t base_address, const std::vector<uint32_t>& words);
    std::vector<uint8_t> readBytes(uint32_t base_address, size_t count) const;
    uint8_t readByte(uint32_t address) const;
    uint32_t readWord(uint32_t address) const;
    uint8_t readU8(uint32_t address) const;
    uint16_t readU16(uint32_t address) const;
    uint32_t readU32(uint32_t address) const;
    void writeU8(uint32_t address, uint8_t value);
    void writeU16(uint32_t address, uint16_t value);
    void writeU32(uint32_t address, uint32_t value);

private:
    static constexpr size_t WordCount = 64 * 1024;
    static constexpr size_t BytesPerWord = 4;
    static constexpr size_t ByteCount = WordCount * BytesPerWord;

    std::vector<std::array<LogicValue, 8>> bytes;
    LogicValue previous_clk;
};
