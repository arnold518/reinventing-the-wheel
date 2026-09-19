#include "modules/memory/MemoryContainer.hpp"
#include "MemoryEndpointEngine.hpp"

using namespace circuit::memory;
using namespace circuit::memory_device;
struct BehavioralMemoryContainer::State {
    size_t width, phase = 0, scheduled = 0;
    NativeSequence sequence;
    std::unique_ptr<DeviceModel> device;
    Request request;
    Bits result;
    Status status = Status::Ok;
    LogicValue previous = X;
    bool started = false;
    State(size_t w, Technology t, EndpointConfig c)
        : width(w), sequence(w, t, c), device(makeDeviceModel(w, t, c)),
          request{bits(3,0), bits(32,0), Bits(w,L), Bits(w/8,L)}, result(w,L) {}
};
BehavioralMemoryContainer::BehavioralMemoryContainer(std::string n, size_t w, Technology t, EndpointConfig c)
    : Primitive(std::move(n), "MemoryContainer<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">",
                [w](IOComponent* s) { protocolPins(s, w); }) {
    c.validate(w, t); state_ = std::make_unique<State>(w, t, c);
}
BehavioralMemoryContainer::~BehavioralMemoryContainer() = default;
const MemoryStorage& BehavioralMemoryContainer::memoryStorage() const { return state_->device->storage; }
void BehavioralMemoryContainer::loadMemoryBytes(uint32_t, const std::vector<uint8_t>&) {
    throw std::logic_error("Use ROM image_hex at construction or the memory transaction port");
}
void BehavioralMemoryContainer::initializeBytes(uint32_t address, const std::vector<uint8_t>& bytes) {
    if (state_->started) throw std::logic_error("Memory image provisioning is only allowed before simulation starts");
    state_->device->storage.loadBytes(address, bytes);
}
void BehavioralMemoryContainer::clearContents() {
    if (state_->started) throw std::logic_error("Memory image clearing is only allowed before simulation starts");
    state_->device->storage.clearContents();
}
void BehavioralMemoryContainer::loadWords(uint32_t address, const std::vector<uint32_t>& words) {
    std::vector<uint8_t> bytes;
    for (auto word : words) for (unsigned i = 0; i < 4; ++i) bytes.push_back(uint8_t(word >> (8 * i)));
    initializeBytes(address, bytes);
}
void BehavioralMemoryContainer::setHistoryRecordingEnabled(bool enabled) {
    state_->device->storage.setHistoryRecordingEnabled(enabled);
}
void BehavioralMemoryContainer::evaluate(size_t t, Simulator& sim) {
    auto& s = *state_;
    s.started = true;
    if (s.scheduled <= t) s.scheduled = 0;
    const auto clk = getInputValue("CLK"); bool rising = s.previous == L && clk == H; s.previous = clk;
    const bool reset = getInputValue("RST") != L;
    if (reset) { s.phase = 0; s.result.assign(s.width, L); s.status = Status::Ok; }
    else if (rising) {
        if (s.phase == 0 && s.sequence.available && getInputValue("REQ_VALID") == H) {
            s.request = {read("REQ_OP"), read("REQ_ADDR"), read("REQ_WRITE_DATA"), read("REQ_WRITE_MASK")}; s.phase = 1;
        } else if (s.phase == 1) s.phase = 2;
        else if (s.phase == 2 && s.sequence.done) { s.result = s.sequence.result; s.status = s.sequence.status; s.phase = 3; }
        else if (s.phase == 3 && getInputValue("RSP_READY") == H) s.phase = 0;
    }
    s.device->update(t, s.sequence.outputs);
    s.sequence.step(t, rising, reset, s.phase == 2, s.request,
                    [&](const std::string& pin, size_t width) { return s.device->read(pin, width); });
    s.device->update(t, s.sequence.outputs);
    publish(sim, t, "REQ_READY", !reset && s.phase == 0 && s.sequence.available ? H : L);
    publish(sim, t, "RSP_VALID", !reset && s.phase == 3 ? H : L);
    publish(sim, t, "RSP_READ_DATA", s.result);
    publish(sim, t, "RSP_STATUS", bits(3, unsigned(s.status)));
    const auto a = s.sequence.deadline(), b = s.device->deadline();
    size_t next = !a ? b : !b ? a : std::min(a, b);
    if (next > t && next != s.scheduled) { s.scheduled = next; wake(sim, next); }
}
