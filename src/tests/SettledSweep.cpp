#include "tests/SettledSweep.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/TestHelpers.hpp"
#include <stdexcept>

namespace circuit::test {
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
size_t countComponents(const std::shared_ptr<Component>& component) {
    size_t count = 1;
    for (const auto& child : component->getChildren()) count += countComponents(child);
    return count;
}
} // namespace

SettledSweep::SettledSweep(const ComponentFamily& family, BuildProfile profile)
    : root_(std::dynamic_pointer_cast<IOComponent>(
          builtinComponentCatalog().createRoot(family.request("DUT"), std::move(profile)).root)) {
    require(root_ != nullptr, "Sweep root is not an IO component");
    ComponentBuilder builder(root_);
    simulator_.setHistoryRecordingEnabled(false);
    for (const auto& [name, pin] : root_->getAllInputPins())
        inputs_[pin->getName()] = builder.addNewWireDynamic("IN_" + pin->getName(), pin->getWidth(), nullptr, {pin});
    for (const auto& [name, pin] : root_->getAllOutputPins())
        builder.addNewWireDynamic("OUT_" + pin->getName(), pin->getWidth(), pin, {});
    SimulationTest::scheduleInitialEventsForTree(root_, simulator_, 0);
}

NamedValues SettledSweep::evaluate(const NamedValues& values) {
    require(values.size() == inputs_.size(), "Every sweep input must be driven");
    const auto start = simulator_.getCurrentTime() + 1;
    for (const auto& [name, bits] : values) {
        auto event = makeTestWireUpdate(start, inputs_.at(name), TestValue(bits));
        require(event != nullptr, "Sweep input " + name + " has the wrong width");
        simulator_.scheduleEvent(event);
    }
    const auto settled = simulator_.drainUntilIdle(start + 100000, 10000000);
    require(settled.idle(), "Sweep circuit did not settle: " + describeInputs(values));
    ++vectors_;
    NamedValues result;
    for (const auto& [name, pin] : root_->getAllOutputPins()) result[pin->getName()] = pin->getValueAsVector();
    return result;
}

NamedValues SettledSweep::binary(const std::map<std::string, uint64_t>& values) const {
    NamedValues result;
    for (const auto& [name, value] : values) result[name] = logicBits(width(name), value);
    return result;
}

size_t SettledSweep::width(const std::string& pin) const {
    if (auto input = root_->getInputPinDynamic(pin)) return input->getWidth();
    if (auto output = root_->getOutputPinDynamic(pin)) return output->getWidth();
    throw std::runtime_error("Sweep root has no pin " + pin);
}

size_t SettledSweep::componentCount() const { return countComponents(root_); }

FidelitySweep::FidelitySweep(const ComponentFamily& family)
    : type_(std::string(family.typeName())), gates_(family, strictAllStructural()) {
    require(gates_.componentCount() > 1, type_ + ": gate-level form has no internals");
    if (family.supports(Fidelity::Behavioral)) {
        behavioral_.emplace(family, withExactFidelity(canonicalDefaultProfile(), "DUT", Fidelity::Behavioral));
        require(behavioral_->componentCount() == 1, type_ + ": behavioral form must be one component");
    }
}

NamedValues FidelitySweep::evaluate(const NamedValues& inputs, const NamedValues* reference) {
    auto result = gates_.evaluate(inputs);
    if (reference) requireOutputs(*reference, result, inputs, type_ + " gate-level form vs independent reference");
    if (behavioral_)
        requireOutputs(result, behavioral_->evaluate(inputs), inputs,
            type_ + " behavioral form vs gate-level form (expected = gate-level)");
    return result;
}

std::string describeInputs(const NamedValues& inputs) {
    std::string detail;
    for (const auto& [name, values] : inputs) detail += " " + name + "=" + formatLogicVector(values);
    return detail;
}

void requireOutputs(const NamedValues& expected, const NamedValues& actual,
                    const NamedValues& inputs, const std::string& label) {
    std::string mismatch;
    for (const auto& [name, values] : expected) {
        const auto found = actual.find(name);
        if (found == actual.end()) mismatch += " missing output " + name;
        else if (found->second != values)
            mismatch += " " + name + ": expected " + formatLogicVector(values) + " got " + formatLogicVector(found->second);
    }
    if (!mismatch.empty()) throw std::runtime_error(label + ":" + describeInputs(inputs) + " ->" + mismatch);
}

void requireSoundUnknown(const NamedValues& low, const NamedValues& high, const NamedValues& actual,
                         const NamedValues& inputs, const std::string& label) {
    for (const auto& [name, zero] : low) {
        const auto& one = high.at(name);
        const auto& got = actual.at(name);
        for (size_t bit = 0; bit < zero.size(); ++bit) {
            if (got[bit] == LogicValue::UNKNOWN) continue;
            if (zero[bit] != one[bit] || got[bit] != zero[bit])
                throw std::runtime_error(label + ":" + describeInputs(inputs) + " -> " + name + "[" +
                    std::to_string(bit) + "] is " + formatLogicVector({got[bit]}) +
                    " but the input bit can change it");
        }
    }
}

} // namespace circuit::test
