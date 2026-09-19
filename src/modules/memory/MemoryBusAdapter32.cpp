#include "modules/memory/MemoryBusAdapter32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"

void defineMemoryBusClientPins(IOComponent* self) {
    for (auto name : {"CLK", "RST", "REQ_VALID", "REQ_WRITE", "RSP_READY"}) self->addPin(name, PinType::INPUT);
    self->addPin<32>("REQ_ADDR", PinType::INPUT);
    self->addPin<32>("REQ_WRITE_DATA", PinType::INPUT);
    self->addPin<4>("REQ_WRITE_MASK", PinType::INPUT);
    for (auto name : {"REQ_READY", "RSP_VALID", "RSP_FAULT"}) self->addPin(name, PinType::OUTPUT);
    self->addPin<32>("RSP_READ_DATA", PinType::OUTPUT);
}
MemoryBusAdapter32::MemoryBusAdapter32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        defineMemoryBusClientPins(self);
        for (auto name : {"WORD_ADDR", "WRITE_DATA"}) self->addPin<32>(name, PinType::OUTPUT);
        self->addPin<4>("WRITE_MASK", PinType::OUTPUT);
        self->addPin("WE", PinType::OUTPUT);
        self->addPin("READ_EN", PinType::OUTPUT);
        self->addPin<32>("MEMORY_READ_DATA", PinType::INPUT);
        self->addPin("MEMORY_FAULT", PinType::INPUT);
    }) {}

void MemoryBusAdapter32::buildInternals(ComponentBuilder& b) {
    using PinPtr = std::shared_ptr<PinBase>;
    // Aggregate fanout: a pin has exactly one wire in each hierarchy scope.
    std::map<std::string, std::pair<PinPtr, std::vector<PinPtr>>> nets;
    auto connect = [&](PinPtr source, PinPtr sink) {
        auto& net = nets[source->getID()]; net.first = source; net.second.push_back(sink);
    };
    auto input = [&](const char* name) -> PinPtr { return getInputPinDynamic(name); };
    auto output = [&](const char* name) -> PinPtr { return getOutputPinDynamic(name); };
    auto inv = [&](const std::string& name, PinPtr a) -> PinPtr {
        auto g = b.addNewComponent<NOTGate>(name);
        connect(a, g->getInputPin("IN")); return g->getOutputPin("OUT");
    };
    auto both = [&](const std::string& name, PinPtr a, PinPtr c) -> PinPtr {
        auto g = b.addNewComponent<ANDGate>(name);
        connect(a, g->getInputPin("A")); connect(c, g->getInputPin("B")); return g->getOutputPin("OUT");
    };
    auto either = [&](const std::string& name, PinPtr a, PinPtr c) -> PinPtr {
        auto g = b.addNewComponent<ORGate>(name);
        connect(a, g->getInputPin("A")); connect(c, g->getInputPin("B")); return g->getOutputPin("OUT");
    };
    auto zero = b.addNewComponent<ConstantValue<1>>("ZERO", 0)->getOutputPin("OUT");
    auto one = b.addNewComponent<ConstantValue<1>>("ONE", 1)->getOutputPin("OUT");
    auto cell = [&](const std::string& name, PinPtr enable) {
        auto r = b.add(circuit::families::FlipFlopCell, name);
        connect(enable, r->getInputPin("WE")); connect(input("CLK"), r->getInputPin("CLK"));
        connect(input("RST"), r->getInputPin("RST")); return r;
    };
    auto q0 = cell("PHASE_0", one), q1 = cell("PHASE_1", one);
    auto lo = q0->getOutputPin("Q"), hi = q1->getOutputPin("Q");
    auto nlo = inv("NOT_PHASE_0", lo), nhi = inv("NOT_PHASE_1", hi);
    auto idle = both("IDLE", nhi, nlo), setup = both("SETUP", nhi, lo);
    auto access = both("ACCESS", hi, nlo), response = both("RESPONSE", hi, lo);
    auto running = inv("NOT_RESET", input("RST"));
    auto ready = both("REQUEST_READY", idle, running);
    auto accept = both("ACCEPT", ready, input("REQ_VALID"));
    auto hold = both("HOLD_RESPONSE", response, inv("NOT_RESPONSE_READY", input("RSP_READY")));
    connect(either("NEXT_0", accept, either("NEXT_0_BUSY", access, hold)), q0->getInputPin("D"));
    connect(either("NEXT_1", setup, either("NEXT_1_BUSY", access, hold)), q1->getInputPin("D"));
    connect(ready, output("REQ_READY"));
    connect(both("RESPONSE_VALID", response, running), output("RSP_VALID"));
    auto wordRegister = [&](const std::string& name, PinPtr data, PinPtr enable) {
        auto r = b.add(circuit::families::Register32, name);
        connect(data, r->getInputPin<32>("D")); connect(enable, r->getInputPin("WE"));
        connect(input("CLK"), r->getInputPin("CLK")); connect(input("RST"), r->getInputPin("RST")); return r;
    };
    auto address = wordRegister("REQUEST_ADDRESS", input("REQ_ADDR"), accept);
    auto data = wordRegister("REQUEST_DATA", input("REQ_WRITE_DATA"), accept);
    connect(data->getOutputPin<32>("Q"), output("WRITE_DATA"));
    auto write = cell("REQUEST_WRITE", accept);
    connect(input("REQ_WRITE"), write->getInputPin("D"));
    auto maskIn = b.addNewComponent<BitSplitter<4>>("MASK_IN");
    auto maskOut = b.addNewComponent<BitJoiner<4>>("MASK_OUT");
    connect(input("REQ_WRITE_MASK"), maskIn->getInputPin<4>("IN"));
    for (size_t bit = 0; bit < 4; ++bit) {
        auto mask = cell("REQUEST_MASK_" + std::to_string(bit), accept);
        connect(maskIn->getOutputPin("OUT_" + std::to_string(bit)), mask->getInputPin("D"));
        connect(mask->getOutputPin("Q"), maskOut->getInputPin("IN_" + std::to_string(bit)));
    }
    connect(maskOut->getOutputPin<4>("OUT"), output("WRITE_MASK"));
    auto addrIn = b.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
    auto wordOut = b.addNewComponent<BitJoiner<32>>("WORD_ADDRESS");
    connect(address->getOutputPin<32>("Q"), addrIn->getInputPin<32>("IN"));
    for (size_t bit = 0; bit < 32; ++bit)
        connect(bit < 30 ? addrIn->getOutputPin("OUT_" + std::to_string(bit + 2)) : zero,
                wordOut->getInputPin("IN_" + std::to_string(bit)));
    connect(wordOut->getOutputPin<32>("OUT"), output("WORD_ADDR"));
    auto misaligned = either("MISALIGNED", addrIn->getOutputPin("OUT_0"), addrIn->getOutputPin("OUT_1"));
    auto aligned = inv("ALIGNED", misaligned);
    auto enabled = both("ACCESS_ENABLED", both("ACCESS_RUNNING", access, running), aligned);
    auto writing = write->getOutputPin("Q");
    auto reading = inv("REQUEST_READ", writing);
    connect(both("MEMORY_WRITE_ENABLE", enabled, writing), output("WE"));
    connect(both("MEMORY_READ_ENABLE", enabled, reading), output("READ_EN"));
    auto fault = cell("RESPONSE_FAULT", access);
    connect(either("ACCESS_FAULT", misaligned, input("MEMORY_FAULT")), fault->getInputPin("D"));
    connect(fault->getOutputPin("Q"), output("RSP_FAULT"));
    auto resultIn = b.addNewComponent<BitSplitter<32>>("RESULT_BITS");
    auto resultOut = b.addNewComponent<BitJoiner<32>>("RESULT_WORD");
    connect(input("MEMORY_READ_DATA"), resultIn->getInputPin<32>("IN"));
    auto returnData = both("RETURN_READ_DATA", aligned, reading);
    for (size_t bit = 0; bit < 32; ++bit)
        connect(both("RESULT_BIT_" + std::to_string(bit), returnData, resultIn->getOutputPin("OUT_" + std::to_string(bit))),
                resultOut->getInputPin("IN_" + std::to_string(bit)));
    auto result = wordRegister("RESPONSE_DATA", resultOut->getOutputPin<32>("OUT"), access);
    connect(result->getOutputPin<32>("Q"), output("RSP_READ_DATA"));
    size_t index = 0;
    for (const auto& [id, net] : nets)
        b.addNewWireDynamic("net_" + std::to_string(index++), net.first->getWidth(), net.first, net.second);
}
