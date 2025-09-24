#include "Bindings.hpp"
#include "simulator/SimulationTest.hpp"

#include "tests/FullCircuitTest.hpp"
#include "tests/HalfAdderTest.hpp"
#include "tests/FullAdderTest.hpp"

namespace py = pybind11;

void bindTests(py::module_& m) {

    // --- FullCircuitTest Binding ---
    py::class_<FullCircuitTest, SimulationTest, std::shared_ptr<FullCircuitTest>>(m, "FullCircuitTest", "A concrete test scenario that builds a sequential circuit.", py::module_local(false))
        .def(py::init<>())
        .def("get_run_duration", &FullCircuitTest::getRunDuration, "Returns the total duration for the simulation test.");

    // --- HalfAdderTest Binding ---
    py::class_<HalfAdderTest, SimulationTest, std::shared_ptr<HalfAdderTest>>(m, "HalfAdderTest", "A concrete test scenario that builds a sequential circuit.", py::module_local(false))
        .def(py::init<>())
        .def("get_run_duration", &HalfAdderTest::getRunDuration, "Returns the total duration for the simulation test.");

    // --- FullAdderTest Binding ---
    py::class_<FullAdderTest, SimulationTest, std::shared_ptr<FullAdderTest>>(m, "FullAdderTest", "A concrete test scenario that builds a sequential circuit.", py::module_local(false))
        .def(py::init<>())
        .def("get_run_duration", &FullAdderTest::getRunDuration, "Returns the total duration for the simulation test.");
}