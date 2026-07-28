#include "Bindings.hpp"

#include "simulator/SimulationTest.hpp"
#include "tests/TestRegistry.hpp"
#include <pybind11/stl.h>

void bindTests(py::module_& m) {
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
