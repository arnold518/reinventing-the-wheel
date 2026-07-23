#include "Bindings.hpp"
#include "simulator/SimulationTest.hpp"

#include "tests/ArithmeticLogicTests.hpp"
#include "tests/CorePrimitiveTests.hpp"
#include "tests/FullCircuitTest.hpp"
#include "tests/MemoryComponentTests.hpp"
#include "tests/ALU32LowerLevelSliceTests.hpp"
#include "tests/RV32IBlockStandaloneTests.hpp"
#include "tests/RV32IProgramTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include "tests/RV32IReferenceSystemTests.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/UtilityComponentTests.hpp"
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {
template<typename TestT>
void bindSimulationScenario(py::module_& m, const char* name, const char* description) {
    py::class_<TestT, SimulationTest, std::shared_ptr<TestT>>(m, name, description, py::module_local(false))
        .def(py::init<>())
        .def("get_run_duration", &TestT::getRunDuration, "Returns the total duration for the simulation test.");
}
}

void bindTests(py::module_& m) {
    bindSimulationScenario<FullCircuitTest>(m, "FullCircuitTest", "Sequential integration circuit scenario.");
    bindSimulationScenario<WireTemplateTest>(m, "WireTemplateTest", "Wire and pin template regression scenario.");
    bindSimulationScenario<SimulatorAdvanceAndRecordTest>(
        m,
        "SimulatorAdvanceAndRecordTest",
        "Simulator advance-and-record regression scenario.");
    bindSimulationScenario<SimulatorUnwiredOutputPinHistoryTest>(
        m,
        "SimulatorUnwiredOutputPinHistoryTest",
        "Simulator time-travel regression for unwired output pins.");
    bindSimulationScenario<NOTGateTest>(m, "NOTGateTest", "NOT gate regression scenario.");
    bindSimulationScenario<ANDGateTest>(m, "ANDGateTest", "AND gate regression scenario.");
    bindSimulationScenario<ORGateTest>(m, "ORGateTest", "OR gate regression scenario.");
    bindSimulationScenario<XORGateTest>(m, "XORGateTest", "XOR gate regression scenario.");
    bindSimulationScenario<NANDGateTest>(m, "NANDGateTest", "NAND gate regression scenario.");
    bindSimulationScenario<NORGateTest>(m, "NORGateTest", "NOR gate regression scenario.");
    bindSimulationScenario<SRLatchTest>(m, "SRLatchTest", "Structural active-low SR latch regression scenario.");
    bindSimulationScenario<GatedDLatchTest>(m, "GatedDLatchTest", "Structural gated D latch regression scenario.");
    bindSimulationScenario<DFlipFlopTest>(m, "DFlipFlopTest", "Structural master-slave D flip-flop regression scenario.");
    bindSimulationScenario<ClockGeneratorTest>(m, "ClockGeneratorTest", "Clock generator timing regression scenario.");
    bindSimulationScenario<MemoryBitBehavioralContractTest>(m, "MemoryBitBehavioralContractTest", "Behavioral memory-bit timing and write-enable scenario.");
    bindSimulationScenario<Memory64Kx32Test>(m, "Memory64Kx32Test", "Behavioral 64K-word RV32I memory scenario.");
    bindSimulationScenario<MemoryBitStructuralContractTest>(m, "MemoryBitStructuralContractTest", "Structural memory-bit timing and write-enable scenario.");
    bindSimulationScenario<Register32StructuralContractTest>(m, "Register32StructuralContractTest", "Structural 32-bit register timing and write-enable scenario.");
    bindSimulationScenario<Register32CellArrayContractTest>(
        m,
        "Register32CellArrayContractTest",
        "Structural 32-bit register assembled from independently evaluated bit cells.");
    bindSimulationScenario<Register32BehavioralContractTest>(
        m,
        "Register32BehavioralContractTest",
        "Behavioral 32-bit register contract scenario.");
    bindSimulationScenario<RegisterFile4x32Test>(m, "RegisterFile4x32Test", "Four-entry register file scenario using behavioral memory bits.");
    bindSimulationScenario<RegisterFile32x32StructuralContractTest>(m, "RegisterFile32x32StructuralContractTest", "32-entry register file scenario using behavioral memory bits.");
    bindSimulationScenario<RegisterFile32x32BehavioralContractTest>(m, "RegisterFile32x32BehavioralContractTest", "Compact behavioral 32-entry register file scenario.");
    bindSimulationScenario<RegisterFile32x32BehavioralUnknownPolicyTest>(
        m,
        "RegisterFile32x32BehavioralUnknownPolicyTest",
        "Behavioral 32-entry register file unknown-state scenario.");
    bindSimulationScenario<Memory4x32Test>(m, "Memory4x32Test", "Four-word memory slice scenario.");
    bindSimulationScenario<Memory32x32Test>(m, "Memory32x32Test", "Thirty-two-word memory slice scenario.");
    bindSimulationScenario<RV32IReferenceSystemProgram1Test>(
        m,
        "RV32IReferenceSystemProgram1Test",
        "Reference RV32I system program 1 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram2Test>(
        m,
        "RV32IReferenceSystemProgram2Test",
        "Reference RV32I system program 2 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram3Test>(
        m,
        "RV32IReferenceSystemProgram3Test",
        "Reference RV32I system program 3 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram4Test>(
        m,
        "RV32IReferenceSystemProgram4Test",
        "Reference RV32I system program 4 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram5Test>(
        m,
        "RV32IReferenceSystemProgram5Test",
        "Reference RV32I system program 5 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram6Test>(
        m,
        "RV32IReferenceSystemProgram6Test",
        "Reference RV32I system program 6 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram7Test>(
        m,
        "RV32IReferenceSystemProgram7Test",
        "Reference RV32I system program 7 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram8Test>(
        m,
        "RV32IReferenceSystemProgram8Test",
        "Reference RV32I system program 8 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram9Test>(
        m,
        "RV32IReferenceSystemProgram9Test",
        "Reference RV32I system program 9 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram10Test>(
        m,
        "RV32IReferenceSystemProgram10Test",
        "Reference RV32I system program 10 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram11Test>(
        m,
        "RV32IReferenceSystemProgram11Test",
        "Reference RV32I system program 11 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram12Test>(
        m,
        "RV32IReferenceSystemProgram12Test",
        "Reference RV32I system program 12 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram13Test>(
        m,
        "RV32IReferenceSystemProgram13Test",
        "Reference RV32I system program 13 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram14Test>(
        m,
        "RV32IReferenceSystemProgram14Test",
        "Reference RV32I system program 14 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram15Test>(
        m,
        "RV32IReferenceSystemProgram15Test",
        "Reference RV32I system program 15 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RV32IReferenceSystemProgram16Test>(
        m,
        "RV32IReferenceSystemProgram16Test",
        "Reference RV32I system program 16 lockstep scenario with visible instruction and data memories.");
    bindSimulationScenario<RewireUnpackTest>(m, "RewireUnpackTest", "Rewire unpack regression scenario.");
    bindSimulationScenario<RewirePackTest>(m, "RewirePackTest", "Rewire pack regression scenario.");
    bindSimulationScenario<RewireSliceTest>(m, "RewireSliceTest", "Rewire slice regression scenario.");
    bindSimulationScenario<RewireZeroExtendTest>(m, "RewireZeroExtendTest", "Rewire zero-extension regression scenario.");
    bindSimulationScenario<RewireSignExtendTest>(m, "RewireSignExtendTest", "Rewire sign-extension regression scenario.");
    bindSimulationScenario<RewireUnmappedHighTest>(m, "RewireUnmappedHighTest", "Rewire unmapped-high regression scenario.");
    bindSimulationScenario<RewireWidth5Test>(m, "RewireWidth5Test", "5-bit rewire regression scenario.");
    bindSimulationScenario<RewireUnmappedUnknownTest>(m, "RewireUnmappedUnknownTest", "Rewire unmapped-unknown regression scenario.");
    bindSimulationScenario<RewireValidationTest>(m, "RewireValidationTest", "Rewire validation regression scenario.");
    bindSimulationScenario<BitSplitter8Test>(m, "BitSplitter8Test", "8-bit splitter regression scenario.");
    bindSimulationScenario<BitJoiner8Test>(m, "BitJoiner8Test", "8-bit joiner regression scenario.");
    bindSimulationScenario<BitSplitter16Test>(m, "BitSplitter16Test", "16-bit splitter regression scenario.");
    bindSimulationScenario<BitJoiner32Test>(m, "BitJoiner32Test", "32-bit joiner regression scenario.");
    bindSimulationScenario<BitJoiner8UnknownTest>(m, "BitJoiner8UnknownTest", "8-bit joiner unknown-preservation scenario.");
    bindSimulationScenario<ConstantValue1HighTest>(m, "ConstantValue1HighTest", "Single-bit high constant scenario.");
    bindSimulationScenario<ConstantValue1LowTest>(m, "ConstantValue1LowTest", "Single-bit low constant scenario.");
    bindSimulationScenario<ConstantValue8Test>(m, "ConstantValue8Test", "8-bit constant scenario.");
    bindSimulationScenario<ConstantValue32Test>(m, "ConstantValue32Test", "32-bit constant scenario.");
    bindSimulationScenario<HalfAdderTest>(m, "HalfAdderTest", "Half-adder truth-table scenario.");
    bindSimulationScenario<FullAdderTest>(m, "FullAdderTest", "Full-adder truth-table scenario.");
    bindSimulationScenario<AND8Test>(m, "AND8Test", "8-bit AND regression scenario.");
    bindSimulationScenario<OR8Test>(m, "OR8Test", "8-bit OR regression scenario.");
    bindSimulationScenario<XOR8Test>(m, "XOR8Test", "8-bit XOR regression scenario.");
    bindSimulationScenario<NOT8Test>(m, "NOT8Test", "8-bit NOT regression scenario.");
    bindSimulationScenario<NAND8Test>(m, "NAND8Test", "8-bit NAND regression scenario.");
    bindSimulationScenario<NOR8Test>(m, "NOR8Test", "8-bit NOR regression scenario.");
    bindSimulationScenario<Mux2to1Test>(m, "Mux2to1Test", "2:1 one-bit mux regression scenario.");
    bindSimulationScenario<Mux4to1Test>(m, "Mux4to1Test", "4:1 one-bit mux regression scenario.");
    bindSimulationScenario<Mux8to1Test>(m, "Mux8to1Test", "8:1 one-bit mux regression scenario.");
    bindSimulationScenario<Mux16to1Test>(m, "Mux16to1Test", "16:1 one-bit mux regression scenario.");
    bindSimulationScenario<Mux32to1Test>(m, "Mux32to1Test", "32:1 one-bit mux regression scenario.");
    bindSimulationScenario<Mux2to1_8bitTest>(m, "Mux2to1_8bitTest", "2:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux2to1_4bitTest>(m, "Mux2to1_4bitTest", "2:1 4-bit mux regression scenario.");
    bindSimulationScenario<Mux2to1_32bitTest>(m, "Mux2to1_32bitTest", "2:1 32-bit mux regression scenario.");
    bindSimulationScenario<Mux4to1_8bitTest>(m, "Mux4to1_8bitTest", "4:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux4to1_32bitTest>(m, "Mux4to1_32bitTest", "4:1 32-bit mux regression scenario.");
    bindSimulationScenario<Mux8to1_8bitTest>(m, "Mux8to1_8bitTest", "8:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux8to1_32bitTest>(m, "Mux8to1_32bitTest", "8:1 32-bit mux regression scenario.");
    bindSimulationScenario<Mux16to1_8bitTest>(m, "Mux16to1_8bitTest", "16:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux32to1_32bitTest>(m, "Mux32to1_32bitTest", "32:1 32-bit mux regression scenario.");
    bindSimulationScenario<Decoder2to4Test>(m, "Decoder2to4Test", "2-bit enabled one-hot decoder regression scenario.");
    bindSimulationScenario<Decoder5to32Test>(m, "Decoder5to32Test", "5-bit enabled one-hot decoder regression scenario.");
    bindSimulationScenario<Adder8Test>(m, "Adder8Test", "8-bit adder truth-table scenario.");
    bindSimulationScenario<TwosComplement8Test>(m, "TwosComplement8Test", "8-bit two's-complement regression scenario.");
    bindSimulationScenario<Subtractor8Test>(m, "Subtractor8Test", "8-bit subtractor regression scenario.");
    bindSimulationScenario<SubtractorWithBorrow8Test>(m, "SubtractorWithBorrow8Test", "8-bit subtractor-with-borrow regression scenario.");
    bindSimulationScenario<Incrementer8Test>(m, "Incrementer8Test", "8-bit incrementer regression scenario.");
    bindSimulationScenario<Decrementer8Test>(m, "Decrementer8Test", "8-bit decrementer regression scenario.");
    bindSimulationScenario<EqualityChecker8Test>(m, "EqualityChecker8Test", "8-bit equality checker regression scenario.");
    bindSimulationScenario<Comparator8Test>(m, "Comparator8Test", "8-bit unsigned comparator regression scenario.");
    bindSimulationScenario<SignedComparator8Test>(m, "SignedComparator8Test", "8-bit signed comparator regression scenario.");
    bindSimulationScenario<ShiftLeftLogical8Test>(m, "ShiftLeftLogical8Test", "8-bit logical-left shifter regression scenario.");
    bindSimulationScenario<ShiftRightLogical8Test>(m, "ShiftRightLogical8Test", "8-bit logical-right shifter regression scenario.");
    bindSimulationScenario<ShiftRightArithmetic8Test>(m, "ShiftRightArithmetic8Test", "8-bit arithmetic-right shifter regression scenario.");
    bindSimulationScenario<ZeroDetect8Test>(m, "ZeroDetect8Test", "8-bit zero detector truth-table scenario.");
    bindSimulationScenario<ALU8Test>(m, "ALU8Test", "8-bit ALU truth-table scenario.");
    bindSimulationScenario<Adder32Test>(m, "Adder32Test", "Structural 32-bit adder regression scenario.");
    bindSimulationScenario<AddSub32Test>(m, "AddSub32Test", "Structural 32-bit add/subtract regression scenario.");
    bindSimulationScenario<Logic32Test>(m, "Logic32Test", "Structural 32-bit bitwise logic regression scenario.");
    bindSimulationScenario<ZeroDetect32Test>(m, "ZeroDetect32Test", "Structural 32-bit zero-detector regression scenario.");
    bindSimulationScenario<Comparator32Test>(m, "Comparator32Test", "Structural 32-bit comparator regression scenario.");
    bindSimulationScenario<Shifter32Test>(m, "Shifter32Test", "Structural 32-bit barrel-shifter regression scenario.");
    bindSimulationScenario<ALU32StructuralContractTest>(m, "ALU32StructuralContractTest", "Structural 32-bit ALU regression scenario.");
    bindSimulationScenario<ALU32BehavioralContractTest>(m, "ALU32BehavioralContractTest", "Behavioral 32-bit ALU regression scenario.");
    bindSimulationScenario<ALU32LowerLevelSliceTest>(m, "ALU32LowerLevelSliceTest", "Structural 4-bit add/sub slice and 32-bit ALU regression scenario.");
    bindSimulationScenario<RV32IControlFlowStructuralContractTest>(m, "RV32IControlFlowStructuralContractTest", "Standalone structural RV32I control-flow scenario.");
    bindSimulationScenario<RV32IControlFlowBehavioralContractTest>(m, "RV32IControlFlowBehavioralContractTest", "Standalone behavioral RV32I control-flow scenario.");
    bindSimulationScenario<RV32IDecodeControlStructuralContractTest>(m, "RV32IDecodeControlStructuralContractTest", "Standalone structural RV32I decode/control scenario.");
    bindSimulationScenario<RV32IDecodeControlBehavioralContractTest>(m, "RV32IDecodeControlBehavioralContractTest", "Standalone behavioral RV32I decode/control scenario.");
    bindSimulationScenario<RV32IExecutionStatusStructuralContractTest>(m, "RV32IExecutionStatusStructuralContractTest", "Standalone structural RV32I execution/status scenario.");
    bindSimulationScenario<RV32IExecutionStatusBehavioralContractTest>(m, "RV32IExecutionStatusBehavioralContractTest", "Standalone behavioral RV32I execution/status scenario.");
    bindSimulationScenario<RV32IProgramLoaderTest>(
        m,
        "RV32IProgramLoaderTest",
        "RV32I program-loader fixture with preloaded behavioral memory.");
    bindSimulationScenario<RV32ISingleCycleSystemSmokeTest>(
        m,
        "RV32ISingleCycleSystemSmokeTest",
        "Structural RV32I single-cycle core ADDI/EBREAK smoke scenario.");
    bindSimulationScenario<RV32ISingleCycleSystemContractTest>(
        m,
        "RV32ISingleCycleSystemContractTest",
        "Structural RV32I reset, enable, halt, hold, and recovery scenario.");
#define BIND_STRUCTURAL_RV32I_PROGRAM(NUMBER) \
    bindSimulationScenario<RV32ISingleCycleSystemProgram##NUMBER##Test>( \
        m, \
        "RV32ISingleCycleSystemProgram" #NUMBER "Test", \
        "Structural RV32I system program " #NUMBER " lockstep scenario.");
    BIND_STRUCTURAL_RV32I_PROGRAM(1)
    BIND_STRUCTURAL_RV32I_PROGRAM(2)
    BIND_STRUCTURAL_RV32I_PROGRAM(3)
    BIND_STRUCTURAL_RV32I_PROGRAM(4)
    BIND_STRUCTURAL_RV32I_PROGRAM(5)
    BIND_STRUCTURAL_RV32I_PROGRAM(6)
    BIND_STRUCTURAL_RV32I_PROGRAM(7)
    BIND_STRUCTURAL_RV32I_PROGRAM(8)
    BIND_STRUCTURAL_RV32I_PROGRAM(9)
    BIND_STRUCTURAL_RV32I_PROGRAM(10)
    BIND_STRUCTURAL_RV32I_PROGRAM(11)
    BIND_STRUCTURAL_RV32I_PROGRAM(12)
    BIND_STRUCTURAL_RV32I_PROGRAM(13)
    BIND_STRUCTURAL_RV32I_PROGRAM(14)
    BIND_STRUCTURAL_RV32I_PROGRAM(15)
    BIND_STRUCTURAL_RV32I_PROGRAM(16)
#undef BIND_STRUCTURAL_RV32I_PROGRAM

#define BIND_BALANCED_RV32I_PROGRAM(NUMBER) \
    bindSimulationScenario<RV32IBalancedSystemProgram##NUMBER##Test>( \
        m, \
        "RV32IBalancedSystemProgram" #NUMBER "Test", \
        "Balanced-profile RV32I system program " #NUMBER " lockstep scenario.");
    BIND_BALANCED_RV32I_PROGRAM(1)
    BIND_BALANCED_RV32I_PROGRAM(2)
    BIND_BALANCED_RV32I_PROGRAM(3)
    BIND_BALANCED_RV32I_PROGRAM(4)
    BIND_BALANCED_RV32I_PROGRAM(5)
    BIND_BALANCED_RV32I_PROGRAM(6)
    BIND_BALANCED_RV32I_PROGRAM(7)
    BIND_BALANCED_RV32I_PROGRAM(8)
    BIND_BALANCED_RV32I_PROGRAM(9)
    BIND_BALANCED_RV32I_PROGRAM(10)
    BIND_BALANCED_RV32I_PROGRAM(11)
    BIND_BALANCED_RV32I_PROGRAM(12)
    BIND_BALANCED_RV32I_PROGRAM(13)
    BIND_BALANCED_RV32I_PROGRAM(14)
    BIND_BALANCED_RV32I_PROGRAM(15)
    BIND_BALANCED_RV32I_PROGRAM(16)
#undef BIND_BALANCED_RV32I_PROGRAM

    m.def("get_registered_test_names", &getRegisteredTestNames, "Returns test names available through the C++ test registry.");
}
