#include "tests/RV32IExternalValidationTests.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
uint64_t parseUnsigned(
    const std::string& text,
    const std::string& label) {
    const bool hexadecimal =
        text.size() > 2
        && text[0] == '0'
        && (text[1] == 'x' || text[1] == 'X');
    const char* begin =
        text.data() + (hexadecimal ? 2 : 0);
    const char* end =
        text.data() + text.size();
    uint64_t value = 0;
    const auto result = std::from_chars(
        begin,
        end,
        value,
        hexadecimal ? 16 : 10);
    if (result.ec != std::errc{}
        || result.ptr != end) {
        throw std::invalid_argument(
            "Invalid " + label + ": " + text);
    }
    return value;
}

void usage(const char* executable) {
    std::cerr
        << "Usage: " << executable
        << " <rv32i-elf> [tohost-address]"
        << " [maximum-instructions] [fidelity]\n"
        << "Defaults: tohost-address=0x3ffc0,"
        << " maximum-instructions=100000,"
        << " fidelity=both\n"
        << "Fidelity: behavioral, structural, or both\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 5) {
        usage(argv[0]);
        return 2;
    }
    try {
        const std::filesystem::path elf_path(argv[1]);
        const auto tohost =
            argc >= 3
                ? parseUnsigned(argv[2], "tohost address")
                : uint64_t{0x0003ffc0U};
        const auto maximum_instructions =
            argc >= 4
                ? parseUnsigned(
                    argv[3],
                    "maximum instruction count")
                : uint64_t{100000};
        if (tohost > UINT32_MAX
            || maximum_instructions > SIZE_MAX) {
            throw std::out_of_range(
                "Validation argument exceeds host width");
        }

        const std::string fidelity =
            argc >= 5 ? argv[4] : "both";
        rv32i::test::ExternalFixtureRunResult result;
        if (fidelity == "behavioral") {
            result = rv32i::test::runExternalFixture(
                elf_path,
                circuit::Fidelity::Behavioral,
                static_cast<uint32_t>(tohost),
                static_cast<size_t>(
                    maximum_instructions));
        } else if (fidelity == "structural") {
            result = rv32i::test::runExternalFixture(
                elf_path,
                circuit::Fidelity::Structural,
                static_cast<uint32_t>(tohost),
                static_cast<size_t>(
                    maximum_instructions));
        } else if (fidelity == "both") {
            result =
                rv32i::test::validateExternalFixture(
                    elf_path,
                    static_cast<uint32_t>(tohost),
                    static_cast<size_t>(
                        maximum_instructions))
                    .structural;
        } else {
            throw std::invalid_argument(
                "Invalid fidelity: " + fidelity);
        }
        std::cout
            << "PASS " << elf_path.string()
            << " fidelity=" << fidelity
            << " tohost=" << result.tohost
            << " instructions="
            << result.instruction_count
            << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
