#include "tests/RV32ISingleCycleTests.hpp"

#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "simulator/Event.hpp"
#include "tests/RV32IProgramCases.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void drive(Simulator& simulator, size_t time, const std::shared_ptr<Wire<>>& wire, bool value) {
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(
        time, wire, value ? LogicValue::HIGH : LogicValue::LOW));
}

rv32i::RV32IMemorySize memorySize(uint64_t encoded) {
    switch (encoded) {
        case 0: return rv32i::RV32IMemorySize::Byte;
        case 1: return rv32i::RV32IMemorySize::Halfword;
        case 2: return rv32i::RV32IMemorySize::Word;
        default: return rv32i::RV32IMemorySize::None;
    }
}

RV32ISystemProgramCase structuralProgramCase(size_t number) {
    auto test_case = rv32iProgramCase(number);
    test_case.name = "RV32ISingleCycleSystemProgram" + std::to_string(number) + "Test";
    test_case.max_cycles_per_instruction = 1;
    test_case.cycle_time_step = 4000;
    return test_case;
}

void verifyBalancedBuild(const circuit::BuildResult& build) {
    require(build.root != nullptr, "balanced profile did not build a root");
    require(build.profile != nullptr && build.profile->name() == "rv32i-balanced",
            "balanced profile identity is incorrect");
    require(build.root->getSelectedFidelity() == "structural",
            "balanced system root must be structural");
    require(build.root->getProfileFingerprint() == build.profile->fingerprint(),
            "balanced system root fingerprint does not match its profile");

    const std::map<std::string, circuit::Fidelity> expected{
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT", circuit::Fidelity::Structural},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE", circuit::Fidelity::Structural},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.INSTRUCTION_MEMORY", circuit::Fidelity::Behavioral},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.DATA_MEMORY", circuit::Fidelity::Behavioral},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.CONTROL_FLOW", circuit::Fidelity::Structural},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.DECODE_CONTROL", circuit::Fidelity::Structural},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.REGISTER_FILE", circuit::Fidelity::Behavioral},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.ALU", circuit::Fidelity::Structural},
        {"RV32I_SINGLE_CYCLE_SYSTEM_ROOT.CORE.EXECUTION_STATUS", circuit::Fidelity::Structural},
    };

    std::map<std::string, circuit::BuildManifestEntry> entries;
    for (const auto& entry : build.manifest->entries()) {
        entries.emplace(entry.path, entry);
        require(!entry.selection.used_unavailable_exception,
                "balanced profile used an unplanned fallback at " + entry.path);
    }
    for (const auto& [path, fidelity] : expected) {
        const auto found = entries.find(path);
        require(found != entries.end(), "balanced manifest omitted " + path);
        require(found->second.selection.fidelity == fidelity,
                "balanced profile selected the wrong fidelity at " + path);
    }
    require(entries.at("RV32I_SINGLE_CYCLE_SYSTEM_ROOT").effective_fidelity
                == circuit::EffectiveFidelity::Mixed,
            "balanced system manifest must report a mixed subtree");
}
}

void RV32ISingleCycleSystemSmokeTest::setupCircuit() {
    root = Component::create<RV32ISingleCycleSystem>("RV32I_SINGLE_CYCLE_SYSTEM_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

std::string RV32ISingleCycleSystemSmokeTest::getTestName() const {
    return "RV32ISingleCycleSystemSmokeTest";
}

size_t RV32ISingleCycleSystemSmokeTest::getRunDuration() const {
    return 14000;
}

std::vector<SimulationTest::SimulationCheckpoint> RV32ISingleCycleSystemSmokeTest::getCheckpoints() const {
    return {
        {1900, "reset", "PC, registers, halt, and trap state are cleared", 0},
        {5900, "ADDI settled", "structural decode, register read, ALU, and permissions are ready", 1},
        {7000, "ADDI committed", "x1 receives 7 and PC advances to 4", 2},
        {11900, "EBREAK settled", "halt request is ready with normal writes suppressed", 3},
        {14000, "halt committed", "HALTED is latched and PC remains on EBREAK", 4},
    };
}

void RV32ISingleCycleSystemSmokeTest::buildCircuit() {
    system_ = std::dynamic_pointer_cast<RV32ISingleCycleSystem>(root);
    require(system_ != nullptr, "RV32I single-cycle smoke root type");

    clk_wire_ = builder->addNewWire("CLK_IN", nullptr, {system_->getInputPin("CLK")});
    rst_wire_ = builder->addNewWire("RST_IN", nullptr, {system_->getInputPin("RST")});
    enable_wire_ = builder->addNewWire("ENABLE_IN", nullptr, {system_->getInputPin("ENABLE")});
    builder->addNewWire<32>("PC_OUT", system_->getOutputPin<32>("PC"), {});
    builder->addNewWire("HALTED_OUT", system_->getOutputPin("HALTED"), {});
    builder->addNewWire("TRAPPED_OUT", system_->getOutputPin("TRAPPED"), {});
}

void RV32ISingleCycleSystemSmokeTest::setInitialState() {
    system_->clearInstructionMemory();
    system_->clearDataMemory();
    system_->loadProgram(rv32i::RV32IProgram::fromWords({
        0x00700093U, // addi x1, x0, 7
        0x00100073U, // ebreak
    }));

    drive(*sim, 0, clk_wire_, false);
    drive(*sim, 0, rst_wire_, true);
    drive(*sim, 0, enable_wire_, true);
    drive(*sim, 2000, rst_wire_, false);
    drive(*sim, 6000, clk_wire_, true);
    drive(*sim, 6500, clk_wire_, false);
    drive(*sim, 12000, clk_wire_, true);
    drive(*sim, 12500, clk_wire_, false);
}

void RV32ISingleCycleSystemSmokeTest::verifyResults() {
    const auto state = system_->snapshotState(2);
    require(state.pc == 4, "structural smoke PC should remain on EBREAK");
    require(state.readRegister(0) == 0, "structural smoke x0 must remain zero");
    require(state.readRegister(1) == 7, "structural smoke ADDI should write x1=7");
    require(state.halted, "structural smoke EBREAK should latch HALTED");
    require(!state.trapped, "structural smoke should not trap");
    require(state.trap_cause == rv32i::RV32IExecutionTrapCause::None,
            "structural smoke trap cause should remain None");
}

void RV32ISingleCycleSystemContractTest::setupCircuit() {
    root = Component::create<RV32ISingleCycleSystem>("RV32I_SINGLE_CYCLE_CONTRACT_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

std::string RV32ISingleCycleSystemContractTest::getTestName() const {
    return "RV32ISingleCycleSystemContractTest";
}

size_t RV32ISingleCycleSystemContractTest::getRunDuration() const {
    return 33000;
}

std::vector<SimulationTest::SimulationCheckpoint> RV32ISingleCycleSystemContractTest::getCheckpoints() const {
    return {
        {1900, "reset", "architectural state is zero", 0},
        {8000, "disabled edge", "ENABLE=0 holds PC and registers", 1},
        {14000, "enabled ADDI", "ENABLE=1 commits x1=1 and PC=4", 2},
        {20000, "EBREAK", "halt is latched without advancing PC", 3},
        {26000, "post-halt edge", "halted state suppresses later writes", 4},
        {33000, "reset recovery", "reset clears PC, register, halt, and trap state", 5},
    };
}

void RV32ISingleCycleSystemContractTest::buildCircuit() {
    system_ = std::dynamic_pointer_cast<RV32ISingleCycleSystem>(root);
    require(system_ != nullptr, "structural RV32I contract root type");
    clk_wire_ = builder->addNewWire("CLK_IN", nullptr, {system_->getInputPin("CLK")});
    rst_wire_ = builder->addNewWire("RST_IN", nullptr, {system_->getInputPin("RST")});
    enable_wire_ = builder->addNewWire("ENABLE_IN", nullptr, {system_->getInputPin("ENABLE")});
    builder->addNewWire<32>("PC_OUT", system_->getOutputPin<32>("PC"), {});
    builder->addNewWire("HALTED_OUT", system_->getOutputPin("HALTED"), {});
    builder->addNewWire("TRAPPED_OUT", system_->getOutputPin("TRAPPED"), {});
}

void RV32ISingleCycleSystemContractTest::setInitialState() {
    system_->clearInstructionMemory();
    system_->clearDataMemory();
    system_->loadProgram(rv32i::RV32IProgram::fromWords({
        0x00100093U, // addi x1, x0, 1
        0x00100073U, // ebreak
    }));
    drive(*sim, 0, clk_wire_, false);
    drive(*sim, 0, rst_wire_, true);
    drive(*sim, 0, enable_wire_, false);
    drive(*sim, 2000, rst_wire_, false);
    drive(*sim, 6000, clk_wire_, true);
    drive(*sim, 6500, clk_wire_, false);
    drive(*sim, 9000, enable_wire_, true);
    drive(*sim, 12000, clk_wire_, true);
    drive(*sim, 12500, clk_wire_, false);
    drive(*sim, 18000, clk_wire_, true);
    drive(*sim, 18500, clk_wire_, false);
    drive(*sim, 24000, clk_wire_, true);
    drive(*sim, 24500, clk_wire_, false);
    drive(*sim, 28000, rst_wire_, true);
    drive(*sim, 31000, rst_wire_, false);
    drive(*sim, 31000, enable_wire_, false);
}

void RV32ISingleCycleSystemContractTest::verifyResults() {
    const auto check = [&](size_t time, uint32_t pc, uint32_t x1, bool halted) {
        sim->setCircuitStateAtTime(time);
        const auto state = system_->snapshotState();
        require(state.pc == pc, "structural contract PC at time " + std::to_string(time));
        require(state.readRegister(1) == x1,
                "structural contract x1 at time " + std::to_string(time));
        require(state.halted == halted,
                "structural contract HALTED at time " + std::to_string(time));
        require(!state.trapped,
                "structural contract TRAPPED at time " + std::to_string(time));
    };
    check(8000, 0, 0, false);
    check(14000, 4, 1, false);
    check(20000, 4, 1, true);
    check(26000, 4, 1, true);
    check(33000, 0, 0, false);
}

void RV32ISingleCycleSystemProgramTestBase::setupCircuit() {
    if (useBalancedProfile()) {
        const auto catalog = circuit::createBuiltinComponentCatalog();
        auto request = rv32i::educationalSystemRequest(
            "RV32I_SINGLE_CYCLE_SYSTEM_ROOT");
        auto profile = rv32i::balancedSystemProfile(*catalog, request);
        auto build = catalog->createRoot(request, std::move(profile));
        verifyBalancedBuild(build);
        root = std::move(build.root);
    } else {
        root = Component::create<RV32ISingleCycleSystem>("RV32I_SINGLE_CYCLE_SYSTEM_ROOT");
    }
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

size_t RV32ISingleCycleSystemProgramTestBase::getRunDuration() const {
    return std::max(visual_run_duration_, RV32IInstructionLockstepTest::getRunDuration());
}

std::vector<SimulationTest::SimulationCheckpoint>
RV32ISingleCycleSystemProgramTestBase::getCheckpoints() const {
    auto checkpoints = RV32IInstructionLockstepTest::getCheckpoints();
    if (committed_instruction_count_ == 0) {
        for (auto& checkpoint : checkpoints) {
            checkpoint.time += visual_time_origin_;
        }
    }
    return checkpoints;
}

void RV32ISingleCycleSystemProgramTestBase::buildCircuit() {
    system_ = std::dynamic_pointer_cast<RV32ISingleCycleSystem>(root);
    require(system_ != nullptr, "structural RV32I program root type");
    clk_wire_ = builder->addNewWire("CLK_IN", nullptr, {system_->getInputPin("CLK")});
    rst_wire_ = builder->addNewWire("RST_IN", nullptr, {system_->getInputPin("RST")});
    enable_wire_ = builder->addNewWire("ENABLE_IN", nullptr, {system_->getInputPin("ENABLE")});
    builder->addNewWire<32>("PC_OUT", system_->getOutputPin<32>("PC"), {});
    builder->addNewWire("HALTED_OUT", system_->getOutputPin("HALTED"), {});
    builder->addNewWire("TRAPPED_OUT", system_->getOutputPin("TRAPPED"), {});
}

void RV32ISingleCycleSystemProgramTestBase::initializeComponentForLockstep(
    const RV32ISystemProgramCase& test_case
) {
    require(system_ != nullptr, "structural RV32I system is not initialized");
    require(test_case.initial_pc == 0, "first structural core supports reset PC zero");
    for (size_t index = 0; index < test_case.initial_registers.size(); ++index) {
        require(test_case.initial_registers[index] == 0,
                "first structural core program fixtures require zero initial registers");
    }

    system_->clearInstructionMemory();
    system_->clearDataMemory();
    system_->loadProgram(test_case.program, test_case.program_base);
    for (const auto& data : test_case.initial_data) {
        system_->loadDataBytes(data.address, data.bytes);
    }

    committed_instruction_count_ = 0;
    last_access_ = {};
    scheduleInitialEvents(0);
    drive(*sim, 0, clk_wire_, false);
    drive(*sim, 0, rst_wire_, true);
    drive(*sim, 0, enable_wire_, true);
    sim->advanceAndRecord(1000);
    drive(*sim, 1000, rst_wire_, false);
    sim->advanceAndRecord(5000);
    visual_time_origin_ = sim->getCurrentTime();
    visual_run_duration_ = visual_time_origin_
                         + test_case.expected_result.instruction_count * test_case.cycle_time_step;
    for (size_t cycle = 0; cycle < test_case.expected_result.instruction_count; ++cycle) {
        const size_t cycle_start = visual_time_origin_ + cycle * test_case.cycle_time_step;
        drive(*sim, cycle_start + 1000, clk_wire_, true);
        drive(*sim, cycle_start + 1500, clk_wire_, false);
    }
    last_observed_memory_time_ = sim->getCurrentTime();
}

void RV32ISingleCycleSystemProgramTestBase::clockComponentOneCycle(
    size_t cycle_index,
    size_t cycle_start_time
) {
    (void)cycle_index;
    const auto core = system_->core();
    require(core != nullptr, "structural RV32I core is not initialized");
    require(core->getOutputPin("INSTRUCTION_ATTEMPT")->getValue() == LogicValue::HIGH,
            getTestName() + ": instruction did not settle before its active edge");

    last_access_ = {};
    const bool read = core->getOutputPin("DMEM_READ_EN")->getValue() == LogicValue::HIGH;
    const bool write = core->getOutputPin("DMEM_WRITE_EN")->getValue() == LogicValue::HIGH;
    if (read || write) {
        last_access_.kind = read
            ? rv32i::RV32IMemoryAccessKind::Read
            : rv32i::RV32IMemoryAccessKind::Write;
        last_access_.size = memorySize(core->getOutputPin<2>("DMEM_SIZE")->getValueAsUInt64());
        last_access_.sign_extend = read
            && core->getOutputPin("DMEM_SIGN_EXTEND")->getValue() == LogicValue::HIGH;
        last_access_.address = static_cast<uint32_t>(
            core->getOutputPin<32>("DMEM_ADDR")->getValueAsUInt64());
        last_access_.write_data = write
            ? static_cast<uint32_t>(core->getOutputPin<32>("DMEM_WRITE_DATA")->getValueAsUInt64())
            : 0;
        last_access_.fault = system_->dataMemory()->getOutputPin("FAULT")->getValue() == LogicValue::HIGH;
        if (read && !last_access_.fault) {
            last_access_.read_data = static_cast<uint32_t>(
                system_->dataMemory()->getOutputPin<32>("READ_DATA")->getValueAsUInt64());
        }
    }

    (void)cycle_start_time;
    ++committed_instruction_count_;
}

rv32i::RV32IState RV32ISingleCycleSystemProgramTestBase::snapshotComponentState() const {
    require(system_ != nullptr, "structural RV32I system is not initialized");
    return system_->snapshotState(committed_instruction_count_);
}

rv32i::RV32IMemoryTrace RV32ISingleCycleSystemProgramTestBase::lastDataMemoryAccess() const {
    return last_access_;
}

std::map<uint32_t, uint8_t> RV32ISingleCycleSystemProgramTestBase::lastDataMemoryWrites() const {
    require(system_ != nullptr && system_->dataMemory() != nullptr,
            "structural RV32I data memory is not initialized");
    const auto current_time = sim->getCurrentTime();
    const auto writes = system_->dataMemory()->getByteWritesInTimeRange(
        last_observed_memory_time_, current_time);
    last_observed_memory_time_ = current_time;
    return writes;
}

void RV32ISingleCycleSystemProgramTestBase::verifyResults() {
    RV32IInstructionLockstepTest::verifyResults();
    verifyRV32IProgramExpectedResult(
        getCase(),
        snapshotComponentState(),
        *system_->dataMemory(),
        sim->getCurrentTime(),
        getTestName());
}

#define DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(NUMBER) \
RV32ISystemProgramCase RV32ISingleCycleSystemProgram##NUMBER##Test::getCase() const { \
    return structuralProgramCase(NUMBER); \
}

DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(1)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(2)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(3)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(4)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(5)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(6)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(7)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(8)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(9)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(10)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(11)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(12)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(13)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(14)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(15)
DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE(16)

#undef DEFINE_STRUCTURAL_RV32I_PROGRAM_CASE

#define DEFINE_BALANCED_RV32I_PROGRAM_CASE(NUMBER) \
RV32ISystemProgramCase RV32IBalancedSystemProgram##NUMBER##Test::getCase() const { \
    auto test_case = structuralProgramCase(NUMBER); \
    test_case.name = "RV32IBalancedSystemProgram" #NUMBER "Test"; \
    return test_case; \
}

DEFINE_BALANCED_RV32I_PROGRAM_CASE(1)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(2)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(3)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(4)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(5)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(6)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(7)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(8)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(9)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(10)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(11)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(12)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(13)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(14)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(15)
DEFINE_BALANCED_RV32I_PROGRAM_CASE(16)

#undef DEFINE_BALANCED_RV32I_PROGRAM_CASE
