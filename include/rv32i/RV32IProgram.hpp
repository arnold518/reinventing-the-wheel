#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class BehavioralMemory64Kx32;

namespace rv32i {

class RV32IProgram {
public:
    RV32IProgram() = default;
    explicit RV32IProgram(std::vector<uint8_t> bytes);

    static RV32IProgram fromBytes(std::vector<uint8_t> bytes);
    static RV32IProgram fromWords(const std::vector<uint32_t>& words);

    const std::vector<uint8_t>& bytes() const { return program_bytes; }
    size_t sizeBytes() const { return program_bytes.size(); }
    bool empty() const { return program_bytes.empty(); }

    uint8_t byteAt(size_t index) const;
    uint32_t wordAt(size_t word_index) const;
    void loadInto(BehavioralMemory64Kx32& memory, uint32_t base_address = 0) const;

private:
    std::vector<uint8_t> program_bytes;
};

}
