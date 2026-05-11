#include "Bindings.hpp"
#include "simulator/SimulationTest.hpp"

#include "tests/ArithmeticLogicTests.hpp"
#include "tests/CorePrimitiveTests.hpp"
#include "tests/FullCircuitTest.hpp"
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
    bindSimulationScenario<NOTGateTest>(m, "NOTGateTest", "NOT gate regression scenario.");
    bindSimulationScenario<ANDGateTest>(m, "ANDGateTest", "AND gate regression scenario.");
    bindSimulationScenario<ORGateTest>(m, "ORGateTest", "OR gate regression scenario.");
    bindSimulationScenario<XORGateTest>(m, "XORGateTest", "XOR gate regression scenario.");
    bindSimulationScenario<NANDGateTest>(m, "NANDGateTest", "NAND gate regression scenario.");
    bindSimulationScenario<NORGateTest>(m, "NORGateTest", "NOR gate regression scenario.");
    bindSimulationScenario<DFlipFlopTest>(m, "DFlipFlopTest", "D flip-flop timing regression scenario.");
    bindSimulationScenario<ClockGeneratorTest>(m, "ClockGeneratorTest", "Clock generator timing regression scenario.");
    bindSimulationScenario<RewireUnpackTest>(m, "RewireUnpackTest", "Rewire unpack regression scenario.");
    bindSimulationScenario<RewirePackTest>(m, "RewirePackTest", "Rewire pack regression scenario.");
    bindSimulationScenario<RewireSliceTest>(m, "RewireSliceTest", "Rewire slice regression scenario.");
    bindSimulationScenario<RewireZeroExtendTest>(m, "RewireZeroExtendTest", "Rewire zero-extension regression scenario.");
    bindSimulationScenario<RewireSignExtendTest>(m, "RewireSignExtendTest", "Rewire sign-extension regression scenario.");
    bindSimulationScenario<RewireUnmappedHighTest>(m, "RewireUnmappedHighTest", "Rewire unmapped-high regression scenario.");
    bindSimulationScenario<RewireUnmappedUnknownTest>(m, "RewireUnmappedUnknownTest", "Rewire unmapped-unknown regression scenario.");
    bindSimulationScenario<RewireValidationTest>(m, "RewireValidationTest", "Rewire validation regression scenario.");
    bindSimulationScenario<BitSplitter8Test>(m, "BitSplitter8Test", "8-bit splitter regression scenario.");
    bindSimulationScenario<BitJoiner8Test>(m, "BitJoiner8Test", "8-bit joiner regression scenario.");
    bindSimulationScenario<BitSplitter16Test>(m, "BitSplitter16Test", "16-bit splitter regression scenario.");
    bindSimulationScenario<BitJoiner32Test>(m, "BitJoiner32Test", "32-bit joiner regression scenario.");
    bindSimulationScenario<BitJoiner8UnknownTest>(m, "BitJoiner8UnknownTest", "8-bit joiner unknown-preservation scenario.");
    bindSimulationScenario<ConstantValue1HighTest>(m, "ConstantValue1HighTest", "Single-bit high constant scenario.");
    bindSimulationScenario<ConstantValue1Low8TriggerTest>(m, "ConstantValue1Low8TriggerTest", "Single-bit low constant with 8-bit trigger scenario.");
    bindSimulationScenario<ConstantValue8Test>(m, "ConstantValue8Test", "8-bit constant scenario.");
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
    bindSimulationScenario<Mux2to1_8bitTest>(m, "Mux2to1_8bitTest", "2:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux4to1_8bitTest>(m, "Mux4to1_8bitTest", "4:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux8to1_8bitTest>(m, "Mux8to1_8bitTest", "8:1 8-bit mux regression scenario.");
    bindSimulationScenario<Mux16to1_8bitTest>(m, "Mux16to1_8bitTest", "16:1 8-bit mux regression scenario.");
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

    m.def("get_registered_test_names", &getRegisteredTestNames, "Returns test names available through the C++ test registry.");
}
