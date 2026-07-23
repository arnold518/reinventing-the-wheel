#include "tests/TestRegistry.hpp"

#include "tests/ArithmeticLogicTests.hpp"
#include "tests/CorePrimitiveTests.hpp"
#include "tests/ComponentSelectionTests.hpp"
#include "tests/FullCircuitTest.hpp"
#include "tests/MemoryComponentTests.hpp"
#include "tests/ALU32LowerLevelSliceTests.hpp"
#include "tests/RV32IBlockStandaloneTests.hpp"
#include "tests/RV32IBlockEquivalenceTests.hpp"
#include "tests/RV32IControlTests.hpp"
#include "tests/RV32IDecoderTests.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include "tests/RV32IInstructionOracleTests.hpp"
#include "tests/RV32IProgramTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include "tests/RV32IReferenceSystemTests.hpp"
#include "tests/UtilityComponentTests.hpp"
#include <algorithm>

namespace {
template<typename TestT>
TestRegistryEntry entry(const std::string& name) {
    return {name, [] { return std::make_unique<TestT>(); }};
}
}

const std::vector<TestRegistryEntry>& getTestRegistry() {
    static const std::vector<TestRegistryEntry> registry{
        entry<FullCircuitTest>("FullCircuitTest"),
        entry<WireTemplateTest>("WireTemplateTest"),
        entry<SimulatorAdvanceAndRecordTest>("SimulatorAdvanceAndRecordTest"),
        entry<SimulatorUnwiredOutputPinHistoryTest>("SimulatorUnwiredOutputPinHistoryTest"),
        entry<BuildProfileTest>("BuildProfileTest"),
        entry<ComponentCatalogSelectionTest>("ComponentCatalogSelectionTest"),
        entry<BuiltinComponentCatalogInventoryTest>("BuiltinComponentCatalogInventoryTest"),
        entry<RecursiveBuildProfileTest>("RecursiveBuildProfileTest"),
        entry<MemoryBitEquivalenceTest>("MemoryBitEquivalenceTest"),
        entry<Register32EquivalenceTest>("Register32EquivalenceTest"),
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
        entry<MemoryBitBehavioralContractTest>("MemoryBitBehavioralContractTest"),
        entry<Memory64Kx32Test>("Memory64Kx32Test"),
        entry<MemoryBitStructuralContractTest>("MemoryBitStructuralContractTest"),
        entry<Register32StructuralContractTest>("Register32StructuralContractTest"),
        entry<Register32CellArrayContractTest>("Register32CellArrayContractTest"),
        entry<Register32BehavioralContractTest>("Register32BehavioralContractTest"),
        entry<RegisterFile4x32Test>("RegisterFile4x32Test"),
        entry<RegisterFile32x32StructuralContractTest>("RegisterFile32x32StructuralContractTest"),
        entry<RegisterFile32x32BehavioralContractTest>("RegisterFile32x32BehavioralContractTest"),
        entry<RegisterFile32x32BehavioralUnknownPolicyTest>("RegisterFile32x32BehavioralUnknownPolicyTest"),
        entry<Memory4x32Test>("Memory4x32Test"),
        entry<Memory32x32Test>("Memory32x32Test"),
        entry<RewireUnpackTest>("RewireUnpackTest"),
        entry<RewirePackTest>("RewirePackTest"),
        entry<RewireSliceTest>("RewireSliceTest"),
        entry<RewireZeroExtendTest>("RewireZeroExtendTest"),
        entry<RewireSignExtendTest>("RewireSignExtendTest"),
        entry<RewireUnmappedHighTest>("RewireUnmappedHighTest"),
        entry<RewireWidth5Test>("RewireWidth5Test"),
        entry<RewireUnmappedUnknownTest>("RewireUnmappedUnknownTest"),
        entry<RewireValidationTest>("RewireValidationTest"),
        entry<BitSplitter8Test>("BitSplitter8Test"),
        entry<BitJoiner8Test>("BitJoiner8Test"),
        entry<BitSplitter16Test>("BitSplitter16Test"),
        entry<BitJoiner32Test>("BitJoiner32Test"),
        entry<BitJoiner8UnknownTest>("BitJoiner8UnknownTest"),
        entry<ConstantValue1HighTest>("ConstantValue1HighTest"),
        entry<ConstantValue1LowTest>("ConstantValue1LowTest"),
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
        entry<ALU32StructuralContractTest>("ALU32StructuralContractTest"),
        entry<ALU32BehavioralContractTest>("ALU32BehavioralContractTest"),
        entry<ALU32LowerLevelSliceTest>("ALU32LowerLevelSliceTest"),
        entry<RV32IControlFlowStructuralContractTest>("RV32IControlFlowStructuralContractTest"),
        entry<RV32IControlFlowBehavioralContractTest>("RV32IControlFlowBehavioralContractTest"),
        entry<RV32IDecodeControlStructuralContractTest>("RV32IDecodeControlStructuralContractTest"),
        entry<RV32IDecodeControlBehavioralContractTest>("RV32IDecodeControlBehavioralContractTest"),
        entry<RV32IExecutionStatusStructuralContractTest>("RV32IExecutionStatusStructuralContractTest"),
        entry<RV32IExecutionStatusBehavioralContractTest>("RV32IExecutionStatusBehavioralContractTest"),
        entry<RV32IControlFlowUnitEquivalenceTest>("RV32IControlFlowUnitEquivalenceTest"),
        entry<RV32IDecodeControlUnitEquivalenceTest>("RV32IDecodeControlUnitEquivalenceTest"),
        entry<RV32IRegisterFileEquivalenceTest>("RV32IRegisterFileEquivalenceTest"),
        entry<ALU32EquivalenceTest>("ALU32EquivalenceTest"),
        entry<RV32IExecutionControlStatusUnitEquivalenceTest>("RV32IExecutionControlStatusUnitEquivalenceTest"),
        entry<RV32IBitPatternMatcherTest>("RV32IBitPatternMatcherTest"),
        entry<RV32IDecoderTest>("RV32IDecoderTest"),
        entry<RV32IControlTest>("RV32IControlTest"),
        entry<RV32IProgramLoaderTest>("RV32IProgramLoaderTest"),
        entry<RV32IInstructionOracleTest>("RV32IInstructionOracleTest"),
        entry<RV32IInstructionLockstepHarnessTest>("RV32IInstructionLockstepHarnessTest"),
        entry<RV32IInstructionLockstepMismatchDetectionTest>("RV32IInstructionLockstepMismatchDetectionTest"),
        entry<RV32ISingleCycleSystemSmokeTest>("RV32ISingleCycleSystemSmokeTest"),
        entry<RV32ISingleCycleSystemContractTest>("RV32ISingleCycleSystemContractTest"),
        entry<RV32ISingleCycleSystemProgram1Test>("RV32ISingleCycleSystemProgram1Test"),
        entry<RV32ISingleCycleSystemProgram2Test>("RV32ISingleCycleSystemProgram2Test"),
        entry<RV32ISingleCycleSystemProgram3Test>("RV32ISingleCycleSystemProgram3Test"),
        entry<RV32ISingleCycleSystemProgram4Test>("RV32ISingleCycleSystemProgram4Test"),
        entry<RV32ISingleCycleSystemProgram5Test>("RV32ISingleCycleSystemProgram5Test"),
        entry<RV32ISingleCycleSystemProgram6Test>("RV32ISingleCycleSystemProgram6Test"),
        entry<RV32ISingleCycleSystemProgram7Test>("RV32ISingleCycleSystemProgram7Test"),
        entry<RV32ISingleCycleSystemProgram8Test>("RV32ISingleCycleSystemProgram8Test"),
        entry<RV32ISingleCycleSystemProgram9Test>("RV32ISingleCycleSystemProgram9Test"),
        entry<RV32ISingleCycleSystemProgram10Test>("RV32ISingleCycleSystemProgram10Test"),
        entry<RV32ISingleCycleSystemProgram11Test>("RV32ISingleCycleSystemProgram11Test"),
        entry<RV32ISingleCycleSystemProgram12Test>("RV32ISingleCycleSystemProgram12Test"),
        entry<RV32ISingleCycleSystemProgram13Test>("RV32ISingleCycleSystemProgram13Test"),
        entry<RV32ISingleCycleSystemProgram14Test>("RV32ISingleCycleSystemProgram14Test"),
        entry<RV32ISingleCycleSystemProgram15Test>("RV32ISingleCycleSystemProgram15Test"),
        entry<RV32ISingleCycleSystemProgram16Test>("RV32ISingleCycleSystemProgram16Test"),
        entry<RV32IBalancedSystemProgram1Test>("RV32IBalancedSystemProgram1Test"),
        entry<RV32IBalancedSystemProgram2Test>("RV32IBalancedSystemProgram2Test"),
        entry<RV32IBalancedSystemProgram3Test>("RV32IBalancedSystemProgram3Test"),
        entry<RV32IBalancedSystemProgram4Test>("RV32IBalancedSystemProgram4Test"),
        entry<RV32IBalancedSystemProgram5Test>("RV32IBalancedSystemProgram5Test"),
        entry<RV32IBalancedSystemProgram6Test>("RV32IBalancedSystemProgram6Test"),
        entry<RV32IBalancedSystemProgram7Test>("RV32IBalancedSystemProgram7Test"),
        entry<RV32IBalancedSystemProgram8Test>("RV32IBalancedSystemProgram8Test"),
        entry<RV32IBalancedSystemProgram9Test>("RV32IBalancedSystemProgram9Test"),
        entry<RV32IBalancedSystemProgram10Test>("RV32IBalancedSystemProgram10Test"),
        entry<RV32IBalancedSystemProgram11Test>("RV32IBalancedSystemProgram11Test"),
        entry<RV32IBalancedSystemProgram12Test>("RV32IBalancedSystemProgram12Test"),
        entry<RV32IBalancedSystemProgram13Test>("RV32IBalancedSystemProgram13Test"),
        entry<RV32IBalancedSystemProgram14Test>("RV32IBalancedSystemProgram14Test"),
        entry<RV32IBalancedSystemProgram15Test>("RV32IBalancedSystemProgram15Test"),
        entry<RV32IBalancedSystemProgram16Test>("RV32IBalancedSystemProgram16Test"),
        entry<RV32IReferenceSystemContractTest>("RV32IReferenceSystemContractTest"),
        entry<RV32IReferenceSystemProgram1Test>("RV32IReferenceSystemProgram1Test"),
        entry<RV32IReferenceSystemProgram2Test>("RV32IReferenceSystemProgram2Test"),
        entry<RV32IReferenceSystemProgram3Test>("RV32IReferenceSystemProgram3Test"),
        entry<RV32IReferenceSystemProgram4Test>("RV32IReferenceSystemProgram4Test"),
        entry<RV32IReferenceSystemProgram5Test>("RV32IReferenceSystemProgram5Test"),
        entry<RV32IReferenceSystemProgram6Test>("RV32IReferenceSystemProgram6Test"),
        entry<RV32IReferenceSystemProgram7Test>("RV32IReferenceSystemProgram7Test"),
        entry<RV32IReferenceSystemProgram8Test>("RV32IReferenceSystemProgram8Test"),
        entry<RV32IReferenceSystemProgram9Test>("RV32IReferenceSystemProgram9Test"),
        entry<RV32IReferenceSystemProgram10Test>("RV32IReferenceSystemProgram10Test"),
        entry<RV32IReferenceSystemProgram11Test>("RV32IReferenceSystemProgram11Test"),
        entry<RV32IReferenceSystemProgram12Test>("RV32IReferenceSystemProgram12Test"),
        entry<RV32IReferenceSystemProgram13Test>("RV32IReferenceSystemProgram13Test"),
        entry<RV32IReferenceSystemProgram14Test>("RV32IReferenceSystemProgram14Test"),
        entry<RV32IReferenceSystemProgram15Test>("RV32IReferenceSystemProgram15Test"),
        entry<RV32IReferenceSystemProgram16Test>("RV32IReferenceSystemProgram16Test"),
    };
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
