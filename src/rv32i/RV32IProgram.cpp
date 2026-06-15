#include "rv32i/RV32IProgram.hpp"

#include "modules/memory/BehavioralMemory64Kx32.hpp"
#include <stdexcept>
#include <utility>

namespace rv32i {

RV32IProgram::RV32IProgram(std::vector<uint8_t> bytes)
    : program_bytes(std::move(bytes)) {}

RV32IProgram RV32IProgram::fromBytes(std::vector<uint8_t> bytes) {
    return RV32IProgram(std::move(bytes));
}

RV32IProgram RV32IProgram::fromWords(const std::vector<uint32_t>& words) {
    std::vector<uint8_t> bytes;
    bytes.reserve(words.size() * 4);
    for (const auto word : words) {
        bytes.push_back(static_cast<uint8_t>(word & 0xFFU));
        bytes.push_back(static_cast<uint8_t>((word >> 8) & 0xFFU));
        bytes.push_back(static_cast<uint8_t>((word >> 16) & 0xFFU));
        bytes.push_back(static_cast<uint8_t>((word >> 24) & 0xFFU));
    }
    return RV32IProgram(std::move(bytes));
}

uint8_t RV32IProgram::byteAt(size_t index) const {
    if (index >= program_bytes.size()) {
        throw std::out_of_range("RV32IProgram byte index is outside program size");
    }
    return program_bytes[index];
}

uint32_t RV32IProgram::wordAt(size_t word_index) const {
    const size_t byte_index = word_index * 4;
    if (byte_index > program_bytes.size() || program_bytes.size() - byte_index < 4) {
        throw std::out_of_range("RV32IProgram word index is outside complete word range");
    }
    return static_cast<uint32_t>(program_bytes[byte_index])
         | (static_cast<uint32_t>(program_bytes[byte_index + 1]) << 8)
         | (static_cast<uint32_t>(program_bytes[byte_index + 2]) << 16)
         | (static_cast<uint32_t>(program_bytes[byte_index + 3]) << 24);
}

void RV32IProgram::loadInto(BehavioralMemory64Kx32& memory, uint32_t base_address) const {
    memory.loadBytes(base_address, program_bytes);
}

}
