#include "tests/RV32IElfTests.hpp"

#include "modules/memory/Memory64Kx32.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(
            "RV32IElfLoaderTest failed: " + message);
    }
}

void setU16(
    std::vector<uint8_t>& bytes,
    size_t offset,
    uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] =
        static_cast<uint8_t>(value >> 8);
}

void setU32(
    std::vector<uint8_t>& bytes,
    size_t offset,
    uint32_t value) {
    for (size_t byte = 0; byte < 4; ++byte) {
        bytes[offset + byte] =
            static_cast<uint8_t>(value >> (byte * 8));
    }
}

std::vector<uint8_t> validElf() {
    constexpr size_t ProgramOffset = 0x100;
    std::vector<uint8_t> bytes(ProgramOffset + 4, 0);
    bytes[0] = 0x7f;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 1;
    bytes[5] = 1;
    bytes[6] = 1;
    setU16(bytes, 16, 2);
    setU16(bytes, 18, 243);
    setU32(bytes, 20, 1);
    setU32(bytes, 24, 0);
    setU32(bytes, 28, 52);
    setU16(bytes, 40, 52);
    setU16(bytes, 42, 32);
    setU16(bytes, 44, 1);

    setU32(bytes, 52, 1);
    setU32(bytes, 56, ProgramOffset);
    setU32(bytes, 60, 0);
    setU32(bytes, 64, 0);
    setU32(bytes, 68, 4);
    setU32(bytes, 72, 8);
    setU32(bytes, 76, 5);
    setU32(bytes, 80, 0x100);

    setU32(bytes, ProgramOffset, 0x00100093U);
    return bytes;
}

template<typename Fn>
void requireRejected(Fn&& operation, const std::string& label) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(
        "RV32IElfLoaderTest failed: "
        + label + " was accepted");
}

void verifyValidElf() {
    const auto image =
        rv32i::RV32IElfImage::fromBytes(validElf());
    require(image.entryPoint() == 0, "entry point");
    require(image.segments().size() == 1, "segment count");
    const auto& segment = image.segments().front();
    require(segment.address == 0, "segment address");
    require(segment.executable(), "executable flag");
    require(segment.readable(), "readable flag");
    require(!segment.writable(), "writable flag");
    require(segment.bytes.size() == 8, "BSS expansion");
    require(
        segment.bytes[0] == 0x93
        && segment.bytes[1] == 0x00
        && segment.bytes[2] == 0x10
        && segment.bytes[3] == 0x00,
        "file bytes");
    require(
        segment.bytes[4] == 0
        && segment.bytes[5] == 0
        && segment.bytes[6] == 0
        && segment.bytes[7] == 0,
        "zero-filled memory tail");
    image.requireFitsMemory(Memory64Kx32::capacityBytes());
}

void verifyMalformedElfsAreRejected() {
    auto bytes = validElf();
    bytes[0] = 0;
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "bad magic");

    bytes = validElf();
    bytes[4] = 2;
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "ELF64 image");

    bytes = validElf();
    setU16(bytes, 18, 62);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "non-RISC-V machine");

    bytes = validElf();
    setU32(bytes, 68, 9);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "file range beyond image");

    bytes = validElf();
    setU32(bytes, 72, 3);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "file size larger than memory size");

    bytes = validElf();
    setU32(bytes, 60, 4);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "different virtual and physical addresses");

    bytes = validElf();
    setU32(bytes, 80, 3);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "non-power-of-two alignment");

    bytes = validElf();
    setU32(bytes, 76, 4);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "entry point outside executable segment");
}

void verifyMemoryLimit() {
    auto bytes = validElf();
    setU32(bytes, 24, 0x0003fff8U);
    setU32(bytes, 60, 0x0003fff8U);
    setU32(bytes, 64, 0x0003fff8U);
    setU32(bytes, 80, 4);
    const auto image =
        rv32i::RV32IElfImage::fromBytes(bytes);
    bool rejected = false;
    try {
        image.requireFitsMemory(
            Memory64Kx32::capacityBytes() - 1);
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    require(rejected, "memory limit must be enforced");

    bytes = validElf();
    setU32(bytes, 24, 0x0003fffcU);
    setU32(bytes, 60, 0x0003fffcU);
    setU32(bytes, 64, 0x0003fffcU);
    setU32(bytes, 80, 4);
    requireRejected(
        [&] { rv32i::RV32IElfImage::fromBytes(bytes); },
        "segment beyond CircuitSim memory");
}
} // namespace

std::string RV32IElfLoaderTest::getTestName() const {
    return "RV32IElfLoaderTest";
}

void RV32IElfLoaderTest::verifyResults() {
    verifyValidElf();
    verifyMalformedElfsAreRejected();
    verifyMemoryLimit();
}
