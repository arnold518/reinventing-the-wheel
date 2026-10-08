#include "modules/memory/MemoryController.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/memory/DRAM.hpp"
#include "modules/memory/ROM.hpp"
#include "modules/memory/NORFlash.hpp"
#include "modules/memory/SRAM.hpp"
#include "MemoryEndpointEngine.hpp"
#include "MemoryHandshakeWiring.hpp"
#include "MemoryFFController.hpp"
#include <algorithm>

using namespace circuit;
using namespace circuit::memory;
using namespace circuit::memory_device;
void defineMemoryTransactionPortPins(IOComponent* s, size_t w) {
    protocolPins(s, w);
    for (auto n : {"BACK_READY", "DONE"}) s->addPin(n, PinType::INPUT);
    for (auto p : {"START", "FINISH"}) s->addPin(p, PinType::OUTPUT);
    for (auto [name, width] : std::vector<std::pair<std::string,size_t>>{{"OP",3},{"ADDR",32},{"DATA",w},{"MASK",w/8}})
        s->addPinDynamic(name, PinType::OUTPUT, width);
    s->addPinDynamic("RESULT", PinType::INPUT, w); s->addPin<3>("STATUS", PinType::INPUT);
}
void defineMemoryResponseBufferPins(IOComponent* s, size_t width) {
    for (auto p : {"CLK","RST","IN_VALID","OUT_READY"}) s->addPin(p,PinType::INPUT);
    for (auto p : {"IN_READY","OUT_VALID","FULL"}) s->addPin(p,PinType::OUTPUT);
    s->addPinDynamic("IN_DATA",PinType::INPUT,width);
    s->addPin<3>("IN_STATUS",PinType::INPUT);
    s->addPinDynamic("OUT_DATA",PinType::OUTPUT,width);
    s->addPin<3>("OUT_STATUS",PinType::OUTPUT);
}
MemoryResponseBuffer::MemoryResponseBuffer(std::string n,size_t width)
    : IOComponent(std::move(n),[width](IOComponent* s){defineMemoryResponseBufferPins(s,width);}),
      width_(width),type_("MemoryResponseBuffer<"+std::to_string(width)+">") {}
void MemoryResponseBuffer::buildInternals(ComponentBuilder& b) {
    memory_detail::StructuralWiring w(b);
    auto full=b.add(families::ClockedStateBit,"OCCUPIED");
    for(auto p:{"CLK","RST"}) w.connect(getInputPin(p),full->getInputPin(p));
    auto running=w.invert("RUNNING",getInputPin("RST"));
    auto room=w.either("SPACE",w.invert("EMPTY",full->getOutputPin("Q")),getInputPin("OUT_READY"));
    auto ready=w.both("INPUT_READY",running,room);
    auto incoming=w.both("IN_TRANSFER",getInputPin("IN_VALID"),ready);
    auto retain=w.either("RETAIN_NEW",full->getOutputPin("Q"),w.invert("OUTPUT_BLOCKED",getInputPin("OUT_READY")));
    auto capture=w.both("CAPTURE",incoming,retain);
    auto hold=w.both("HOLD",full->getOutputPin("Q"),w.invert("NOT_TAKEN",getInputPin("OUT_READY")));
    w.connect(w.either("NEXT_FULL",capture,hold),full->getInputPin("D"));
    w.connect(ready,getOutputPin("IN_READY"));
    w.connect(full->getOutputPin("Q"),getOutputPin("FULL"));
    w.connect(w.both("VALID",running,w.either("HAS_RESPONSE",full->getOutputPin("Q"),getInputPin("IN_VALID"))),getOutputPin("OUT_VALID"));
    for(auto [name,width]:std::vector<std::pair<std::string,size_t>>{{"DATA",width_},{"STATUS",3}}) {
        auto reg=b.add(families::registerWord(width), "HELD_"+name);
        for(auto p:{"CLK","RST"}) w.connect(getInputPin(p),reg->getInputPin(p));
        w.connect(capture,reg->getInputPin("WE"));
        w.connect(getInputPinDynamic("IN_"+name),reg->getInputPinDynamic("D"));
        auto selected=memory_detail::payloadMux(b,w,"SELECT_"+name,getInputPinDynamic("IN_"+name),reg->getOutputPinDynamic("Q"),full->getOutputPin("Q"));
        w.connect(selected,getOutputPinDynamic("OUT_"+name));
    }
    w.finish();
}
namespace {
class NativeSequencer : public Primitive, public MemoryActivityView {
    size_t width_;
    Technology technology_;
    NativeSequence sequence_;
    LogicValue previous_ = X;
    size_t scheduled_ = 0;
    bool history_enabled_;
public:
    NativeSequencer(std::string n, size_t w, Technology t, EndpointConfig c)
        : Primitive(std::move(n), "MemoryNativeSequencer<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">",
            [=](IOComponent* s) {
                for (auto p : {"CLK", "RST", "START", "FINISH"}) s->addPin(p, PinType::INPUT);
                for (auto [name, width] : std::vector<std::pair<std::string,size_t>>{{"OP",3},{"ADDR",32},{"DATA",w},{"MASK",w/8}})
                    s->addPinDynamic(name, PinType::INPUT, width);
                for (auto p : {"BACK_READY", "DONE"}) s->addPin(p, PinType::OUTPUT);
                s->addPinDynamic("RESULT", PinType::OUTPUT, w); s->addPin<3>("STATUS", PinType::OUTPUT);
                nativePins(s, w, t, true);
            }), width_(w), technology_(t), sequence_(w, t, c), history_enabled_(c.record_history) {}
    void evaluate(size_t time, Simulator& sim) override {
        if (scheduled_ <= time) scheduled_ = 0;
        auto clk = getInputValue("CLK"); bool rising = previous_ == L && clk == H; previous_ = clk;
        Request request{read("OP"), read("ADDR"), read("DATA"), read("MASK")};
        sequence_.step(time, rising, getInputValue("RST") != L, getInputValue("START") == H, request,
                       [&](const std::string& pin, size_t) { return read("MEM_" + pin); },getInputValue("FINISH")==H);
        publish(sim, time, "BACK_READY", sequence_.available ? H : L);
        publish(sim, time, "DONE", sequence_.done ? H : L);
        publish(sim, time, "RESULT", sequence_.result);
        publish(sim, time, "STATUS", bits(3, unsigned(sequence_.status)));
        for (auto& [name, pin] : getAllOutputPins()) if (name.starts_with("MEM_")) {
            publish(sim, time, name, signal(sequence_.outputs, name.substr(4), pin->getWidth()));
        }
        if (auto due = sequence_.deadline(); due > time && due != scheduled_) { scheduled_ = due; wake(sim, due); }
        if (history_enabled_ && sim.isHistoryRecordingEnabled()) {
            MemoryActivity activity{sequence_.phaseName(), sequence_.maintenanceActive(), {}};
            for (const auto& name : nativeActivityInputs(technology_))
                activity.signals[name] = sequence_.outputs.at(name);
            for (auto name : {"BUSY", "ERROR", "READ_VALID"})
                if (getAllInputPins().contains(std::string("MEM_") + name))
                    activity.signals[name] = read(std::string("MEM_") + name);
            recordMemoryActivity(time, std::move(activity));
        }
    }
};
}
MemoryTransactionPort::MemoryTransactionPort(std::string n, size_t w)
    : IOComponent(std::move(n), [w](IOComponent* s) { defineMemoryTransactionPortPins(s, w); }), width_(w),
      type_("MemoryTransactionPort<" + std::to_string(w) + ">") {}
void MemoryTransactionPort::buildInternals(ComponentBuilder& b) {
    memory_detail::StructuralWiring w(b);
    auto pending=b.add(families::ClockedStateBit,"PENDING");
    auto response=b.addNewComponent<MemoryResponseBuffer>("RESPONSE",width_);
    for(auto p:{"CLK","RST"}) {
        w.connect(getInputPin(p),pending->getInputPin(p));
        w.connect(getInputPin(p),response->getInputPin(p));
    }
    w.connect(getInputPin("RSP_READY"),response->getInputPin("OUT_READY"));
    auto completion=w.both("COMPLETION",pending->getOutputPin("Q"),getInputPin("DONE"));
    auto finish=w.both("FINISH",completion,response->getOutputPin("IN_READY"));
    // Capacity is one outstanding transaction, including a held response.
    auto response_taken=w.both("RESPONSE_TAKEN",completion,getInputPin("RSP_READY"));
    auto slot=w.either("REQUEST_SLOT",w.invert("IDLE",pending->getOutputPin("Q")),response_taken);
    auto ready=w.reduce("READY",{w.invert("RUNNING",getInputPin("RST")),slot,
        getInputPin("BACK_READY"),response->getOutputPin("IN_READY")},true);
    auto accept=w.both("ACCEPT",ready,getInputPin("REQ_VALID"));
    w.connect(w.either("NEXT_PENDING",accept,w.both("KEEP_PENDING",pending->getOutputPin("Q"),
        w.invert("NOT_FINISHED",finish))),pending->getInputPin("D"));
    w.connect(ready,getOutputPin("REQ_READY"));
    w.connect(accept,getOutputPin("START"));
    w.connect(finish,getOutputPin("FINISH"));
    w.connect(completion,response->getInputPin("IN_VALID"));
    w.connect(getInputPinDynamic("RESULT"),response->getInputPinDynamic("IN_DATA"));
    w.connect(getInputPin<3>("STATUS"),response->getInputPin<3>("IN_STATUS"));
    for(auto [source,target]:std::vector<std::pair<std::string,std::string>>{
        {"OUT_VALID","RSP_VALID"},{"OUT_DATA","RSP_READ_DATA"},{"OUT_STATUS","RSP_STATUS"}})
        w.connect(response->getOutputPinDynamic(source),getOutputPinDynamic(target));
    // Native sequencer captures the fields on START's rising edge.
    for(auto [source,target]:std::vector<std::pair<std::string,std::string>>{
        {"REQ_OP","OP"},{"REQ_ADDR","ADDR"},{"REQ_WRITE_DATA","DATA"},{"REQ_WRITE_MASK","MASK"}})
        w.connect(memory_detail::bridge(b,w,"REQUEST_"+target,getInputPinDynamic(source)),getOutputPinDynamic(target));
    w.finish();
}
MemoryController::MemoryController(std::string n, size_t w, Technology t, EndpointConfig c)
    : IOComponent(std::move(n), [=](IOComponent* s) { protocolPins(s, w); nativePins(s, w, t, true); }),
      width_(w), technology_(t), config_(c), type_("MemoryController<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">") { c.validate(w, t); }
void MemoryController::buildInternals(ComponentBuilder& b) {
    if (technology_ == Technology::FF) {
        memory_detail::buildFFController(b,*this,width_,config_.depth_words,config_.always_ready);
        return;
    }
    memory_detail::StructuralWiring w(b);
    auto front = b.addNewComponent<MemoryTransactionPort>("TRANSACTION", width_);
    auto control = b.addNewComponent<NativeSequencer>("DEVICE_CONTROL", width_, technology_, config_);
    for (const auto& [name, pin] : getAllInputPins()) {
        if (name.starts_with("MEM_")) w.connect(pin, control->getInputPinDynamic(name));
        else {
            w.connect(pin, front->getInputPinDynamic(name));
            if (name == "CLK" || name == "RST") w.connect(pin, control->getInputPinDynamic(name));
        }
    }
    for (const auto& [name, pin] : getAllOutputPins()) {
        w.connect((name.starts_with("MEM_") ? control : std::static_pointer_cast<IOComponent>(front))->getOutputPinDynamic(name), pin);
    }
    for (auto n : {"START", "FINISH", "OP", "ADDR", "DATA", "MASK"}) w.connect(front->getOutputPinDynamic(n), control->getInputPinDynamic(n));
    for (auto n : {"BACK_READY", "DONE", "RESULT", "STATUS"}) w.connect(control->getOutputPinDynamic(n), front->getInputPinDynamic(n));
    w.finish();
}
MemoryEndpoint::MemoryEndpoint(std::string n, size_t w, Technology t, EndpointConfig c, bool scalable_device)
    : IOComponent(std::move(n), [w](IOComponent* s) { protocolPins(s, w); }), width_(w), technology_(t),
      config_(c), scalable_device_(scalable_device),
      type_("MemoryEndpoint<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">") { c.validate(w, t, !scalable_device); }
void MemoryEndpoint::buildInternals(ComponentBuilder& b) {
    memory_detail::StructuralWiring w(b);
    auto controller = b.addNewComponent<MemoryController>("CONTROLLER", width_, technology_, config_);
    std::shared_ptr<IOComponent> device;
    if (scalable_device_) {
        device = memory_device::child(b,*this,memoryContractId(technology_,"scalable-device",width_),
                                      "MEMORY",endpointParameters(config_));
    } else {
        // An expanded device may itself be selected at behavioral fidelity.
        // Preserve the same storage policy as the compact-device path.
        const auto request = nativeDeviceRequest(width_, technology_, config_);
        device = memory_device::child(b, *this, request.contract_id, "MEMORY", request.parameters);
    }
    storage_ = std::dynamic_pointer_cast<MemoryStateView>(device);
    for (const auto& [name, pin] : getAllInputPins()) w.connect(pin, controller->getInputPinDynamic(name));
    for (const auto& [name, pin] : getAllOutputPins()) w.connect(controller->getOutputPinDynamic(name), pin);
    for (const auto& [name, pin] : controller->getAllOutputPins()) if (name.starts_with("MEM_"))
        w.connect(pin, device->getInputPinDynamic(name.substr(4)));
    for (const auto& [name, pin] : controller->getAllInputPins()) if (name.starts_with("MEM_"))
        w.connect(device->getOutputPinDynamic(name.substr(4)), pin);
    w.finish();
}

const std::shared_ptr<MemoryStateView>& MemoryEndpoint::storageView() const {
    if (!storage_) throw std::logic_error("Expanded cell storage has no compact memory snapshot; inspect its cells");
    return storage_;
}
const MemoryStorage& MemoryEndpoint::memoryStorage() const { return storageView()->memoryStorage(); }
void MemoryEndpoint::loadMemoryBytes(uint32_t a, const std::vector<uint8_t>& bytes) {
    // Keep runtime writes on the transaction port, exactly like the fused form.
    (void)a; (void)bytes;
    throw std::logic_error("Use initialize_memory_bytes before simulation or the memory transaction port");
}
void MemoryEndpoint::initializeMemoryBytes(uint32_t a, const std::vector<uint8_t>& bytes) { storageView()->initializeMemoryBytes(a, bytes); }
void MemoryEndpoint::clearMemoryImage() { storageView()->clearMemoryImage(); }
void MemoryEndpoint::setMemoryHistoryRecordingEnabled(bool enabled) { storageView()->setMemoryHistoryRecordingEnabled(enabled); }
