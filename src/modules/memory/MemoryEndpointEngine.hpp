#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <optional>

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

// The controller sequencer as one part: the gate-level static sequencer
// (SRAM, ROM) or command sequencer (DRAM, NOR flash) of MemorySequencers.cpp,
// cycle for cycle. Every value here is a state bit, register or gate of that
// circuit; it acts only at clock edges and on reset.
class NativeSequence {
public:
    NativeSequence(size_t, Technology, EndpointConfig);
    // `read` gives the device's outputs as they were before this edge.
    void step(size_t time, bool clock_edge, bool reset, bool start, const Request&,
              const std::function<Bits(const std::string&, size_t)>& read, bool finish = false);
    // The device clock follows the controller clock with no delay.
    void clock(LogicValue level) { if (command_ || config_.clocked) outputs["CLK"] = {level}; }
    // The command sequencer shows its next command combinationally from the
    // device's (registered) outputs: recompute the pins from them.
    void present(const std::function<Bits(const std::string&, size_t)>& read) { if (command_) commandPins(read); }
    // A clocked SRAM macro is shown the request combinationally while START
    // is high (the transaction port's acceptance).
    void presentRequest(bool start, const Request& request) {
        if (command_ || !config_.clocked) return;
        shown_start_ = start; shown_ = request; staticPins();
    }
    size_t deadline() const { return 0; }
    const char* phaseName() const {
        if (!command_) return access_ ? "static-access" : "idle";
        return presenting_ ? "command-issue" : waiting_ ? "command-wait" : "idle";
    }
    bool maintenanceActive() const { return refreshing_ && active_; }
    bool done = false, available = true;
    // LAST: the result shown is the transaction's last (burst words before it
    // are not). `space`: the transaction port holds no response (set by the
    // owner before each step and presentation).
    bool last = false, space = true;
    Bits result;
    Status status = Status::Ok;
    Signals outputs;
private:
    size_t width_;
    Technology technology_;
    EndpointConfig config_;
    bool command_;
    Bits word_reg_, data_reg_, mask_reg_;
    // Static sequencer: ACCESS, IS_WRITE and the wait counter; for a clocked
    // macro the request shown this cycle.
    bool access_ = false, writing_ = false, shown_start_ = false;
    // A burst: BURST, the words still to come.
    bool burst_ = false;
    size_t beats_left_ = 0;
    // Whether the current access completes at this edge, and starts a
    // further burst word (StaticMemorySequencer's COMPLETE and NEXT_WORD).
    bool staticComplete() const { return access_ && wait_count_ == 0 && (!done || space); }
    bool nextWord() const { return staticComplete() && burst_ && beats_left_ > 0; }
    Bits nextWordAddress() const;
    Request shown_;
    size_t wait_count_ = 0;
    // Command sequencer: sequence bits, the pending steps (DRAM: PRECHARGE,
    // ACTIVATE, ACCESS, REFRESH; NOR: WREN, OPERATION), the current command
    // code, the operation kind, open-row knowledge and refresh state.
    bool active_ = false, waiting_ = false, presenting_ = false;
    bool is_write_ = false, is_program_ = false, is_erase_ = false, is_burst_ = false;
    std::vector<bool> needs_;
    unsigned current_ = 0;
    bool row_known_ = false, refreshing_ = false, refresh_pending_ = false;
    uint32_t open_row_ = 0;
    size_t refresh_counter_ = 0;
    void stepStatic(bool clock_edge, bool reset, bool start, const Request&,
                    const std::function<Bits(const std::string&, size_t)>& read, bool finish);
    void stepCommand(bool clock_edge, bool reset, bool start, const Request&,
                     const std::function<Bits(const std::string&, size_t)>& read, bool finish);
    void staticPins();
    void commandPins(const std::function<Bits(const std::string&, size_t)>& read);
    // The shown step this cycle (an index into the pending steps), if any.
    std::optional<size_t> shownStep(const std::function<Bits(const std::string&, size_t)>& read) const;
    unsigned stepCode(size_t step) const;
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
