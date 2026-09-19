#include "tests/MemoryComponentTests.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/utility/Constant.hpp"
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
void hierarchy(const std::shared_ptr<Component>& root, bool compact_word = false) {
    require(root->getChildren().size() == 2, "Register32 must wrap one word and a mask");
    auto word = child(root, "STORAGE");
    require(std::string(word->getTypeName()) == "FFWord<32>", "Register32 storage is not FFWord");
    auto mask = std::dynamic_pointer_cast<ConstantValue<4>>(child(root, "FULL_WRITE_MASK"));
    require(mask && mask->getConstantValue() == 15, "not a full-word write mask");
    if (compact_word) require(word->getChildren().empty(), "word override not honored");
    else for (size_t i = 0; i < 32; ++i)
        require(std::string(child(word, "CELL_" + std::to_string(i))->getTypeName()) == "FlipFlopCell",
                "storage must use FF cells");
}
}
int main() {
    try {
        hierarchy(Component::create<Register32>("CONTEXT_FREE"));
        Register32Test definition;
        const auto& spec = definition.getSpec();
        const auto& scenario = spec.scenarios.front();
        ComponentTestRunner runner;
        auto reference = runner.run(spec, scenario, withExactFidelity(canonicalDefaultProfile(),
            spec.instance_name, Fidelity::Behavioral));
        for (const std::string mode : {"structural", "word", "cells"}) {
            std::vector<ProfileRule> rules{preferFidelity(Fidelity::Structural,
                ProfileSelector::exactPath(spec.instance_name))};
            if (mode == "word") rules.push_back(preferFidelity(Fidelity::Behavioral,
                ProfileSelector::exactPath(spec.instance_name + ".STORAGE")));
            if (mode == "cells") rules.push_back(preferFidelity(Fidelity::Behavioral,
                ProfileSelector::contract("memory.write-enabled-bit")));
            auto run = runner.run(spec, scenario, withProfileOverrides(canonicalDefaultProfile(), rules));
            ComponentTestRunner::compare(reference, run);
            hierarchy(run.root, mode == "word");
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
        // Keep the register shell structural in real CPU users, but compact
        // its word/cells. Programs exercise memory, jumps and register hazards.
        for (bool pipeline : {false, true}) for (const std::string number : {"03", "05", "07"})
          for (const std::string mode : (number == "03" ? std::vector<std::string>{"word", "cells"}
                                                       : std::vector<std::string>{"word"})) {
            auto test = createTestByName(std::string(pipeline ? "RV32IFiveStageCoreProgramTest/program-"
                                                             : "RV32ISingleCycleSystemTest/program-") + number);
            test->setBuildProfile(withProfileOverrides(test->getBuildProfile(), {
                preferFidelity(Fidelity::Structural, ProfileSelector::contract("memory.register.width32")),
                preferFidelity(mode == "word" ? Fidelity::Behavioral : Fidelity::Structural,
                    ProfileSelector::contract("memory.ff-word.32")),
                preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("memory.write-enabled-bit")),
            }));
            require(test->run(), "RV32I program failed with new Register32");
            size_t registers = 0;
            std::function<void(const std::shared_ptr<Component>&)> visit = [&](const auto& component) {
                if (std::string(component->getTypeName()) == "Register32") {
                    hierarchy(component, mode == "word"); ++registers;
                }
                for (const auto& c : component->getChildren()) visit(c);
            };
            visit(test->getRoot());
            require(registers > 0, "CPU test did not exercise structural Register32");
            std::cout << (pipeline ? "pipeline" : "single-cycle") << "/" << mode << " program-" << number
                      << ": " << registers << " new Register32 instances passed\n";
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
