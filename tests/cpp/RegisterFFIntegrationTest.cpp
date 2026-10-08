#include "tests/MemoryComponentTests.hpp"
#include "modules/memory/Register.hpp"
#include "components/Component.tpp"
#include "tests/TestRegistry.hpp"
#include <functional>
#include <iostream>

using namespace circuit;
using namespace circuit::test;
namespace {
void require(bool condition, const std::string& why) {
    if (!condition) throw std::runtime_error(why);
}
std::shared_ptr<Component> child(const std::shared_ptr<Component>& parent, const std::string& name) {
    for (const auto& item : parent->getChildren()) if (item->getName() == name) return item;
    throw std::runtime_error("Missing child " + name);
}
void hierarchy(const std::shared_ptr<Component>& root, bool compact = false) {
    require(std::string(root->getTypeName()) == "Register<32>", "wrong register type");
    if (compact) require(root->getChildren().empty(), "behavioral register must be compact");
    else for (size_t i = 0; i < 32; ++i)
        require(std::string(child(root, "BIT_" + std::to_string(i))->getTypeName()) == "FlipFlopCell",
                "structural register must use FF cells");
}
}
int main() {
    try {
        hierarchy(Component::create<Register<32>>("CONTEXT_FREE"));
        RegisterTest definition;
        const auto& spec = definition.getSpec();
        const auto& scenario = spec.scenarios.front();
        ComponentTestRunner runner;
        auto reference = runner.run(spec, scenario, withExactFidelity(canonicalDefaultProfile(),
            spec.instance_name, Fidelity::Behavioral));
        for (const std::string mode : {"structural", "behavioral", "cells"}) {
            std::vector<ProfileRule> rules{preferFidelity(mode == "behavioral" ? Fidelity::Behavioral : Fidelity::Structural,
                ProfileSelector::exactPath(spec.instance_name))};
            if (mode == "cells") rules.push_back(preferFidelity(Fidelity::Behavioral,
                ProfileSelector::contract("memory.ff.cell")));
            auto run = runner.run(spec, scenario, withProfileOverrides(canonicalDefaultProfile(), rules));
            ComponentTestRunner::compare(reference, run);
            hierarchy(run.root, mode == "behavioral");
            auto io = std::dynamic_pointer_cast<IOComponent>(run.root);
            auto expected = std::dynamic_pointer_cast<IOComponent>(reference.root);
            for (size_t i = run.checkpoints.size(); i-- > 0;) {
                run.simulator->setCircuitStateAtTime(run.checkpoints[i].actual_time);
                reference.simulator->setCircuitStateAtTime(reference.checkpoints[i].actual_time);
                require(io->getOutputPin<32>("Q")->getValueAsVector() == expected->getOutputPin<32>("Q")->getValueAsVector(),
                    "scrubbed register value differs");
            }
            std::cout << mode << ": " << run.checkpoints.size() << " checkpoints and reverse history passed\n";
        }
        // Exercise both register fidelities in CPU users.
        for (bool pipeline : {false, true}) for (const std::string number : {"03", "05", "07"})
          for (const std::string& mode : (number == "03" ? std::vector<std::string>{"behavioral", "cells"}
                                                       : std::vector<std::string>{"behavioral"})) {
            auto test = createTestByName(std::string(pipeline ? "RV32IFiveStageCoreProgramTest/program-"
                                                             : "RV32ISingleCycleSystemTest/program-") + number);
            test->setBuildProfile(withProfileOverrides(test->getBuildProfile(), {
                preferFidelity(mode == "behavioral" ? Fidelity::Behavioral : Fidelity::Structural,
                    ProfileSelector::contract("memory.register.32")),
                preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("memory.ff.cell")),
            }));
            require(test->run(), "RV32I program failed with new Register<32>");
            size_t registers = 0;
            std::function<void(const std::shared_ptr<Component>&)> visit = [&](const auto& component) {
                if (std::string(component->getTypeName()) == "Register<32>") {
                    hierarchy(component, mode == "behavioral"); ++registers;
                }
                for (const auto& c : component->getChildren()) visit(c);
            };
            visit(test->getRoot());
            require(registers > 0, "CPU test did not exercise structural Register<32>");
            std::cout << (pipeline ? "pipeline" : "single-cycle") << "/" << mode << " program-" << number
                      << ": " << registers << " new Register<32> instances passed\n";
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
