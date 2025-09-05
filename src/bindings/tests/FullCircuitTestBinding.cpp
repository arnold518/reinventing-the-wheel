#include "Bindings.hpp"
#include "tests/FullCircuitTest.hpp"
#include "simulator/SimulationTest.hpp" // Required for the inheritance declaration

namespace py = pybind11;

void bindFullCircuitTest(py::module_& m) {
    // This binding declares that FullCircuitTest is the C++ class,
    // it inherits from the already-bound SimulationTest,
    // and it's managed by a shared_ptr.
    py::class_<FullCircuitTest, SimulationTest, std::shared_ptr<FullCircuitTest>>(m, "FullCircuitTest", "A concrete test scenario that builds a sequential circuit.", py::module_local(false))
        
        // 1. Bind the constructor so we can create it in Python.
        .def(py::init<>())

        // 2. Bind the specific methods Python needs to call from this derived class.
        //    The other necessary methods (setup_circuit, get_root, get_simulator) are
        //    already available because they were bound in the SimulationTest base class.
        .def("get_run_duration", &FullCircuitTest::getRunDuration, 
            "Returns the total duration for the simulation test.");
}