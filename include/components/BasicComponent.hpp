#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/IOComponent.hpp"
#include "components/PartDelays.hpp"
#include "basic/Wire.hpp"
#include "ForwardDeclarations.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace circuit::state { class Writer; class Reader; }

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
    SimulationTime delay;
    // Inertial delay: an output change still waiting to be published is
    // withdrawn when a later evaluation would replace it before it settles,
    // so a pulse shorter than the delay never appears. Logic gates use it;
    // under the "unit" profile no pulse can be shorter than one tick, so it
    // changes nothing there. CIRCUITSIM_TRANSPORT_DELAY=1 turns it off.
    virtual bool inertialDelay() const { return false; }
    // The tick at which an output computed now is published.
    SimulationTime publishTime(const std::string& output_pin, SimulationTime current_sim_time);
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name,
                           LogicValue new_value, SimulationTime current_sim_time);

    template<size_t WIDTH>
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name,
                           uint64_t new_value, SimulationTime current_sim_time) {
        if (auto pin = getOutputPin<WIDTH>(pin_name)) {
            pin->setValueFromUInt64(new_value);
            if (auto wire = pin->getExternalWire()) {
                simulator.scheduleEvent(circuit::makeEvent<WireUpdateEvent>(
                    publishTime(pin_name, current_sim_time), wire, pin->packed()));
            } else {
                simulator.recordPinChange(
                    publishTime(pin_name, current_sim_time), pin, pin->getValueAsVector());
            }
        }
    }

    template<size_t WIDTH>
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name,
                           const std::vector<LogicValue>& new_value,
                           SimulationTime current_sim_time) {
        if (auto pin = getOutputPin<WIDTH>(pin_name)) {
            pin->setValueFromVector(new_value);
            if (auto wire = pin->getExternalWire()) {
                simulator.scheduleEvent(circuit::makeEvent<WireUpdateEvent>(
                    publishTime(pin_name, current_sim_time), wire, pin->packed()));
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
                                  SimulationTime current_sim_time);
    // Publishes a packed value of the pin's own width on an output pin.
    void _updateOutputPacked(Simulator& simulator, const std::shared_ptr<PinBase>& pin,
                             const PackedLogic& new_value, SimulationTime current_sim_time);

public:
    BasicComponent(std::string name, SimulationTime delay_val, PinInitFunction initializer = nullptr);
    static constexpr const char* TypeName = "BasicComponent";
    const char* getTypeName() const override { return TypeName; }

    SimulationTime getDelay() const;
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
    void evaluateAt(SimulationTime current_time, Simulator& simulator);
    // A part with a delay table watches its own setup times: an input listed
    // in the table's `setup` that changed too shortly before a rising clock
    // edge is counted. The gates would capture such data late or wrongly; the
    // compact part still captures it, so a run is only a faithful model of
    // the gates while this count stays zero.
    static size_t setupViolations();
    static void resetSetupViolations();
    virtual void scheduleInitialEvents(Simulator& simulator, SimulationTime current_time);
    virtual void evaluate(SimulationTime current_time, Simulator& simulator) = 0;
    ~BasicComponent() override;

    // Saving a machine (circuit::state::saveMachine). Pins, wires and the
    // timing state below are saved for every part; a part says what else it
    // holds. Unknown: not yet declared, and such a part cannot be saved, so a
    // forgotten member can never silently change a resumed run.
    enum class StateSupport { Unknown, Stateless, Saved };
    virtual StateSupport stateSupport() const { return StateSupport::Unknown; }
    // Members beyond pins and timing; only for StateSupport::Saved.
    virtual void saveOwnState(circuit::state::Writer&) const {}
    virtual void restoreOwnState(circuit::state::Reader&) {}
    // The timing state every part has: the delay table's per-input memory and
    // the inertial-delay changes still due. Events are named by their number
    // in the saved queue (-1: no longer queued).
    void saveTimingState(circuit::state::Writer& out, const std::function<int64_t(const Event*)>& event_number) const;
    void restoreTimingState(circuit::state::Reader& in, const std::function<std::shared_ptr<Event>(int64_t)>& event_at);
    static void setSetupViolations(size_t count);
    // After a restore, with the simulator's restored time: a part that keeps
    // its own history for viewing starts it at the restored state.
    virtual void stateRestored(SimulationTime, const Simulator&) {}

private:
    struct TimedOutputs;
    std::unique_ptr<TimedOutputs> timed_;
    // Weak: the simulator owns the event; a part must not keep wires alive.
    struct PendingChange { SimulationTime time; LogicValue value; std::weak_ptr<Event> event; };
    std::vector<std::pair<std::string, std::vector<PendingChange>>> pending_changes_;
};
