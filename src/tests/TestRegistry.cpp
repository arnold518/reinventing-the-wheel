#include "tests/TestRegistry.hpp"

#include "tests/ArithmeticLogicTests.hpp"
#include "tests/CorePrimitiveTests.hpp"
#include "tests/ComponentSelectionTests.hpp"
#include "tests/FullCircuitTest.hpp"
#include "tests/MemoryComponentTests.hpp"
#include "tests/ALU32LowerLevelSliceTests.hpp"
#include "tests/RV32IBlockStandaloneTests.hpp"
#include "tests/RV32IControlTests.hpp"
#include "tests/RV32IDecoderTests.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "tests/RV32IInstructionOracleTests.hpp"
#include "tests/RV32IProgramCases.hpp"
#include "tests/RV32IProgramTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include "tests/UtilityComponentTests.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include <algorithm>
#include <set>

namespace {
template<typename TestT>
TestRegistryEntry entry(const std::string& name) {
    return {
        name,
        name,
        RegisteredTestKind::Infrastructure,
        {},
        {},
        {"simulation"},
        [] { return std::make_unique<TestT>(); },
    };
}

RegisteredTestKind componentKind(
    const std::string& contract_id) {
    static const std::set<std::string> sequential{
        "timing.clock",
        "sequential.sr-latch",
        "sequential.gated-d-latch",
        "sequential.d-flip-flop",
        "memory.write-enabled-bit",
        "memory.register.width32",
        "rv32i.control-flow",
        "rv32i.execution-status",
    };
    static const std::set<std::string> memory{
        "memory.register-file.4x32",
        "rv32i.register-file",
        "memory.word-array.4x32",
        "memory.word-array.32x32",
        "rv32i.memory.64k-x32",
    };
    static const std::set<std::string> program{
        "rv32i.core.educational-single-cycle",
        "rv32i.system.educational-single-cycle",
    };
    if (sequential.count(contract_id) != 0) {
        return RegisteredTestKind::Sequential;
    }
    if (memory.count(contract_id) != 0) {
        return RegisteredTestKind::Memory;
    }
    if (program.count(contract_id) != 0) {
        return RegisteredTestKind::Program;
    }
    return RegisteredTestKind::TruthTable;
}

void appendUnique(
    std::vector<std::string>& values,
    const std::string& value) {
    if (std::find(values.begin(), values.end(), value)
        == values.end()) {
        values.push_back(value);
    }
}

void attachComponentMetadata(
    std::vector<TestRegistryEntry>& entries) {
    const auto& catalog = circuit::builtinComponentCatalog();
    for (const auto& contract : catalog.contracts()) {
        std::set<std::string> evidence_ids;
        for (const auto& implementation :
             catalog.implementationsFor(contract.id)) {
            evidence_ids.insert(
                implementation.evidence.contract_tests.begin(),
                implementation.evidence.contract_tests.end());
        }
        for (const auto& evidence_id : evidence_ids) {
            const auto found = std::find_if(
                entries.begin(), entries.end(),
                [&](const auto& candidate) {
                    return candidate.name == evidence_id;
                });
            if (found == entries.end()) {
                continue;
            }
            appendUnique(found->contract_ids, contract.id);
            found->kind = componentKind(contract.id);
            appendUnique(found->labels, "contract");
            appendUnique(
                found->labels,
                toString(found->kind));
        }
    }
}

void classify(
    std::vector<TestRegistryEntry>& entries,
    const std::set<std::string>& names,
    RegisteredTestKind kind) {
    for (auto& entry : entries) {
        if (names.count(entry.name) == 0 || !entry.contract_ids.empty()) {
            continue;
        }
        entry.kind = kind;
        appendUnique(entry.labels, toString(kind));
    }
}

void attachNonComponentMetadata(
    std::vector<TestRegistryEntry>& entries) {
    classify(
        entries,
        {
            "FullCircuitTest",
            "RV32IInstructionLockstepHarnessTest",
            "RV32IProfileToggleSweepTest",
        },
        RegisteredTestKind::Integration);
    classify(
        entries,
        {
            "ALU32RepresentativeSliceTest",
            "RewireValidationTest",
            "RV32IDecoderTest",
            "RV32IControlTest",
            "RV32IProgramLoaderTest",
            "RV32IInstructionOracleTest",
            "RV32IInstructionLockstepMismatchDetectionTest",
        },
        RegisteredTestKind::Semantic);
    for (auto& entry : entries) {
        if (entry.kind == RegisteredTestKind::Infrastructure) {
            appendUnique(entry.labels, "infrastructure");
        }
    }
}

void markVisualizable(
    std::vector<TestRegistryEntry>& entries) {
    static const std::set<std::string> additional{
        "FullCircuitTest",
        "WireTemplateTest",
        "SimulatorAdvanceAndRecordTest",
        "SimulatorUnwiredOutputPinHistoryTest",
        "RewireValidationTest",
        "ALU32RepresentativeSliceTest",
        "RV32IProgramLoaderTest",
    };
    for (auto& entry : entries) {
        entry.visualizable =
            !entry.contract_ids.empty()
            || additional.count(entry.name) != 0;
    }
}
}

const char* toString(RegisteredTestKind kind) {
    switch (kind) {
        case RegisteredTestKind::Infrastructure:
            return "infrastructure";
        case RegisteredTestKind::Semantic:
            return "semantic";
        case RegisteredTestKind::Integration:
            return "integration";
        case RegisteredTestKind::TruthTable:
            return "truth-table";
        case RegisteredTestKind::Sequential:
            return "sequential";
        case RegisteredTestKind::Memory:
            return "memory";
        case RegisteredTestKind::Program:
            return "program";
    }
    return "unknown";
}

const std::vector<TestRegistryEntry>& getTestRegistry() {
    static const std::vector<TestRegistryEntry> registry = [] {
        std::vector<TestRegistryEntry> entries{
        entry<FullCircuitTest>("FullCircuitTest"),
        entry<WireTemplateTest>("WireTemplateTest"),
        entry<SimulatorAdvanceAndRecordTest>("SimulatorAdvanceAndRecordTest"),
        entry<SimulatorUnwiredOutputPinHistoryTest>("SimulatorUnwiredOutputPinHistoryTest"),
        entry<SimulatorDrainUntilIdleTest>("SimulatorDrainUntilIdleTest"),
        entry<BuildProfileTest>("BuildProfileTest"),
        entry<ComponentCatalogSelectionTest>("ComponentCatalogSelectionTest"),
        entry<BuiltinComponentCatalogInventoryTest>("BuiltinComponentCatalogInventoryTest"),
        entry<RecursiveBuildProfileTest>("RecursiveBuildProfileTest"),
        entry<ComponentTestModelTest>("ComponentTestModelTest"),
        entry<NOTGateTest>("NOTGateTest"),
        entry<ANDGateTest>("ANDGateTest"),
        entry<ORGateTest>("ORGateTest"),
        entry<XORGateTest>("XORGateTest"),
        entry<NANDGateTest>("NANDGateTest"),
        entry<NORGateTest>("NORGateTest"),
        entry<SRLatchTest>("SRLatchTest"),
        entry<GatedDLatchTest>("GatedDLatchTest"),
        entry<DFlipFlopTest>("DFlipFlopTest"),
        entry<ClockGeneratorTest>("ClockGeneratorTest"),
        entry<MemoryBitTest>("MemoryBitTest"),
        entry<Memory64Kx32Test>("Memory64Kx32Test"),
        entry<Register32Test>("Register32Test"),
        entry<RegisterFile4x32Test>("RegisterFile4x32Test"),
        entry<RegisterFile32x32Test>("RegisterFile32x32Test"),
        entry<Memory4x32Test>("Memory4x32Test"),
        entry<Memory32x32Test>("Memory32x32Test"),
        entry<RewireTest>("RewireTest"),
        entry<RewireValidationTest>("RewireValidationTest"),
        entry<BitSplitter1Test>("BitSplitter1Test"),
        entry<BitSplitter2Test>("BitSplitter2Test"),
        entry<BitSplitter3Test>("BitSplitter3Test"),
        entry<BitSplitter4Test>("BitSplitter4Test"),
        entry<BitSplitter5Test>("BitSplitter5Test"),
        entry<BitSplitter8Test>("BitSplitter8Test"),
        entry<BitSplitter16Test>("BitSplitter16Test"),
        entry<BitSplitter32Test>("BitSplitter32Test"),
        entry<BitJoiner1Test>("BitJoiner1Test"),
        entry<BitJoiner2Test>("BitJoiner2Test"),
        entry<BitJoiner3Test>("BitJoiner3Test"),
        entry<BitJoiner4Test>("BitJoiner4Test"),
        entry<BitJoiner5Test>("BitJoiner5Test"),
        entry<BitJoiner8Test>("BitJoiner8Test"),
        entry<BitJoiner16Test>("BitJoiner16Test"),
        entry<BitJoiner32Test>("BitJoiner32Test"),
        entry<ConstantValue1Test>("ConstantValue1Test"),
        entry<ConstantValue2Test>("ConstantValue2Test"),
        entry<ConstantValue4Test>("ConstantValue4Test"),
        entry<ConstantValue8Test>("ConstantValue8Test"),
        entry<ConstantValue32Test>("ConstantValue32Test"),
        entry<HalfAdderTest>("HalfAdderTest"),
        entry<FullAdderTest>("FullAdderTest"),
        entry<AND8Test>("AND8Test"),
        entry<OR8Test>("OR8Test"),
        entry<XOR8Test>("XOR8Test"),
        entry<NOT8Test>("NOT8Test"),
        entry<NAND8Test>("NAND8Test"),
        entry<NOR8Test>("NOR8Test"),
        entry<Mux2to1Test>("Mux2to1Test"),
        entry<Mux4to1Test>("Mux4to1Test"),
        entry<Mux8to1Test>("Mux8to1Test"),
        entry<Mux16to1Test>("Mux16to1Test"),
        entry<Mux32to1Test>("Mux32to1Test"),
        entry<Mux2to1_8bitTest>("Mux2to1_8bitTest"),
        entry<Mux2to1_4bitTest>("Mux2to1_4bitTest"),
        entry<Mux2to1_32bitTest>("Mux2to1_32bitTest"),
        entry<Mux4to1_8bitTest>("Mux4to1_8bitTest"),
        entry<Mux4to1_32bitTest>("Mux4to1_32bitTest"),
        entry<Mux8to1_8bitTest>("Mux8to1_8bitTest"),
        entry<Mux8to1_32bitTest>("Mux8to1_32bitTest"),
        entry<Mux16to1_8bitTest>("Mux16to1_8bitTest"),
        entry<Mux32to1_32bitTest>("Mux32to1_32bitTest"),
        entry<Decoder2to4Test>("Decoder2to4Test"),
        entry<Decoder5to32Test>("Decoder5to32Test"),
        entry<Adder8Test>("Adder8Test"),
        entry<TwosComplement8Test>("TwosComplement8Test"),
        entry<Subtractor8Test>("Subtractor8Test"),
        entry<SubtractorWithBorrow8Test>("SubtractorWithBorrow8Test"),
        entry<Incrementer8Test>("Incrementer8Test"),
        entry<Decrementer8Test>("Decrementer8Test"),
        entry<EqualityChecker8Test>("EqualityChecker8Test"),
        entry<Comparator8Test>("Comparator8Test"),
        entry<SignedComparator8Test>("SignedComparator8Test"),
        entry<ShiftLeftLogical8Test>("ShiftLeftLogical8Test"),
        entry<ShiftRightLogical8Test>("ShiftRightLogical8Test"),
        entry<ShiftRightArithmetic8Test>("ShiftRightArithmetic8Test"),
        entry<ZeroDetect8Test>("ZeroDetect8Test"),
        entry<ALU8Test>("ALU8Test"),
        entry<Adder32Test>("Adder32Test"),
        entry<AddSub32Test>("AddSub32Test"),
        entry<Logic32Test>("Logic32Test"),
        entry<ZeroDetect32Test>("ZeroDetect32Test"),
        entry<Comparator32Test>("Comparator32Test"),
        entry<Shifter32Test>("Shifter32Test"),
        entry<ALU32Test>("ALU32Test"),
        entry<ALU32RepresentativeSliceTest>("ALU32RepresentativeSliceTest"),
        entry<RV32IControlFlowUnitTest>("RV32IControlFlowUnitTest"),
        entry<RV32IDecodeControlUnitTest>("RV32IDecodeControlUnitTest"),
        entry<RV32IExecutionControlStatusUnitTest>(
            "RV32IExecutionControlStatusUnitTest"),
        entry<RV32IBitPatternMatcherTest>("RV32IBitPatternMatcherTest"),
        entry<RV32IDecoderTest>("RV32IDecoderTest"),
        entry<RV32IControlTest>("RV32IControlTest"),
        entry<RV32IProgramLoaderTest>("RV32IProgramLoaderTest"),
        entry<RV32IInstructionOracleTest>("RV32IInstructionOracleTest"),
        entry<RV32IInstructionLockstepHarnessTest>("RV32IInstructionLockstepHarnessTest"),
        entry<RV32IInstructionLockstepMismatchDetectionTest>("RV32IInstructionLockstepMismatchDetectionTest"),
        entry<RV32ISingleCycleCoreTest>("RV32ISingleCycleCoreTest"),
        entry<RV32IProfileToggleSweepTest>(
            "RV32IProfileToggleSweepTest"),
        };
        for (size_t program_number = 1;
             program_number <= 16;
             ++program_number) {
            const auto name =
                rv32iProgramScenarioName(program_number);
            entries.push_back({
                name,
                "RV32ISingleCycleSystemTest",
                RegisteredTestKind::Program,
                {},
                "program-"
                    + std::string(program_number < 10 ? "0" : "")
                    + std::to_string(program_number),
                {"simulation", "contract", "program", "rv32i", "system", "slow"},
                [program_number] {
                    return std::make_unique<
                        RV32ISingleCycleSystemTest>(
                            program_number);
                },
            });
        }
        attachComponentMetadata(entries);
        attachNonComponentMetadata(entries);
        markVisualizable(entries);
        return entries;
    }();
    return registry;
}

std::unique_ptr<SimulationTest> createTestByName(const std::string& name) {
    const auto& registry = getTestRegistry();
    auto it = std::find_if(registry.begin(), registry.end(),
        [&](const TestRegistryEntry& entry) {
            return entry.name == name;
        });
    if (it == registry.end()) {
        return nullptr;
    }
    return it->create();
}

std::vector<std::string> getRegisteredTestNames() {
    std::vector<std::string> names;
    for (const auto& entry : getTestRegistry()) {
        names.push_back(entry.name);
    }
    return names;
}
