#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace rv32i {

struct RV32IElfSegment {
    uint32_t address = 0;
    uint32_t flags = 0;
    std::vector<uint8_t> bytes;

    bool executable() const { return (flags & 0x1U) != 0; }
    bool writable() const { return (flags & 0x2U) != 0; }
    bool readable() const { return (flags & 0x4U) != 0; }
};

/**
 * Strict loader for the small bare-metal ELF32 files accepted by CircuitSim.
 *
 * This intentionally accepts less than a general operating-system ELF loader:
 * RV32, little-endian, statically linked ET_EXEC images with unambiguous
 * physical/virtual addresses and non-overlapping PT_LOAD segments.
 */
class RV32IElfImage {
public:
    static RV32IElfImage fromBytes(const std::vector<uint8_t>& bytes);
    static RV32IElfImage fromFile(const std::filesystem::path& path);

    uint32_t entryPoint() const { return entry_point_; }
    const std::vector<RV32IElfSegment>& segments() const {
        return segments_;
    }

    void requireFitsMemory(size_t capacity_bytes) const;

private:
    uint32_t entry_point_ = 0;
    std::vector<RV32IElfSegment> segments_;
};

} // namespace rv32i
