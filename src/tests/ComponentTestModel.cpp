#include "tests/ComponentTestModel.hpp"

#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "simulator/Event.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace circuit::test {
namespace {

void scheduleInitialEventsForTree(
    const std::shared_ptr<Component>& component,
    Simulator& simulator,
    size_t time) {
    if (!component) {
        return;
    }
    if (auto evaluated = std::dynamic_pointer_cast<BasicComponent>(component)) {
        evaluated->scheduleInitialEvents(simulator, time);
    }
    for (const auto& child : component->getChildren()) {
        scheduleInitialEventsForTree(child, simulator, time);
    }
}

std::shared_ptr<Event> makeWireUpdate(
    size_t time,
    const std::shared_ptr<WireBase>& wire,
    const LogicVector& values) {
    if (!wire || values.size() != wire->getWidth()) {
        return nullptr;
    }

    auto create = [&]<size_t Width>() -> std::shared_ptr<Event> {
        return std::make_shared<WireUpdateEvent<Width>>(
            time,
            std::dynamic_pointer_cast<Wire<Width>>(wire),
            values);
    };

    switch (wire->getWidth()) {
        case 1: return create.operator()<1>();
        case 2: return create.operator()<2>();
        case 3: return create.operator()<3>();
        case 4: return create.operator()<4>();
        case 5: return create.operator()<5>();
        case 8: return create.operator()<8>();
        case 16: return create.operator()<16>();
        case 32: return create.operator()<32>();
        default: return nullptr;
    }
}

PinSnapshot snapshotOutputs(const IOComponent& component) {
    PinSnapshot snapshot;
    for (const auto& [name, pin] : component.getAllOutputPins()) {
        snapshot.outputs.emplace(name, pin->getValueAsVector());
    }
    return snapshot;
}

std::string checkpointDescription(
    const RunArtifact& artifact,
    const RunCheckpoint& checkpoint) {
    return artifact.test_id + "/" + artifact.scenario_id + "/"
        + checkpoint.id + " (" + toString(artifact.root_fidelity) + ")";
}

void requireExpected(
    const RunArtifact& artifact,
    const RunCheckpoint& checkpoint,
    const NamedValues& expected) {
    for (const auto& [name, expected_value] : expected) {
        const auto found = checkpoint.pins.outputs.find(name);
        if (found == checkpoint.pins.outputs.end()) {
            throw std::runtime_error(
                checkpointDescription(artifact, checkpoint)
                + ": expected output pin '" + name + "' does not exist");
        }
        if (found->second != expected_value) {
            throw std::runtime_error(
                checkpointDescription(artifact, checkpoint)
                + ": output '" + name + "' is "
                + formatLogicVector(found->second) + ", expected "
                + formatLogicVector(expected_value));
        }
    }
}

size_t checkedDeadline(size_t start, size_t delta) {
    if (delta > std::numeric_limits<size_t>::max() - start) {
        throw std::overflow_error("Component test action deadline overflow");
    }
    return start + delta;
}

} // namespace

LogicVector logicBits(size_t width, uint64_t value) {
    LogicVector result(width, LogicValue::LOW);
    for (size_t bit = 0; bit < width; ++bit) {
        result[bit] = ((value >> bit) & 1ULL) != 0
            ? LogicValue::HIGH
            : LogicValue::LOW;
    }
    return result;
}

LogicVector logicBit(LogicValue value) {
    return {value};
}

LogicVector logicBit(bool value) {
    return logicBit(value ? LogicValue::HIGH : LogicValue::LOW);
}

std::string formatLogicVector(const LogicVector& values) {
    std::ostringstream out;
    out << '[';
    for (size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            out << ',';
        }
        out << values[index];
    }
    out << ']';
    return out.str();
}

ActionScenario actionScenarioFromRows(
    std::string scenario_id,
    const std::string& contract_id,
    const std::vector<TestRow>& rows,
    RunLimits limits,
    const ParameterMap& parameters) {
    const auto& catalog = builtinComponentCatalog();
    const auto& contract = catalog.contract(contract_id);
    std::shared_ptr<IOComponent> parameterized_schema;
    const auto widthOf = [&](const std::string& name, PinType direction) {
        const auto found = std::find_if(
            contract.pins.begin(), contract.pins.end(),
            [&](const auto& pin) {
                return pin.name == name && pin.direction == direction;
            });
        if (found != contract.pins.end()) {
            return found->width;
        }
        if (!parameterized_schema) {
            std::optional<Fidelity> fidelity;
            for (const auto candidate :
                 {Fidelity::Structural, Fidelity::Behavioral}) {
                const auto implementations =
                    catalog.implementationsFor(contract_id);
                const auto available = std::find_if(
                    implementations.begin(),
                    implementations.end(),
                    [&](const auto& implementation) {
                        return implementation.fidelity == candidate
                            && implementation.supports(parameters);
                    });
                if (available != implementations.end()) {
                    fidelity = candidate;
                    break;
                }
            }
            if (!fidelity) {
                throw std::runtime_error(
                    "Row adapter cannot construct parameterized schema for "
                    "contract '" + contract_id + "'");
            }
            auto profile = withExactFidelity(
                canonicalDefaultProfile(),
                "ROW_SCHEMA",
                *fidelity,
                "row-schema");
            auto build = catalog.createRoot(
                {
                    contract_id,
                    "ROW_SCHEMA",
                    parameters,
                    {},
                    {},
                    {},
                },
                std::move(profile));
            parameterized_schema =
                std::dynamic_pointer_cast<IOComponent>(build.root);
        }
        const auto pin = direction == PinType::INPUT
            ? parameterized_schema->getInputPinDynamic(name)
            : parameterized_schema->getOutputPinDynamic(name);
        if (!pin) {
            throw std::runtime_error(
                "Row adapter cannot find pin '" + name
                + "' in contract '" + contract_id + "'");
        }
        return pin->getWidth();
    };
    const auto convert = [](const TestValue& value, size_t width) {
        if (value.hasLogicVector()) {
            if (value.values.size() != width) {
                throw std::runtime_error(
                    "Logic-vector TestValue width does not match pin width");
            }
            return value.values;
        }
        if (value.multi) {
            return logicBits(width, value.numeric);
        }
        if (width != 1) {
            throw std::runtime_error(
                "Single-bit TestValue used for a multi-bit pin");
        }
        return logicBit(value.logic);
    };

    ActionScenario scenario;
    scenario.id = std::move(scenario_id);
    scenario.parameters = parameters;
    scenario.limits = limits;
    scenario.actions.reserve(rows.size());
    for (size_t index = 0; index < rows.size(); ++index) {
        ScenarioAction action;
        action.checkpoint_id = "row-" + std::to_string(index);
        action.checkpoint_kind = CheckpointKind::Settled;
        action.detail = testRowCheckpointDetail(rows[index]);
        for (const auto& [name, value] : rows[index].inputs) {
            action.inputs.emplace(
                name, convert(value, widthOf(name, PinType::INPUT)));
        }
        for (const auto& [name, value] : rows[index].outputs) {
            action.expected_outputs.emplace(
                name, convert(value, widthOf(name, PinType::OUTPUT)));
        }
        scenario.actions.push_back(std::move(action));
    }
    return scenario;
}

ComponentTestRunner::ComponentTestRunner(const ComponentCatalog& catalog)
    : catalog_(&catalog) {}

std::vector<Fidelity> ComponentTestRunner::availableFidelities(
    const ComponentTestSpec& spec,
    const ActionScenario* scenario) const {
    const auto& parameters =
        scenario && !scenario->parameters.empty()
        ? scenario->parameters
        : spec.parameters;
    std::vector<Fidelity> result;
    for (const auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        const auto implementations =
            catalog_->implementationsFor(spec.contract_id);
        const auto found = std::find_if(
            implementations.begin(), implementations.end(),
            [&](const auto& implementation) {
                return implementation.fidelity == fidelity
                    && implementation.supports(parameters);
            });
        if (found != implementations.end()) {
            result.push_back(fidelity);
        }
    }
    return result;
}

RunArtifact ComponentTestRunner::run(
    const ComponentTestSpec& spec,
    const ActionScenario& scenario,
    BuildProfile profile) const {
    if (spec.id.empty() || spec.contract_id.empty()
        || spec.instance_name.empty() || scenario.id.empty()) {
        throw std::invalid_argument(
            "Component test spec, contract, instance, and scenario IDs are required");
    }
    if (scenario.actions.empty()) {
        throw std::invalid_argument("Component test scenario requires actions");
    }
    if (scenario.limits.max_events_per_action == 0) {
        throw std::invalid_argument(
            "Component test scenario requires a positive event limit");
    }

    const auto& parameters = scenario.parameters.empty()
        ? spec.parameters
        : scenario.parameters;
    ComponentBuildRequest request{
        spec.contract_id,
        spec.instance_name,
        parameters,
        {},
        {},
        {},
    };
    auto build = catalog_->createRoot(
        std::move(request),
        std::move(profile));
    auto io = std::dynamic_pointer_cast<IOComponent>(build.root);
    if (!io) {
        throw std::runtime_error("Component test DUT is not an IOComponent");
    }

    auto simulator = std::make_shared<Simulator>();
    ComponentBuilder builder(build.root);
    std::map<std::string, std::shared_ptr<WireBase>> input_wires;
    for (const auto& [name, pin] : io->getAllInputPins()) {
        auto wire = builder.addNewWireDynamic(
            "TEST_INPUT_" + name, pin->getWidth(), nullptr, {pin});
        if (!wire) {
            throw std::runtime_error(
                "Unsupported input width for pin '" + name + "'");
        }
        input_wires.emplace(name, std::move(wire));
    }
    for (const auto& [name, pin] : io->getAllOutputPins()) {
        auto wire = builder.addNewWireDynamic(
            "TEST_OUTPUT_" + name, pin->getWidth(), pin, {});
        if (!wire) {
            throw std::runtime_error(
                "Unsupported output width for pin '" + name + "'");
        }
    }

    const auto& metadata = build.root->getInstanceMetadata();
    if (!metadata) {
        throw std::runtime_error(
            "Component test DUT has no catalog selection metadata");
    }

    RunArtifact artifact{
        spec.id,
        scenario.id,
        metadata->fidelity,
        build.root,
        simulator,
        build.profile,
        build.manifest,
        {},
        false,
    };

    scheduleInitialEventsForTree(build.root, *simulator, 0);
    size_t next_action_time = 0;
    for (const auto& action : scenario.actions) {
        if (action.inputs.empty() && !input_wires.empty()) {
            throw std::invalid_argument(
                spec.id + "/" + scenario.id
                + ": every action for a component with inputs must drive "
                  "at least one input");
        }
        for (const auto& [name, values] : action.inputs) {
            const auto found = input_wires.find(name);
            if (found == input_wires.end()) {
                throw std::runtime_error(
                    spec.id + "/" + scenario.id
                    + ": input pin '" + name + "' does not exist");
            }
            auto event = makeWireUpdate(
                next_action_time, found->second, values);
            if (!event) {
                throw std::runtime_error(
                    spec.id + "/" + scenario.id
                    + ": input width/value mismatch for '" + name + "'");
            }
            simulator->scheduleEvent(std::move(event));
        }

        const auto drain = simulator->drainUntilIdle(
            checkedDeadline(
                next_action_time, scenario.limits.max_time_per_action),
            scenario.limits.max_events_per_action);
        if (!drain.idle()) {
            const auto reason = drain.status == DrainStatus::DeadlineReached
                ? "deadline"
                : "event limit";
            throw std::runtime_error(
                spec.id + "/" + scenario.id
                + ": action '" + action.checkpoint_id
                + "' did not become idle before its " + reason);
        }

        if (action.emit_checkpoint) {
            if (action.checkpoint_id.empty()) {
                throw std::invalid_argument(
                    "Emitted checkpoint requires a stable ID");
            }
            RunCheckpoint checkpoint{
                action.checkpoint_id,
                action.checkpoint_kind,
                drain.final_time,
                snapshotOutputs(*io),
            };
            if (!artifact.checkpoints.empty()
                && checkpoint.actual_time
                    <= artifact.checkpoints.back().actual_time) {
                throw std::runtime_error(
                    spec.id + "/" + scenario.id
                    + ": checkpoint times are not strictly increasing");
            }
            requireExpected(artifact, checkpoint, action.expected_outputs);
            artifact.checkpoints.push_back(std::move(checkpoint));
        }

        if (drain.final_time == std::numeric_limits<size_t>::max()) {
            throw std::overflow_error("Component test action time overflow");
        }
        next_action_time = drain.final_time + 1;
    }

    artifact.completed = true;
    return artifact;
}

ScenarioRunSet ComponentTestRunner::runAll(
    const ComponentTestSpec& spec,
    const ActionScenario& scenario,
    BuildProfile profile) const {
    ScenarioRunSet result;
    const auto fidelities = availableFidelities(spec, &scenario);
    if (fidelities.empty()) {
        throw std::runtime_error(
            "No selectable fidelity for component test '" + spec.id + "'");
    }
    for (const auto fidelity : fidelities) {
        auto run_profile = withExactFidelity(
            profile,
            spec.instance_name,
            fidelity,
            profile.name() + "-" + toString(fidelity));
        result.targets.push_back(
            run(spec, scenario, std::move(run_profile)));
    }
    for (size_t index = 1; index < result.targets.size(); ++index) {
        compare(result.targets.front(), result.targets[index]);
    }
    return result;
}

void ComponentTestRunner::compare(
    const RunArtifact& expected,
    const RunArtifact& actual) {
    if (!expected.completed || !actual.completed) {
        throw std::runtime_error("Cannot compare incomplete run artifacts");
    }
    if (expected.test_id != actual.test_id
        || expected.scenario_id != actual.scenario_id) {
        throw std::runtime_error("Run artifact identities differ");
    }
    if (expected.checkpoints.size() != actual.checkpoints.size()) {
        throw std::runtime_error(
            expected.test_id + "/" + expected.scenario_id
            + ": checkpoint counts differ");
    }
    for (size_t index = 0; index < expected.checkpoints.size(); ++index) {
        const auto& left = expected.checkpoints[index];
        const auto& right = actual.checkpoints[index];
        if (left.id != right.id || left.kind != right.kind) {
            throw std::runtime_error(
                expected.test_id + "/" + expected.scenario_id
                + ": checkpoint identity/order differs at index "
                + std::to_string(index));
        }
        if (left.pins != right.pins) {
            throw std::runtime_error(
                expected.test_id + "/" + expected.scenario_id + "/"
                + left.id
                + ": public output snapshots differ between "
                + toString(expected.root_fidelity) + " and "
                + toString(actual.root_fidelity));
        }
    }
}

ComponentScenarioTest::ComponentScenarioTest(
    ComponentTestSpec spec,
    std::string scenario_id,
    BuildProfile profile)
    : spec_(std::move(spec)),
      scenario_id_(std::move(scenario_id)),
      profile_(std::move(profile)) {}

void ComponentScenarioTest::setBuildProfile(
    BuildProfile profile) {
    profile_ = std::move(profile);
    artifacts_.clear();
    root.reset();
    sim = std::make_shared<Simulator>();
    builder.reset();
    initial_events_scheduled_ = false;
}

std::string ComponentScenarioTest::getTestName() const {
    return spec_.id;
}

const ActionScenario& ComponentScenarioTest::selectedScenario() const {
    if (scenario_id_.empty()) {
        if (spec_.scenarios.empty()) {
            throw std::out_of_range(
                spec_.id + ": test has no scenarios");
        }
        return spec_.scenarios.front();
    }
    const auto found = std::find_if(
        spec_.scenarios.begin(), spec_.scenarios.end(),
        [&](const auto& scenario) {
            return scenario.id == scenario_id_;
        });
    if (found == spec_.scenarios.end()) {
        throw std::out_of_range(
            spec_.id + ": unknown scenario '" + scenario_id_ + "'");
    }
    return *found;
}

std::vector<const ActionScenario*>
ComponentScenarioTest::scenariosToRun() const {
    if (!scenario_id_.empty()) {
        return {&selectedScenario()};
    }
    std::vector<const ActionScenario*> result;
    result.reserve(spec_.scenarios.size());
    for (const auto& scenario : spec_.scenarios) {
        result.push_back(&scenario);
    }
    return result;
}

void ComponentScenarioTest::adoptArtifact(const RunArtifact& artifact) {
    root = artifact.root;
    sim = artifact.simulator;
    builder.reset();
    initial_events_scheduled_ = true;
}

void ComponentScenarioTest::setupCircuit() {
    ComponentTestRunner runner;
    const auto& scenario = selectedScenario();
    const auto fidelities =
        runner.availableFidelities(spec_, &scenario);
    if (fidelities.empty()) {
        throw std::runtime_error(
            spec_.id + ": no selectable implementation");
    }
    artifacts_.clear();
    auto profile = profile_;
    const auto& parameters = scenario.parameters.empty()
        ? spec_.parameters
        : scenario.parameters;
    const auto root_decision = profile.decide(
        spec_.instance_name,
        0,
        spec_.contract_id,
        parameters);
    if (!root_decision.fidelity) {
        profile = withExactFidelity(
            std::move(profile),
            spec_.instance_name,
            fidelities.front(),
            profile_.name() + "-preview");
    }
    artifacts_.push_back(
        runner.run(
            spec_,
            scenario,
            std::move(profile)));
    adoptArtifact(artifacts_.front());
}

bool ComponentScenarioTest::run() {
    std::cout << "--- Running Test: " << getTestName()
              << (scenario_id_.empty() ? "" : "/" + scenario_id_)
              << " ---" << std::endl;
    try {
        ComponentTestRunner runner;
        artifacts_.clear();
        for (const auto* scenario : scenariosToRun()) {
            auto runs =
                runner.runAll(spec_, *scenario, profile_);
            artifacts_.insert(
                artifacts_.end(),
                std::make_move_iterator(runs.targets.begin()),
                std::make_move_iterator(runs.targets.end()));
        }
        if (artifacts_.empty()) {
            throw std::runtime_error(
                getTestName() + ": no scenario artifacts were produced");
        }
        adoptArtifact(artifacts_.front());
        std::cout << "[PASS] Test '" << getTestName()
                  << (scenario_id_.empty() ? "" : "/" + scenario_id_)
                  << "' completed successfully." << std::endl;
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] Test '" << getTestName()
                  << (scenario_id_.empty() ? "" : "/" + scenario_id_)
                  << "' threw an exception: " << error.what() << std::endl;
        return false;
    }
}

size_t ComponentScenarioTest::getRunDuration() const {
    if (artifacts_.empty() || artifacts_.front().checkpoints.empty()) {
        return 0;
    }
    return artifacts_.front().checkpoints.back().actual_time;
}

std::vector<SimulationTest::SimulationCheckpoint>
ComponentScenarioTest::getCheckpoints() const {
    std::vector<SimulationCheckpoint> result;
    if (artifacts_.empty()) {
        return result;
    }
    result.reserve(artifacts_.front().checkpoints.size());
    size_t index = 0;
    for (const auto& checkpoint : artifacts_.front().checkpoints) {
        result.push_back({
            checkpoint.actual_time,
            checkpoint.id,
            artifacts_.front().scenario_id,
            index++,
        });
    }
    return result;
}

} // namespace circuit::test
