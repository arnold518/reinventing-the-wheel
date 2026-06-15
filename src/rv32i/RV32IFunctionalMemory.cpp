#include "rv32i/RV32IFunctionalMemory.hpp"

#include <algorithm>
#include <stdexcept>

namespace rv32i {

RV32IFunctionalMemory::RV32IFunctionalMemory(size_t byte_count)
    : bytes(byte_count, 0) {}

bool RV32IFunctionalMemory::canAccess(uint32_t address, size_t count) const {
    return count <= bytes.size() && address <= bytes.size() - count;
}

void RV32IFunctionalMemory::requireRange(uint32_t address, size_t count) const {
    if (!canAccess(address, count)) {
        throw std::out_of_range("RV32IFunctionalMemory address range is outside memory capacity");
    }
}

void RV32IFunctionalMemory::clear() {
    std::fill(bytes.begin(), bytes.end(), 0);
}

void RV32IFunctionalMemory::loadProgram(const RV32IProgram& program, uint32_t base_address) {
    loadBytes(base_address, program.bytes());
}

void RV32IFunctionalMemory::loadBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    requireRange(base_address, data.size());
    std::copy(data.begin(), data.end(), bytes.begin() + static_cast<size_t>(base_address));
}

void RV32IFunctionalMemory::loadWords(uint32_t base_address, const std::vector<uint32_t>& words) {
    if ((base_address & 0x3U) != 0) {
        throw std::invalid_argument("RV32IFunctionalMemory word load address must be 4-byte aligned");
    }

    std::vector<uint8_t> data;
    data.reserve(words.size() * 4);
    for (const auto word : words) {
        data.push_back(static_cast<uint8_t>(word & 0xFFU));
        data.push_back(static_cast<uint8_t>((word >> 8) & 0xFFU));
        data.push_back(static_cast<uint8_t>((word >> 16) & 0xFFU));
        data.push_back(static_cast<uint8_t>((word >> 24) & 0xFFU));
    }
    loadBytes(base_address, data);
}

std::vector<uint8_t> RV32IFunctionalMemory::readBytes(uint32_t base_address, size_t count) const {
    requireRange(base_address, count);
    return std::vector<uint8_t>(
        bytes.begin() + static_cast<size_t>(base_address),
        bytes.begin() + static_cast<size_t>(base_address) + count);
}

uint8_t RV32IFunctionalMemory::readU8(uint32_t address) const {
    requireRange(address, 1);
    return bytes[static_cast<size_t>(address)];
}

uint16_t RV32IFunctionalMemory::readU16(uint32_t address) const {
    requireRange(address, 2);
    return static_cast<uint16_t>(bytes[static_cast<size_t>(address)])
         | static_cast<uint16_t>(static_cast<uint16_t>(bytes[static_cast<size_t>(address) + 1]) << 8);
}

uint32_t RV32IFunctionalMemory::readU32(uint32_t address) const {
    requireRange(address, 4);
    return static_cast<uint32_t>(bytes[static_cast<size_t>(address)])
         | (static_cast<uint32_t>(bytes[static_cast<size_t>(address) + 1]) << 8)
         | (static_cast<uint32_t>(bytes[static_cast<size_t>(address) + 2]) << 16)
         | (static_cast<uint32_t>(bytes[static_cast<size_t>(address) + 3]) << 24);
}

void RV32IFunctionalMemory::writeU8(uint32_t address, uint8_t value) {
    requireRange(address, 1);
    bytes[static_cast<size_t>(address)] = value;
}

void RV32IFunctionalMemory::writeU16(uint32_t address, uint16_t value) {
    requireRange(address, 2);
    bytes[static_cast<size_t>(address)] = static_cast<uint8_t>(value & 0xFFU);
    bytes[static_cast<size_t>(address) + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void RV32IFunctionalMemory::writeU32(uint32_t address, uint32_t value) {
    requireRange(address, 4);
    bytes[static_cast<size_t>(address)] = static_cast<uint8_t>(value & 0xFFU);
    bytes[static_cast<size_t>(address) + 1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[static_cast<size_t>(address) + 2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    bytes[static_cast<size_t>(address) + 3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
}

}
