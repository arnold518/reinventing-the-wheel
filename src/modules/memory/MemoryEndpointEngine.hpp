#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include <deque>
#include <functional>
#include <memory>

namespace circuit::memory {
using namespace memory_device;
using Signals = std::map<std::string, Bits>;
inline Bits signal(const Signals& s, const std::string& key, size_t width = 1) {
    auto it = s.find(key); return it == s.end() ? Bits(width, L) : it->second;
}
inline bool high(const Signals& s, const std::string& key) { return signal(s, key) == Bits{H}; }
inline size_t after(size_t t, size_t delay) {
    if (delay > size_t(-1) - t) throw std::overflow_error("Memory event time overflow");
    return t + delay;
}
struct Request { Bits op, address, data, mask; };
Status validateRequest(const Request&, size_t width, Technology, const EndpointConfig&);
// Only pins belonging to the selected native interface, not the sequencer's
// superset of scratch signals for all technologies.
std::vector<std::string> nativeActivityInputs(Technology);

// Device-specific sequencing is a timed digital controller primitive. It does
// not own contents; the same primitive drives either native pins or a compact
// device model inside the behavioral endpoint.
class NativeSequence {
public:
    NativeSequence(size_t, Technology, EndpointConfig);
    void step(size_t time, bool clock_edge, bool reset, bool start, const Request&,
              const std::function<Bits(const std::string&, size_t)>& read, bool finish = false);
    size_t deadline() const { return due_; }
    const char* phaseName() const {
        static constexpr const char* names[]{"idle", "array-setup", "array-write", "array-read", "static-access",
            "command-raise", "command-lower", "command-wait", "release"};
        return names[static_cast<size_t>(phase_)];
    }
    bool maintenanceActive() const { return maintenance_ && phase_ != Phase::Idle; }
    bool done = false, available = true;
    Bits result;
    Status status = Status::Ok;
    Signals outputs;
private:
    enum class Phase { Idle, ArraySetup, ArrayWrite, ArrayRead, StaticAccess,
                       CommandRaise, CommandLower, CommandWait, Release };
    struct Command { unsigned op; uint32_t address; };
    size_t width_, due_ = 0, next_refresh_ = 0;
    Technology technology_;
    EndpointConfig config_;
    Phase phase_ = Phase::Idle;
    Request request_;
    std::deque<Command> commands_;
    bool aborted_ = false, maintenance_ = false, row_open_ = false;
    size_t row_ = 0;
    void idlePins();
    void beginCommand(size_t);
    void finish(size_t);
};

class DeviceModel {
public:
    DeviceModel(size_t width, EndpointConfig config, LogicValue fill);
    virtual ~DeviceModel() = default;
    virtual void update(size_t, const Signals&) = 0;
    Bits read(const std::string& name, size_t width) const { return signal(outputs, name, width); }
    size_t deadline() const { return due_; }
    MemoryStorage storage;
protected:
    size_t width_, due_ = 0;
    EndpointConfig config_;
    Signals outputs;
    LogicValue previous_clock_ = X;
    bool edge(const Signals&);
    void writeWord(size_t, uint32_t, const Bits&, const Bits&, bool program = false);
};
std::unique_ptr<DeviceModel> makeDeviceModel(size_t, Technology, EndpointConfig);
}
