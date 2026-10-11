#pragma once

#include "components/IOComponent.hpp"
#include "components/PartDelays.hpp"
#include "basic/Wire.hpp"
#include "ForwardDeclarations.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <memory>
#include <string>
#include <vector>

/**
 * A directly evaluated simulation component.
 *
 * BasicComponent is the only evaluated base class. Both foundational simulation
 * primitives (for example an AND gate) and compact contract implementations
 * derive from it. Whether an instance represents structural-terminal or
 * behavioral fidelity is catalog/instance metadata, not C++ inheritance.
 */
class BasicComponent : public IOComponent
{
protected:
    size_t delay;
    // The tick at which an output computed now is published.
    size_t publishTime(const std::string& output_pin, size_t current_sim_time);
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name,
                           LogicValue new_value, size_t current_sim_time);

    template<size_t WIDTH>
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name,
                           uint64_t new_value, size_t current_sim_time) {
        if (auto pin = getOutputPin<WIDTH>(pin_name)) {
            pin->setValueFromUInt64(new_value);
            if (auto wire = pin->getExternalWire()) {
                simulator.scheduleEvent(std::make_shared<WireUpdateEvent<WIDTH>>(
                    publishTime(pin_name, current_sim_time), wire, new_value));
            } else {
                simulator.recordPinChange(
                    publishTime(pin_name, current_sim_time), pin, pin->getValueAsVector());
            }
        }
    }

    template<size_t WIDTH>
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name,
                           const std::vector<LogicValue>& new_value,
                           size_t current_sim_time) {
        if (auto pin = getOutputPin<WIDTH>(pin_name)) {
            pin->setValueFromVector(new_value);
            if (auto wire = pin->getExternalWire()) {
                simulator.scheduleEvent(std::make_shared<WireUpdateEvent<WIDTH>>(
                    publishTime(pin_name, current_sim_time), wire, new_value));
            } else {
                simulator.recordPinChange(
                    publishTime(pin_name, current_sim_time), pin, pin->getValueAsVector());
            }
        }
    }

    // Width-erased form of the overloads above for pins whose width is only
    // known at run time. A missing pin or a width mismatch is ignored, as the
    // typed overloads do; a width without a Wire instantiation is an error.
    void _updateOutputWireDynamic(Simulator& simulator, const std::string& pin_name,
                                  const std::vector<LogicValue>& new_value,
                                  size_t current_sim_time);

public:
    BasicComponent(std::string name, size_t delay_val, PinInitFunction initializer = nullptr);
    static constexpr const char* TypeName = "BasicComponent";
    const char* getTypeName() const override { return TypeName; }

    size_t getDelay() const;
    // How a structural-terminal primitive that is not made of gates times its
    // outputs, for the timing analysis. Both kinds publish only while being
    // evaluated, one delay later.
    //   InputsOnly: it is evaluated only when an input changes.
    //   SelfTimed:  it also wakes itself for later, timed steps.
    // Unknown: the analysis needs a contract from elsewhere.
    enum class PrimitiveTiming { Unknown, InputsOnly, SelfTimed };
    virtual PrimitiveTiming primitiveTiming() const { return PrimitiveTiming::Unknown; }
    // Per-output timing. Empty table: every output uses the single delay.
    void setDelays(circuit::PartDelays delays);
    const circuit::PartDelays* getDelays() const;
    // Entry point of the simulator: notes which inputs changed, then evaluates.
    void evaluateAt(size_t current_time, Simulator& simulator);
    // A part with a delay table watches its own setup times: an input listed
    // in the table's `setup` that changed too shortly before a rising clock
    // edge is counted. The gates would capture such data late or wrongly; the
    // compact part still captures it, so a run is only a faithful model of
    // the gates while this count stays zero.
    static size_t setupViolations();
    static void resetSetupViolations();
    virtual void scheduleInitialEvents(Simulator& simulator, size_t current_time);
    virtual void evaluate(size_t current_time, Simulator& simulator) = 0;
    ~BasicComponent() override;

private:
    struct TimedOutputs;
    std::unique_ptr<TimedOutputs> timed_;
};
