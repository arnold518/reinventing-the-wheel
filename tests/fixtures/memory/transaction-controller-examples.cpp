// Executed after the exact 3.2, 3.3, and 3.1.6 documentation excerpts.
#include <cassert>
#include <iostream>
#include <random>

namespace {
using namespace circuit::memory;

template<std::size_t W>
Request request(Operation op, std::uint32_t address, std::uint32_t value = 0) {
    return {bits(3, unsigned(op)), bits(32, address), bits(W, value), Bits(W / 8, H)};
}

template<std::size_t W>
void check_port() {
    memory_example::Port<W> port;
    typename memory_example::Port<W>::Inputs in;
    in.reset=true;
    auto out=port.step(in);
    assert(!out.request_ready&&!out.response_valid&&!out.start);
    in.reset=false; in.request_valid=true;
    in.request=request<W>(Operation::Write,12,0xa5);
    out=port.step(in);
    assert(!out.request_ready&&out.request.address==bits(32,12));
    in.backend_ready=true;
    out=port.step(in); // Live bypass, no edge or extra SETUP cycle.
    assert(out.start&&out.request_ready&&out.request.data==bits(W,0xa5));
    in.rising_edge=true; out=port.step(in);
    assert(!out.start&&!out.request_ready);
    in.rising_edge=false; in.request_valid=false; in.backend_ready=false;
    in.request=request<W>(Operation::Read,0);
    in.done=true; in.result=bits(W,0x5a); in.status=bits(3,5);
    out=port.step(in);
    assert(out.response_valid&&out.finish&&out.read_data==bits(W,0x5a));
    assert(out.request.address==bits(32,0)); // Sequencer, not frontend, owns request.
    in.rising_edge=true; port.step(in); // Retain blocked result.
    in.done=false; in.result.assign(W,X); in.status.assign(3,X);
    for (unsigned i=0;i<4;++i) {
        out=port.step(in);
        assert(out.response_valid&&out.read_data==bits(W,0x5a)&&out.status==bits(3,5));
    }
    in.rising_edge=false; in.backend_ready=true; in.response_ready=true;
    in.request_valid=true; out=port.step(in);
    assert(out.request_ready&&out.start&&out.response_valid);
    in.rising_edge=true; out=port.step(in); // Consume old response + accept new.
    assert(!out.request_ready&&!out.response_valid);
    in.reset=true; out=port.step(in);
    assert(!out.request_ready&&!out.response_valid&&!out.start);

    memory_example::ResponseBuffer<W> buffer;
    typename memory_example::ResponseBuffer<W>::Inputs b;
    b.valid=true;b.ready=true;b.data=bits(W,0x31);
    auto view=buffer.outputs(b);
    assert(view.ready&&view.valid&&view.data==b.data);
    b.edge=true; buffer.step(b); // Empty bypass consumes without filling.
    b.valid=false; assert(!buffer.outputs(b).valid);
    b.ready=false;b.valid=true;buffer.step(b);
    b.data=bits(W,0x42);b.ready=true;
    assert(buffer.outputs(b).data==bits(W,0x31)); // Old result wins.
    buffer.step(b);b.valid=false;b.ready=false;
    assert(buffer.outputs(b).data==bits(W,0x42)&&buffer.outputs(b).valid);
}

struct RelayModel { static constexpr auto technology = Technology::Container; };

template<std::size_t W, class Model>
struct Device {
    std::unique_ptr<DeviceModel> model;
    explicit Device(EndpointConfig config) : model(makeDeviceModel(W, Model::technology, config)) {}
    void update(std::size_t time, const Signals& pins) { model->update(time, pins); }
    Bits read(const std::string& pin, std::size_t width) const { return model->read(pin, width); }
    std::size_t deadline() const { return model->deadline(); }
};

template<std::size_t W>
struct Device<W, RelayModel> {
    memory_example::CompactEndpoint<W, SRAMModel> model;
    typename memory_example::CompactEndpoint<W, SRAMModel>::Outputs out;
    bool previous = false;
    explicit Device(EndpointConfig config) : model(config) {}
    void update(std::size_t time, const Signals& pins) {
        typename memory_example::CompactEndpoint<W, SRAMModel>::Inputs in;
        const bool clock = high(pins, "CLK");
        in.rising_edge = clock && !previous; previous = clock;
        in.reset = high(pins, "RST"); in.request_valid = high(pins, "REQ_VALID");
        in.response_ready = high(pins, "RSP_READY");
        in.request = {signal(pins, "REQ_OP", 3), signal(pins, "REQ_ADDR", 32),
                      signal(pins, "REQ_WRITE_DATA", W), signal(pins, "REQ_WRITE_MASK", W / 8)};
        out = model.step(time, in);
    }
    Bits read(const std::string& pin, std::size_t) const {
        if (pin == "REQ_READY") return {out.request_ready ? H : L};
        if (pin == "RSP_VALID") return {out.response_valid ? H : L};
        if (pin == "RSP_READ_DATA") return out.read_data;
        assert(pin == "RSP_STATUS"); return bits(3, unsigned(out.status));
    }
    std::size_t deadline() const { return model.next_event_time(); }
};

template<std::size_t W, class Model>
struct ControllerRig {
    memory_example::Controller<W> controller;
    Device<W, Model> device; // Separate storage, never part of Controller.
    typename memory_example::Controller<W>::Inputs in;
    typename memory_example::Port<W>::Outputs out;
    std::size_t time = 0, next_clock = 64;
    unsigned native_accesses = 0;

    explicit ControllerRig(EndpointConfig config = {})
        : controller(Model::technology, config), device(config) {
        in.reset = true; evaluate(); in.reset = false;
        time = 1; evaluate();
    }
    void evaluate() {
        device.update(time, controller.native_pins());
        Signals sampled;
        if constexpr (Model::technology == Technology::Container) {
            // The clock bypasses the controller's registered control updates.
            // Both endpoints sample the pre-edge handshake, as physical FFs do.
            for (auto [pin, width] : std::vector<std::pair<std::string, std::size_t>>{
                    {"REQ_READY", 1}, {"RSP_VALID", 1}, {"RSP_READ_DATA", W}, {"RSP_STATUS", 3}})
                sampled[pin] = device.read(pin, width);
            auto clocked = controller.native_pins();
            clocked["CLK"] = {in.clock ? H : L};
            device.update(time, clocked);
        }
        out = controller.step(time, in, [&](const std::string& pin, std::size_t width) {
            if constexpr (Model::technology == Technology::Container) return signal(sampled, pin, width);
            else {
                if constexpr (Model::technology == Technology::FF) device.update(time, controller.native_pins());
                return device.read(pin, width);
            }
        });
        const auto& pins = controller.native_pins();
        if (high(pins, "WE") || high(pins, "READ_EN") || !high(pins, "CE_N") ||
            number(signal(pins, "CMD", 3)) || high(pins, "REQ_VALID")) ++native_accesses;
        device.update(time, pins);
    }
    void tick() {
        auto next = next_clock;
        for (auto due : {controller.next_event_time(), device.deadline()}) {
            if (due) { assert(due > time); next = std::min(next, due); }
        }
        time = next;
        if (time == next_clock) { in.clock = !in.clock; next_clock += 64; }
        evaluate();
    }
    template<class Predicate>
    void until(Predicate predicate) {
        for (unsigned i = 0; !predicate(); ++i) { assert(i < 20000); tick(); }
    }
    auto transact(const Request& payload) {
        until([&] { return out.request_ready; });
        in.request_valid = true; in.request = payload;
        until([&] { return !out.request_ready; });
        in.request_valid = false;
        in.request = request<W>(Operation::EraseBlock, 0xffffffffu);
        until([&] { return out.response_valid; });
        auto result = out;
        for (unsigned i = 0; i < 4; ++i) {
            tick();
            assert(out.response_valid && out.read_data == result.read_data && out.status == result.status);
        }
        in.response_ready = true;
        until([&] { return !out.response_valid; });
        in.response_ready = false;
        return result;
    }
    void expect(const Request& payload, Status status, Bits data = Bits(W, L)) {
        auto result = transact(payload);
        assert(result.status == bits(3, unsigned(status)) && result.read_data == data);
    }
};

template<std::size_t W, class Model>
void check_controller() {
    ControllerRig<W, Model> rig;
    auto before = rig.native_accesses;
    rig.expect(request<W>(Operation::Read, 4 * (W / 8)), Status::Range);
    if constexpr (Model::technology != Technology::DRAM) {
        assert(before == rig.native_accesses); // Reject before native access/relay.
    }
    if constexpr (Model::technology == Technology::ROM) {
        rig.expect(request<W>(Operation::Read, 0), Status::Ok);
        rig.expect(request<W>(Operation::Write, 0, 0xa5), Status::ReadOnly);
    } else {
        constexpr bool flash = Model::technology == Technology::NORFlash;
        auto op = flash ? Operation::Program : Operation::Write;
        rig.expect(request<W>(op, 0, 0xa5), Status::Ok);
        rig.expect(request<W>(Operation::Read, 0), Status::Ok, bits(W, 0xa5));
        rig.expect(request<W>(op, 3 * (W / 8), 0x5a), Status::Ok);
        rig.expect(request<W>(Operation::Read, 3 * (W / 8)), Status::Ok, bits(W, 0x5a));
        if constexpr (flash) {
            rig.expect(request<W>(Operation::EraseBlock, 0), Status::Ok);
            rig.expect(request<W>(Operation::Read, 0), Status::Ok, Bits(W, H));
            rig.expect(request<W>(Operation::Read, 3 * (W / 8)), Status::Ok, bits(W, 0x5a));
        }
    }
    rig.expect(request<W>(Operation(7), 0), Status::Unsupported);
}

void check_controller_reset_drains_program() {
    EndpointConfig config;
    config.program_ticks = 20000; config.erase_ticks = 30000;
    ControllerRig<8, NORFlashModel> rig(config);
    rig.until([&] { return rig.out.request_ready; });
    rig.in.request = request<8>(Operation::Program, 0, 0x5a);
    rig.in.request_valid = true;
    rig.until([&] { return !rig.out.request_ready; });
    rig.in.request_valid = false;
    auto target = rig.time + 2048;
    rig.until([&] { return rig.time >= target; });
    assert(rig.device.model->storage.readLogic(0, 1) == Bits(8, H));
    rig.in.reset = true; rig.tick();
    rig.in.reset = false;
    rig.until([&] { assert(!rig.out.response_valid); return rig.out.request_ready; });
    rig.expect(request<8>(Operation::Read, 0), Status::Ok, bits(8, 0x5a));
}

void check_endpoint_construction() {
    auto endpoint = make_sram_endpoint();
    assert(endpoint->getChildren().size() == 2);
    std::shared_ptr<IOComponent> controller, memory;
    for (const auto& child : endpoint->getChildren()) {
        if (child->getName() == "CONTROLLER") controller = std::dynamic_pointer_cast<IOComponent>(child);
        if (child->getName() == "MEMORY") memory = std::dynamic_pointer_cast<IOComponent>(child);
    }
    assert(controller && memory);
    for (const auto& [name, pin] : controller->getAllOutputPins())
        if (name.starts_with("MEM_"))
            assert(pin->getExternalWireBase() && pin->getExternalWireBase() ==
                   memory->getInputPinDynamic(name.substr(4))->getExternalWireBase());
    for (const auto& [name, pin] : controller->getAllInputPins())
        if (name.starts_with("MEM_"))
            assert(pin->getExternalWireBase() && pin->getExternalWireBase() ==
                   memory->getOutputPinDynamic(name.substr(4))->getExternalWireBase());
}

template<std::size_t W, class Model>
void check_endpoint_reference() {
    memory_example::Endpoint<W, Device<W, Model>> endpoint(Model::technology, EndpointConfig{});
    typename memory_example::Controller<W>::Inputs in;
    in.reset = true;
    auto out = endpoint.step(0, in);
    in.reset = false;
    std::size_t time = 0, clock_due = 64;
    auto tick = [&] {
        auto due = endpoint.next_event_time();
        auto next = due ? std::min(due, clock_due) : clock_due;
        assert(next > time); time = next;
        if (time == clock_due) { in.clock = !in.clock; clock_due += 64; }
        out = endpoint.step(time, in);
    };
    auto until = [&](auto predicate) {
        for (unsigned i = 0; !predicate(); ++i) { assert(i < 20000); tick(); }
    };
    until([&] { return out.request_ready; });
    in.request = request<W>(Operation::Read, 0); in.request_valid = true;
    until([&] { return !out.request_ready; }); in.request_valid = false;
    until([&] { return out.response_valid; });
    assert(out.status == bits(3, 0));
    const auto expected = out.read_data;
    for (unsigned i = 0; i < 20; ++i) { tick(); assert(out.response_valid && out.read_data == expected); }
    in.response_ready = true;
    until([&] { return !out.response_valid; });
}

template<std::size_t W>
void check_sequence_against_simulator() {
    std::mt19937 random(518 + W);
    for (auto technology : {Technology::FF, Technology::SRAM, Technology::DRAM,
                           Technology::ROM, Technology::NORFlash, Technology::Container}) {
        EndpointConfig config;
        config.refresh_interval_ticks = 1024;
        memory_example::Sequence reference(W, technology, config);
        NativeSequence actual(W, technology, config);
        Request req = request<W>(Operation::Read, 0);
        // Every call uses identical externally supplied feedback; neither
        // implementation can obtain expected outputs from the other.
        for (std::size_t time = 0; time < 24000; ++time) {
            if (time % 71 == 0) {
                req = request<W>(Operation(random() % 8), random() % (W + 3), random());
                if (random() % 7 == 0) req.op[0] = X;
                if (random() % 7 == 0) req.address[0] = X;
                if (random() % 7 == 0) req.mask[0] = X;
            }
            bool edge = time % 64 == 0, reset = time % 997 == 0, start = time % 239 < 120;
            Signals feedback;
            feedback["BUSY"] = {time % 19 == 0 ? H : L};
            feedback["ERROR"] = {time % 31 == 0 ? H : L};
            feedback["FAULT"] = {time % 43 == 0 ? X : L};
            feedback["READ_VALID"] = {time % 13 ? H : L};
            feedback["READ_DATA"] = feedback["RSP_READ_DATA"] = bits(W, random());
            feedback["REQ_READY"] = {time % 7 ? H : L};
            feedback["RSP_VALID"] = {time % 11 ? H : L};
            feedback["RSP_STATUS"] = bits(3, random() % 8);
            auto read = [&](const std::string& pin, std::size_t width) { return signal(feedback, pin, width); };
            const bool finish_response=time%17!=0;
            reference.step(time, edge, reset, start, req, read, finish_response);
            actual.step(time, edge, reset, start, req, read, finish_response);
            assert(reference.outputs == actual.outputs && reference.result == actual.result);
            assert(reference.available == actual.available && reference.done == actual.done);
            assert(reference.status == actual.status && reference.deadline() == actual.deadline());
        }
    }
}

template<std::size_t W>
void check_width() {
    check_port<W>();
    check_controller<W, FFModel>(); check_controller<W, SRAMModel>();
    check_controller<W, DRAMModel>(); check_controller<W, ROMModel>();
    check_controller<W, NORFlashModel>(); check_controller<W, RelayModel>();
    check_endpoint_reference<W, FFModel>(); check_endpoint_reference<W, SRAMModel>();
    check_endpoint_reference<W, DRAMModel>(); check_endpoint_reference<W, ROMModel>();
    check_endpoint_reference<W, NORFlashModel>(); check_endpoint_reference<W, RelayModel>();
    check_sequence_against_simulator<W>();
}
} // namespace

int main() {
    check_width<8>(); check_width<16>(); check_width<32>();
    check_controller_reset_drains_program();
    check_endpoint_construction();
    std::cout << "3.2/3.3 excerpts: frontend, six controllers/endpoints x 8/16/32, wiring, and 432000 sequencer comparisons passed\n";
}
