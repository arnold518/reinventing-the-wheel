#include "tests/MemoryComponentTests.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "components/Component.tpp"
#include "tests/TestRegistry.hpp"
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
void checkHierarchy(const std::shared_ptr<Component>& root) {
    require(root->getChildren().size() == 6, "register file must group its storage");
    auto bank = child(root, "STORAGE");
    require(std::string(bank->getTypeName()) == "FFStorageBank<32,31>", "wrong bank geometry");
    require(bank->getChildren().size() == 31, "x0 must not allocate storage");
    auto word = child(bank, "WORD_0");
    require(std::string(word->getTypeName()) == "FFWord<32>", "bank must use FFWord");
    require(std::string(child(word, "CELL_0")->getTypeName()) == "FlipFlopCell", "word must use FF cells");
}
}
int main() {
    try {
        // Parameterized construction must also work without a catalog context.
        checkHierarchy(Component::create<RegisterFile32x32>("CONTEXT_FREE"));
        RegisterFile32x32Test definition;
        auto spec = definition.getSpec();
        auto scenario = spec.scenarios.front();
        // Regression: an uncertain enable with old=1 and new=1 follows the
        // AND/OR feedback mux (X), not optimistic two-candidate merging (1).
        const auto drive = [&](NamedValues inputs) {
            scenario.actions.push_back({"", CheckpointKind::Settled, std::move(inputs), {}, false, ""});
        };
        drive({{"RST",logicBit(true)},{"CLK",logicBit(false)}});
        drive({{"RST",logicBit(false)},{"REG_WRITE",logicBit(true)},
               {"RD_ADDR",logicBits(5,1)},{"RS1_ADDR",logicBits(5,1)},
               {"RS2_ADDR",logicBits(5,0)},{"WRITE_DATA",logicBits(32,0xffffffff)}});
        drive({{"CLK",logicBit(true)}});
        drive({{"CLK",logicBit(false)}});
        drive({{"REG_WRITE",logicBit(LogicValue::UNKNOWN)}});
        scenario.actions.push_back({"uncertain-enable-agreed-ones", CheckpointKind::AfterEdge,
            {{"CLK",logicBit(true)}}, {{"RS1_DATA",LogicVector(32,LogicValue::UNKNOWN)},
                                      {"RS2_DATA",logicBits(32,0)}},true,""});
        ComponentTestRunner runner;
        auto reference = runner.run(spec, scenario, withExactFidelity(canonicalDefaultProfile(),
            spec.instance_name, Fidelity::Behavioral));
        auto reference_view = std::dynamic_pointer_cast<RegisterStateView>(reference.root);
        require(bool(reference_view), "behavioral register-state capability missing");
        for (const auto& mode : {"structural", "bank", "words", "cells"}) {
            std::vector<ProfileRule> rules{preferFidelity(Fidelity::Structural,
                ProfileSelector::exactPath(spec.instance_name))};
            if (std::string(mode) == "bank") rules.push_back(preferFidelity(Fidelity::Behavioral,
                ProfileSelector::exactPath(spec.instance_name + ".STORAGE")));
            if (std::string(mode) == "words") rules.push_back(preferFidelity(Fidelity::Behavioral,
                ProfileSelector::contract("memory.ff-word.32")));
            if (std::string(mode) == "cells") rules.push_back(preferFidelity(Fidelity::Behavioral,
                ProfileSelector::contract("memory.write-enabled-bit")));
            auto run = runner.run(spec, scenario, withProfileOverrides(canonicalDefaultProfile(), rules));
            ComponentTestRunner::compare(reference, run);
            if (std::string(mode) == "structural") checkHierarchy(run.root);
            auto view = std::dynamic_pointer_cast<RegisterStateView>(run.root);
            require(bool(view), "structural register-state capability missing");
            // Scrub backwards, checking all 32 registers, not just the read pins.
            for (size_t i = run.checkpoints.size(); i-- > 0;) {
                const auto time = run.checkpoints[i].actual_time;
                run.simulator->setCircuitStateAtTime(time);
                require(view->getRegisterStateAtTime(time) == reference_view->getRegisterStateAtTime(
                    reference.checkpoints[i].actual_time), "register history mismatch: " + std::string(mode)
                    + "/" + run.checkpoints[i].id);
            }
            std::cout << mode << ": " << run.checkpoints.size() << " checkpoints and full register history passed\n";
        }
        // Exercise the structural register-file shell in both actual CPU users.
        // Compact only its FF bank so these focused program runs stay small.
        for (bool pipeline : {false, true}) {
            auto program = createTestByName(pipeline ? "RV32IFiveStageCoreProgramTest/program-03"
                                                     : "RV32ISingleCycleSystemTest/program-03");
            const auto path = std::string("RV32I_PROGRAM_ROOT.CORE.")
                + "DECODE.REGISTER_FILE";
            program->setBuildProfile(withProfileOverrides(program->getBuildProfile(), {
                preferFidelity(Fidelity::Structural, ProfileSelector::exactPath(path)),
                preferFidelity(Fidelity::Behavioral, ProfileSelector::exactPath(path + ".STORAGE")),
            }));
            require(program->run(), "RV32I program failed with FF-bank register file");
            auto core = child(program->getRoot(), "CORE");
            auto regfile = child(child(core, "DECODE"), "REGISTER_FILE");
            require(regfile->getChildren().size() == 6, "CPU override did not select new structural file");
            require(child(regfile,"STORAGE")->getChildren().empty(), "CPU bank was not compact");
            std::cout << (pipeline ? "pipeline" : "single-cycle") << ": program-03 with new FF bank passed\n";
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
