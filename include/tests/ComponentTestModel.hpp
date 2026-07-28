#pragma once

#include "basic/LogicValue.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include "tests/TestHelpers.hpp"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Component;

namespace circuit::test {

using LogicVector = std::vector<LogicValue>;
using NamedValues = std::map<std::string, LogicVector>;

enum class CheckpointKind {
    Settled,
    AfterEdge,
    TransactionComplete,
    InstructionCommit,
    SignalTransition,
    AbsoluteTime,
};

struct PinSnapshot {
    NamedValues outputs;

    bool operator==(const PinSnapshot&) const = default;
};

struct RunCheckpoint {
    std::string id;
    CheckpointKind kind = CheckpointKind::Settled;
    size_t actual_time = 0;
    PinSnapshot pins;
};

struct RunLimits {
    size_t max_time_per_action = 1'000'000;
    size_t max_events_per_action = 2'000'000;
};

struct ScenarioAction {
    std::string checkpoint_id;
    CheckpointKind checkpoint_kind = CheckpointKind::Settled;
    NamedValues inputs;
    NamedValues expected_outputs;
    bool emit_checkpoint = true;
    std::string detail;
};

struct ActionScenario {
    std::string id;
    ParameterMap parameters;
    std::vector<ScenarioAction> actions;
    RunLimits limits;
};

struct ComponentTestSpec {
    std::string id;
    std::string contract_id;
    std::string instance_name = "DUT";
    ParameterMap parameters;
    std::vector<ActionScenario> scenarios;
};

struct RunArtifact {
    std::string test_id;
    std::string scenario_id;
    Fidelity root_fidelity = Fidelity::Structural;
    std::shared_ptr<Component> root;
    std::shared_ptr<Simulator> simulator;
    std::shared_ptr<const BuildProfile> profile;
    std::shared_ptr<BuildManifest> manifest;
    std::vector<RunCheckpoint> checkpoints;
    bool completed = false;
};

struct ScenarioRunSet {
    std::vector<RunArtifact> targets;
};

LogicVector logicBits(size_t width, uint64_t value);
LogicVector logicBit(LogicValue value);
LogicVector logicBit(bool value);
std::string formatLogicVector(const LogicVector& values);
ActionScenario actionScenarioFromRows(
    std::string scenario_id,
    const std::string& contract_id,
    const std::vector<TestRow>& rows,
    RunLimits limits = {},
    const ParameterMap& parameters = {});

class ComponentTestRunner {
public:
    explicit ComponentTestRunner(
        const ComponentCatalog& catalog = builtinComponentCatalog());

    std::vector<Fidelity> availableFidelities(
        const ComponentTestSpec& spec,
        const ActionScenario* scenario = nullptr) const;

    RunArtifact run(
        const ComponentTestSpec& spec,
        const ActionScenario& scenario,
        BuildProfile profile = canonicalDefaultProfile()) const;

    ScenarioRunSet runAll(
        const ComponentTestSpec& spec,
        const ActionScenario& scenario,
        BuildProfile profile = canonicalDefaultProfile()) const;

    static void compare(const RunArtifact& expected,
                        const RunArtifact& actual);

private:
    const ComponentCatalog* catalog_;
};

class ComponentScenarioTest : public SimulationTest {
public:
    ComponentScenarioTest(
        ComponentTestSpec spec,
        std::string scenario_id = {},
        BuildProfile profile = canonicalDefaultProfile());

    std::string getTestName() const override;
    void setupCircuit() override;
    bool run() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
    bool isSimulationPrecomputed() const override { return true; }

    const std::vector<RunArtifact>& getRunArtifacts() const {
        return artifacts_;
    }
    bool supportsBuildProfile() const override { return true; }
    BuildProfile getBuildProfile() const override { return profile_; }
    void setBuildProfile(BuildProfile profile) override;

protected:
    void buildCircuit() override {}
    void setInitialState() override {}
    void runSimulation() override {}
    void verifyResults() override {}

private:
    const ActionScenario& selectedScenario() const;
    std::vector<const ActionScenario*> scenariosToRun() const;
    void adoptArtifact(const RunArtifact& artifact);

    ComponentTestSpec spec_;
    std::string scenario_id_;
    BuildProfile profile_;
    std::vector<RunArtifact> artifacts_;
};

} // namespace circuit::test
