#pragma once
// Shared equipment for headless sweep tests: build one family under a profile,
// drive every input, settle, and read every output. Expected values come from
// an independent reference in the test, never from production value functions.
#include "components/IOComponent.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "simulator/Simulator.hpp"
#include "tests/ComponentTestModel.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace circuit::test {

class SettledSweep {
public:
    SettledSweep(const ComponentFamily& family, BuildProfile profile);
    // Every input pin must be driven. Returns every output pin.
    NamedValues evaluate(const NamedValues& inputs);
    // Binary values sized by this root's pin widths.
    NamedValues binary(const std::map<std::string, uint64_t>& values) const;
    size_t width(const std::string& pin) const;
    size_t componentCount() const;
    size_t vectors() const { return vectors_; }
    const std::shared_ptr<IOComponent>& root() const { return root_; }

private:
    std::shared_ptr<IOComponent> root_;
    Simulator simulator_;
    std::map<std::string, std::shared_ptr<WireBase>> inputs_;
    size_t vectors_ = 0;
};

// The strict gate-level form, plus the behavioral form when the family has
// one. evaluate() runs both and requires identical outputs (four-state
// included). With a reference, the gate-level outputs named there are checked
// against it first, so a wrong circuit is not reported as a form mismatch.
class FidelitySweep {
public:
    explicit FidelitySweep(const ComponentFamily& family);
    NamedValues evaluate(const NamedValues& inputs, const NamedValues* reference = nullptr);
    SettledSweep& gates() { return gates_; }
    bool hasBehavioral() const { return behavioral_.has_value(); }
    size_t vectors() const { return gates_.vectors(); }

private:
    std::string type_;
    SettledSweep gates_;
    std::optional<SettledSweep> behavioral_;
};

std::string describeInputs(const NamedValues& inputs);
// Checks only the outputs named in `expected`.
void requireOutputs(const NamedValues& expected, const NamedValues& actual,
                    const NamedValues& inputs, const std::string& label);
// Four-state soundness: `actual` came from inputs with one unknown bit, and
// `low`/`high` are the reference outputs with that bit resolved to 0 and 1.
// Each output bit must be UNKNOWN, or the known value both resolutions agree on.
void requireSoundUnknown(const NamedValues& low, const NamedValues& high, const NamedValues& actual,
                         const NamedValues& inputs, const std::string& label);

} // namespace circuit::test
