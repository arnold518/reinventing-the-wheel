#include "Bindings.hpp"
#include "components/selection/BuildProfile.hpp"
#include "simulator/SimulationTest.hpp"
#include <algorithm>
#include <pybind11/stl.h>

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
    std::vector<SimulationTest::SimulationCheckpoint> getCheckpoints() const override {
        PYBIND11_OVERRIDE(std::vector<SimulationTest::SimulationCheckpoint>, SimulationTest, getCheckpoints);
    }
    bool isSimulationPrecomputed() const override {
        PYBIND11_OVERRIDE(bool, SimulationTest, isSimulationPrecomputed);
    }
    bool supportsBuildProfile() const override {
        PYBIND11_OVERRIDE(bool, SimulationTest, supportsBuildProfile);
    }
    circuit::BuildProfile getBuildProfile() const override {
        PYBIND11_OVERRIDE(
            circuit::BuildProfile,
            SimulationTest,
            getBuildProfile);
    }
    void setBuildProfile(
        circuit::BuildProfile profile) override {
        PYBIND11_OVERRIDE(
            void,
            SimulationTest,
            setBuildProfile,
            std::move(profile));
    }
};


void bindSimulationTest(py::module_& m) {
    py::class_<circuit::BuildProfile>(
        m,
        "BuildProfile",
        "One recursive component-fidelity selection policy.")
        .def_property_readonly(
            "name",
            &circuit::BuildProfile::name)
        .def_property_readonly(
            "fingerprint",
            &circuit::BuildProfile::fingerprint)
        .def(
            "serialize",
            &circuit::BuildProfile::serialize);

    m.def(
        "canonical_default_profile",
        &circuit::canonicalDefaultProfile,
        "Returns the structural-when-available default profile.");
    m.def(
        "profile_with_exact_overrides",
        [](circuit::BuildProfile base,
           const std::map<std::string, std::string>& overrides,
           const std::string& name) {
            std::vector<circuit::ProfileRule> rules;
            rules.reserve(overrides.size());
            for (const auto& [path, value] : overrides) {
                circuit::Fidelity fidelity;
                if (value == "structural") {
                    fidelity = circuit::Fidelity::Structural;
                } else if (value == "behavioral") {
                    fidelity = circuit::Fidelity::Behavioral;
                } else {
                    throw py::value_error(
                        "Fidelity override for '" + path
                        + "' must be 'structural' or 'behavioral'");
                }
                rules.push_back(circuit::preferFidelity(
                    fidelity,
                    circuit::ProfileSelector::exactPath(path),
                    "visualizer exact-path override"));
            }
            return circuit::withProfileOverrides(
                std::move(base),
                std::move(rules),
                name);
        },
        py::arg("base"),
        py::arg("overrides"),
        py::arg("name") = "visualizer-profile",
        "Appends deterministic exact-path fidelity overrides.");

    py::class_<SimulationTest::SimulationCheckpoint>(m, "SimulationCheckpoint")
        .def_readonly("time", &SimulationTest::SimulationCheckpoint::time)
        .def_readonly("label", &SimulationTest::SimulationCheckpoint::label)
        .def_readonly("detail", &SimulationTest::SimulationCheckpoint::detail)
        .def_readonly("row_index", &SimulationTest::SimulationCheckpoint::row_index);

    py::class_<SimulationTest, PySimulationTest, std::shared_ptr<SimulationTest>>(m, "SimulationTest", "The abstract base class for all simulation tests.", py::module_local(false))
        
        .def(py::init<>()) // Bind the default constructor

        // --- Bind the new public methods for Python ---

        .def("setup_circuit", &SimulationTest::setupCircuit,
            "Builds the circuit and sets its initial state.")

        .def("schedule_initial_events", &SimulationTest::scheduleInitialEvents,
            py::arg("time") = 0,
            "Schedules startup events for source-like components.")
        
        .def("get_root", &SimulationTest::getRoot,
            "Returns the root component of the built circuit.")
            
        .def("get_simulator", &SimulationTest::getSimulator,
            // This policy is crucial: it tells Python it does not own the Simulator object,
            // preventing Python from trying to delete it when the pointer goes out of scope.
            py::return_value_policy::reference,
            "Returns a non-owning handle to the simulator instance.")

        .def("get_run_duration", &SimulationTest::getRunDuration,
            "Returns the duration required by this scenario.")

        .def("get_checkpoints", &SimulationTest::getCheckpoints,
            "Returns semantic timestamps such as settled truth-table rows.")

        .def("is_simulation_precomputed", &SimulationTest::isSimulationPrecomputed,
            "Returns true when setup already produced the complete history.")

        .def(
            "supports_build_profile",
            &SimulationTest::supportsBuildProfile,
            "Returns true when this scenario can be rebuilt with a profile.")
        .def(
            "get_build_profile",
            &SimulationTest::getBuildProfile,
            "Returns the scenario's current recursive build profile.")
        .def(
            "set_build_profile",
            &SimulationTest::setBuildProfile,
            py::arg("profile"),
            "Replaces the profile used by the next setup.");
}
