#include "rv32i/RV32IElfImage.hpp"

#include "modules/memory/Memory64Kx32.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace rv32i {
namespace {
constexpr size_t Elf32HeaderSize = 52;
constexpr size_t Elf32ProgramHeaderSize = 32;
constexpr uint32_t ProgramHeaderLoad = 1;
constexpr uint16_t ElfTypeExecutable = 2;
constexpr uint16_t ElfMachineRiscV = 243;

[[noreturn]] void reject(const std::string& reason) {
    throw std::invalid_argument("RV32I ELF rejected: " + reason);
}

void requireRange(
    size_t offset,
    size_t count,
    size_t size,
    const std::string& label) {
    if (offset > size || count > size - offset) {
        reject(label + " is outside the file");
    }
}

uint16_t readU16(
    const std::vector<uint8_t>& bytes,
    size_t offset,
    const std::string& label) {
    requireRange(offset, 2, bytes.size(), label);
    return static_cast<uint16_t>(bytes[offset])
         | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
}

uint32_t readU32(
    const std::vector<uint8_t>& bytes,
    size_t offset,
    const std::string& label) {
    requireRange(offset, 4, bytes.size(), label);
    return static_cast<uint32_t>(bytes[offset])
         | (static_cast<uint32_t>(bytes[offset + 1]) << 8)
         | (static_cast<uint32_t>(bytes[offset + 2]) << 16)
         | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

bool isPowerOfTwo(uint32_t value) {
    return value != 0 && (value & (value - 1U)) == 0;
}

uint64_t endAddress(const RV32IElfSegment& segment) {
    return static_cast<uint64_t>(segment.address)
         + static_cast<uint64_t>(segment.bytes.size());
}
} // namespace

RV32IElfImage RV32IElfImage::fromBytes(
    const std::vector<uint8_t>& bytes) {
    requireRange(0, Elf32HeaderSize, bytes.size(), "ELF32 header");
    if (bytes[0] != 0x7f || bytes[1] != 'E'
        || bytes[2] != 'L' || bytes[3] != 'F') {
        reject("bad ELF magic");
    }
    if (bytes[4] != 1) {
        reject("image is not ELF32");
    }
    if (bytes[5] != 1) {
        reject("image is not little-endian");
    }
    if (bytes[6] != 1) {
        reject("unsupported ELF identification version");
    }
    if (readU16(bytes, 16, "e_type") != ElfTypeExecutable) {
        reject("image is not a statically linked executable");
    }
    if (readU16(bytes, 18, "e_machine") != ElfMachineRiscV) {
        reject("e_machine is not RISC-V");
    }
    if (readU32(bytes, 20, "e_version") != 1) {
        reject("unsupported ELF version");
    }
    if (readU16(bytes, 40, "e_ehsize") != Elf32HeaderSize) {
        reject("unexpected ELF32 header size");
    }
    if (readU16(bytes, 42, "e_phentsize")
        != Elf32ProgramHeaderSize) {
        reject("unexpected ELF32 program-header size");
    }

    const auto program_header_offset =
        readU32(bytes, 28, "e_phoff");
    const auto program_header_count =
        readU16(bytes, 44, "e_phnum");
    if (program_header_count == 0) {
        reject("image has no program headers");
    }
    const uint64_t program_header_bytes =
        static_cast<uint64_t>(program_header_count)
        * Elf32ProgramHeaderSize;
    if (program_header_bytes
        > std::numeric_limits<size_t>::max()) {
        reject("program-header table is too large");
    }
    requireRange(
        program_header_offset,
        static_cast<size_t>(program_header_bytes),
        bytes.size(),
        "program-header table");

    RV32IElfImage image;
    image.entry_point_ = readU32(bytes, 24, "e_entry");
    if ((image.entry_point_ & 0x3U) != 0) {
        reject("entry point is not four-byte aligned");
    }

    for (size_t index = 0;
         index < program_header_count;
         ++index) {
        const size_t offset =
            static_cast<size_t>(program_header_offset)
            + index * Elf32ProgramHeaderSize;
        if (readU32(bytes, offset, "p_type")
            != ProgramHeaderLoad) {
            continue;
        }

        const auto file_offset =
            readU32(bytes, offset + 4, "p_offset");
        const auto virtual_address =
            readU32(bytes, offset + 8, "p_vaddr");
        const auto physical_address =
            readU32(bytes, offset + 12, "p_paddr");
        const auto file_size =
            readU32(bytes, offset + 16, "p_filesz");
        const auto memory_size =
            readU32(bytes, offset + 20, "p_memsz");
        const auto flags =
            readU32(bytes, offset + 24, "p_flags");
        const auto alignment =
            readU32(bytes, offset + 28, "p_align");

        if (file_size > memory_size) {
            reject("PT_LOAD file size exceeds its memory size");
        }
        if (virtual_address != physical_address) {
            reject("PT_LOAD virtual and physical addresses differ");
        }
        if (alignment > 1) {
            if (!isPowerOfTwo(alignment)) {
                reject("PT_LOAD alignment is not a power of two");
            }
            if ((file_offset % alignment)
                != (physical_address % alignment)) {
                reject("PT_LOAD address and file offset violate alignment");
            }
        }
        requireRange(
            file_offset,
            file_size,
            bytes.size(),
            "PT_LOAD file range");
        const uint64_t memory_end =
            static_cast<uint64_t>(physical_address)
            + memory_size;
        if (memory_end
            > (uint64_t{1}
               << std::numeric_limits<uint32_t>::digits)) {
            reject("PT_LOAD address range overflows RV32");
        }
        if (memory_end > Memory64Kx32::capacityBytes()) {
            reject("PT_LOAD address range exceeds CircuitSim memory");
        }
        if (memory_size == 0) {
            continue;
        }

        RV32IElfSegment segment;
        segment.address = physical_address;
        segment.flags = flags;
        segment.bytes.assign(
            bytes.begin() + file_offset,
            bytes.begin() + file_offset + file_size);
        segment.bytes.resize(memory_size, 0);
        image.segments_.push_back(std::move(segment));
    }

    if (image.segments_.empty()) {
        reject("image has no non-empty PT_LOAD segments");
    }
    std::sort(
        image.segments_.begin(),
        image.segments_.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.address < rhs.address;
        });
    for (size_t index = 1;
         index < image.segments_.size();
         ++index) {
        if (endAddress(image.segments_[index - 1])
            > image.segments_[index].address) {
            reject("PT_LOAD segments overlap");
        }
    }

    const auto entry_segment = std::find_if(
        image.segments_.begin(),
        image.segments_.end(),
        [&](const auto& segment) {
            return segment.executable()
                && image.entry_point_ >= segment.address
                && static_cast<uint64_t>(image.entry_point_)
                    < endAddress(segment);
        });
    if (entry_segment == image.segments_.end()) {
        reject("entry point is not inside an executable PT_LOAD segment");
    }
    return image;
}

RV32IElfImage RV32IElfImage::fromFile(
    const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error(
            "Cannot open RV32I ELF file: " + path.string());
    }
    std::vector<uint8_t> bytes{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        throw std::runtime_error(
            "Cannot read RV32I ELF file: " + path.string());
    }
    return fromBytes(bytes);
}

void RV32IElfImage::requireFitsMemory(
    size_t capacity_bytes) const {
    for (const auto& segment : segments_) {
        const uint64_t end = endAddress(segment);
        if (end > capacity_bytes) {
            throw std::out_of_range(
                "RV32I ELF segment exceeds the configured memory");
        }
    }
    if (entry_point_ >= capacity_bytes) {
        throw std::out_of_range(
            "RV32I ELF entry point exceeds the configured memory");
    }
}

} // namespace rv32i
