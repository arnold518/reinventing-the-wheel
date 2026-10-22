#pragma once

#include "components/selection/BuiltinComponentCatalog.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/TestHelpers.hpp"
#include "modules/composite/WordDatapath.hpp"
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

template<circuit::word::Unit U,size_t W,circuit::word::Operations Operations,
         circuit::word::AdderTopology Topology,bool FullFlags=true>
struct WordTestTarget {
    static const circuit::ComponentFamily& family() {
        return circuit::word::family(U,W,{Operations,Topology,FullFlags});
    }
};
template<circuit::word::Operations Operations>
struct ControlTestTarget {
    static const circuit::ComponentFamily& family() {return circuit::word::controlFamily(Operations);}
};

template<typename ComponentT>
class TruthTableComponentTest : public circuit::test::ComponentScenarioTest {
public:
    TruthTableComponentTest(
        std::string test_name,
        std::string root_name,
        std::vector<TestRow> rows,
        circuit::ParameterMap parameters = {},
        std::string contract_id = {})
        : circuit::test::ComponentScenarioTest(
              makeSpec(
                  std::move(test_name),
                  std::move(root_name),
                  std::move(rows),
                  std::move(parameters),
                  std::move(contract_id)),
              "rows") {}

private:
    static std::string contractFor(
        const circuit::ParameterMap& parameters) {
        if constexpr (requires { ComponentT::family(); }) {
            return std::string(ComponentT::family().id());
        } else {
        const auto& catalog = circuit::builtinComponentCatalog();
        std::set<std::string> matches;
        for (const auto& contract : catalog.contracts()) {
            for (const auto& implementation :
                 catalog.implementationsFor(contract.id)) {
                if (implementation.concrete_type_name
                        != ComponentT::TypeName
                    || !implementation.supports(parameters)) {
                    continue;
                }
                matches.insert(contract.id);
            }
        }
        if (matches.size() != 1) {
            throw std::runtime_error(
                "TruthTableComponentTest requires exactly one catalog contract "
                "for concrete type '" + std::string(ComponentT::TypeName)
                + "', found " + std::to_string(matches.size()));
        }
        return *matches.begin();
        }
    }

    static circuit::test::ComponentTestSpec makeSpec(
        std::string test_name,
        std::string root_name,
        std::vector<TestRow> rows,
        circuit::ParameterMap parameters,
        std::string contract_id) {
        if (contract_id.empty()) {
            contract_id = contractFor(parameters);
        }
        auto scenario = circuit::test::actionScenarioFromRows(
            "rows",
            contract_id,
            rows,
            {1'000'000, 4'000'000},
            parameters);
        return {
            std::move(test_name),
            contract_id,
            std::move(root_name),
            std::move(parameters),
            {std::move(scenario)},
        };
    }
};
