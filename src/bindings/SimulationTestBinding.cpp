#include "Bindings.hpp"
#include "simulator/SimulationTest.hpp"

// A trampoline class is REQUIRED for pybind11 to handle abstract classes.
// It allows Python-derived classes to correctly override C++ virtual methods.
class PySimulationTest : public SimulationTest {
public:
    // Inherit the constructor from the base class
    using SimulationTest::SimulationTest;

    // Provide overrides for ALL pure virtual methods in the hierarchy.
    std::string getTestName() const override { PYBIND11_OVERRIDE_PURE(std::string, SimulationTest, getTestName); }
    void buildCircuit() override { PYBIND11_OVERRIDE_PURE(void, SimulationTest, buildCircuit); }
    void setInitialState() override { PYBIND11_OVERRIDE_PURE(void, SimulationTest, setInitialState); }
    void verifyResults() override { PYBIND11_OVERRIDE_PURE(void, SimulationTest, verifyResults); }
    
    // Use PYBIND11_OVERRIDE for non-pure virtual methods if you want them to be overridable in Python.
    size_t getRunDuration() const override { PYBIND11_OVERRIDE(size_t, SimulationTest, getRunDuration); }
};


void bindSimulationTest(py::module_& m) {
    py::class_<SimulationTest, PySimulationTest, std::shared_ptr<SimulationTest>>(m, "SimulationTest", "The abstract base class for all simulation tests.", py::module_local(false))
        
        .def(py::init<>()) // Bind the default constructor

        // --- Bind the new public methods for Python ---

        .def("setup_circuit", &SimulationTest::setupCircuit,
            "Builds the circuit and sets its initial state.")
        
        .def("get_root", &SimulationTest::getRoot,
            "Returns the root component of the built circuit.")
            
        .def("get_simulator", &SimulationTest::getSimulator,
            // This policy is crucial: it tells Python it does not own the Simulator object,
            // preventing Python from trying to delete it when the pointer goes out of scope.
            py::return_value_policy::reference,
            "Returns a non-owning handle to the simulator instance.");
}