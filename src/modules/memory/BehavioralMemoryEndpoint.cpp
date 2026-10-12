#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "MemoryEndpointEngine.hpp"

using namespace circuit::memory;
using namespace circuit::memory_device;
struct BehavioralMemoryEndpoint::State {
    size_t width, scheduled = 0;
    Technology technology;
    EndpointConfig config;
    // The handshake state is the gates' own, in four-state form.
    LogicValue pending = L, done_seen = L, busy_seen = L;
    ResponseBuffer buffer;
    NativeSequence sequence;
    std::unique_ptr<DeviceModel> device;
    Request request;
    LogicValue previous = X;
    bool started = false;
    State(size_t w, Technology t, EndpointConfig c)
        : width(w), technology(t), config(c), buffer(w), sequence(w, t, c), device(makeDeviceModel(w, t, c)),
          request{bits(3,0), bits(32,0), Bits(w,L), Bits(w/8,L)} {}
};
BehavioralMemoryEndpoint::BehavioralMemoryEndpoint(std::string n, size_t w, Technology t, EndpointConfig c)
    : Primitive(std::move(n), "MemoryEndpoint<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">",
                [w](IOComponent* s) { protocolPins(s, w); }) {
    c.validate(w, t); state_ = std::make_unique<State>(w, t, c);
}
BehavioralMemoryEndpoint::~BehavioralMemoryEndpoint() = default;
Technology BehavioralMemoryEndpoint::technology() const noexcept { return state_->technology; }
bool BehavioralMemoryEndpoint::hasStarted() const noexcept { return state_->started; }
const MemoryStorage& BehavioralMemoryEndpoint::memoryStorage() const { return state_->device->storage; }
void BehavioralMemoryEndpoint::loadMemoryBytes(uint32_t, const std::vector<uint8_t>&) {
    throw std::logic_error("Use ROM image_hex at construction or the memory transaction port");
}
void BehavioralMemoryEndpoint::initializeBytes(uint32_t address, const std::vector<uint8_t>& bytes) {
    if (state_->started) throw std::logic_error("Memory image provisioning is only allowed before simulation starts");
    state_->device->storage.loadBytes(address, bytes);
}
void BehavioralMemoryEndpoint::clearContents() {
    if (state_->started) throw std::logic_error("Memory image clearing is only allowed before simulation starts");
    state_->device->storage.clearContents();
}
void BehavioralMemoryEndpoint::loadWords(uint32_t address, const std::vector<uint32_t>& words) {
    std::vector<uint8_t> bytes;
    for (auto word : words) for (unsigned i = 0; i < 4; ++i) bytes.push_back(uint8_t(word >> (8 * i)));
    initializeBytes(address, bytes);
}
void BehavioralMemoryEndpoint::setHistoryRecordingEnabled(bool enabled) {
    state_->device->storage.setHistoryRecordingEnabled(enabled);
}
void BehavioralMemoryEndpoint::evaluate(size_t t, Simulator& sim) {
    auto& s=*state_;
    s.started=true;
    if(s.scheduled<=t)s.scheduled=0;
    auto clk=getInputValue("CLK");
    const bool edge=s.previous==L && clk==H;
    s.previous=clk;
    const auto reset=clean(getInputValue("RST"));
    // The always-ready FF specialization has no response storage. Its
    // consumer contract requires RSP_READY=1, and this model must not create
    // a hidden hold register if that contract is violated.
    const auto take=s.config.always_ready?H:clean(getInputValue("RSP_READY"));
    const auto issue=clean(getInputValue("REQ_VALID"));
    const Request request{read("REQ_OP"),read("REQ_ADDR"),read("REQ_WRITE_DATA"),read("REQ_WRITE_MASK")};
    const bool fast=s.technology==Technology::FF;
    struct View { LogicValue complete, finish, ready, accept, incoming; };
    const auto view=[&] {
        const auto room=s.buffer.room(take);
        const auto complete=both(s.pending,s.done_seen);
        const auto slot=fast?H:both(either(no(s.pending),both(complete,take)),both(no(s.busy_seen),bit(s.sequence.available)));
        const auto ready=both(both(no(reset),room),slot);
        const auto accept=both(ready,issue);
        return View{complete,both(complete,both(no(reset),room)),ready,accept,fast?accept:complete};
    };
    // The flip-flop device answers in the same cycle: its read data is the
    // word when the read is accepted, 0 when it is not.
    const auto fastStatus=fast?validateRequest(request,s.width,Technology::FF,s.config):Status::Ok;
    const auto fastData=[&](LogicValue accept) {
        if(fastStatus!=Status::Ok||number(request.op)!=0||accept==L) return Bits(s.width,L);
        auto word=s.device->storage.readLogic(number(request.address),s.width/8);
        if(accept!=H) for(auto& value:word) fade(value); // a stored 0 reads 0 either way
        return word;
    };
    // A sequenced device's result and status are shown only with the captured
    // completion (see MemoryTransactionPort).
    const auto shown=[&](Bits value,LogicValue completion) {
        if(!fast) for(auto& one:value) one=both(completion,clean(one));
        return value;
    };
    const auto before=view();
    if(!s.config.always_ready||!fast)
        s.buffer.clock(reset,edge,before.incoming,take,fast?fastData(before.accept):shown(s.sequence.result,before.complete),
            shown(bits(3,unsigned(fast?fastStatus:s.sequence.status)),before.complete));
    // DONE and BACK_READY as captured at this edge (see MemoryTransactionPort).
    if(reset==H) s.pending=s.done_seen=s.busy_seen=L;
    else if(reset!=L) {fade(s.pending);fade(s.done_seen);fade(s.busy_seen);}
    else if(edge&&!fast) {
        s.pending=either(before.accept,both(s.pending,no(before.finish)));
        s.done_seen=both(bit(s.sequence.done),no(before.finish));
        s.busy_seen=bit(!s.sequence.available);
    }
    if(fast) {
        // Writes are triggered once, by acceptance on the shared edge.
        const bool writing=fastStatus==Status::Ok&&number(request.op)==1;
        Signals pins{{"CLK",{clk}},{"WORD_ADDR",bits(32,number(request.address)/(s.width/8))},
            {"WE",{both(both(no(reset),before.accept),bit(writing))}},
            {"READ_EN",{L}},{"WRITE_DATA",request.data},{"WRITE_MASK",request.mask}};
        s.device->update(t,pins);
    } else {
        // The sequencer starts and finishes only on a clean 1, as its
        // gate-level form does.
        s.device->update(t,s.sequence.outputs);
        s.sequence.step(t,edge,reset!=L,before.accept==H,request,
            [&](const std::string& pin,size_t width){return s.device->read(pin,width);},before.finish==H);
        s.device->update(t,s.sequence.outputs);
    }
    // Recompute the combinational view after the edge.
    const auto now=view();
    const auto data=fast?fastData(now.accept):shown(s.sequence.result,now.complete);
    const auto status=shown(bits(3,unsigned(fast?fastStatus:s.sequence.status)),now.complete);
    publish(sim,t,"REQ_READY",now.ready);
    publish(sim,t,"RSP_VALID",s.buffer.valid(reset,now.incoming));
    publish(sim,t,"RSP_READ_DATA",s.buffer.select(data,s.buffer.data));
    publish(sim,t,"RSP_STATUS",s.buffer.select(status,s.buffer.status));
    if (sim.isHistoryRecordingEnabled() && s.device->storage.isHistoryRecordingEnabled()) {
        MemoryActivity activity{fast ? "combinational" : s.sequence.phaseName(), s.sequence.maintenanceActive(), {}};
        if (!fast) {
            for (const auto& name : nativeActivityInputs(s.technology))
                activity.signals[name] = s.sequence.outputs.at(name);
            if (s.technology == Technology::DRAM || s.technology == Technology::NORFlash)
                for (auto name : {"BUSY", "ERROR", "READ_VALID"})
                    activity.signals[name] = s.device->read(name, 1);
            if (s.technology == Technology::DRAM)
                activity.signals["ROW_OPEN"] = s.device->read("ROW_OPEN", 1);
            if (s.technology == Technology::NORFlash)
                activity.signals["WRITE_ENABLED"] = s.device->read("WRITE_ENABLED", 1);
        }
        recordMemoryActivity(t, std::move(activity));
    }
    const auto a=s.sequence.deadline(),b=s.device->deadline();
    const auto next=!a?b:!b?a:std::min(a,b);
    if(!fast&&next>t&&next!=s.scheduled){s.scheduled=next;wake(sim,next);}
}
