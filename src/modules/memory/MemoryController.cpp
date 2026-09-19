#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/memory/DRAM.hpp"
#include "modules/memory/ROM.hpp"
#include "modules/memory/NORFlash.hpp"
#include "modules/memory/SRAM.hpp"
#include "MemoryEndpointEngine.hpp"

using namespace circuit;
using namespace circuit::memory;
using namespace circuit::memory_device;
void defineMemoryTransactionPortPins(IOComponent* s, size_t w) {
    protocolPins(s, w);
    for (auto n : {"BACK_READY", "DONE"}) s->addPin(n, PinType::INPUT);
    s->addPin("START", PinType::OUTPUT);
    for (auto [name, width] : std::vector<std::pair<std::string,size_t>>{{"OP",3},{"ADDR",32},{"DATA",w},{"MASK",w/8}})
        s->addPinDynamic(name, PinType::OUTPUT, width);
    s->addPinDynamic("RESULT", PinType::INPUT, w); s->addPin<3>("STATUS", PinType::INPUT);
}
// A register is deliberately composed from the existing FF cell family.
MemoryPayloadRegister::MemoryPayloadRegister(std::string n, size_t w) : IOComponent(std::move(n), [w](IOComponent* s) {
        s->addPinDynamic("D", PinType::INPUT, w); s->addPinDynamic("Q", PinType::OUTPUT, w);
        for (auto p : {"CLK", "RST", "WE"}) s->addPin(p, PinType::INPUT);
    }), width_(w), type_("MemoryPayloadRegister<" + std::to_string(w) + ">") {}
void MemoryPayloadRegister::buildInternals(ComponentBuilder& b) {
        memory_detail::StructuralWiring wires(b);
        std::shared_ptr<IOComponent> split, join;
        if (width_ != 1) {
            if (width_ == 2) {
                split = b.addNewComponent<BitSplitter<2>>("INPUT_BITS");
                join = b.addNewComponent<BitJoiner<2>>("OUTPUT_BITS");
            } else if (width_ == 4) {
                split = b.addNewComponent<BitSplitter<4>>("INPUT_BITS");
                join = b.addNewComponent<BitJoiner<4>>("OUTPUT_BITS");
            } else if (width_ == 3) {
                split = b.addNewComponent<BitSplitter<3>>("INPUT_BITS");
                join = b.addNewComponent<BitJoiner<3>>("OUTPUT_BITS");
            } else {
                split = bitAdapter(b, "INPUT_BITS", width_, true);
                join = bitAdapter(b, "OUTPUT_BITS", width_, false);
            }
            wires.connect(getInputPinDynamic("D"), split->getInputPinDynamic("IN"));
            wires.connect(join->getOutputPinDynamic("OUT"), getOutputPinDynamic("Q"));
        }
        for (size_t bit = 0; bit < width_; ++bit) {
            auto cell = b.add(families::FlipFlopCell, "BIT_" + std::to_string(bit));
            for (auto p : {"CLK", "RST", "WE"}) wires.connect(getInputPin(p), cell->getInputPin(p));
            wires.connect(split ? split->getOutputPin("OUT_" + std::to_string(bit)) : getInputPin("D"), cell->getInputPin("D"));
            wires.connect(cell->getOutputPin("Q"), join ? join->getInputPin("IN_" + std::to_string(bit)) : getOutputPin("Q"));
        }
        wires.finish();
    }
namespace {
class NativeSequencer : public Primitive {
    size_t width_;
    Technology technology_;
    NativeSequence sequence_;
    LogicValue previous_ = X;
    size_t scheduled_ = 0;
public:
    NativeSequencer(std::string n, size_t w, Technology t, EndpointConfig c)
        : Primitive(std::move(n), "MemoryNativeSequencer<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">",
            [=](IOComponent* s) {
                for (auto p : {"CLK", "RST", "START"}) s->addPin(p, PinType::INPUT);
                for (auto [name, width] : std::vector<std::pair<std::string,size_t>>{{"OP",3},{"ADDR",32},{"DATA",w},{"MASK",w/8}})
                    s->addPinDynamic(name, PinType::INPUT, width);
                for (auto p : {"BACK_READY", "DONE"}) s->addPin(p, PinType::OUTPUT);
                s->addPinDynamic("RESULT", PinType::OUTPUT, w); s->addPin<3>("STATUS", PinType::OUTPUT);
                nativePins(s, w, t, true);
            }), width_(w), technology_(t), sequence_(w, t, c) {}
    void evaluate(size_t time, Simulator& sim) override {
        if (scheduled_ <= time) scheduled_ = 0;
        auto clk = getInputValue("CLK"); bool rising = previous_ == L && clk == H; previous_ = clk;
        Request request{read("OP"), read("ADDR"), read("DATA"), read("MASK")};
        sequence_.step(time, rising, getInputValue("RST") != L, getInputValue("START") == H, request,
                       [&](const std::string& pin, size_t) { return read("MEM_" + pin); });
        publish(sim, time, "BACK_READY", sequence_.available ? H : L);
        publish(sim, time, "DONE", sequence_.done ? H : L);
        publish(sim, time, "RESULT", sequence_.result);
        publish(sim, time, "STATUS", bits(3, unsigned(sequence_.status)));
        for (auto& [name, pin] : getAllOutputPins()) if (name.starts_with("MEM_")) {
            if (technology_ == Technology::Container && name == "MEM_CLK") publish(sim, time, name, clk);
            else publish(sim, time, name, signal(sequence_.outputs, name.substr(4), pin->getWidth()));
        }
        if (auto due = sequence_.deadline(); due > time && due != scheduled_) { scheduled_ = due; wake(sim, due); }
    }
};
}
MemoryTransactionPort::MemoryTransactionPort(std::string n, size_t w)
    : IOComponent(std::move(n), [w](IOComponent* s) { defineMemoryTransactionPortPins(s, w); }), width_(w),
      type_("MemoryTransactionPort<" + std::to_string(w) + ">") {}
void MemoryTransactionPort::buildInternals(ComponentBuilder& b) {
    memory_detail::StructuralWiring w(b);
    auto one = w.constant("ONE", true);
    auto cell = [&](const char* name) {
        auto c = b.add(families::FlipFlopCell, name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), c->getInputPin(p));
        w.connect(one, c->getInputPin("WE")); return c;
    };
    auto q0 = cell("PHASE_0"), q1 = cell("PHASE_1");
    auto lo = q0->getOutputPin("Q"), hi = q1->getOutputPin("Q");
    auto nlo = w.invert("NOT_0", lo), nhi = w.invert("NOT_1", hi);
    auto idle = w.both("IDLE", nlo, nhi), setup = w.both("SETUP", lo, nhi);
    auto wait = w.both("WAIT", hi, nlo), response = w.both("RESPONSE", hi, lo);
    auto running = w.invert("NOT_RESET", getInputPin("RST"));
    auto ready = w.both("READY", w.both("AVAILABLE", idle, getInputPin("BACK_READY")), running);
    auto accept = w.both("ACCEPT", ready, getInputPin("REQ_VALID"));
    auto complete = w.both("COMPLETE", wait, getInputPin("DONE"));
    auto hold = w.both("HOLD", response, w.invert("NOT_RSP_READY", getInputPin("RSP_READY")));
    w.connect(w.either("NEXT_0", accept, w.either("NEXT_0_BUSY", complete, hold)), q0->getInputPin("D"));
    w.connect(w.either("NEXT_1", setup, w.either("NEXT_1_BUSY", wait, hold)), q1->getInputPin("D"));
    w.connect(ready, getOutputPin("REQ_READY"));
    w.connect(w.both("VALID", response, running), getOutputPin("RSP_VALID"));
    w.connect(w.both("START_RUNNING", wait, running), getOutputPin("START"));
    auto reg = [&](const char* name, const char* input, const char* output, size_t width, auto enable) {
        auto r = b.addNewComponent<MemoryPayloadRegister>(name, width);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), r->getInputPin(p));
        w.connect(enable, r->getInputPin("WE"));
        w.connect(getInputPinDynamic(input), r->getInputPinDynamic("D"));
        w.connect(r->getOutputPinDynamic("Q"), getOutputPinDynamic(output));
    };
    reg("REQUEST_OP", "REQ_OP", "OP", 3, accept);
    reg("REQUEST_ADDRESS", "REQ_ADDR", "ADDR", 32, accept);
    reg("REQUEST_DATA", "REQ_WRITE_DATA", "DATA", width_, accept);
    reg("REQUEST_MASK", "REQ_WRITE_MASK", "MASK", width_ / 8, accept);
    reg("RESPONSE_DATA", "RESULT", "RSP_READ_DATA", width_, complete);
    reg("RESPONSE_STATUS", "STATUS", "RSP_STATUS", 3, complete);
    w.finish();
}
MemoryController::MemoryController(std::string n, size_t w, Technology t, EndpointConfig c)
    : IOComponent(std::move(n), [=](IOComponent* s) { protocolPins(s, w); nativePins(s, w, t, true); }),
      width_(w), technology_(t), config_(c), type_("MemoryController<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">") { c.validate(w, t); }
void MemoryController::buildInternals(ComponentBuilder& b) {
    memory_detail::StructuralWiring w(b);
    auto front = b.addNewComponent<MemoryTransactionPort>("TRANSACTION", width_);
    auto control = b.addNewComponent<NativeSequencer>("DEVICE_CONTROL", width_, technology_, config_);
    std::shared_ptr<Rewire> relay_clock;
    if (technology_ == Technology::Container) {
        // A parent input cannot directly drive a parent output through this
        // simulator's two-scope pin boundary. Rewire is an explicit zero-delay
        // bridge, so the downstream endpoint samples the same bus edge.
        relay_clock = b.addNewComponent<Rewire>("BUS_CLOCK",
            std::vector<Rewire::WireSpec>{{"IN",1}}, std::vector<Rewire::WireSpec>{{"OUT",1}},
            identity_mapping("IN",0,1,"OUT"));
        w.connect(getInputPin("CLK"),relay_clock->getInputPin("IN"));
    }
    for (const auto& [name, pin] : getAllInputPins()) {
        if (name.starts_with("MEM_")) w.connect(pin, control->getInputPinDynamic(name));
        else {
            w.connect(pin, front->getInputPinDynamic(name));
            if (name == "CLK" || name == "RST") w.connect(pin, control->getInputPinDynamic(name));
        }
    }
    for (const auto& [name, pin] : getAllOutputPins()) {
        if (relay_clock && name == "MEM_CLK") w.connect(relay_clock->getOutputPin("OUT"), pin);
        else w.connect((name.starts_with("MEM_") ? control : std::static_pointer_cast<IOComponent>(front))->getOutputPinDynamic(name), pin);
    }
    for (auto n : {"START", "OP", "ADDR", "DATA", "MASK"}) w.connect(front->getOutputPinDynamic(n), control->getInputPinDynamic(n));
    for (auto n : {"BACK_READY", "DONE", "RESULT", "STATUS"}) w.connect(control->getOutputPinDynamic(n), front->getInputPinDynamic(n));
    w.finish();
}
MemoryEndpoint::MemoryEndpoint(std::string n, size_t w, Technology t, EndpointConfig c)
    : IOComponent(std::move(n), [w](IOComponent* s) { protocolPins(s, w); }), width_(w), technology_(t),
      config_(c), type_("MemoryEndpoint<" + std::string(technologyName(t)) + "," + std::to_string(w) + ">") { c.validate(w, t, t != Technology::Container); }
void MemoryEndpoint::buildInternals(ComponentBuilder& b) {
    memory_detail::StructuralWiring w(b);
    auto controller = b.addNewComponent<MemoryController>("CONTROLLER", width_, technology_, config_);
    std::shared_ptr<IOComponent> device;
    if (technology_ == Technology::Container) {
        device = b.addNewComponent<BehavioralMemoryContainer>("MEMORY", width_, Technology::SRAM, config_);
    } else {
        ParameterMap params{{"depth_words", std::to_string(config_.depth_words)}};
        std::string id;
        if (technology_ == Technology::FF) id = "memory.container.ff.";
        else {
            id = "memory." + std::string(technologyName(technology_)) + ".device.";
            params["words_per_row"] = std::to_string(config_.words_per_row);
            if (technology_ != Technology::ROM) params["phase_ticks"] = std::to_string(config_.phase_ticks);
            if (technology_ == Technology::DRAM) params["retention_ticks"] = std::to_string(config_.retention_ticks);
            if (technology_ == Technology::ROM) params["image_hex"] = config_.image_hex;
            if (technology_ == Technology::NORFlash) {
                params["block_words"] = std::to_string(config_.block_words);
                params["program_ticks"] = std::to_string(config_.program_ticks);
                params["erase_ticks"] = std::to_string(config_.erase_ticks);
            }
        }
        id += std::to_string(width_);
        device = memory_device::child(b, *this, id, "MEMORY", params);
    }
    for (const auto& [name, pin] : getAllInputPins()) w.connect(pin, controller->getInputPinDynamic(name));
    for (const auto& [name, pin] : getAllOutputPins()) w.connect(controller->getOutputPinDynamic(name), pin);
    for (const auto& [name, pin] : controller->getAllOutputPins()) if (name.starts_with("MEM_"))
        w.connect(pin, device->getInputPinDynamic(name.substr(4)));
    for (const auto& [name, pin] : controller->getAllInputPins()) if (name.starts_with("MEM_"))
        w.connect(device->getOutputPinDynamic(name.substr(4)), pin);
    w.finish();
}
