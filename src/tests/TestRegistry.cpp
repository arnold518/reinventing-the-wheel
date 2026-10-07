#include "tests/TestRegistry.hpp"

#include "tests/ArithmeticLogicTests.hpp"
#include "tests/ALU8Tests.hpp"
#include "tests/ALUIntegrationTests.hpp"
#include "tests/CorePrimitiveTests.hpp"
#include "tests/ComponentSelectionTests.hpp"
#include "tests/FullCircuitTest.hpp"
#include "tests/MemoryComponentTests.hpp"
#include "tests/FFMemoryTests.hpp"
#include "tests/SRAMTests.hpp"
#include "tests/MemoryDeviceTests.hpp"
#include "tests/MemoryEndpointTests.hpp"
#include "tests/ALU32LowerLevelSliceTests.hpp"
#include "tests/RV32IBlockStandaloneTests.hpp"
#include "tests/RV32IMemoryAccessTests.hpp"
#include "tests/RV32IControlTests.hpp"
#include "tests/RV32IDecoderTests.hpp"
#include "tests/RV32IElfTests.hpp"
#include "tests/RV32IExternalValidationTests.hpp"
#include "tests/RV32IFiveStageTests.hpp"
#include "tests/RV32IPredictionTests.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "tests/RV32IInstructionOracleTests.hpp"
#include "tests/RV32IPipelineBlockTests.hpp"
#include "tests/RV32IProgramCases.hpp"
#include "tests/RV32IProgramTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include "tests/RV32ISingleCycleStageTests.hpp"
#include "tests/RV32IBaselinePipelineTests.hpp"
#include "tests/UtilityComponentTests.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
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
        "rv32i.control-flow",
        "rv32i.memory-request",
        "rv32i.execution-status",
        "rv32i.single-cycle.fetch",
        "rv32i.single-cycle.decode",
        "rv32i.pipeline-register.if-id",
        "rv32i.pipeline-register.id-ex",
        "rv32i.pipeline-register.ex-mem",
        "rv32i.pipeline-register.mem-wb",
        "memory.response-buffer.8",
        "memory.response-buffer.16",
        "memory.response-buffer.32",
        "rv32i.pipeline.stage.fetch",
        "rv32i.pipeline.stage.decode",
        "rv32i.baseline.fetch",
        "rv32i.baseline.decode",
        "rv32i.prediction.predictor",
        "rv32i.prediction.path",
    };
    static const std::set<std::string> memory{
        "rv32i.register-file",
        };
    static const std::set<std::string> program{
        "rv32i.core.educational-single-cycle",
        "rv32i.core.educational-five-stage",
        "rv32i.system.educational-single-cycle",
        "rv32i.core.baseline-pipeline",
    };
    if (sequential.count(contract_id) != 0 || contract_id.starts_with("memory.ff-word.") ||
        contract_id.starts_with("memory.register.")) {
        return RegisteredTestKind::Sequential;
    }
    if (memory.count(contract_id) != 0 || contract_id.starts_with("memory.endpoint.") ||
        contract_id.starts_with("memory.scalable.") || contract_id.starts_with("memory.controller.") ||
        contract_id.starts_with("memory.device.") ||
        contract_id.starts_with("memory.transaction-port.") || contract_id.starts_with("memory.container.") ||
        contract_id.starts_with("memory.ff-storage-bank.") || contract_id.starts_with("memory.sram.") ||
        contract_id.starts_with("memory.dram.") || contract_id.starts_with("memory.rom.") ||
        contract_id.starts_with("memory.nor-flash.") || contract_id.starts_with("memory.matrix.")) {
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
            "RV32IExternalValidationSmokeTest",
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
            "RV32IElfLoaderTest",
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
        "RewireValidationTest",
        "RV32IProgramLoaderTest",
        "FFAlwaysReadyControllerTest",
        "RegisterTest",
    };
    static const std::set<std::string> headless{
        "RegisterWidthsTest", "ClockedStateBitTest",
    };
    for (auto& entry : entries) {
        if (headless.count(entry.name) != 0) {
            entry.visualizable = false;
            continue;
        }
        entry.visualizable =
            entry.visualizable
            || !entry.contract_ids.empty()
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
        entry<CircuitTimingAnalyzerTest>("CircuitTimingAnalyzerTest"),
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
        entry<FlipFlopCellTest>("FlipFlopCellTest"),
        entry<MemoryStorageTest>("MemoryStorageTest"),
        entry<MemoryTransactionPortTest>("MemoryTransactionPortTest"),
        entry<MemoryResponseBufferTest>("MemoryResponseBufferTest"),
        entry<RegisterWidthsTest>("RegisterWidthsTest"),
        entry<ClockedStateBitTest>("ClockedStateBitTest"),
        entry<MemoryControllerTest>("MemoryControllerTest"),
        entry<FFAlwaysReadyControllerTest>("FFAlwaysReadyControllerTest"),
        entry<MemoryEndpointFFTest>("MemoryEndpointFFTest"),
        entry<MemoryEndpointSRAMTest>("MemoryEndpointSRAMTest"),
        entry<MemoryEndpointDRAMTest>("MemoryEndpointDRAMTest"),
        entry<MemoryEndpointROMTest>("MemoryEndpointROMTest"),
        entry<MemoryEndpointFlashTest>("MemoryEndpointFlashTest"),
        entry<MemoryEndpointRelayTest>("MemoryEndpointRelayTest"),
        entry<ScalableMemoryEndpointTest>("ScalableMemoryEndpointTest"),
        entry<NativeMemoryContainerTest>("NativeMemoryContainerTest"),
        entry<NativeMemoryDeviceContractTest>("NativeMemoryDeviceContractTest"),
        entry<MemoryEndpointContractTest>("MemoryEndpointContractTest"),
        entry<MemoryEndpointTimingTest>("MemoryEndpointTimingTest"),
        entry<FFMemoryTest>("FFMemoryTest"),
        entry<FFWordTest>("FFWordTest"),
        entry<FFWordContractTest>("FFWordContractTest"),
        entry<FFStorageBankTest>("FFStorageBankTest"),
        entry<FFStorageBankContractTest>("FFStorageBankContractTest"),
        entry<FFMemoryFidelityTest>("FFMemoryFidelityTest"),
        entry<SRAMCellTest>("SRAMCellTest"),
        entry<SRAMRowTest>("SRAMRowTest"),
        entry<SRAMBitlinePairTest>("SRAMBitlinePairTest"),
        entry<SRAMRowDecoderTest>("SRAMRowDecoderTest"),
        entry<SRAMPrechargeTest>("SRAMPrechargeTest"),
        entry<SRAMWriteDriverTest>("SRAMWriteDriverTest"),
        entry<SRAMSenseAmplifierTest>("SRAMSenseAmplifierTest"),
        entry<SRAMColumnSelectorTest>("SRAMColumnSelectorTest"),
        entry<SRAMAccessControlTest>("SRAMAccessControlTest"),
        entry<SRAMOutputBufferTest>("SRAMOutputBufferTest"),
        entry<SRAMStorageBankTest>("SRAMStorageBankTest"),
        entry<SRAMTest>("SRAMTest"),
        entry<SRAMContractTest>("SRAMContractTest"),
        entry<SRAMTimingTest>("SRAMTimingTest"),
        entry<MatrixRowDecoderTest>("MatrixRowDecoderTest"),
        entry<MatrixColumnMuxTest>("MatrixColumnMuxTest"),
        entry<MatrixWriteDistributorTest>("MatrixWriteDistributorTest"),
        entry<StaticReadColumnTest>("StaticReadColumnTest"),
        entry<MemoryOutputBufferTest>("MemoryOutputBufferTest"),
        entry<ROMCellTest>("ROMCellTest"),
        entry<ROMRowTest>("ROMRowTest"),
        entry<ROMAddressControlTest>("ROMAddressControlTest"),
        entry<ROMStorageBankTest>("ROMStorageBankTest"),
        entry<ROMTest>("ROMTest"),
        entry<DRAMCellTest>("DRAMCellTest"),
        entry<DRAMRowTest>("DRAMRowTest"),
        entry<DRAMSenseColumnTest>("DRAMSenseColumnTest"),
        entry<DRAMStorageBankTest>("DRAMStorageBankTest"),
        entry<DRAMCommandControlTest>("DRAMCommandControlTest"),
        entry<DRAMTest>("DRAMTest"),
        entry<NORFlashCellTest>("NORFlashCellTest"),
        entry<NORFlashRowTest>("NORFlashRowTest"),
        entry<NORFlashStorageBankTest>("NORFlashStorageBankTest"),
        entry<NORFlashCommandControlTest>("NORFlashCommandControlTest"),
        entry<NORFlashTest>("NORFlashTest"),
        entry<MemoryHelperContractTest>("MemoryHelperContractTest"),
        entry<MemoryDeviceContractTest>("MemoryDeviceContractTest"),
        entry<MemoryDeviceTimingTest>("MemoryDeviceTimingTest"),
        entry<FFMemoryContractTest>("FFMemoryContractTest"),
        entry<RegisterTest>("RegisterTest"),
        entry<RegisterFile32x32Test>("RegisterFile32x32Test"),
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
        entry<XOR8Test>("XOR8Test"),
        entry<NOT8Test>("NOT8Test"),
        entry<Mux2to1Test>("Mux2to1Test"),
        entry<Mux4to1Test>("Mux4to1Test"),
        entry<Mux8to1Test>("Mux8to1Test"),
        entry<Mux32to1Test>("Mux32to1Test"),
        entry<Mux2to1_4bitTest>("Mux2to1_4bitTest"),
        entry<Mux2to1_32bitTest>("Mux2to1_32bitTest"),
        entry<Mux4to1_32bitTest>("Mux4to1_32bitTest"),
        entry<Mux8to1_8bitTest>("Mux8to1_8bitTest"),
        entry<Mux8to1_32bitTest>("Mux8to1_32bitTest"),
        entry<Mux32to1_32bitTest>("Mux32to1_32bitTest"),
        entry<Decoder5to32Test>("Decoder5to32Test"),
        entry<Adder8Test>("Adder8Test"),
        entry<TwosComplement8Test>("TwosComplement8Test"),
        entry<Subtractor8Test>("Subtractor8Test"),
        entry<SubtractorWithBorrow8Test>("SubtractorWithBorrow8Test"),
        entry<Incrementer8Test>("Incrementer8Test"),
        entry<Decrementer8Test>("Decrementer8Test"),
        entry<EqualityChecker8Test>("EqualityChecker8Test"),
        entry<ZeroDetect8Test>("ZeroDetect8Test"),
        entry<ALU8Test>("ALU8Test"),
        entry<AddSub8Test>("AddSub8Test"),
        entry<ArithmeticUnit8Test>("ArithmeticUnit8Test"),
        entry<LogicUnit8Test>("LogicUnit8Test"),
        entry<BarrelShifter8Test>("BarrelShifter8Test"),
        entry<CompareSelect8Test>("CompareSelect8Test"),
        entry<ALU8FlagUnitTest>("ALU8FlagUnitTest"),
        entry<ALU8ContractTest>("ALU8ContractTest"),
        entry<ALU8FidelityContractTest>("ALU8FidelityContractTest"),
        entry<ALU8ControlDecoderTest>("ALU8ControlDecoderTest"),
        entry<ALU8ResultSelectorTest>("ALU8ResultSelectorTest"),
        entry<ALU32ControlDecoderTest>("ALU32ControlDecoderTest"),
        entry<ALU32CompareUnitTest>("ALU32CompareUnitTest"),
        entry<ALU32ResultSelectorTest>("ALU32ResultSelectorTest"),
        entry<ALU32FlagUnitTest>("ALU32FlagUnitTest"),
        entry<ALUHierarchyTest>("ALUHierarchyTest"),
        entry<Adder32Test>("Adder32Test"),
        entry<AddSub32Test>("AddSub32Test"),
        entry<RV32IMemoryLanesTest>("RV32IMemoryLanesTest"),
        entry<RV32IMemoryRequestTest>("RV32IMemoryRequestTest"),
        entry<Logic32Test>("Logic32Test"),
        entry<ZeroDetect32Test>("ZeroDetect32Test"),
        entry<Shifter32Test>("Shifter32Test"),
        entry<ALU32Test>("ALU32Test"),
        entry<ALU32CoreTest>("ALU32CoreTest"),
        entry<ALU32RepresentativeSliceTest>("ALU32RepresentativeSliceTest"),
        entry<RV32IControlFlowUnitTest>("RV32IControlFlowUnitTest"),
        entry<RV32IDecodeControlUnitTest>("RV32IDecodeControlUnitTest"),
        entry<RV32IExecutionControlStatusUnitTest>(
            "RV32IExecutionControlStatusUnitTest"),
        entry<RV32IIFIDPipelineRegisterTest>(
            "RV32IIFIDPipelineRegisterTest"),
        entry<RV32IIDEXPipelineRegisterTest>(
            "RV32IIDEXPipelineRegisterTest"),
        entry<RV32IEXMEMPipelineRegisterTest>(
            "RV32IEXMEMPipelineRegisterTest"),
        entry<RV32IMEMWBPipelineRegisterTest>(
            "RV32IMEMWBPipelineRegisterTest"),
        entry<RV32IForwardingUnitTest>(
            "RV32IForwardingUnitTest"),
        entry<RV32IHazardDetectionUnitTest>(
            "RV32IHazardDetectionUnitTest"),
        entry<RV32IMemoryAlignmentUnitTest>(
            "RV32IMemoryAlignmentUnitTest"),
        entry<RV32IPipelineRetirementUnitTest>(
            "RV32IPipelineRetirementUnitTest"),
        entry<RV32IPipelineCoordinatorTest>(
            "RV32IPipelineCoordinatorTest"),
        entry<RV32IFetchStageTest>(
            "RV32IFetchStageTest"),
        entry<RV32IDecodeStageTest>(
            "RV32IDecodeStageTest"),
        entry<RV32IExecuteStageTest>(
            "RV32IExecuteStageTest"),
        entry<RV32IMemoryStageTest>(
            "RV32IMemoryStageTest"),
        entry<RV32IPipelineExecutionStatusTest>("RV32IPipelineExecutionStatusTest"),
        entry<RV32IWritebackStageTest>(
            "RV32IWritebackStageTest"),
        entry<RV32IFiveStageCoreTest>(
            "RV32IFiveStageCoreTest"),
        entry<RV32IPredictionCounterTest>("RV32IPredictionCounterTest"),
        entry<RV32IBranchPredecodeTest>("RV32IBranchPredecodeTest"),
        entry<RV32IBranchPredictorTest>("RV32IBranchPredictorTest"),
        entry<RV32IBranchRecoveryTest>("RV32IBranchRecoveryTest"),
        entry<RV32IPredictionPathTest>("RV32IPredictionPathTest"),
        entry<RV32IBitPatternMatcherTest>("RV32IBitPatternMatcherTest"),
        entry<RV32IDecoderTest>("RV32IDecoderTest"),
        entry<RV32IControlTest>("RV32IControlTest"),
        entry<RV32IElfLoaderTest>("RV32IElfLoaderTest"),
        entry<RV32IExternalValidationSmokeTest>(
            "RV32IExternalValidationSmokeTest"),
        entry<RV32IProgramLoaderTest>("RV32IProgramLoaderTest"),
        entry<RV32IInstructionOracleTest>("RV32IInstructionOracleTest"),
        entry<RV32IInstructionLockstepHarnessTest>("RV32IInstructionLockstepHarnessTest"),
        entry<RV32IInstructionLockstepMismatchDetectionTest>("RV32IInstructionLockstepMismatchDetectionTest"),
        entry<RV32ISingleCycleCoreTest>("RV32ISingleCycleCoreTest"),
        entry<RV32ISingleCycleFetchStageTest>("RV32ISingleCycleFetchStageTest"),
        entry<RV32ISingleCycleDecodeStageTest>("RV32ISingleCycleDecodeStageTest"),
        entry<RV32ISingleCycleExecuteStageTest>("RV32ISingleCycleExecuteStageTest"),
        entry<RV32ISingleCycleMemoryStageTest>("RV32ISingleCycleMemoryStageTest"),
        entry<RV32ISingleCycleWritebackStageTest>("RV32ISingleCycleWritebackStageTest"),
        entry<RV32ISingleCycleNextPCStageTest>("RV32ISingleCycleNextPCStageTest"),
        entry<RV32ISingleCycleStageIntegrationTest>("RV32ISingleCycleStageIntegrationTest"),
        entry<RV32IBaselineFetchStageTest>("RV32IBaselineFetchStageTest"),
        entry<RV32IBaselineDecodeStageTest>("RV32IBaselineDecodeStageTest"),
        entry<RV32IBaselineExecuteStageTest>("RV32IBaselineExecuteStageTest"),
        entry<RV32IBaselineMemoryStageTest>("RV32IBaselineMemoryStageTest"),
        entry<RV32IBaselineDependencyUnitTest>("RV32IBaselineDependencyUnitTest"),
        entry<RV32IBaselineControlTest>("RV32IBaselineControlTest"),
        entry<RV32IBaselinePipelineCoreTest>("RV32IBaselinePipelineCoreTest"),
        entry<RV32IProfileToggleSweepTest>(
            "RV32IProfileToggleSweepTest"),
        };
        for (size_t program_number = 1;
             program_number <= RV32IProgramCaseCount;
             ++program_number) {
            const auto name =
                rv32iProgramScenarioName(program_number);
            std::vector<std::string> labels{
                "simulation", "contract", "program", "rv32i",
                "system", "slow"};
            if (rv32iProgramCaseIsPerformance(program_number)) {
                labels.push_back("performance");
            }
            entries.push_back({
                name,
                "RV32ISingleCycleSystemTest",
                RegisteredTestKind::Program,
                {},
                "program-"
                    + std::string(program_number < 10 ? "0" : "")
                    + std::to_string(program_number),
                std::move(labels),
                [program_number] {
                    return std::make_unique<
                        RV32ISingleCycleSystemTest>(
                            program_number);
                },
            });
        }
        for (size_t program_number = 1;
             program_number <= RV32IProgramCaseCount;
             ++program_number) {
            const auto name =
                rv32iFiveStageProgramScenarioName(
                    program_number);
            std::vector<std::string> labels{
                "simulation", "integration", "program", "rv32i",
                "pipeline", "slow"};
            if (rv32iProgramCaseIsPerformance(program_number)) {
                labels.push_back("performance");
            }
            entries.push_back({
                name,
                "RV32IFiveStageCoreProgramTest",
                RegisteredTestKind::Program,
                {},
                "program-"
                    + std::string(
                        program_number < 10 ? "0" : "")
                    + std::to_string(program_number),
                std::move(labels),
                [program_number] {
                    return std::make_unique<
                        RV32IFiveStageCoreProgramTest>(
                            program_number, false, std::nullopt);
                },
                true,
            });
        }
        for(size_t n=1;n<=RV32IProgramCaseCount;++n) {
            const std::string scenario="program-"+std::string(n<10?"0":"")+std::to_string(n);
            std::vector<std::string> labels{"simulation","integration","program","rv32i","pipeline","baseline","slow"};
            if (n > RV32ICorrectnessProgramCaseCount) labels.push_back("performance");
            entries.push_back({"RV32IBaselinePipelineProgramTest/"+scenario,"RV32IBaselinePipelineProgramTest",
                RegisteredTestKind::Program,{},scenario,std::move(labels),
                [n]{return std::make_unique<RV32IBaselinePipelineProgramTest>(n);},true});
        }
        for (auto policy : {rv32i::PredictionPolicy::NotTaken,rv32i::PredictionPolicy::Taken,
                            rv32i::PredictionPolicy::BackwardTaken,rv32i::PredictionPolicy::OneBit,rv32i::PredictionPolicy::TwoBit}) {
            for (size_t n = 1; n <= RV32IProgramCaseCount; ++n) {
                const std::string scenario=std::string(rv32i::predictionPolicyName(policy))+"/program-"+std::string(n<10?"0":"")+std::to_string(n);
                std::vector<std::string> labels{"simulation","integration","program","rv32i","pipeline","prediction","slow"};
                if (n > RV32ICorrectnessProgramCaseCount) labels.push_back("performance");
                entries.push_back({"RV32IPredictionProgramTest/"+scenario,"RV32IPredictionProgramTest",RegisteredTestKind::Program,
                    {},scenario,std::move(labels),
                    [n,policy]{return std::make_unique<RV32IFiveStageCoreProgramTest>(n,false,rv32i::PredictionConfig{policy,16});},true});
            }
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
