// Read-only documentation probe. All decoding/execution stays in the reusable
// production reference libraries; expected values live in the documentation.
#include "rv32i/RV32IInstructionOracle.hpp"
#include "tests/RV32IProgramCases.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "components/Component.tpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
void printProgram(size_t number) {
    const auto fixture = rv32iProgramCase(number);
    rv32i::RV32IState state;
    state.pc = fixture.initial_pc;
    state.x = fixture.initial_registers;
    rv32i::RV32IFunctionalMemory imem(fixture.memory_size_bytes), dmem(fixture.memory_size_bytes);
    imem.loadProgram(fixture.program, fixture.program_base);
    for (const auto& data : fixture.initial_data) dmem.loadBytes(data.address, data.bytes);
    std::cout << std::dec << "{\"number\":" << number << ",\"name\":" << std::quoted(fixture.name)
              << ",\"base\":" << fixture.program_base
              << ",\"initialPC\":" << state.pc << ",\"capacity\":" << fixture.memory_size_bytes
              << ",\"initialRegisters\":[";
    for (size_t reg = 0; reg < 32; ++reg) {
        if (reg) std::cout << ',';
        std::cout << state.readRegister(reg);
    }
    std::cout << "],\"words\":[";
    for (size_t i = 0; i < fixture.program.sizeBytes() / 4; ++i) {
        if (i) std::cout << ',';
        std::cout << fixture.program.wordAt(i);
    }
    std::cout << "],\"initialData\":{";
    bool first = true;
    for (const auto& data : fixture.initial_data) {
        for (size_t i = 0; i < data.bytes.size(); ++i) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << '"' << data.address + i << "\":" << unsigned(data.bytes[i]);
        }
    }
    std::map<uint32_t, uint8_t> writes;
    std::cout << "},\"steps\":[";
    first = true;
    while (!state.halted && !state.trapped && state.instruction_count < fixture.max_instructions) {
        const auto trace = rv32i::RV32IInstructionOracle::step(state, imem, dmem);
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"pc\":" << trace.pc_before << ",\"nextPC\":" << trace.pc_after
                  << ",\"branchTaken\":" << trace.branch_taken << ",\"registers\":[";
        for (size_t reg = 0; reg < 32; ++reg) {
            if (reg) std::cout << ',';
            std::cout << state.readRegister(reg);
        }
        std::cout << "]}";
        if (trace.memory.kind == rv32i::RV32IMemoryAccessKind::Write && !trace.memory.fault) {
            const size_t bytes = trace.memory.size == rv32i::RV32IMemorySize::Byte ? 1
                               : trace.memory.size == rv32i::RV32IMemorySize::Halfword ? 2 : 4;
            for (size_t i = 0; i < bytes; ++i) {
                writes[trace.memory.address + i] = (trace.memory.write_data >> (8 * i)) & 0xff;
            }
        }
    }
    // Keep this probe tied to the fixture's explicit assertions as well as
    // returning the actual reference state for the Markdown checks.
    verifyRV32IProgramExpectedResult(fixture, state, writes, "RV32IReferenceDocumentationTest");
    std::cout << "],\"pc\":" << state.pc << ",\"halted\":" << state.halted
              << ",\"trapped\":" << state.trapped << ",\"cause\":" << unsigned(state.trap_cause)
              << ",\"count\":" << state.instruction_count << ",\"registers\":[";
    for (size_t reg = 0; reg < 32; ++reg) {
        if (reg) std::cout << ',';
        std::cout << state.readRegister(reg);
    }
    std::cout << "],\"writes\":{";
    first = true;
    for (const auto& [address, value] : writes) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << '"' << address << "\":" << unsigned(value);
    }
    const auto& expected = fixture.expected_result;
    std::cout << "},\"expected\":{\"pc\":" << expected.pc
              << ",\"count\":" << expected.instruction_count
              << ",\"halted\":" << expected.halted << ",\"trapped\":" << expected.trapped
              << ",\"cause\":" << unsigned(expected.trap_cause) << ",\"registers\":{";
    first = true;
    for (const auto& reg : expected.registers) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << '"' << unsigned(reg.index) << "\":" << reg.value;
    }
    std::cout << "},\"writes\":{";
    first = true;
    for (const auto& [address, value] : expected.bus_writes) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << '"' << address << "\":" << unsigned(value);
    }
    std::cout << "}}}\n";
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::invalid_argument("Usage: RV32IReferenceProbe decode|step|program|pins");
        const std::string mode = argv[1];
        if (mode == "pins") {
            // Instantiate only the public schema, not a full structural core.
            auto core = Component::create<IOComponent>("SCHEMA",
                circuit::families::RV32ISingleCycleCore.pinInitializer());
            for (auto& [name, pin] : core->getAllInputPins())
                std::cout << name << " Input " << pin->getWidth() << '\n';
            for (auto& [name, pin] : core->getAllOutputPins())
                std::cout << name << " Output " << pin->getWidth() << '\n';
            return 0;
        }
        if (mode == "program") {
            size_t number = 0;
            while (std::cin >> number) printProgram(number);
            if (!std::cin.eof()) throw std::invalid_argument("Malformed program number");
            return 0;
        }
        if (mode != "decode" && mode != "step") throw std::invalid_argument("Unknown mode");
        uint32_t word = 0;
        while (std::cin >> std::hex >> word) {
            const auto decoded = rv32i::RV32IDecoder::decode(word);
            if (mode == "decode") {
                std::cout << rv32i::toString(decoded.instruction) << ' '
                          << rv32i::toString(decoded.format) << ' '
                          << rv32i::toString(decoded.status) << ' '
                          << std::dec << unsigned(decoded.rd) << ' '
                          << unsigned(decoded.rs1) << ' ' << unsigned(decoded.rs2) << ' '
                          << decoded.immediate << ' ' << unsigned(decoded.shamt) << '\n';
                continue;
            }
            uint32_t a = 0, b = 0;
            if (!(std::cin >> std::hex >> a >> b)) throw std::invalid_argument("Missing A/B");
            rv32i::RV32IState state;
            state.pc = 0x40;
            state.writeRegister(6, a);
            state.writeRegister(7, b);
            rv32i::RV32IFunctionalMemory imem, dmem;
            imem.loadWords(0x40, {word});
            dmem.writeU32(0x100, 0x80ff7f01U);
            const auto trace = rv32i::RV32IInstructionOracle::step(state, imem, dmem);
            std::cout << rv32i::toString(decoded.instruction) << ' '
                      << std::hex << state.readRegister(5) << ' ' << state.pc << ' '
                      << dmem.readU32(0x100) << ' '
                      << (state.trapped ? rv32i::toString(state.trap_cause)
                                      : state.halted ? "Halt" : "Run") << ' '
                      << state.readRegister(0) << ' ' << state.readRegister(6) << ' '
                      << state.readRegister(7) << ' ' << state.instruction_count << '\n';
            (void)trace;
        }
        if (!std::cin.eof()) throw std::invalid_argument("Malformed hexadecimal input");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
