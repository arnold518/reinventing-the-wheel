#pragma once

#include "components/selection/BuildProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "tests/ComponentRowsTest.hpp"
#include <string>
#include <utility>
#include <vector>

/**
 * Runs one row table against a component family at a requested fidelity.
 *
 * Structural and behavioral contract-test classes can therefore share the
 * same row generator without naming or constructing implementation classes.
 */
class ComponentFamilyRowsTest : public ComponentRowsTest<IOComponent> {
public:
    ComponentFamilyRowsTest(
        std::string test_name,
        std::string root_name,
        std::vector<TestRow> rows,
        const circuit::ComponentFamily& family,
        circuit::Fidelity fidelity)
        : ComponentRowsTest<IOComponent>(
              std::move(test_name), std::move(root_name), std::move(rows)),
          family_(family),
          fidelity_(fidelity) {}

protected:
    std::shared_ptr<Component> createTestComponent(
        const std::string& name) const override {
        auto profile = circuit::BuildProfileBuilder(
            "rows-test-" + circuit::toString(fidelity_))
            .addRule(circuit::preferFidelity(
                fidelity_,
                circuit::ProfileSelector::exactPath(name),
                "row test selects the requested fidelity"))
            .build();
        return circuit::builtinComponentCatalog()
            .createRoot(family_.request(name), std::move(profile))
            .root;
    }

private:
    const circuit::ComponentFamily& family_;
    circuit::Fidelity fidelity_;
};
