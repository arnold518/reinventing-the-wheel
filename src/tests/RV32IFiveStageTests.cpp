#include "tests/RV32IFiveStageTests.hpp"

#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "simulator/Event.hpp"
#include "tests/RV32IProgramCases.hpp"
#include "tests/RV32IProgramRoot.hpp"
#include <stdexcept>
#include <utility>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void drive(
    Simulator& simulator,
    size_t time,
    const std::shared_ptr<Wire<>>& wire,
    bool value) {
    simulator.scheduleEvent(
        std::make_shared<WireUpdateEvent<>>(
            time,
            wire,
            value ? LogicValue::HIGH : LogicValue::LOW));
}

rv32i::RV32IMemorySize memorySize(uint64_t encoded) {
    switch (encoded) {
        case 0: return rv32i::RV32IMemorySize::Byte;
        case 1: return rv32i::RV32IMemorySize::Halfword;
        case 2: return rv32i::RV32IMemorySize::Word;
        default: return rv32i::RV32IMemorySize::None;
    }
}

RV32ISystemProgramCase fiveStageProgramCase(size_t number) {
    auto test_case = rv32iProgramCase(number);
    test_case.name =
        rv32iFiveStageProgramScenarioName(number);
    test_case.max_cycles_per_instruction = 16;
    test_case.cycle_time_step = 100;
    return test_case;
}

circuit::BuildProfile fiveStageStructuralCoreProfile() {
    return rv32i::architectureStructuralProfile(
        circuit::builtinComponentCatalog(),
        "rv32i-five-stage-structural-core");
}

void compareProgramCheckpoints(
    const std::vector<SimulationTest::SimulationCheckpoint>& expected,
    const std::vector<SimulationTest::SimulationCheckpoint>& actual,
    const std::string& label) {
    require(
        expected.size() == actual.size(),
        label + ": instruction checkpoint count differs");
    for (size_t index = 0; index < expected.size(); ++index) {
        require(
            expected[index].row_index == actual[index].row_index,
            label + ": commit ordinal differs at checkpoint "
                + std::to_string(index));
        require(
            expected[index].label == actual[index].label,
            label + ": instruction identity differs at checkpoint "
                + std::to_string(index));
        require(
            expected[index].detail == actual[index].detail,
            label + ": architectural trace differs at checkpoint "
                + std::to_string(index));
    }
}
} // namespace

std::string rv32iFiveStageProgramScenarioName(
    size_t program_number) {
    (void)rv32iProgramCase(program_number);
    return "RV32IFiveStageCoreProgramTest/program-"
        + std::string(program_number < 10 ? "0" : "")
        + std::to_string(program_number);
}

RV32IFiveStageCoreProgramTest::
RV32IFiveStageCoreProgramTest(size_t program_number)
    : program_number_(program_number),
      profile_(fiveStageStructuralCoreProfile()) {
    (void)rv32iFiveStageProgramScenarioName(program_number_);
}

bool RV32IFiveStageCoreProgramTest::run() {
    try {
        RV32IFiveStageCoreProgramTest answer_sheet(
            program_number_);
        answer_sheet.profile_ = circuit::strictAllBehavioral();
        if (!answer_sheet.SimulationTest::run()) {
            return false;
        }

        if (!SimulationTest::run()) {
            return false;
        }
        compareProgramCheckpoints(
            answer_sheet.getCheckpoints(),
            getCheckpoints(),
            getCase().name + " selected profile");
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] Test '" << getTestName()
                  << "' comparison threw an exception: "
                  << error.what() << std::endl;
        return false;
    }
}

void RV32IFiveStageCoreProgramTest::setupCircuit() {
    const auto shared_profile =
        std::make_shared<const circuit::BuildProfile>(profile_);
    auto manifest = std::make_shared<circuit::BuildManifest>(
        shared_profile->name(),
        shared_profile->fingerprint());
    auto scope = circuit::BuildContext::rootScope(
        circuit::builtinComponentCatalog(),
        shared_profile,
        manifest);
    auto root_context = scope->child(
        RV32IProgramRoot::RootName, std::nullopt);
    program_root_ = Component::createWithContext<RV32IProgramRoot>(
        root_context,
        RV32IProgramRoot::RootName,
        circuit::families::RV32IFiveStageCore,
        50);
    root = program_root_;
    builder = std::make_unique<ComponentBuilder>(
        root, root_context);
    buildCircuit();
    setInitialState();
    runSimulation();
}

void RV32IFiveStageCoreProgramTest::setBuildProfile(
    circuit::BuildProfile profile) {
    profile_ = std::move(profile);
    root.reset();
    sim = std::make_shared<Simulator>();
    builder.reset();
    initial_events_scheduled_ = false;
    core_.reset();
    state_view_.reset();
    instruction_memory_.reset();
    data_memory_.reset();
    program_root_.reset();
    rst_wire_.reset();
    enable_wire_.reset();
    simulation_precomputed_ = false;
}

RV32ISystemProgramCase
RV32IFiveStageCoreProgramTest::getCase() const {
    return fiveStageProgramCase(program_number_);
}

void RV32IFiveStageCoreProgramTest::buildCircuit() {
    require(program_root_ != nullptr, "five-stage program root was not built");
    core_ = program_root_->core();
    state_view_ = program_root_->stateView();
    instruction_memory_ = program_root_->instructionMemory();
    data_memory_ = program_root_->dataMemory();
    require(core_ != nullptr, "five-stage core was not built");
    require(
        state_view_ != nullptr,
        "five-stage core lacks architectural-state observation");
    require(
        instruction_memory_ != nullptr
            && data_memory_ != nullptr,
        "five-stage program memories were not built");

    rst_wire_ = builder->addNewWire(
        "RST_IN", nullptr, {program_root_->getInputPin("RST")});
    enable_wire_ = builder->addNewWire(
        "ENABLE_IN",
        nullptr,
        {program_root_->getInputPin("ENABLE")});
    builder->addNewWire<32>(
        "PC_OUT", program_root_->getOutputPin<32>("PC"), {});
    builder->addNewWire(
        "HALTED_OUT", program_root_->getOutputPin("HALTED"), {});
    builder->addNewWire(
        "TRAPPED_OUT", program_root_->getOutputPin("TRAPPED"), {});
    builder->addNewWire<4>(
        "TRAP_CAUSE_OUT",
        program_root_->getOutputPin<4>("TRAP_CAUSE"), {});
}

void RV32IFiveStageCoreProgramTest::runSimulation() {
    if (simulation_precomputed_) {
        return;
    }
    RV32IInstructionLockstepTest::runSimulation();
    simulation_precomputed_ = true;
}

std::vector<SimulationTest::PerformanceMetric>
RV32IFiveStageCoreProgramTest::getPerformanceMetrics() const {
    auto metrics = RV32IInstructionLockstepTest::getPerformanceMetrics();
    const auto cycles = totalCycles();
    const double occupancy = cycles == 0
        ? 0.0
        : static_cast<double>(occupied_pipeline_slots_)
            / static_cast<double>(cycles * 4U);
    metrics.insert(metrics.begin() + 4, {
        {"pipeline.load_use_stall_cycles", static_cast<double>(load_use_stall_cycles_), "cycles", "Cycles blocked by an immediately dependent load."},
        {"pipeline.memory_stall_cycles", static_cast<double>(memory_stall_cycles_), "cycles", "Cycles waiting for data memory readiness."},
        {"pipeline.data_port_stall_cycles", static_cast<double>(data_port_stall_cycles_), "cycles", "Cycles serializing a simultaneous store and load on the single data-memory port."},
        {"pipeline.flush_events", static_cast<double>(pipeline_flushes_), "flushes", "Cycles that flush younger work after a redirect or exception."},
        {"pipeline.average_occupancy", occupancy, "fraction", "Average occupied fraction of the four inter-stage registers."},
    });
    return metrics;
}

void RV32IFiveStageCoreProgramTest::
initializeComponentForLockstep(
    const RV32ISystemProgramCase& test_case) {
    require(
        test_case.initial_pc == 0,
        "five-stage core fixtures currently require reset PC zero");
    for (const auto value : test_case.initial_registers) {
        require(
            value == 0,
            "five-stage core fixtures currently require zero "
            "initial registers");
    }

    instruction_memory_->clearContents();
    data_memory_->clearContents();
    test_case.program.loadInto(
        *instruction_memory_, test_case.program_base);
    for (const auto& data : test_case.initial_data) {
        data_memory_->loadBytes(data.address, data.bytes);
    }

    scheduleInitialEvents(0);
    program_root_->initializeFixedInputs(*sim, 0);
    drive(*sim, 0, rst_wire_, true);
    drive(*sim, 0, enable_wire_, true);
    program_root_->startClock(*sim, 20);
    sim->advanceAndRecord(70);
    drive(*sim, 70, rst_wire_, false);
    sim->advanceAndRecord(100);
    load_use_stall_cycles_ = 0;
    memory_stall_cycles_ = 0;
    data_port_stall_cycles_ = 0;
    pipeline_flushes_ = 0;
    occupied_pipeline_slots_ = 0;
}

void RV32IFiveStageCoreProgramTest::observePerformanceCycle() {
    const auto highOutput = [&](const char* name) {
        return core_->getOutputPin(name)->getValue()
            == LogicValue::HIGH;
    };
    load_use_stall_cycles_ += highOutput("LOAD_USE_STALL");
    memory_stall_cycles_ += highOutput("MEMORY_STALL");
    data_port_stall_cycles_ += highOutput("DATA_PORT_STALL");
    pipeline_flushes_ += highOutput("PIPELINE_FLUSH");
    for (const auto* signal : {
             "IF_ID_VALID", "ID_EX_VALID",
             "EX_MEM_VALID", "MEM_WB_VALID"}) {
        occupied_pipeline_slots_ += highOutput(signal);
    }
}

void RV32IFiveStageCoreProgramTest::clockComponentOneCycle(
    size_t cycle_index,
    size_t cycle_start_time) {
    (void)cycle_index;
    (void)cycle_start_time;
}

rv32i::RV32IState
RV32IFiveStageCoreProgramTest::
snapshotComponentState() const {
    require(
        state_view_ != nullptr,
        "five-stage state view is not initialized");
    auto state = state_view_->snapshotArchitecturalState()
        .toKnownState();
    state.instruction_count =
        core_->getOutputPin<32>("RETIRED_COUNT")
            ->getValueAsUInt64();
    return state;
}

rv32i::RV32IMemoryTrace
RV32IFiveStageCoreProgramTest::
lastDataMemoryAccess() const {
    rv32i::RV32IMemoryTrace trace;
    const bool read =
        core_->getOutputPin("RETIRED_MEM_READ")
            ->getValue() == LogicValue::HIGH;
    const bool write =
        core_->getOutputPin("RETIRED_MEM_WRITE")
            ->getValue() == LogicValue::HIGH;
    if (!read && !write) {
        return trace;
    }
    trace.kind = read
        ? rv32i::RV32IMemoryAccessKind::Read
        : rv32i::RV32IMemoryAccessKind::Write;
    trace.size = memorySize(
        core_->getOutputPin<2>("RETIRED_MEM_SIZE")
            ->getValueAsUInt64());
    trace.sign_extend =
        read
        && core_->getOutputPin(
                "RETIRED_MEM_SIGN_EXTEND")
                   ->getValue() == LogicValue::HIGH;
    trace.address = static_cast<uint32_t>(
        core_->getOutputPin<32>("RETIRED_MEM_ADDR")
            ->getValueAsUInt64());
    trace.write_data = write
        ? static_cast<uint32_t>(
              core_->getOutputPin<32>(
                       "RETIRED_MEM_WRITE_DATA")
                  ->getValueAsUInt64())
        : 0;
    trace.read_data = read
        ? static_cast<uint32_t>(
              core_->getOutputPin<32>(
                       "RETIRED_MEM_READ_DATA")
                  ->getValueAsUInt64())
        : 0;
    trace.fault =
        core_->getOutputPin("RETIRED_MEM_FAULT")
            ->getValue() == LogicValue::HIGH;
    return trace;
}

std::map<uint32_t, uint8_t>
RV32IFiveStageCoreProgramTest::
lastDataMemoryWrites() const {
    // A real five-stage pipeline performs a store in MEM, one cycle before
    // that instruction retires in WB.  Per-retirement lockstep must therefore
    // compare the retiring instruction's trace, not all physical writes that
    // happened since the previous retirement (which can already include the
    // next instruction).  verifyResults() independently checks the complete
    // physical memory write history at the end of the program.
    const auto access = lastDataMemoryAccess();
    std::map<uint32_t, uint8_t> writes;
    if (access.kind != rv32i::RV32IMemoryAccessKind::Write
        || access.fault) {
        return writes;
    }
    size_t byte_count = 0;
    switch (access.size) {
        case rv32i::RV32IMemorySize::Byte: byte_count = 1; break;
        case rv32i::RV32IMemorySize::Halfword: byte_count = 2; break;
        case rv32i::RV32IMemorySize::Word: byte_count = 4; break;
        case rv32i::RV32IMemorySize::None: break;
    }
    for (size_t index = 0; index < byte_count; ++index) {
        writes.emplace(
            access.address + static_cast<uint32_t>(index),
            static_cast<uint8_t>(
                (access.write_data >> (index * 8U)) & 0xffU));
    }
    return writes;
}

void RV32IFiveStageCoreProgramTest::verifyResults() {
    RV32IInstructionLockstepTest::verifyResults();
    const auto& expected = getCase().expected_five_stage;
    if (expected.defined) {
        require(totalCycles() == expected.cycles,
                getTestName() + ": five-stage cycle count changed");
        require(load_use_stall_cycles_
                    == expected.load_use_stall_cycles,
                getTestName() + ": load-use stall count changed");
        require(memory_stall_cycles_
                    == expected.memory_stall_cycles,
                getTestName() + ": memory stall count changed");
        require(data_port_stall_cycles_
                    == expected.data_port_stall_cycles,
                getTestName() + ": data-port stall count changed");
        require(pipeline_flushes_ == expected.pipeline_flushes,
                getTestName() + ": pipeline flush count changed");
    }
    verifyRV32IProgramExpectedResult(
        getCase(),
        snapshotComponentState(),
        data_memory_->getByteWritesInTimeRange(
            0, sim->getCurrentTime()),
        getTestName());
}
