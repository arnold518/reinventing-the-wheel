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
// The handshake gates as four-state functions, for the compact controller and
// endpoint: an unknown reset, request or response-ready bit leaves X where
// the gate-level state bits and registers hold X, and nowhere else.
inline LogicValue both(LogicValue a, LogicValue b) { return a == L || b == L ? L : a == H && b == H ? H : X; }
inline LogicValue either(LogicValue a, LogicValue b) { return a == H || b == H ? H : a == L && b == L ? L : X; }
inline LogicValue no(LogicValue a) { return a == H ? L : a == L ? H : X; }
inline LogicValue clean(LogicValue a) { return a == H || a == L ? a : X; }
inline LogicValue bit(bool value) { return value ? H : L; }
// A state bit or register bit losing its reset: 0 stays 0, the rest is unknown.
inline void fade(LogicValue& value) { if (value != L) value = X; }
// The one-entry response buffer (MemoryResponseBuffer as built).
struct ResponseBuffer {
    LogicValue full = L;
    Bits data, status = Bits(3, L);
    explicit ResponseBuffer(size_t width) : data(width, L) {}
    LogicValue room(LogicValue take) const { return either(no(full), take); }
    // The clocked update: `offered` is IN_VALID with (in_data, in_status).
    void clock(LogicValue reset, bool edge, LogicValue offered, LogicValue take, const Bits& in_data, const Bits& in_status) {
        if (reset == H) { full = L; data.assign(data.size(), L); status.assign(3, L); return; }
        if (reset != L) { fade(full); for (auto* held : {&data, &status}) for (auto& value : *held) fade(value); return; }
        if (!edge) return;
        const auto capture = both(both(offered, room(take)), either(full, no(take)));
        const auto hold = both(full, no(take));
        const auto store = [&](Bits& held, const Bits& in) {
            for (size_t i = 0; i < held.size(); ++i) {
                if (capture == H) held[i] = clean(in[i]);
                else if (capture != L) held[i] = held[i] == L && in[i] == L ? L : X;
            }
        };
        store(data, in_data); store(status, in_status);
        full = either(capture, hold);
    }
    LogicValue valid(LogicValue reset, LogicValue offered) const { return both(no(reset), either(full, offered)); }
    // The output multiplexers: the held word when full, else the offered one.
    Bits select(const Bits& offered, const Bits& held) const {
        Bits out(held.size());
        for (size_t i = 0; i < held.size(); ++i)
            out[i] = either(both(clean(offered[i]), no(full)), both(held[i], full));
        return out;
    }
};

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
    // An operation has ended (finished_: with a response) and the next clock
    // edge has not yet shown it; work_* hold its result until then.
    bool finished_ = false, settling_ = false;
    Bits work_result_;
    Status work_status_ = Status::Ok;
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
    // `enable` is the word's write enable: on an unknown enable or mask bit a
    // stored bit stays 0 only where the old and the new bit are both 0.
    void writeWord(size_t, uint32_t, const Bits&, const Bits&, bool program = false, LogicValue enable = H);
};
std::unique_ptr<DeviceModel> makeDeviceModel(size_t, Technology, EndpointConfig);
}
