#include "Bindings.hpp"

#include "simulator/SimulationTest.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/RV32IProgramRoot.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <pybind11/stl.h>

void bindTests(py::module_& m) {
    m.def("create_layout_topology", [](const std::string& name) -> std::shared_ptr<Component> {
        auto test = createTestByName(name);
        if (!test) throw py::key_error("Unknown simulation test: " + name);
        if (const auto scenario_test = dynamic_cast<circuit::test::ComponentScenarioTest*>(test.get())) {
            const auto& spec = scenario_test->getSpec();
            const auto& scenario = scenario_test->getSelectedScenario();
            const auto& parameters = scenario.parameters.empty() ? spec.parameters : scenario.parameters;
            auto profile = scenario_test->getBuildProfile();
            circuit::test::ComponentTestRunner runner;
            const auto fidelities = runner.availableFidelities(spec, &scenario);
            if (fidelities.empty()) throw std::invalid_argument("No layout implementation: " + name);
            if (!profile.decide(spec.instance_name, 0, spec.contract_id, parameters).fidelity) {
                profile = circuit::withExactFidelity(profile, spec.instance_name, fidelities.front(), profile.name() + "-preview");
            }
            return circuit::builtinComponentCatalog().createRoot(
                {spec.contract_id, spec.instance_name, parameters, {}, {}, {}}, std::move(profile)).root;
        }
        const bool single = name.starts_with("RV32ISingleCycleSystemTest/program-");
        const bool pipeline = name.starts_with("RV32IFiveStageCoreProgramTest/program-");
        if (single || pipeline) {
            const auto profile = std::make_shared<const circuit::BuildProfile>(test->getBuildProfile());
            const auto manifest = std::make_shared<circuit::BuildManifest>(profile->name(), profile->fingerprint());
            const auto context = circuit::BuildContext::rootScope(circuit::builtinComponentCatalog(), profile, manifest)
                ->child(RV32IProgramRoot::RootName, std::nullopt);
            return Component::createWithContext<RV32IProgramRoot>(context, RV32IProgramRoot::RootName,
                pipeline ? circuit::families::RV32IFiveStageCore : circuit::families::RV32ISingleCycleCore,
                pipeline ? 50 : 2000);
        }
        // Legacy fixture setup builds/preloads only; reject future exceptions
        // instead of silently treating an executed scenario as topology-only.
        test->setupCircuit();
        if (test->getSimulator()->getPerformanceCounters().processed_events != 0)
            throw std::runtime_error("Layout construction unexpectedly simulated: " + name);
        return test->getRoot();
    }, py::arg("name"), "Construct a registered preview hierarchy without running stimuli or recording history.");
    m.def(
        "get_registered_test_names",
        &getRegisteredTestNames,
        "Returns test names available through the C++ test registry.");
    m.def(
        "get_registered_test_descriptors",
        [] {
            py::list result;
            for (const auto& entry : getTestRegistry()) {
                py::dict descriptor;
                descriptor["name"] = entry.name;
                descriptor["logical_test_id"] =
                    entry.logical_test_id;
                descriptor["kind"] = toString(entry.kind);
                descriptor["contract_ids"] = entry.contract_ids;
                descriptor["scenario_id"] = entry.scenario_id;
                descriptor["labels"] = entry.labels;
                descriptor["visualizable"] = entry.visualizable;
                result.append(std::move(descriptor));
            }
            return result;
        },
        "Returns registry metadata for every test scenario.");
    m.def(
        "create_test_by_name",
        [](const std::string& name) {
            auto test = createTestByName(name);
            if (!test) {
                throw py::key_error(
                    "Unknown simulation test: " + name);
            }
            return std::shared_ptr<SimulationTest>(
                std::move(test));
        },
        py::arg("name"),
        "Creates any registered test by its registry name.");
}
