#include "modules/memory/TwoPortMemoryArbiter.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/basic/Gate.hpp"
#include <bit>
#include <map>
#include <stdexcept>
#include "MemoryHandshakeWiring.hpp"

namespace circuit::memory {

template<size_t Width>
TwoPortMemoryArbiter<Width>::TwoPortMemoryArbiter(std::string name, size_t block_words)
    : IOComponent(std::move(name), [](IOComponent* self) {
          protocolPins(self, Width, "I_");
          protocolPins(self, Width, "D_");
          protocolPins(self, Width, "MEM_", true);
      }), block_words_(block_words), type_("TwoPortMemoryArbiter<" + std::to_string(Width) + ">") {
    static_assert(Width == 8 || Width == 16 || Width == 32);
    if (!std::has_single_bit(block_words_) || block_words_ > 16)
        throw std::invalid_argument("Arbiter burst block must be a power of two up to 16 words");
}

template<size_t Width>
void TwoPortMemoryArbiter<Width>::buildInternals(ComponentBuilder& builder) {
    memory_detail::StructuralWiring w(builder);
    auto busy = builder.add(circuit::families::ClockedStateBit, "BUSY");
    auto owner_d = builder.add(circuit::families::ClockedStateBit, "OWNER_D");
    auto prefer_d = builder.add(circuit::families::ClockedStateBit, "PREFER_D");
    // Both requesters run on one clock. The state is clocked straight from
    // I_CLK: a gate in the clock path (such as an AND of both clocks) would
    // delay this part's edge behind the requesters' (clock skew). D_CLK must
    // carry the same clock and is not read.
    auto shared_clock = getInputPin("I_CLK");
    auto reset = w.either("RESET", getInputPin("I_RST"), getInputPin("D_RST"));
    for (auto state : {busy, owner_d, prefer_d}) {
        w.connect(shared_clock, state->getInputPin("CLK"));
        w.connect(reset, state->getInputPin("RST"));
    }
    w.connect(memory_detail::bridge(builder, w, "LOWER_CLOCK", shared_clock),
              getOutputPin("MEM_CLK"));
    w.connect(memory_detail::bridge(builder, w, "LOWER_RESET", reset),
              getOutputPin("MEM_RST"));

    auto active = busy->getOutputPin("Q");
    auto idle = w.invert("IDLE", active);
    auto d_wants = getInputPin("D_REQ_VALID");
    auto i_wants = getInputPin("I_REQ_VALID");
    auto select_d = w.both("SELECT_D", d_wants,
        w.either("D_PRIORITY_OR_I_IDLE", prefer_d->getOutputPin("Q"),
                 w.invert("I_IDLE", i_wants)));
    auto select_i = w.invert("SELECT_I", select_d);
    auto offered = w.both("LOWER_OFFER", idle, w.either("ANY_REQUEST", i_wants, d_wants));
    w.connect(offered, getOutputPin("MEM_REQ_VALID"));
    memory_detail::StructuralWiring::PinPtr selected_op;
    for (auto field : {"REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK"}) {
        auto selected = memory_detail::payloadMux(builder, w, std::string("SELECT_") + field,
            getInputPinDynamic(std::string("I_") + field),
            getInputPinDynamic(std::string("D_") + field), select_d);
        w.connect(selected, getOutputPinDynamic(std::string("MEM_") + field));
        if (std::string(field) == "REQ_OP") selected_op = selected;
    }
    auto lower_ready = getInputPin("MEM_REQ_READY");
    w.connect(w.reduce("I_REQ_READY", {idle, select_i, lower_ready}, true),
              getOutputPin("I_REQ_READY"));
    w.connect(w.reduce("D_REQ_READY", {idle, select_d, lower_ready}, true),
              getOutputPin("D_REQ_READY"));
    auto accepted = w.both("ACCEPT", offered, lower_ready);
    // Each port has a one-entry response buffer (bypassed when the requester
    // is ready). A response leaves the bus into its owner's buffer whenever
    // that buffer is empty, whatever the requester does: a requester that is
    // not taking responses (a core stalled on its other port) cannot hold the
    // bus, and its RSP_READY does not reach the memory in the same cycle.
    std::map<std::string, std::shared_ptr<MemoryResponseBuffer>> buffers;
    for (const std::string port : {"I", "D"}) {
        auto buffer = builder.addNewComponent<MemoryResponseBuffer>(port + "_RESPONSE", Width);
        w.connect(shared_clock, buffer->getInputPin("CLK"));
        w.connect(reset, buffer->getInputPin("RST"));
        w.connect(getInputPin(port + "_RSP_READY"), buffer->getInputPin("OUT_READY"));
        buffers[port] = buffer;
    }
    auto owner_empty = memory_detail::payloadMux(builder, w, "OWNER_BUFFER_EMPTY",
        w.invert("I_BUFFER_EMPTY", buffers["I"]->getOutputPin("FULL")),
        w.invert("D_BUFFER_EMPTY", buffers["D"]->getOutputPin("FULL")), owner_d->getOutputPin("Q"));
    auto lower_response_ready = w.both("LOWER_RESPONSE_READY", active, owner_empty);
    w.connect(lower_response_ready, getOutputPin("MEM_RSP_READY"));
    auto complete = w.both("COMPLETE", lower_response_ready, getInputPin("MEM_RSP_VALID"));
    // A granted burst read (op 4) ends with its last word: BURST is held for
    // the grant, BEAT_i counts the words passed on.
    auto op_bits = builder.addNewComponent<BitSplitter<3>>("SELECTED_OP_BITS");
    w.connect(selected_op, op_bits->getInputPin<3>("IN"));
    auto burst_request = w.reduce("BURST_REQUEST", {w.invert("NOT_OP_0", op_bits->getOutputPin("OUT_0")),
        w.invert("NOT_OP_1", op_bits->getOutputPin("OUT_1")), op_bits->getOutputPin("OUT_2")}, true);
    auto burst = builder.add(circuit::families::ClockedStateBit, "BURST");
    std::vector<memory_detail::StructuralWiring::PinPtr> beat;
    std::vector<std::shared_ptr<IOComponent>> beat_bits;
    const size_t beat_width = std::countr_zero(block_words_);
    for (size_t i = 0; i < beat_width; ++i) {
        beat_bits.push_back(builder.add(circuit::families::ClockedStateBit, "BEAT_" + std::to_string(i)));
        beat.push_back(beat_bits.back()->getOutputPin("Q"));
    }
    for (auto state : beat_bits) { w.connect(shared_clock, state->getInputPin("CLK")); w.connect(reset, state->getInputPin("RST")); }
    w.connect(shared_clock, burst->getInputPin("CLK"));
    w.connect(reset, burst->getInputPin("RST"));
    auto last_beat = beat_width ? w.reduce("LAST_BEAT", beat, true) : w.constant("ONE_BEAT", true);
    auto final_complete = w.both("FINAL_COMPLETE", complete,
        w.either("SINGLE_OR_LAST", w.invert("NOT_BURST", burst->getOutputPin("Q")), last_beat));
    w.connect(w.either("NEXT_BURST", w.both("ACCEPT_BURST", accepted, burst_request),
        w.both("HOLD_BURST", burst->getOutputPin("Q"), w.invert("NOT_ACCEPTING", accepted))), burst->getInputPin("D"));
    // BEAT counts completions of a burst and returns to 0 with its last word.
    memory_detail::StructuralWiring::PinPtr carry = w.both("BEAT_STEP", complete, burst->getOutputPin("Q"));
    for (size_t i = 0; i < beat_width; ++i) {
        const auto n = std::to_string(i);
        auto next = w.binary<XORGate>("BEAT_NEXT_" + n, beat[i], carry);
        carry = w.both("BEAT_CARRY_" + n, beat[i], carry);
        w.connect(w.both("BEAT_KEEP_" + n, next, w.invert("BEAT_NOT_ACCEPT_" + n, accepted)), beat_bits[i]->getInputPin("D"));
    }
    w.connect(w.either("NEXT_BUSY", accepted,
        w.both("HOLD_BUSY", active, w.invert("NOT_COMPLETE", final_complete))),
        busy->getInputPin("D"));
    w.connect(w.either("NEXT_OWNER_D", w.both("ACCEPT_D", accepted, select_d),
        w.both("HOLD_OWNER_D", owner_d->getOutputPin("Q"),
               w.invert("NOT_ACCEPT", accepted))), owner_d->getInputPin("D"));
    w.connect(w.either("NEXT_PREFER_D", w.both("ACCEPT_I", accepted, select_i),
        w.both("HOLD_PREFER_D", prefer_d->getOutputPin("Q"),
               w.invert("NOT_ACCEPT_FOR_PRIORITY", accepted))), prefer_d->getInputPin("D"));

    auto i_response = w.reduce("I_RESPONSE", {lower_response_ready,
        w.invert("OWNER_I", owner_d->getOutputPin("Q")), getInputPin("MEM_RSP_VALID")}, true);
    auto d_response = w.reduce("D_RESPONSE", {lower_response_ready, owner_d->getOutputPin("Q"),
        getInputPin("MEM_RSP_VALID")}, true);
    for (const auto& [port, incoming] : {std::pair<std::string, memory_detail::StructuralWiring::PinPtr>{"I", i_response}, {"D", d_response}}) {
        auto buffer = buffers[port];
        w.connect(incoming, buffer->getInputPin("IN_VALID"));
        w.connect(getInputPinDynamic("MEM_RSP_READ_DATA"), buffer->getInputPinDynamic("IN_DATA"));
        w.connect(this->template getInputPin<3>("MEM_RSP_STATUS"), buffer->template getInputPin<3>("IN_STATUS"));
        w.connect(buffer->getOutputPin("OUT_VALID"), getOutputPin(port + "_RSP_VALID"));
        w.connect(buffer->getOutputPinDynamic("OUT_DATA"), getOutputPinDynamic(port + "_RSP_READ_DATA"));
        w.connect(buffer->template getOutputPin<3>("OUT_STATUS"), this->template getOutputPin<3>(port + "_RSP_STATUS"));
    }
    w.finish();
}

template class TwoPortMemoryArbiter<8>;
template class TwoPortMemoryArbiter<16>;
template class TwoPortMemoryArbiter<32>;

} // namespace circuit::memory
