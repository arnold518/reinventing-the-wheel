#include "tests/RV32IProgramTests.hpp"

#include "components/Component.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "rv32i/RV32IProgram.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using rv32i::RV32IProgram;

std::string hex32(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << value;
    return out.str();
}

void fail(const std::string& message) {
    throw std::runtime_error("RV32IProgramLoaderTest failed: " + message);
}

template <typename ActualT, typename ExpectedT>
void expectEq(const ActualT& actual, const ExpectedT& expected, const std::string& label) {
    if (actual != expected) {
        std::ostringstream out;
        out << label << ": expected " << expected << ", got " << actual;
        fail(out.str());
    }
}

void expectWord(uint32_t actual, uint32_t expected, const std::string& label) {
    if (actual != expected) {
        fail(label + ": expected " + hex32(expected) + ", got " + hex32(actual));
    }
}

void expectBytes(const std::vector<uint8_t>& actual,
                 const std::vector<uint8_t>& expected,
                 const std::string& label) {
    if (actual.size() != expected.size()) {
        std::ostringstream out;
        out << label << ": expected " << expected.size() << " bytes, got " << actual.size();
        fail(out.str());
    }

    for (size_t index = 0; index < actual.size(); ++index) {
        if (actual[index] != expected[index]) {
            std::ostringstream out;
            out << label << ": byte " << index
                << " expected 0x" << std::hex << static_cast<unsigned>(expected[index])
                << ", got 0x" << static_cast<unsigned>(actual[index]);
            fail(out.str());
        }
    }
}

template <typename ExceptionT, typename Fn>
void expectThrows(Fn&& fn, const std::string& label) {
    try {
        fn();
    } catch (const ExceptionT&) {
        return;
    } catch (const std::exception& error) {
        fail(label + ": threw wrong exception: " + error.what());
    }
    fail(label + ": did not throw");
}

void testProgramWordPacking() {
    const auto program = RV32IProgram::fromWords({
        0x00100093U, // addi x1, x0, 1
        0x00208113U, // addi x2, x1, 2
        0x00100073U, // ebreak
    });

    expectEq(program.empty(), false, "program should not be empty");
    expectEq(program.sizeBytes(), static_cast<size_t>(12), "program byte size");
    expectBytes(program.bytes(), {
        0x93, 0x00, 0x10, 0x00,
        0x13, 0x81, 0x20, 0x00,
        0x73, 0x00, 0x10, 0x00,
    }, "fromWords uses RV32I little-endian byte order");

    expectWord(program.wordAt(0), 0x00100093U, "word 0 readback");
    expectWord(program.wordAt(1), 0x00208113U, "word 1 readback");
    expectWord(program.wordAt(2), 0x00100073U, "word 2 readback");
    expectEq(static_cast<unsigned>(program.byteAt(8)), 0x73U, "byteAt returns program byte");

    expectThrows<std::out_of_range>([&] { program.byteAt(program.sizeBytes()); },
                                    "byteAt rejects index at end");
    expectThrows<std::out_of_range>([&] { program.wordAt(3); },
                                    "wordAt rejects word past end");

    const auto partial = RV32IProgram::fromBytes({0x01, 0x02, 0x03, 0x04, 0x05});
    expectWord(partial.wordAt(0), 0x04030201U, "wordAt accepts complete first word");
    expectThrows<std::out_of_range>([&] { partial.wordAt(1); },
                                    "wordAt rejects trailing partial word");
}

void testMemoryPreloadReadback() {
    auto memory = Component::create<Memory64Kx32>("PROGRAM_MEMORY");
    expectEq(Memory64Kx32::capacityWords(), static_cast<size_t>(64 * 1024), "memory word capacity");
    expectEq(Memory64Kx32::capacityBytes(), static_cast<size_t>(64 * 1024 * 4), "memory byte capacity");

    expectWord(memory->readWord(0), 0x00000000U, "memory initializes to zero");

    memory->loadBytes(3, {0xaa, 0xbb, 0xcc, 0xdd, 0xee});
    expectBytes(memory->readBytes(3, 5), {0xaa, 0xbb, 0xcc, 0xdd, 0xee},
                "loadBytes supports byte-granular base address");
    expectEq(static_cast<unsigned>(memory->readByte(5)), 0xccU, "readByte returns selected byte");
    expectWord(memory->readWord(4), 0xeeddccbbU, "aligned readWord assembles little-endian bytes");

    memory->loadWords(16, {0x11223344U, 0xaabbccddU});
    expectBytes(memory->readBytes(16, 8), {
        0x44, 0x33, 0x22, 0x11,
        0xdd, 0xcc, 0xbb, 0xaa,
    }, "loadWords stores words little-endian");
    expectWord(memory->readWord(16), 0x11223344U, "first loaded word");
    expectWord(memory->readWord(20), 0xaabbccddU, "second loaded word");

    const auto program = RV32IProgram::fromWords({
        0x00100093U,
        0x00208113U,
        0x00100073U,
    });
    program.loadInto(*memory, 0x100);
    expectWord(memory->readWord(0x100), 0x00100093U, "program load word 0");
    expectWord(memory->readWord(0x104), 0x00208113U, "program load word 1");
    expectWord(memory->readWord(0x108), 0x00100073U, "program load ebreak");

    const auto last_two = static_cast<uint32_t>(Memory64Kx32::capacityBytes() - 2);
    memory->loadBytes(last_two, {0x12, 0x34});
    expectBytes(memory->readBytes(last_two, 2), {0x12, 0x34}, "loadBytes accepts final valid byte range");

    expectThrows<std::out_of_range>(
        [&] { memory->loadBytes(static_cast<uint32_t>(Memory64Kx32::capacityBytes() - 1), {0x12, 0x34}); },
        "loadBytes rejects range crossing memory end");
    expectThrows<std::out_of_range>(
        [&] { memory->readBytes(static_cast<uint32_t>(Memory64Kx32::capacityBytes() - 1), 2); },
        "readBytes rejects range crossing memory end");
    expectThrows<std::invalid_argument>([&] { memory->loadWords(2, {0x12345678U}); },
                                        "loadWords requires aligned base");
    expectThrows<std::invalid_argument>([&] { memory->readWord(2); },
                                        "readWord requires aligned address");

    memory->clearContents();
    expectWord(memory->readWord(0x100), 0x00000000U, "clearContents clears program bytes");
    expectBytes(memory->readBytes(last_two, 2), {0x00, 0x00}, "clearContents clears final byte range");
}
}

void RV32IProgramLoaderTest::setupCircuit() {
    auto profile = circuit::withExactFidelity(
        circuit::canonicalDefaultProfile(),
        "RV32I_PROGRAM_LOADER_ROOT",
        circuit::Fidelity::Behavioral,
        "rv32i-program-loader");
    auto build = circuit::builtinComponentCatalog().createRoot(
        circuit::families::Memory64Kx32.request(
            "RV32I_PROGRAM_LOADER_ROOT"),
        std::move(profile));
    root = std::move(build.root);
    builder = std::make_unique<ComponentBuilder>(root);

    auto memory = std::dynamic_pointer_cast<Memory64Kx32>(root);
    if (!memory) {
        throw std::runtime_error("RV32IProgramLoaderTest root must be Memory64Kx32");
    }
    const auto program = RV32IProgram::fromWords({
        0x00100093U,
        0x00208113U,
        0x00100073U,
    });
    program.loadInto(*memory, 0);
}

std::string RV32IProgramLoaderTest::getTestName() const {
    return "RV32IProgramLoaderTest";
}

std::vector<SimulationTest::SimulationCheckpoint>
RV32IProgramLoaderTest::getCheckpoints() const {
    return {{
        0,
        "program-loaded",
        "Three RV32I instructions are preloaded at addresses 0x0, 0x4, and 0x8.",
        0,
    }};
}

void RV32IProgramLoaderTest::verifyResults() {
    testProgramWordPacking();
    testMemoryPreloadReadback();
}
