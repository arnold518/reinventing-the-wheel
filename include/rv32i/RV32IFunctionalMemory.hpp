#pragma once

#include "rv32i/RV32IProgram.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rv32i {

class RV32IFunctionalMemory {
public:
    static constexpr size_t DefaultCapacityBytes = 64 * 1024 * 4;

    explicit RV32IFunctionalMemory(size_t byte_count = DefaultCapacityBytes);

    size_t sizeBytes() const { return bytes.size(); }
    bool canAccess(uint32_t address, size_t count) const;

    void clear();
    void loadProgram(const RV32IProgram& program, uint32_t base_address = 0);
    void loadBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadWords(uint32_t base_address, const std::vector<uint32_t>& words);

    std::vector<uint8_t> readBytes(uint32_t base_address, size_t count) const;
    uint8_t readU8(uint32_t address) const;
    uint16_t readU16(uint32_t address) const;
    uint32_t readU32(uint32_t address) const;

    void writeU8(uint32_t address, uint8_t value);
    void writeU16(uint32_t address, uint16_t value);
    void writeU32(uint32_t address, uint32_t value);

private:
    std::vector<uint8_t> bytes;

    void requireRange(uint32_t address, size_t count) const;
};

}
