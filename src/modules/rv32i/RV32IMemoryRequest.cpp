#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/utility/Constant.hpp"
#include "../memory/MemoryHandshakeWiring.hpp"
#include <tuple>

using namespace circuit;
using circuit::detail::StructuralWiring;

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

RV32IMemoryRequest::RV32IMemoryRequest(std::string name, bool read_only, bool address_is_tag)
    : IOComponent(std::move(name), [read_only, address_is_tag](IOComponent* s) {
        for (auto n : {"CLK", "RST", "ISSUE", "TAKE"}) s->addPin(n, PinType::INPUT);
        if (!address_is_tag) s->addPin<32>("TAG", PinType::INPUT);
        s->addPin<32>("ADDR", PinType::INPUT);
        if (!read_only) {
            s->addPin<32>("WRITE_DATA", PinType::INPUT);
            s->addPin<3>("OP", PinType::INPUT);
            s->addPin<4>("WRITE_MASK", PinType::INPUT);
        }
        for (auto n : {"DONE", "FAULT"}) s->addPin(n, PinType::OUTPUT);
        s->addPin<32>("READ_DATA", PinType::OUTPUT);
        rv32i::memoryRequestPins(s, "BUS_");
    }), read_only_(read_only), address_is_tag_(address_is_tag) {}

void RV32IMemoryRequest::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto state = [&](const char* name) {
        auto cell = b.add(families::ClockedStateBit, name);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), cell->getInputPin(p));
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
        if (address_is_tag_ && input == "TAG") continue;
        if (read_only_ && (input == "OP" || input == "WRITE_DATA" || input == "WRITE_MASK")) continue;
        auto reg = b.add(families::registerWord(width), "REQUEST_"+input);
        for (auto p : {"CLK", "RST"}) w.connect(getInputPin(p), reg->getInputPin(p));
        w.connect(start, reg->getInputPin("WE"));
        w.connect(getInputPinDynamic(input), reg->getInputPinDynamic("D"));
        if (output.empty() || (address_is_tag_ && input == "ADDR")) tag=reg;
        else w.connect(memory_detail::payloadMux(b,w,"SELECT_"+input,getInputPinDynamic(input),reg->getOutputPinDynamic("Q"),busy->getOutputPin("Q")),getOutputPinDynamic("BUS_"+output));
        if (address_is_tag_ && input == "ADDR")
            w.connect(memory_detail::payloadMux(b,w,"SELECT_"+input,getInputPinDynamic(input),reg->getOutputPinDynamic("Q"),busy->getOutputPin("Q")),getOutputPin<32>("BUS_REQ_ADDR"));
    }
    if (read_only_) {
        auto op = b.addNewComponent<ConstantValue<3>>("READ_OPERATION", 0);
        auto data = b.addNewComponent<ConstantValue<32>>("NO_WRITE_DATA", 0);
        auto mask = b.addNewComponent<ConstantValue<4>>("FULL_READ_MASK", 15);
        w.connect(op->getOutputPin<3>("OUT"), getOutputPin<3>("BUS_REQ_OP"));
        w.connect(data->getOutputPin<32>("OUT"), getOutputPin<32>("BUS_REQ_WRITE_DATA"));
        w.connect(mask->getOutputPin<4>("OUT"), getOutputPin<4>("BUS_REQ_WRITE_MASK"));
    }
    auto compare = b.addNewComponent<RV32IRequestTagMatch>("TAG_MATCH");
    w.connect(getInputPin<32>(address_is_tag_ ? "ADDR" : "TAG"), compare->getInputPin<32>("CURRENT"));
    w.connect(tag->getOutputPin<32>("Q"), compare->getInputPin<32>("SAVED"));
    auto mismatch = compare->getOutputPin("CHANGED");
    auto discard = w.both("DISCARD",busy->getOutputPin("Q"),w.either("STALE", abandoned->getOutputPin("Q"), mismatch));
    auto matching = w.invert("MATCHING", discard);
    auto sending = w.both("SENDING", w.either("REQUEST_PRESENT",busy->getOutputPin("Q"),getInputPin("ISSUE")), unsent);
    auto valid = w.both("VALID", sending, running);
    auto accepted = w.both("ACCEPTED", valid, getInputPin("BUS_REQ_READY"));
    auto consume = w.either("CONSUME", getInputPin("TAKE"), discard);
    auto eligible=w.either("RESPONSE_ELIGIBLE",sent->getOutputPin("Q"),accepted);
    // Prepare the independent request/response qualifications before the late
    // TAKE decision. Reassociation preserves all four-state gate values.
    auto response_present=w.either("RESPONSE_PRESENT",sent->getOutputPin("Q"),sending);
    auto response_running=w.both("RESPONSE_RUNNING",response_present,running);
    auto ready = w.both("READY_RUNNING", response_running, consume);
    auto can_finish=w.both("CAN_FINISH",response_running,getInputPin("BUS_RSP_VALID"));
    auto finished = w.both("FINISHED", can_finish, consume);
    auto unfinished = w.invert("UNFINISHED", finished);
    auto busy_next = w.either("BUSY_NEXT", busy->getOutputPin("Q"), start);
    auto sent_next = w.either("SENT_NEXT", sent->getOutputPin("Q"), accepted);
    auto discard_next = w.both("DISCARD_NEXT", busy->getOutputPin("Q"), discard);
    w.connect(w.both("KEEP_BUSY", busy_next, unfinished), busy->getInputPin("D"));
    w.connect(w.both("KEEP_SENT", sent_next, unfinished), sent->getInputPin("D"));
    w.connect(w.both("KEEP_ABANDONED", discard_next, unfinished), abandoned->getInputPin("D"));
    auto response = w.both("RESPONSE", eligible, getInputPin("BUS_RSP_VALID"));
    auto usable = w.both("USABLE", response, matching);
    w.connect(w.both("DONE_RUNNING", usable, running), getOutputPin("DONE"));
    w.connect(valid, getOutputPin("BUS_REQ_VALID"));
    w.connect(ready, getOutputPin("BUS_RSP_READY"));
    auto status = b.addNewComponent<BitSplitter<3>>("STATUS");
    w.connect(getInputPin<3>("BUS_RSP_STATUS"), status->getInputPin<3>("IN"));
    w.connect(w.reduce("FAULT", {status->getOutputPin("OUT_0"),status->getOutputPin("OUT_1"),status->getOutputPin("OUT_2")},false),getOutputPin("FAULT"));
    // Not a buffered response: the endpoint owns the data until it is taken.
    w.connect(getInputPin<32>("BUS_RSP_READ_DATA"), getOutputPin<32>("READ_DATA"));
    w.finish();
}
