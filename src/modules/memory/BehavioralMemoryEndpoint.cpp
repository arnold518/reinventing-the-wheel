#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "MemoryEndpointEngine.hpp"

using namespace circuit::memory;
using namespace circuit::memory_device;
struct BehavioralMemoryEndpoint::State {
    size_t width, scheduled = 0;
    Technology technology;
    EndpointConfig config;
    bool pending = false, full = false;
    NativeSequence sequence;
    std::unique_ptr<DeviceModel> device;
    Request request;
    Bits result;
    Status status = Status::Ok;
    LogicValue previous = X;
    bool started = false;
    State(size_t w, Technology t, EndpointConfig c)
        : width(w), technology(t), config(c), sequence(w, t, c), device(makeDeviceModel(w, t, c)),
          request{bits(3,0), bits(32,0), Bits(w,L), Bits(w/8,L)}, result(w,L) {}
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
    const bool reset=getInputValue("RST")!=L;
    // The always-ready FF specialization has no response storage. Its
    // consumer contract requires RSP_READY=1, and this model must not create
    // a hidden hold register if that contract is violated.
    const bool take=s.config.always_ready||getInputValue("RSP_READY")==H;
    const bool issue=getInputValue("REQ_VALID")==H;
    const Request request{read("REQ_OP"),read("REQ_ADDR"),read("REQ_WRITE_DATA"),read("REQ_WRITE_MASK")};
    const bool room=!s.full||take;
    const bool fast=s.technology==Technology::FF;
    const bool complete=s.pending&&s.sequence.done;
    const bool ready=!reset&&room&&(fast||((!s.pending||(complete&&take))&&s.sequence.available));
    const bool accept=ready&&issue;
    const bool incoming=fast?accept:complete;
    Bits data(s.width,L);
    Status status=Status::Ok;
    if(fast) {
        status=validateRequest(request,s.width,Technology::FF,s.config);
        if(accept && status==Status::Ok && number(request.op)==0)
            data=s.device->storage.readLogic(number(request.address),s.width/8);
    } else {
        data=s.sequence.result;
        status=s.sequence.status;
    }
    if(reset) {
        s.pending=s.full=false;
        s.result.assign(s.width,L);s.status=Status::Ok;
    } else if(edge) {
        const bool capture=incoming&&room&&(s.full||!take);
        const bool held=s.full&&!take;
        if(capture){s.result=data;s.status=status;}
        s.full=capture||held;
        if(!fast)s.pending=accept||(s.pending&&!(complete&&room));
    }
    if(fast) {
        // Writes are triggered once, by acceptance on the shared edge.
        Signals pins{{"CLK",{clk}},{"WORD_ADDR",bits(32,number(request.address)/(s.width/8))},
            {"WE",{!reset&&accept&&status==Status::Ok&&number(request.op)==1?H:L}},
            {"READ_EN",{L}},{"WRITE_DATA",request.data},{"WRITE_MASK",request.mask}};
        s.device->update(t,pins);
    } else {
        s.device->update(t,s.sequence.outputs);
        s.sequence.step(t,edge,reset,accept,request,
            [&](const std::string& pin,size_t width){return s.device->read(pin,width);},complete&&room);
        s.device->update(t,s.sequence.outputs);
    }
    // Recompute the combinational view after the edge.
    const bool room_now=!s.full||take;
    const bool complete_now=s.pending&&s.sequence.done;
    const bool ready_now=!reset&&room_now&&(fast||((!s.pending||(complete_now&&take))&&s.sequence.available));
    const bool incoming_now=fast?(ready_now&&issue):complete_now;
    if(fast) {
        data.assign(s.width,L);
        if(ready_now&&issue&&status==Status::Ok&&number(request.op)==0)
            data=s.device->storage.readLogic(number(request.address),s.width/8);
    }
    if(!fast){data=s.sequence.result;status=s.sequence.status;}
    publish(sim,t,"REQ_READY",ready_now?H:L);
    publish(sim,t,"RSP_VALID",!reset&&(s.full||incoming_now)?H:L);
    publish(sim,t,"RSP_READ_DATA",s.full?s.result:data);
    publish(sim,t,"RSP_STATUS",bits(3,unsigned(s.full?s.status:status)));
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
