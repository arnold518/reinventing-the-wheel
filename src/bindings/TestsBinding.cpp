#include "Bindings.hpp"
#include "simulator/SimulationTest.hpp"

#include "tests/FullCircuitTest.hpp"
#include "tests/HalfAdderTest.hpp"

namespace py = pybind11;

void bindFullCircuitTest(py::module_& m) {
    py::class_<FullCircuitTest, SimulationTest, std::shared_ptr<FullCircuitTest>>(m, "FullCircuitTest", "A concrete test scenario that builds a sequential circuit.", py::module_local(false))
        .def(py::init<>())
        .def("get_run_duration", &FullCircuitTest::getRunDuration, 
            "Returns the total duration for the simulation test.");
}

void bindHalfAdderTest(py::module_& m) {
    py::class_<HalfAdderTest, SimulationTest, std::shared_ptr<HalfAdderTest>>(m, "HalfAdderTest", "A concrete test scenario that builds a sequential circuit.", py::module_local(false))
        .def(py::init<>())
        .def("get_run_duration", &HalfAdderTest::getRunDuration, 
            "Returns the total duration for the simulation test.");
}