#pragma once

#include "simulator/SimulationTime.hpp"
#include "basic/LogicValue.hpp"
#include <memory>
#include <stdexcept>
#include <vector>

class Simulator;
class WireBase;
class PinBase;
class Component;
class MemoryStorage;

// Optional streaming observation, independent of retained simulator histories.
// Callbacks must not drive, seek or advance the observed circuit. Pin-only
// publications can have a future timestamp; settledThrough establishes the
// inclusive frontier after which no earlier event can still be processed.
class SignalTraceObserver {
public:
    virtual ~SignalTraceObserver() = default;
    virtual void wireChanged(SimulationTime, const std::shared_ptr<WireBase>&,
                             const std::vector<LogicValue>&) = 0;
    virtual void pinChanged(SimulationTime, const std::shared_ptr<PinBase>&,
                            const std::vector<LogicValue>&) = 0;
    virtual void settledThrough(SimulationTime) = 0;
    virtual void memoryByteChanged(SimulationTime, const MemoryStorage&, uint32_t, uint8_t, uint8_t) {}
    virtual void memoryFilled(SimulationTime, const MemoryStorage&, LogicValue) {}
    virtual void registersChanged(SimulationTime, const std::shared_ptr<Component>&,
                                  const std::vector<std::vector<LogicValue>>&) {}
};

class SignalTraceSubscription {
public:
    SignalTraceSubscription(const Simulator& simulator, std::shared_ptr<SignalTraceObserver> observer)
        : simulator_(&simulator), observer_(std::move(observer)), previous_(active_) {
        if (!observer_) throw std::invalid_argument("Missing signal trace observer");
        for (auto* entry = active_; entry; entry = entry->previous_)
            if (entry->simulator_ == &simulator) throw std::logic_error("Simulator already has a signal trace");
        active_ = this;
    }
    ~SignalTraceSubscription() { active_ = previous_; }
    SignalTraceSubscription(const SignalTraceSubscription&) = delete;
    SignalTraceSubscription& operator=(const SignalTraceSubscription&) = delete;
    static SignalTraceObserver* observer(const Simulator& simulator) {
        for (auto* entry = active_; entry; entry = entry->previous_)
            if (entry->simulator_ == &simulator) return entry->observer_.get();
        return nullptr;
    }
    // MemoryStorage has no simulator owner. Observers filter by the storage
    // instances captured at subscription setup, ignoring unrelated stores.
    static SignalTraceObserver* memoryObserver() { return active_ ? active_->observer_.get() : nullptr; }
private:
    const Simulator* simulator_;
    std::shared_ptr<SignalTraceObserver> observer_;
    SignalTraceSubscription* previous_;
    inline static thread_local SignalTraceSubscription* active_ = nullptr;
};
