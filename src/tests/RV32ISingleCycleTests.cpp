#include "tests/RV32ISingleCycleTests.hpp"

#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32IReferenceCore.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "simulator/Event.hpp"
#include "tests/RV32IProgramCases.hpp"
#include "tests/RV32IProgramRoot.hpp"
#include "tests/RV32ISystemAnswerSheetRun.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <set>
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

RV32ISystemProgramCase singleCycleProgramCase(size_t number) {
    auto test_case = rv32iProgramCase(number);
    test_case.name = rv32iProgramScenarioName(number);
    test_case.max_cycles_per_instruction = 1;
    test_case.cycle_time_step = 4000;
    return test_case;
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

uint32_t encodeI(
    int32_t immediate,
    uint8_t rs1,
    uint8_t funct3,
    uint8_t rd,
    uint8_t opcode = 0x13) {
    return ((static_cast<uint32_t>(immediate) & 0xfffU) << 20)
        | (static_cast<uint32_t>(rs1) << 15)
        | (static_cast<uint32_t>(funct3) << 12)
        | (static_cast<uint32_t>(rd) << 7)
        | opcode;
}

circuit::test::NamedValues coreStatusOutputs(
    uint32_t pc,
    bool halted,
    bool instruction_attempt) {
    using namespace circuit::test;
    return {
        {"PC", logicBits(32, pc)},
        {"HALTED", logicBit(halted)},
        {"TRAPPED", logicBit(false)},
        {"TRAP_CAUSE", logicBits(4, 0)},
        {"INSTRUCTION_ATTEMPT",
         logicBit(instruction_attempt)},
        {"IMEM_ADDR", logicBits(32, pc)},
        {"IMEM_READ_EN", logicBit(true)},
        {"DMEM_READ_EN", logicBit(false)},
        {"DMEM_WRITE_EN", logicBit(false)},
    };
}

circuit::test::ComponentTestSpec singleCycleCoreSpec() {
    using namespace circuit::test;
    const auto addi_x1 = encodeI(5, 0, 0, 1);
    const auto addi_x2 = encodeI(7, 1, 0, 2);
    constexpr uint32_t EBreak = 0x00100073U;

    ActionScenario scenario{
        "instruction-sequence",
        {},
        {},
        {500'000, 10'000'000},
    };
    scenario.actions = {
        {
            "reset",
            CheckpointKind::Settled,
            {
                {"CLK", logicBit(false)},
                {"RST", logicBit(true)},
                {"ENABLE", logicBit(true)},
                {"IMEM_READ_DATA",
                 LogicVector(32, LogicValue::UNKNOWN)},
                {"IMEM_READY", logicBit(true)},
                {"IMEM_FAULT", logicBit(false)},
                {"DMEM_READ_DATA", logicBits(32, 0)},
                {"DMEM_READY", logicBit(true)},
                {"DMEM_FAULT", logicBit(false)},
            },
            coreStatusOutputs(0, false, false),
        },
        {
            {},
            CheckpointKind::Settled,
            {
                {"RST", logicBit(false)},
                {"IMEM_READY", logicBit(false)},
            },
            {},
            false,
        },
        {
            "wait-for-instruction-memory",
            CheckpointKind::AfterEdge,
            {{"CLK", logicBit(true)}},
            coreStatusOutputs(0, false, false),
        },
        {
            {},
            CheckpointKind::Settled,
            {{"CLK", logicBit(false)}},
            {},
            false,
        },
        {
            {},
            CheckpointKind::Settled,
            {
                {"IMEM_READY", logicBit(true)},
                {"ENABLE", logicBit(false)},
                {"IMEM_READ_DATA", logicBits(32, addi_x1)},
            },
            {},
            false,
        },
        {
            "disabled-hold",
            CheckpointKind::AfterEdge,
            {{"CLK", logicBit(true)}},
            coreStatusOutputs(0, false, false),
        },
        {
            {},
            CheckpointKind::Settled,
            {{"CLK", logicBit(false)}},
            {},
            false,
        },
        {
            {},
            CheckpointKind::Settled,
            {{"ENABLE", logicBit(true)}},
            {},
            false,
        },
        {
            "commit-addi-x1",
            CheckpointKind::InstructionCommit,
            {{"CLK", logicBit(true)}},
            coreStatusOutputs(4, false, true),
        },
        {
            {},
            CheckpointKind::Settled,
            {
                {"CLK", logicBit(false)},
                {"IMEM_READ_DATA", logicBits(32, addi_x2)},
            },
            {},
            false,
        },
        {
            "commit-addi-x2",
            CheckpointKind::InstructionCommit,
            {{"CLK", logicBit(true)}},
            coreStatusOutputs(8, false, true),
        },
        {
            {},
            CheckpointKind::Settled,
            {
                {"CLK", logicBit(false)},
                {"IMEM_READ_DATA", logicBits(32, EBreak)},
            },
            {},
            false,
        },
        {
            "halt",
            CheckpointKind::InstructionCommit,
            {{"CLK", logicBit(true)}},
            coreStatusOutputs(8, true, false),
        },
    };

    return {
        "RV32ISingleCycleCoreTest",
        std::string(
            circuit::families::RV32ISingleCycleCore.id()),
        "RV32I_SINGLE_CYCLE_CORE_ROOT",
        {},
        {std::move(scenario)},
    };
}
}

RV32ISingleCycleCoreTest::RV32ISingleCycleCoreTest()
    : circuit::test::ComponentScenarioTest(
          singleCycleCoreSpec(),
          "instruction-sequence",
          rv32i::withBehavioralMemoryParts(
              circuit::canonicalDefaultProfile())) {}

bool RV32ISingleCycleCoreTest::run() {
    if (!circuit::test::ComponentScenarioTest::run()) {
        return false;
    }
    try {
        const auto& artifacts = getRunArtifacts();
        require(
            artifacts.size() == 2,
            "RV32I core test must run both fidelities");
        for (const auto& artifact : artifacts) {
            const auto state_view =
                std::dynamic_pointer_cast<RV32IStateView>(
                    artifact.root);
            require(
                state_view != nullptr,
                "RV32I core artifact lacks its state-view capability");
            const auto observed =
                state_view->snapshotArchitecturalState();
            const auto known = observed.toKnownState();
            require(
                known.pc == 8
                    && known.x[1] == 5
                    && known.x[2] == 12
                    && known.halted
                    && !known.trapped,
                "RV32I core architectural state is incorrect");
        }
        return true;
    } catch (const std::exception& error) {
        std::cerr
            << "[FAIL] Test 'RV32ISingleCycleCoreTest' "
            << "architectural observation: "
            << error.what() << std::endl;
        return false;
    }
}

void RV32ISystemProfileRun::setupCircuit() {
    const auto shared_profile =
        std::make_shared<const circuit::BuildProfile>(profile_);
    auto manifest = std::make_shared<circuit::BuildManifest>(
        shared_profile->name(), shared_profile->fingerprint());
    auto scope = circuit::BuildContext::rootScope(
        circuit::builtinComponentCatalog(),
        shared_profile,
        manifest);
    auto root_context = scope->child(
        RV32IProgramRoot::RootName, std::nullopt);
    program_root_ = Component::createWithContext<RV32IProgramRoot>(
        root_context,
        RV32IProgramRoot::RootName,
        circuit::families::RV32ISingleCycleCore,
        2000);
    root = program_root_;
    builder = std::make_unique<ComponentBuilder>(
        root, root_context);
    buildCircuit();
    setInitialState();
}

void RV32ISystemProfileRun::setBuildProfile(
    circuit::BuildProfile profile) {
    profile_ = std::move(profile);
    root.reset();
    sim = std::make_shared<Simulator>();
    builder.reset();
    initial_events_scheduled_ = false;
    program_root_.reset();
    core_.reset();
    state_view_.reset();
    instruction_memory_.reset();
    data_memory_.reset();
    rst_wire_.reset();
    enable_wire_.reset();
}

size_t RV32ISystemProfileRun::getRunDuration() const {
    return std::max(visual_run_duration_, RV32IInstructionLockstepTest::getRunDuration());
}

std::vector<SimulationTest::SimulationCheckpoint>
RV32ISystemProfileRun::getCheckpoints() const {
    auto checkpoints = RV32IInstructionLockstepTest::getCheckpoints();
    if (committed_instruction_count_ == 0) {
        for (auto& checkpoint : checkpoints) {
            checkpoint.time += visual_time_origin_;
        }
    }
    return checkpoints;
}

std::vector<SimulationTest::PerformanceMetric>
RV32ISystemProfileRun::getPerformanceMetrics() const {
    if (totalCycles() != 0) {
        return RV32IInstructionLockstepTest::getPerformanceMetrics();
    }
    auto metrics = SimulationTest::getPerformanceMetrics();
    const auto instructions =
        getCase().expected_result.instruction_count;
    metrics.insert(metrics.begin(), {
        {"cpu.hardware_cycles", static_cast<double>(instructions), "cycles", "Single-cycle instructions in the visualized program window."},
        {"cpu.retired_instructions", static_cast<double>(instructions), "instructions", "Architecturally retired instructions, including the terminal instruction."},
        {"cpu.cpi", instructions == 0 ? 0.0 : 1.0, "cycles/instruction", "A single-cycle core completes each instruction in one hardware cycle."},
        {"cpu.ipc", instructions == 0 ? 0.0 : 1.0, "instructions/cycle", "A single-cycle core completes one instruction per hardware cycle."},
    });
    return metrics;
}

void RV32ISystemProfileRun::buildCircuit() {
    require(program_root_ != nullptr, "RV32I program root IO contract");
    core_ = program_root_->core();
    state_view_ = program_root_->stateView();
    instruction_memory_ = program_root_->instructionMemory();
    data_memory_ = program_root_->dataMemory();
    require(core_ != nullptr, "RV32I program root core");
    require(state_view_ != nullptr, "RV32I program root state view");
    require(
        instruction_memory_ != nullptr && data_memory_ != nullptr,
        "RV32I program root memories");
    rst_wire_ = builder->addNewWire(
        "RST_IN", nullptr, {program_root_->getInputPin("RST")});
    enable_wire_ = builder->addNewWire(
        "ENABLE_IN", nullptr, {program_root_->getInputPin("ENABLE")});
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

void RV32ISystemProfileRun::initializeComponentForLockstep(
    const RV32ISystemProgramCase& test_case
) {
    require(
        program_root_ != nullptr,
        "RV32I program root is not initialized");
    require(test_case.initial_pc == 0, "first structural core supports reset PC zero");
    for (size_t index = 0; index < test_case.initial_registers.size(); ++index) {
        require(test_case.initial_registers[index] == 0,
                "first structural core program fixtures require zero initial registers");
    }

    program_root_->clearInstructionMemory();
    program_root_->clearDataMemory();
    program_root_->loadProgram(
        test_case.program, test_case.program_base);
    for (const auto& data : test_case.initial_data) {
        program_root_->loadDataBytes(
            data.address, data.bytes);
    }

    committed_instruction_count_ = 0;
    last_access_ = {};
    scheduleInitialEvents(0);
    program_root_->initializeFixedInputs(*sim, 0);
    drive(*sim, 0, rst_wire_, true);
    drive(*sim, 0, enable_wire_, true);
    program_root_->startClock(*sim, 1000);
    sim->advanceAndRecord(3000);
    drive(*sim, 3000, rst_wire_, false);
    sim->advanceAndRecord(4000);
    visual_time_origin_ = sim->getCurrentTime();
    visual_run_duration_ = visual_time_origin_
                         + test_case.expected_result.instruction_count * test_case.cycle_time_step;
    last_observed_memory_time_ = sim->getCurrentTime();
}

void RV32ISystemProfileRun::clockComponentOneCycle(
    size_t cycle_index,
    size_t cycle_start_time
) {
    (void)cycle_index;
    last_access_ = {};
    const auto reference_core =
        std::dynamic_pointer_cast<RV32IReferenceCore>(core_);
    if (!reference_core) {
        const auto core = core_;
        require(
            core != nullptr,
            "structural RV32I core is not initialized");
        require(
            core->getOutputPin("INSTRUCTION_ATTEMPT")->getValue()
                == LogicValue::HIGH,
            getTestName()
                + ": instruction did not settle before its active edge");

        const bool read =
            core->getOutputPin("DMEM_READ_EN")->getValue()
            == LogicValue::HIGH;
        const bool write =
            core->getOutputPin("DMEM_WRITE_EN")->getValue()
            == LogicValue::HIGH;
        if (read || write) {
            last_access_.kind = read
                ? rv32i::RV32IMemoryAccessKind::Read
                : rv32i::RV32IMemoryAccessKind::Write;
            last_access_.size = memorySize(
                core->getOutputPin<2>("DMEM_SIZE")
                    ->getValueAsUInt64());
            last_access_.sign_extend =
                read
                && core->getOutputPin("DMEM_SIGN_EXTEND")
                       ->getValue()
                    == LogicValue::HIGH;
            last_access_.address = static_cast<uint32_t>(
                core->getOutputPin<32>("DMEM_ADDR")
                    ->getValueAsUInt64());
            last_access_.write_data = write
                ? static_cast<uint32_t>(
                    core->getOutputPin<32>("DMEM_WRITE_DATA")
                        ->getValueAsUInt64())
                : 0;
            const auto memory = data_memory_;
            last_access_.fault =
                memory->getOutputPin("FAULT")->getValue()
                == LogicValue::HIGH;
            if (read && !last_access_.fault) {
                last_access_.read_data =
                    static_cast<uint32_t>(
                        memory->getOutputPin<32>("READ_DATA")
                            ->getValueAsUInt64());
            }
        }
    }

    (void)cycle_start_time;
    ++committed_instruction_count_;
}

rv32i::RV32IState RV32ISystemProfileRun::snapshotComponentState() const {
    require(
        program_root_ != nullptr,
        "RV32I program root is not initialized");
    auto state =
        program_root_->snapshotArchitecturalState().toKnownState();
    state.instruction_count = committed_instruction_count_;
    return state;
}

rv32i::RV32IMemoryTrace RV32ISystemProfileRun::lastDataMemoryAccess() const {
    if (const auto reference =
            std::dynamic_pointer_cast<RV32IReferenceCore>(core_)) {
        return reference->lastDataMemoryAccess();
    }
    return last_access_;
}

std::map<uint32_t, uint8_t> RV32ISystemProfileRun::lastDataMemoryWrites() const {
    require(
        program_root_ != nullptr,
        "RV32I data memory is not initialized");
    const auto current_time = sim->getCurrentTime();
    const auto writes =
        program_root_->dataMemoryWritesInTimeRange(
        last_observed_memory_time_, current_time);
    last_observed_memory_time_ = current_time;
    return writes;
}

void RV32ISystemProfileRun::verifyResults() {
    RV32IInstructionLockstepTest::verifyResults();
    verifyRV32IProgramExpectedResult(
        getCase(),
        snapshotComponentState(),
        program_root_->dataMemoryWritesInTimeRange(
            0, sim->getCurrentTime()),
        getTestName());
}

namespace {
class RV32IProfileToggleRun final
    : public RV32ISystemProfileRun {
public:
    RV32IProfileToggleRun(
        size_t program_number,
        std::string label,
        circuit::BuildProfile profile)
        : program_number_(program_number),
          label_(std::move(label)) {
        setBuildProfile(std::move(profile));
    }

    std::string getTestName() const override {
        return label_;
    }

    bool execute() {
        return SimulationTest::run();
    }

protected:
    RV32ISystemProgramCase getCase() const override {
        return singleCycleProgramCase(program_number_);
    }

private:
    size_t program_number_;
    std::string label_;
};

struct ProfileToggleAuditCase {
    const circuit::ComponentFamily* family;
    size_t program_number;
    std::string context;
};

const std::vector<ProfileToggleAuditCase>&
profileToggleAuditCases() {
    static const std::vector<ProfileToggleAuditCase> cases{
        {&circuit::families::RV32ISingleCycleCore, 1,
         "single-cycle core"},
        {&circuit::families::RV32IControlFlow, 5,
         "jump and link control flow"},
        {&circuit::families::RV32IDecodeControl, 2,
         "R-type and I-type decode"},
        {&circuit::families::RV32IExecutionStatus, 9,
         "illegal-instruction trap state"},
        {&circuit::families::RegisterFile32x32, 7,
         "Fibonacci register dependencies"},
        {&circuit::families::Register32, 7,
         "all register words and program counter"},
        {&circuit::families::MemoryBit, 7,
         "register and program-counter bit cells"},
        {&circuit::families::MemoryBit, 9,
         "halt and trap-status bit cells"},
        {&circuit::families::ALU32, 2,
         "complete ALU operation coverage"},
        {&circuit::families::AddSub32, 2,
         "addition and subtraction"},
        {&circuit::families::Logic32, 2,
         "bitwise logic"},
        {&circuit::families::Shifter32, 2,
         "logical and arithmetic shifts"},
        {&circuit::families::ZeroDetect32, 4,
         "branch equality and result-zero detection"},
    };
    return cases;
}

void visitComponents(
    const std::shared_ptr<Component>& component,
    const std::function<void(const std::shared_ptr<Component>&)>&
        visitor) {
    if (!component) {
        return;
    }
    visitor(component);
    for (const auto& child : component->getChildren()) {
        visitComponents(child, visitor);
    }
}

std::map<std::string, size_t>
selectableContractInventory() {
    const auto profile = std::make_shared<const circuit::BuildProfile>(
        circuit::canonicalDefaultProfile());
    auto manifest = std::make_shared<circuit::BuildManifest>(
        profile->name(), profile->fingerprint());
    auto scope = circuit::BuildContext::rootScope(
        circuit::builtinComponentCatalog(), profile, manifest);
    auto root_context = scope->child(
        RV32IProgramRoot::RootName, std::nullopt);
    auto program_root = Component::createWithContext<RV32IProgramRoot>(
        root_context,
        RV32IProgramRoot::RootName,
        circuit::families::RV32ISingleCycleCore,
        2000);
    std::map<std::string, size_t> inventory;
    visitComponents(
        program_root,
        [&](const auto& component) {
            if (component->isProfileSelectable()) {
                ++inventory[component->getContractId()];
            }
        });
    return inventory;
}

std::string joinContracts(
    const std::set<std::string>& contracts) {
    std::string result;
    for (const auto& contract : contracts) {
        if (!result.empty()) {
            result += ", ";
        }
        result += contract;
    }
    return result;
}
}

RV32ISingleCycleSystemTest::RV32ISingleCycleSystemTest(
    size_t program_number)
    : program_number_(program_number) {
    (void)rv32iProgramScenarioName(program_number_);
}

RV32ISystemProgramCase RV32ISingleCycleSystemTest::getCase() const {
    return singleCycleProgramCase(program_number_);
}

bool RV32ISingleCycleSystemTest::run() {
    try {
        RV32ISystemAnswerSheetRun answer_sheet(program_number_);
        if (!answer_sheet.run()) {
            return false;
        }

        RV32IProfileToggleRun behavioral_profile(
            program_number_,
            getCase().name + "/behavioral-core",
            circuit::strictAllBehavioral());
        if (!behavioral_profile.execute()) {
            return false;
        }
        compareProgramCheckpoints(
            answer_sheet.getCheckpoints(),
            behavioral_profile.getCheckpoints(),
            getCase().name + " behavioral profile");

        if (!SimulationTest::run()) {
            return false;
        }
        compareProgramCheckpoints(
            answer_sheet.getCheckpoints(),
            getCheckpoints(),
            getCase().name + " canonical profile");
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] Test '" << getTestName()
                  << "' comparison threw an exception: "
                  << error.what() << std::endl;
        return false;
    }
}

std::string RV32IProfileToggleSweepTest::getTestName() const {
    return "RV32IProfileToggleSweepTest";
}

void RV32IProfileToggleSweepTest::verifyResults() {
    const auto inventory = selectableContractInventory();
    require(
        !inventory.empty(),
        "RV32I profile-toggle inventory is empty");

    std::set<std::string> covered_contracts;
    size_t selectable_instances = 0;
    for (const auto& [contract, count] : inventory) {
        (void)contract;
        selectable_instances += count;
    }

    for (const auto& audit : profileToggleAuditCases()) {
        const auto contract =
            std::string(audit.family->id());
        const auto expected = inventory.find(contract);
        require(
            expected != inventory.end(),
            "Profile-toggle audit names a contract outside the "
            "canonical RV32I tree: " + contract);
        covered_contracts.insert(contract);

        auto profile = circuit::canonicalDefaultProfile();
        profile = circuit::withProfileOverrides(
            std::move(profile),
            {circuit::preferFidelity(
                circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract(contract),
                "mixed-fidelity audit for " + contract)},
            "rv32i-profile-toggle-" + contract);

        const auto label =
            "RV32IProfileToggleSweepTest/"
            + contract + "/program-"
            + (audit.program_number < 10 ? "0" : "")
            + std::to_string(audit.program_number);
        RV32IProfileToggleRun run(
            audit.program_number,
            label,
            std::move(profile));
        require(
            run.execute(),
            label + " failed in " + audit.context);

        size_t selected_count = 0;
        visitComponents(
            run.getRoot(),
            [&](const auto& component) {
                if (component->getContractId() != contract) {
                    return;
                }
                ++selected_count;
                require(
                    component->isProfileSelectable(),
                    label + " reached a non-selectable target at "
                        + component->getID());
                require(
                    component->getSelectedFidelity()
                        == "behavioral",
                    label + " did not select behavioral fidelity at "
                        + component->getID());
                require(
                    !component
                         ->usedUnavailableFidelityException(),
                    label + " used an unavailable-fidelity fallback at "
                        + component->getID());
            });
        require(
            selected_count == expected->second,
            label + " selected "
                + std::to_string(selected_count)
                + " of " + std::to_string(expected->second)
                + " canonical instances");
        std::cout
            << "[AUDIT] " << contract
            << ": " << selected_count
            << " instance(s) behavioral; "
            << audit.context << std::endl;
    }

    std::set<std::string> inventory_contracts;
    for (const auto& [contract, count] : inventory) {
        (void)count;
        inventory_contracts.insert(contract);
    }
    std::set<std::string> missing;
    std::set_difference(
        inventory_contracts.begin(),
        inventory_contracts.end(),
        covered_contracts.begin(),
        covered_contracts.end(),
        std::inserter(missing, missing.end()));
    require(
        missing.empty(),
        "RV32I profile-toggle audit omitted selectable contracts: "
            + joinContracts(missing));
    std::cout
        << "[AUDIT] Covered all "
        << inventory_contracts.size()
        << " selectable contracts and "
        << selectable_instances
        << " canonical instances." << std::endl;
}
