#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>

namespace circuit {
namespace {

bool containsAll(const std::vector<std::string>& available,
                 const std::vector<std::string>& required) {
    for (const auto& value : required) {
        if (std::find(available.begin(), available.end(), value) == available.end()) {
            return false;
        }
    }
    return true;
}

std::vector<PinDescriptor> schemaOf(const IOComponent& component) {
    std::vector<PinDescriptor> pins;
    for (const auto& [name, pin] : component.getAllInputPins()) {
        pins.push_back({name, pin->getWidth(), PinType::INPUT});
    }
    for (const auto& [name, pin] : component.getAllOutputPins()) {
        pins.push_back({name, pin->getWidth(), PinType::OUTPUT});
    }
    std::sort(pins.begin(), pins.end(), [](const auto& left, const auto& right) {
        if (left.direction != right.direction) {
            return left.direction == PinType::INPUT;
        }
        if (left.name != right.name) return left.name < right.name;
        return left.width < right.width;
    });
    return pins;
}

std::vector<PinDescriptor> normalizedSchema(std::vector<PinDescriptor> pins) {
    std::sort(pins.begin(), pins.end(), [](const auto& left, const auto& right) {
        if (left.direction != right.direction) {
            return left.direction == PinType::INPUT;
        }
        if (left.name != right.name) return left.name < right.name;
        return left.width < right.width;
    });
    return pins;
}

} // namespace

void ComponentCatalog::registerContract(ContractDescriptor descriptor) {
    if (frozen_) {
        throw std::logic_error("Cannot register contract after catalog freeze");
    }
    if (descriptor.id.empty() || descriptor.version == 0) {
        throw std::invalid_argument("Contract requires a non-empty ID and nonzero version");
    }
    if (!contracts_.emplace(descriptor.id, std::move(descriptor)).second) {
        throw std::invalid_argument("Duplicate contract ID");
    }
}

void ComponentCatalog::registerImplementation(ImplementationDescriptor descriptor) {
    if (frozen_) {
        throw std::logic_error("Cannot register implementation after catalog freeze");
    }
    if (descriptor.id.empty() || descriptor.contract_id.empty()) {
        throw std::invalid_argument("Implementation requires IDs");
    }
    if (!hasContract(descriptor.contract_id)) {
        throw std::invalid_argument("Implementation references unknown contract '"
                                    + descriptor.contract_id + "'");
    }
    if (descriptor.terminal_primitive && descriptor.fidelity != Fidelity::Structural) {
        throw std::invalid_argument("Terminal primitive must use structural fidelity");
    }
    if (!descriptor.factory) {
        throw std::invalid_argument("Implementation requires a factory");
    }
    if (!descriptor.supports) {
        descriptor.supports = [](const ParameterMap&) { return true; };
    }
    if (!implementations_.emplace(descriptor.id, std::move(descriptor)).second) {
        throw std::invalid_argument("Duplicate implementation ID");
    }
}

void ComponentCatalog::freeze() {
    if (frozen_) return;
    for (const auto& [contract_id, descriptor] : contracts_) {
        (void)descriptor;
        bool found = false;
        for (const auto& [implementation_id, implementation] : implementations_) {
            (void)implementation_id;
            if (implementation.contract_id == contract_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Contract '" + contract_id
                                     + "' has no registered implementation");
        }
    }
    for (const auto& [id, implementation] : implementations_) {
        if (implementation.fidelity == Fidelity::Behavioral
            && !implementation.reference_only) {
            if (implementation.evidence.status == VerificationStatus::Unverified
                || implementation.evidence.lower_level_evidence.empty()
                || implementation.evidence.contract_tests.empty()
                || (implementation.evidence.equivalence_tests.empty()
                    && implementation.evidence.representative_tests.empty())) {
                throw std::runtime_error("Behavioral implementation '" + id
                    + "' lacks lower-level, contract, and equivalence/representative evidence");
            }
        }
    }
    frozen_ = true;
}

bool ComponentCatalog::frozen() const { return frozen_; }

bool ComponentCatalog::hasContract(const std::string& id) const {
    return contracts_.count(id) != 0;
}

const ContractDescriptor& ComponentCatalog::contract(const std::string& id) const {
    auto found = contracts_.find(id);
    if (found == contracts_.end()) {
        throw std::out_of_range("Unknown contract '" + id + "'");
    }
    return found->second;
}

const ImplementationDescriptor& ComponentCatalog::implementation(
    const std::string& id) const {
    auto found = implementations_.find(id);
    if (found == implementations_.end()) {
        throw std::out_of_range("Unknown implementation '" + id + "'");
    }
    return found->second;
}

std::vector<ContractDescriptor> ComponentCatalog::contracts() const {
    std::vector<ContractDescriptor> result;
    result.reserve(contracts_.size());
    for (const auto& [id, descriptor] : contracts_) {
        (void)id;
        result.push_back(descriptor);
    }
    return result;
}

std::vector<ImplementationDescriptor> ComponentCatalog::implementationsFor(
    const std::string& contract_id) const {
    std::vector<ImplementationDescriptor> result;
    for (const auto& [id, descriptor] : implementations_) {
        (void)id;
        if (descriptor.contract_id == contract_id) {
            result.push_back(descriptor);
        }
    }
    return result;
}

ResolvedSelection ComponentCatalog::resolve(
    const ComponentBuildRequest& request,
    const std::string& path,
    size_t depth,
    const BuildProfile& profile) const {
    if (!frozen_) {
        throw std::logic_error("Component catalog must be frozen before resolution");
    }
    const auto& requested_contract = contract(request.contract_id);
    const auto decision = profile.decide(path, depth, request.contract_id,
                                         request.parameters);

    std::vector<const ImplementationDescriptor*> candidates;
    for (const auto& [id, descriptor] : implementations_) {
        (void)id;
        if (descriptor.contract_id != request.contract_id) continue;
        if (descriptor.reference_only && !profile.allowReference()) continue;
        if (!descriptor.supports(request.parameters)) continue;
        if (!containsAll(descriptor.capabilities, request.required_capabilities)) continue;
        if (!request.semantic_domain.empty()
            && descriptor.evidence.semantic_domain != request.semantic_domain) continue;
        if (!request.observation.empty()
            && descriptor.evidence.observation != request.observation) continue;
        candidates.push_back(&descriptor);
    }
    if (candidates.empty()) {
        throw std::runtime_error("No implementation supports contract request '"
                                 + request.contract_id + "' at '" + path + "'");
    }

    const ImplementationDescriptor* selected = nullptr;
    bool exception = false;

    if (decision.fidelity) {
        for (const auto* candidate : candidates) {
            if (candidate->fidelity != *decision.fidelity) continue;
            if (!selected || candidate->default_priority > selected->default_priority
                || (candidate->default_priority == selected->default_priority
                    && candidate->id < selected->id)) {
                selected = candidate;
            }
        }
        if (!selected) {
            if (profile.unavailablePolicy() == UnavailableFidelityPolicy::Error) {
                throw std::runtime_error("Requested " + toString(*decision.fidelity)
                    + " fidelity is unavailable for '" + request.contract_id
                    + "' at '" + path + "'");
            }
            exception = true;
        }
    }

    if (!selected) {
        for (const auto* candidate : candidates) {
            if (!selected || candidate->default_priority > selected->default_priority
                || (candidate->default_priority == selected->default_priority
                    && candidate->id < selected->id)) {
                selected = candidate;
            }
        }
    }

    std::string reason = decision.matched_rule
        ? decision.reason
        : "contract baseline by deterministic priority";
    if (exception) {
        reason += "; unavailable fidelity exception recorded";
    }

    return {
        requested_contract.id,
        requested_contract.version,
        selected->id,
        selected->fidelity,
        selected->terminal_primitive,
        selected->reference_only,
        exception,
        std::move(reason),
    };
}

const ImplementationDescriptor& ComponentCatalog::descriptorForSelection(
    const ResolvedSelection& selection) const {
    return implementation(selection.implementation_id);
}

std::shared_ptr<Component> ComponentCatalog::createChild(
    const ComponentBuildRequest& request,
    const std::shared_ptr<BuildContext>& parent_context) const {
    if (!parent_context) {
        throw std::invalid_argument("Child creation requires parent BuildContext");
    }
    const auto path = parent_context->childPath(request.instance_name);
    const auto depth = parent_context->childDepth();
    const auto selection = resolve(request, path, depth, parent_context->profile());
    const auto& descriptor = descriptorForSelection(selection);
    auto child_context = parent_context->child(
        request.instance_name, selection, request.parameters);
    auto component = descriptor.factory(
        request.instance_name, request.parameters, child_context);
    validateInstance(contract(request.contract_id), descriptor, component);
    return component;
}

BuildResult ComponentCatalog::createRoot(ComponentBuildRequest request,
                                         BuildProfile profile) const {
    if (request.instance_name.empty()) {
        throw std::invalid_argument("Root request requires an instance name");
    }
    auto shared_profile = std::make_shared<const BuildProfile>(std::move(profile));
    auto manifest = std::make_shared<BuildManifest>(
        shared_profile->name(), shared_profile->fingerprint());
    auto scope = BuildContext::rootScope(*this, shared_profile, manifest);
    auto root = createChild(request, scope);
    manifest->finalizeEffectiveFidelities();
    return {std::move(root), std::move(shared_profile), std::move(manifest)};
}

void ComponentCatalog::validateInstance(
    const ContractDescriptor& expected_contract,
    const ImplementationDescriptor& implementation,
    const std::shared_ptr<Component>& component) const {
    if (!component) {
        throw std::runtime_error("Implementation factory returned null for '"
                                 + implementation.id + "'");
    }
    auto io = std::dynamic_pointer_cast<IOComponent>(component);
    if (!io) {
        throw std::runtime_error("Implementation '" + implementation.id
                                 + "' did not create an IOComponent");
    }

    if (implementation.fidelity == Fidelity::Behavioral) {
        if (!std::dynamic_pointer_cast<BasicComponent>(component)) {
            throw std::runtime_error("Behavioral implementation '" + implementation.id
                                     + "' did not create a BasicComponent");
        }
        if (!component->getChildren().empty()) {
            throw std::runtime_error("Behavioral implementation '" + implementation.id
                                     + "' created functional child topology");
        }
    } else if (implementation.terminal_primitive) {
        if (!std::dynamic_pointer_cast<BasicComponent>(component)) {
            throw std::runtime_error("Terminal implementation '" + implementation.id
                                     + "' did not create a BasicComponent");
        }
    } else if (std::dynamic_pointer_cast<BasicComponent>(component)) {
        throw std::runtime_error("Structural implementation '" + implementation.id
                                 + "' created a directly evaluated BasicComponent "
                                   "without declaring it a terminal primitive");
    }

    if (!expected_contract.pins.empty()) {
        const auto expected = normalizedSchema(expected_contract.pins);
        const auto actual = schemaOf(*io);
        if (expected != actual) {
            std::ostringstream message;
            message << "Pin schema mismatch for implementation '" << implementation.id
                    << "': expected " << expected.size() << " pins, got "
                    << actual.size();
            throw std::runtime_error(message.str());
        }
    }
}

} // namespace circuit
