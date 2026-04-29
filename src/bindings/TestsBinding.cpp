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
    bindSimulationScenario<GateTest>(m, "GateTest", "Primitive gate regression scenario.");
    bindSimulationScenario<DFlipFlopTest>(m, "DFlipFlopTest", "D flip-flop timing regression scenario.");
    bindSimulationScenario<ClockGeneratorTest>(m, "ClockGeneratorTest", "Clock generator timing regression scenario.");
    bindSimulationScenario<RewireTest>(m, "RewireTest", "Rewire utility regression scenario.");
    bindSimulationScenario<BitAdapterTest>(m, "BitAdapterTest", "Bit splitter/joiner regression scenario.");
    bindSimulationScenario<ConstantValueTest>(m, "ConstantValueTest", "Constant output utility regression scenario.");
    bindSimulationScenario<HalfAdderTest>(m, "HalfAdderTest", "Half-adder truth-table scenario.");
    bindSimulationScenario<FullAdderTest>(m, "FullAdderTest", "Full-adder truth-table scenario.");
    bindSimulationScenario<Logic8Test>(m, "Logic8Test", "8-bit logic component regression scenario.");
    bindSimulationScenario<MuxTest>(m, "MuxTest", "Mux component regression scenario.");
    bindSimulationScenario<Adder8Test>(m, "Adder8Test", "8-bit adder truth-table scenario.");
    bindSimulationScenario<Arithmetic8Test>(m, "Arithmetic8Test", "8-bit arithmetic component regression scenario.");
    bindSimulationScenario<Comparator8Test>(m, "Comparator8Test", "8-bit comparator component regression scenario.");
    bindSimulationScenario<Shifter8Test>(m, "Shifter8Test", "8-bit shifter component regression scenario.");
    bindSimulationScenario<ZeroDetect8Test>(m, "ZeroDetect8Test", "8-bit zero detector truth-table scenario.");
    bindSimulationScenario<ALU8Test>(m, "ALU8Test", "8-bit ALU truth-table scenario.");

    m.def("get_registered_test_names", &getRegisteredTestNames, "Returns test names available through the C++ test registry.");
}
