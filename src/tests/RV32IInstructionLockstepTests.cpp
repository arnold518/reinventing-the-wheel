#include "tests/RV32IInstructionLockstepTests.hpp"

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
uint32_t maskSigned(int32_t value, unsigned bits) {
    return static_cast<uint32_t>(value) & ((uint32_t{1} << bits) - 1);
}

uint32_t encodeI(int32_t imm, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode = 0x13) {
    return (maskSigned(imm, 12) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (static_cast<uint32_t>(rd) << 7)
         | opcode;
}

uint32_t encodeS(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t encoded = maskSigned(imm, 12);
    return (((encoded >> 5) & 0x7F) << 25)
         | (static_cast<uint32_t>(rs2) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | ((encoded & 0x1F) << 7)
         | 0x23U;
}

uint32_t encodeB(int32_t imm, uint8_t rs2, uint8_t rs1, uint8_t funct3) {
    const uint32_t encoded = maskSigned(imm, 13);
    return (((encoded >> 12) & 0x1) << 31)
         | (((encoded >> 5) & 0x3F) << 25)
         | (static_cast<uint32_t>(rs2) << 20)
         | (static_cast<uint32_t>(rs1) << 15)
         | (static_cast<uint32_t>(funct3) << 12)
         | (((encoded >> 1) & 0xF) << 8)
         | (((encoded >> 11) & 0x1) << 7)
         | 0x63U;
}

std::string hex32(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

std::string byteWritesToHex(const std::map<uint32_t, uint8_t>& writes) {
    if (writes.empty()) {
        return "{}";
    }

    std::ostringstream out;
    out << "{" << std::hex << std::setfill('0');
    bool first = true;
    for (const auto& [address, value] : writes) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << "0x" << std::setw(8) << address << ":0x" << std::setw(2) << static_cast<unsigned>(value);
    }
    out << "}";
    return out.str();
}

std::string yesNo(bool value) {
    return value ? "true" : "false";
}

std::string writebackSummary(const rv32i::RV32IInstructionTrace& trace) {
    if (!trace.writeback.enabled) {
        return "wb=None";
    }

    std::ostringstream out;
    out << "wb=x" << static_cast<unsigned>(trace.writeback.rd)
        << "=" << hex32(trace.writeback.value);
    if (trace.writeback.ignored_x0) {
        out << " ignored-x0";
    }
    return out.str();
}

std::string memorySummary(const rv32i::RV32IMemoryTrace& memory) {
    if (memory.kind == rv32i::RV32IMemoryAccessKind::None) {
        return "mem=None";
    }

    std::ostringstream out;
    out << "mem=" << rv32i::toString(memory.kind)
        << " " << rv32i::toString(memory.size)
        << " @" << hex32(memory.address);
    if (memory.kind == rv32i::RV32IMemoryAccessKind::Read) {
        out << " -> " << hex32(memory.read_data);
        if (memory.sign_extend) {
            out << " sign-extend";
        }
    } else if (memory.kind == rv32i::RV32IMemoryAccessKind::Write) {
        out << " <- " << hex32(memory.write_data);
    }
    if (memory.fault) {
        out << " fault";
    }
    return out.str();
}

std::string branchSummary(const rv32i::RV32IInstructionTrace& trace) {
    if (trace.control.branch == rv32i::RV32IBranchType::None && trace.control.jump == rv32i::RV32IJumpType::None) {
        return "flow=next";
    }

    std::ostringstream out;
    if (trace.control.branch != rv32i::RV32IBranchType::None) {
        out << "branch=" << rv32i::toString(trace.control.branch)
            << " taken=" << yesNo(trace.branch_taken);
    } else {
        out << "jump=" << rv32i::toString(trace.control.jump);
    }
    return out.str();
}

SimulationTest::SimulationCheckpoint instructionCheckpoint(const rv32i::RV32IInstructionTrace& trace,
                                                           size_t time) {
    std::ostringstream label;
    label << "I" << trace.instruction_index << " "
          << rv32i::toString(trace.decoded.instruction)
          << " @ " << hex32(trace.pc_before);

    std::ostringstream detail;
    detail << "raw=" << hex32(trace.raw_instruction)
           << ", pc_after=" << hex32(trace.pc_after)
           << ", " << writebackSummary(trace)
           << ", " << memorySummary(trace.memory)
           << ", " << branchSummary(trace);
    if (trace.halted) {
        detail << ", halted=true";
    }
    if (trace.trapped) {
        detail << ", trapped=true cause=" << rv32i::toString(trace.trap_cause);
    }

    return {
        time,
        label.str(),
        detail.str(),
        static_cast<size_t>(trace.instruction_index),
    };
}

rv32i::RV32IState initialOracleState(const RV32ISystemProgramCase& test_case) {
    rv32i::RV32IState state;
    state.pc = test_case.initial_pc;
    for (uint8_t reg = 0; reg < test_case.initial_registers.size(); ++reg) {
        state.writeRegister(reg, test_case.initial_registers[reg]);
    }
    state.forceX0();
    return state;
}

rv32i::RV32IFunctionalMemory initialOracleInstructionMemory(const RV32ISystemProgramCase& test_case) {
    rv32i::RV32IFunctionalMemory memory(test_case.memory_size_bytes);
    memory.loadProgram(test_case.program, test_case.program_base);
    return memory;
}

rv32i::RV32IFunctionalMemory initialOracleDataMemory(const RV32ISystemProgramCase& test_case) {
    rv32i::RV32IFunctionalMemory memory(test_case.memory_size_bytes);
    for (const auto& data : test_case.initial_data) {
        memory.loadBytes(data.address, data.bytes);
    }
    return memory;
}

std::vector<SimulationTest::SimulationCheckpoint> singleCycleOracleCheckpoints(
    const RV32ISystemProgramCase& test_case) {
    std::vector<SimulationTest::SimulationCheckpoint> checkpoints;
    if (test_case.program.empty() || test_case.cycle_time_step == 0) {
        return checkpoints;
    }

    auto state = initialOracleState(test_case);
    auto instruction_memory = initialOracleInstructionMemory(test_case);
    auto data_memory = initialOracleDataMemory(test_case);
    checkpoints.reserve(test_case.max_instructions);

    while (!state.halted && !state.trapped && checkpoints.size() < test_case.max_instructions) {
        const auto trace = rv32i::RV32IInstructionOracle::step(state, instruction_memory, data_memory);
        const size_t checkpoint_time = (checkpoints.size() + 1) * test_case.cycle_time_step;
        checkpoints.push_back(instructionCheckpoint(trace, checkpoint_time));
    }

    return checkpoints;
}

size_t memorySizeBytes(rv32i::RV32IMemorySize size) {
    switch (size) {
        case rv32i::RV32IMemorySize::None: return 0;
        case rv32i::RV32IMemorySize::Byte: return 1;
        case rv32i::RV32IMemorySize::Halfword: return 2;
        case rv32i::RV32IMemorySize::Word: return 4;
    }
    return 0;
}

std::map<uint32_t, uint8_t> expandDataMemoryWrites(const rv32i::RV32IMemoryTrace& memory) {
    std::map<uint32_t, uint8_t> writes;
    if (memory.kind != rv32i::RV32IMemoryAccessKind::Write || memory.fault) {
        return writes;
    }

    const size_t byte_count = memorySizeBytes(memory.size);
    for (size_t byte = 0; byte < byte_count; ++byte) {
        writes[static_cast<uint32_t>(memory.address + byte)] =
            static_cast<uint8_t>((memory.write_data >> (byte * 8)) & 0xFFU);
    }
    return writes;
}

class IntentionallyWrongRV32ILockstepProbe : public RV32IInstructionLockstepTest {
protected:
    void buildCircuit() override {}

    RV32ISystemProgramCase getCase() const override {
        RV32ISystemProgramCase test_case;
        test_case.name = "IntentionallyWrongRV32ILockstepProbe";
        test_case.program = rv32i::RV32IProgram::fromWords({
            encodeI(1, 0, 0x0, 1),
            0x00100073U,
        });
        test_case.max_instructions = 2;
        test_case.max_cycles_per_instruction = 1;
        return test_case;
    }

    void initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) override {
        state_ = rv32i::RV32IState{};
        instruction_memory_ = rv32i::RV32IFunctionalMemory(test_case.memory_size_bytes);
        instruction_memory_.loadProgram(test_case.program, test_case.program_base);
        data_memory_ = rv32i::RV32IFunctionalMemory(test_case.memory_size_bytes);
        last_access_ = {};
        last_writes_.clear();
    }

    void clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) override {
        (void)cycle_index;
        (void)cycle_start_time;
        const auto trace = rv32i::RV32IInstructionOracle::step(state_, instruction_memory_, data_memory_);
        last_access_ = trace.memory;
        last_writes_ = expandDataMemoryWrites(trace.memory);
        state_.writeRegister(31, state_.readRegister(31) ^ 1U);
    }

    rv32i::RV32IState snapshotComponentState() const override { return state_; }
    rv32i::RV32IMemoryTrace lastDataMemoryAccess() const override { return last_access_; }
    std::map<uint32_t, uint8_t> lastDataMemoryWrites() const override { return last_writes_; }

private:
    rv32i::RV32IState state_{};
    rv32i::RV32IFunctionalMemory instruction_memory_{};
    rv32i::RV32IFunctionalMemory data_memory_{};
    rv32i::RV32IMemoryTrace last_access_{};
    std::map<uint32_t, uint8_t> last_writes_{};
};

}

std::string RV32IInstructionLockstepTest::getTestName() const {
    return getCase().name;
}

size_t RV32IInstructionLockstepTest::getRunDuration() const {
    return time_origin_ + total_cycles_ * test_case_.cycle_time_step;
}

std::vector<SimulationTest::SimulationCheckpoint> RV32IInstructionLockstepTest::getCheckpoints() const {
    if (!checkpoints_.empty() || completed_) {
        return checkpoints_;
    }

    const auto test_case = test_case_.name.empty() ? getCase() : test_case_;
    return singleCycleOracleCheckpoints(test_case);
}

std::vector<SimulationTest::PerformanceMetric>
RV32IInstructionLockstepTest::getPerformanceMetrics() const {
    auto metrics = SimulationTest::getPerformanceMetrics();
    const auto instructions = retiredInstructions();
    const auto cycles = totalCycles();
    const double cpi = instructions == 0
        ? 0.0
        : static_cast<double>(cycles)
            / static_cast<double>(instructions);
    const double ipc = cycles == 0
        ? 0.0
        : static_cast<double>(instructions)
            / static_cast<double>(cycles);
    metrics.insert(metrics.begin(), {
        {"cpu.hardware_cycles", static_cast<double>(cycles), "cycles", "Clock cycles in the measured program window."},
        {"cpu.retired_instructions", static_cast<double>(instructions), "instructions", "Architecturally retired instructions, including the terminal instruction."},
        {"cpu.cpi", cpi, "cycles/instruction", "Hardware cycles divided by retired instructions."},
        {"cpu.ipc", ipc, "instructions/cycle", "Retired instructions divided by hardware cycles."},
    });
    return metrics;
}

void RV32IInstructionLockstepTest::setInitialState() {
    test_case_ = getCase();
    if (test_case_.name.empty()) {
        fail("RV32ISystemProgramCase requires a non-empty name");
    }
    if (test_case_.program.empty()) {
        fail(test_case_.name + " requires a non-empty program");
    }
    if (test_case_.max_instructions == 0) {
        fail(test_case_.name + " requires max_instructions > 0");
    }
    if (test_case_.max_cycles_per_instruction == 0) {
        fail(test_case_.name + " requires max_cycles_per_instruction > 0");
    }
    if (test_case_.cycle_time_step == 0) {
        fail(test_case_.name + " requires cycle_time_step > 0");
    }

    total_cycles_ = 0;
    time_origin_ = 0;
    completed_ = false;
    checkpoints_.clear();

    setupOracle();
    initializeComponentForLockstep(test_case_);
    time_origin_ = sim->getCurrentTime();

    const auto initial_state = snapshotComponentState();
    last_committed_instruction_count_ = initial_state.instruction_count;
    compareState(initial_state, "initial state");
    compareMemoryEffects(rv32i::RV32IMemoryTrace{}, "initial state");
    // Initialization and reset are not part of the measured program window.
    sim->resetPerformanceCounters();
}

void RV32IInstructionLockstepTest::setupOracle() {
    oracle_state_ = rv32i::RV32IState{};
    oracle_state_.pc = test_case_.initial_pc;
    for (uint8_t reg = 0; reg < test_case_.initial_registers.size(); ++reg) {
        oracle_state_.writeRegister(reg, test_case_.initial_registers[reg]);
    }
    oracle_state_.forceX0();

    oracle_instruction_memory_ = rv32i::RV32IFunctionalMemory(test_case_.memory_size_bytes);
    oracle_instruction_memory_.loadProgram(test_case_.program, test_case_.program_base);
    oracle_data_memory_ = rv32i::RV32IFunctionalMemory(test_case_.memory_size_bytes);
    for (const auto& data : test_case_.initial_data) {
        oracle_data_memory_.loadBytes(data.address, data.bytes);
    }
}

void RV32IInstructionLockstepTest::runSimulation() {
    size_t executed_instructions = 0;

    while (!oracle_state_.halted && !oracle_state_.trapped && executed_instructions < test_case_.max_instructions) {
        const uint64_t previous_commit_count = last_committed_instruction_count_;
        rv32i::RV32IState committed_state;
        bool committed = false;

        for (size_t cycle = 0; cycle < test_case_.max_cycles_per_instruction; ++cycle) {
            const size_t cycle_start_time = time_origin_ + total_cycles_ * test_case_.cycle_time_step;
            observePerformanceCycle();
            clockComponentOneCycle(total_cycles_, cycle_start_time);
            ++total_cycles_;
            sim->advanceAndRecord(time_origin_ + total_cycles_ * test_case_.cycle_time_step);

            auto state = snapshotComponentState();
            if (state.instruction_count == previous_commit_count) {
                continue;
            }
            if (state.instruction_count != previous_commit_count + 1) {
                fail("component committed more than one instruction before lockstep sampling");
            }
            committed_state = state;
            committed = true;
            break;
        }

        if (!committed) {
            fail("component did not commit an instruction within max_cycles_per_instruction");
        }

        const auto expected_trace = rv32i::RV32IInstructionOracle::step(
            oracle_state_, oracle_instruction_memory_, oracle_data_memory_);
        const size_t commit_time = time_origin_ + total_cycles_ * test_case_.cycle_time_step;
        compareState(committed_state, "instruction " + std::to_string(expected_trace.instruction_index));
        compareMemoryEffects(expected_trace.memory, "instruction " + std::to_string(expected_trace.instruction_index));
        checkpoints_.push_back(instructionCheckpoint(expected_trace, commit_time));
        last_committed_instruction_count_ = committed_state.instruction_count;
        ++executed_instructions;
    }

    if (!oracle_state_.halted && !oracle_state_.trapped) {
        fail("instruction lockstep reached max_instructions before halt or trap");
    }

    completed_ = true;
}

void RV32IInstructionLockstepTest::verifyResults() {
    if (!completed_) {
        fail("instruction lockstep run did not complete");
    }
    if (checkpoints_.size() != oracle_state_.instruction_count) {
        fail("instruction checkpoint count mismatch");
    }
}

void RV32IInstructionLockstepTest::compareState(const rv32i::RV32IState& actual,
                                                const std::string& label) const {
    if (actual.instruction_count != oracle_state_.instruction_count) {
        fail(label + ": committed instruction count mismatch");
    }
    if (actual.pc != oracle_state_.pc) {
        fail(label + ": pc mismatch, expected " + hex32(oracle_state_.pc) + ", got " + hex32(actual.pc));
    }
    for (size_t reg = 0; reg < oracle_state_.x.size(); ++reg) {
        if (actual.readRegister(static_cast<uint8_t>(reg)) != oracle_state_.readRegister(static_cast<uint8_t>(reg))) {
            fail(label + ": x" + std::to_string(reg) + " mismatch, expected "
                 + hex32(oracle_state_.readRegister(static_cast<uint8_t>(reg)))
                 + ", got " + hex32(actual.readRegister(static_cast<uint8_t>(reg))));
        }
    }
    if (actual.halted != oracle_state_.halted) {
        fail(label + ": halted mismatch");
    }
    if (actual.trapped != oracle_state_.trapped) {
        fail(label + ": trapped mismatch");
    }
    if (actual.trap_cause != oracle_state_.trap_cause) {
        fail(label + ": trap cause mismatch");
    }
}

void RV32IInstructionLockstepTest::compareMemoryEffects(const rv32i::RV32IMemoryTrace& expected,
                                                        const std::string& label) const {
    const auto actual = lastDataMemoryAccess();
    if (actual.kind != expected.kind) {
        fail(label + ": memory access kind mismatch, expected " + std::string(rv32i::toString(expected.kind))
             + ", got " + std::string(rv32i::toString(actual.kind)));
    }
    if (actual.size != expected.size) {
        fail(label + ": memory access size mismatch, expected " + std::string(rv32i::toString(expected.size))
             + ", got " + std::string(rv32i::toString(actual.size)));
    }
    if (actual.sign_extend != expected.sign_extend) {
        fail(label + ": memory sign_extend mismatch, expected " + yesNo(expected.sign_extend)
             + ", got " + yesNo(actual.sign_extend));
    }
    if (actual.address != expected.address) {
        fail(label + ": memory address mismatch, expected " + hex32(expected.address)
             + ", got " + hex32(actual.address));
    }
    if (actual.write_data != expected.write_data) {
        fail(label + ": memory write_data mismatch, expected " + hex32(expected.write_data)
             + ", got " + hex32(actual.write_data));
    }
    if (actual.read_data != expected.read_data) {
        fail(label + ": memory read_data mismatch, expected " + hex32(expected.read_data)
             + ", got " + hex32(actual.read_data));
    }
    if (actual.fault != expected.fault) {
        fail(label + ": memory fault mismatch, expected " + yesNo(expected.fault)
             + ", got " + yesNo(actual.fault));
    }

    const auto expected_writes = expandDataMemoryWrites(expected);
    const auto actual_writes = lastDataMemoryWrites();
    if (actual_writes != expected_writes) {
        fail(label + ": data-memory writes mismatch, expected " + byteWritesToHex(expected_writes)
             + ", got " + byteWritesToHex(actual_writes));
    }
}

void RV32IInstructionLockstepTest::fail(const std::string& message) const {
    throw std::runtime_error(getTestName() + ": " + message);
}

RV32ISystemProgramCase RV32IInstructionLockstepHarnessTest::getCase() const {
    RV32ISystemProgramCase test_case;
    test_case.name = "RV32IInstructionLockstepHarnessTest";
    test_case.program = rv32i::RV32IProgram::fromWords({
        encodeI(0x80, 0, 0x0, 1),       // addi x1, x0, 0x80
        encodeI(42, 0, 0x0, 2),         // addi x2, x0, 42
        encodeS(0, 2, 1, 0x2),          // sw x2, 0(x1)
        encodeI(0, 1, 0x2, 3, 0x03),    // lw x3, 0(x1)
        encodeB(8, 3, 2, 0x0),          // beq x2, x3, +8
        encodeI(1, 0, 0x0, 4),          // skipped if branch is taken
        0x00100073U,                    // ebreak
    });
    test_case.max_instructions = 8;
    test_case.max_cycles_per_instruction = 1;
    return test_case;
}

void RV32IInstructionLockstepHarnessTest::initializeComponentForLockstep(const RV32ISystemProgramCase& test_case) {
    component_state_ = rv32i::RV32IState{};
    component_state_.pc = test_case.initial_pc;
    for (uint8_t reg = 0; reg < test_case.initial_registers.size(); ++reg) {
        component_state_.writeRegister(reg, test_case.initial_registers[reg]);
    }
    component_state_.forceX0();

    component_instruction_memory_ = rv32i::RV32IFunctionalMemory(test_case.memory_size_bytes);
    component_instruction_memory_.loadProgram(test_case.program, test_case.program_base);
    component_data_memory_ = rv32i::RV32IFunctionalMemory(test_case.memory_size_bytes);
    for (const auto& data : test_case.initial_data) {
        component_data_memory_.loadBytes(data.address, data.bytes);
    }

    last_data_memory_access_ = rv32i::RV32IMemoryTrace{};
    last_data_memory_writes_.clear();
}

void RV32IInstructionLockstepHarnessTest::clockComponentOneCycle(size_t cycle_index, size_t cycle_start_time) {
    (void)cycle_index;
    (void)cycle_start_time;
    if (component_state_.halted || component_state_.trapped) {
        return;
    }
    const auto trace = rv32i::RV32IInstructionOracle::step(
        component_state_, component_instruction_memory_, component_data_memory_);
    last_data_memory_access_ = trace.memory;
    last_data_memory_writes_ = expandDataMemoryWrites(trace.memory);
}

rv32i::RV32IState RV32IInstructionLockstepHarnessTest::snapshotComponentState() const {
    return component_state_;
}

rv32i::RV32IMemoryTrace RV32IInstructionLockstepHarnessTest::lastDataMemoryAccess() const {
    return last_data_memory_access_;
}

std::map<uint32_t, uint8_t> RV32IInstructionLockstepHarnessTest::lastDataMemoryWrites() const {
    return last_data_memory_writes_;
}

std::string RV32IInstructionLockstepMismatchDetectionTest::getTestName() const {
    return "RV32IInstructionLockstepMismatchDetectionTest";
}

void RV32IInstructionLockstepMismatchDetectionTest::verifyResults() {
    IntentionallyWrongRV32ILockstepProbe probe;
    if (probe.run()) {
        throw std::runtime_error("instruction lockstep accepted an intentionally corrupted register state");
    }
}
