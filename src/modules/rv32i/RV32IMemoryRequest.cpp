#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include <tuple>

using namespace circuit;
using memory_detail::StructuralWiring;

namespace {
// The requester compares all tag bits, not just the current PC's low bits.
// Keep the repeated XOR/reduction network in its own expandable block.
class RV32IRequestTagMatch : public IOComponent {
public:
    explicit RV32IRequestTagMatch(std::string name)
        : IOComponent(std::move(name), [](IOComponent* s) {
            s->addPin<32>("CURRENT", PinType::INPUT);
            s->addPin<32>("SAVED", PinType::INPUT);
            s->addPin("CHANGED", PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override { return "RV32IRequestTagMatch"; }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto current = b.addNewComponent<BitSplitter<32>>("CURRENT_TAG");
        auto saved = b.addNewComponent<BitSplitter<32>>("SAVED_TAG");
        w.connect(getInputPin<32>("CURRENT"), current->getInputPin<32>("IN"));
        w.connect(getInputPin<32>("SAVED"), saved->getInputPin<32>("IN"));
        std::vector<StructuralWiring::PinPtr> differences;
        for (size_t i = 0; i < 32; ++i)
            differences.push_back(w.binary<XORGate>("TAG_DIFFERENT_" + std::to_string(i),
                current->getOutputPin("OUT_" + std::to_string(i)), saved->getOutputPin("OUT_" + std::to_string(i))));
        auto changed = w.reduce("TAG_CHANGED", differences, false);
        w.connect(changed, getOutputPin("CHANGED"));
        w.finish();
    }
};
}

void rv32i::memoryRequestPins(IOComponent* s, const std::string& p) {
    for (auto n : {"REQ_VALID", "RSP_READY"}) s->addPin(p+n, PinType::OUTPUT);
    s->addPin<3>(p+"REQ_OP", PinType::OUTPUT);
    for (auto n : {"REQ_ADDR", "REQ_WRITE_DATA"}) s->addPin<32>(p+n, PinType::OUTPUT);
    s->addPin<4>(p+"REQ_WRITE_MASK", PinType::OUTPUT);
    for (auto n : {"REQ_READY", "RSP_VALID"}) s->addPin(p+n, PinType::INPUT);
    s->addPin<32>(p+"RSP_READ_DATA", PinType::INPUT);
    s->addPin<3>(p+"RSP_STATUS", PinType::INPUT);
}

RV32IMemoryRequest::RV32IMemoryRequest(std::string name)
    : IOComponent(std::move(name), [](IOComponent* s) {
        for (auto n : {"CLK", "RST", "ISSUE", "TAKE"}) s->addPin(n, PinType::INPUT);
        for (auto n : {"TAG", "ADDR", "WRITE_DATA"}) s->addPin<32>(n, PinType::INPUT);
        s->addPin<3>("OP", PinType::INPUT);
        s->addPin<4>("WRITE_MASK", PinType::INPUT);
        for (auto n : {"DONE", "FAULT"}) s->addPin(n, PinType::OUTPUT);
        s->addPin<32>("READ_DATA", PinType::OUTPUT);
        rv32i::memoryRequestPins(s, "BUS_");
    }) {}

void RV32IMemoryRequest::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto one = w.constant("ONE", true);
    auto state = [&](const char* name) {
        auto cell = b.add(families::FlipFlopCell, name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), cell->getInputPin(p));
        w.connect(one, cell->getInputPin("WE"));
        return cell;
    };
    auto busy = state("BUSY"), sent = state("SENT"), abandoned = state("ABANDONED");
    auto running = w.invert("RUNNING", getInputPin("RST"));
    auto idle = w.invert("IDLE", busy->getOutputPin("Q"));
    auto unsent = w.invert("UNSENT", sent->getOutputPin("Q"));
    auto issue_idle = w.both("ISSUE_IDLE", getInputPin("ISSUE"), idle);
    auto start = w.both("START", issue_idle, running);
    std::shared_ptr<IOComponent> tag;
    for (auto [input, output, width] : std::vector<std::tuple<std::string,std::string,size_t>>{
            {"TAG","",32}, {"OP","REQ_OP",3}, {"ADDR","REQ_ADDR",32},
            {"WRITE_DATA","REQ_WRITE_DATA",32}, {"WRITE_MASK","REQ_WRITE_MASK",4}}) {
        auto reg = b.addNewComponent<MemoryPayloadRegister>("REQUEST_"+input, width);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), reg->getInputPin(p));
        w.connect(start, reg->getInputPin("WE"));
        w.connect(getInputPinDynamic(input), reg->getInputPinDynamic("D"));
        if (output.empty()) tag=reg;
        else w.connect(reg->getOutputPinDynamic("Q"), getOutputPinDynamic("BUS_"+output));
    }
    auto compare = b.addNewComponent<RV32IRequestTagMatch>("TAG_MATCH");
    w.connect(getInputPin<32>("TAG"), compare->getInputPin<32>("CURRENT"));
    w.connect(tag->getOutputPin<32>("Q"), compare->getInputPin<32>("SAVED"));
    auto mismatch = compare->getOutputPin("CHANGED");
    auto discard = w.either("DISCARD", abandoned->getOutputPin("Q"), mismatch);
    auto matching = w.invert("MATCHING", discard);
    auto sending = w.both("SENDING", busy->getOutputPin("Q"), unsent);
    auto valid = w.both("VALID", sending, running);
    auto accepted = w.both("ACCEPTED", valid, getInputPin("BUS_REQ_READY"));
    auto consume = w.either("CONSUME", getInputPin("TAKE"), discard);
    auto response_ready = w.both("RESPONSE_READY", sent->getOutputPin("Q"), consume);
    auto ready = w.both("READY_RUNNING", response_ready, running);
    auto finished = w.both("FINISHED", ready, getInputPin("BUS_RSP_VALID"));
    auto unfinished = w.invert("UNFINISHED", finished);
    auto busy_next = w.either("BUSY_NEXT", busy->getOutputPin("Q"), start);
    auto sent_next = w.either("SENT_NEXT", sent->getOutputPin("Q"), accepted);
    auto discard_next = w.both("DISCARD_NEXT", busy->getOutputPin("Q"), discard);
    w.connect(w.both("KEEP_BUSY", busy_next, unfinished), busy->getInputPin("D"));
    w.connect(w.both("KEEP_SENT", sent_next, unfinished), sent->getInputPin("D"));
    w.connect(w.both("KEEP_ABANDONED", discard_next, unfinished), abandoned->getInputPin("D"));
    auto response = w.both("RESPONSE", sent->getOutputPin("Q"), getInputPin("BUS_RSP_VALID"));
    auto usable = w.both("USABLE", response, matching);
    w.connect(w.both("DONE_RUNNING", usable, running), getOutputPin("DONE"));
    w.connect(valid, getOutputPin("BUS_REQ_VALID"));
    w.connect(ready, getOutputPin("BUS_RSP_READY"));
    auto status = b.addNewComponent<BitSplitter<3>>("STATUS");
    w.connect(getInputPin<3>("BUS_RSP_STATUS"), status->getInputPin<3>("IN"));
    w.connect(w.reduce("FAULT", {status->getOutputPin("OUT_0"),status->getOutputPin("OUT_1"),status->getOutputPin("OUT_2")},false),getOutputPin("FAULT"));
    // Explicit boundary bridge, not a buffered response. The endpoint owns it.
    auto data = b.addNewComponent<Rewire>("RESPONSE_DATA", std::vector<Rewire::WireSpec>{{"IN",32}},
        std::vector<Rewire::WireSpec>{{"OUT",32}}, identity_mapping("IN",0,32,"OUT"));
    w.connect(getInputPin<32>("BUS_RSP_READ_DATA"), data->getInputPin<32>("IN"));
    w.connect(data->getOutputPin<32>("OUT"), getOutputPin<32>("READ_DATA"));
    w.finish();
}
