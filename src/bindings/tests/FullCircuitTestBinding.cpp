#include "Bindings.hpp"
#include "tests/FullCircuitTest.hpp"
#include "simulator/SimulationTest.hpp"

void bindFullCircuitTest(py::module_& m) {
    py::class_<FullCircuitTest, SimulationTest, std::shared_ptr<FullCircuitTest>>(m, "FullCircuitTest", "A concrete test scenario that builds a sequential circuit.")
        .def(py::init<>());
}