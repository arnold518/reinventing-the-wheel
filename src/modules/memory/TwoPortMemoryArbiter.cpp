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
TwoPortMemoryArbiter<Width>::TwoPortMemoryArbiter(std::string name, size_t block_words, bool pipelined)
    : IOComponent(std::move(name), [](IOComponent* self) {
          protocolPins(self, Width, "I_");
          protocolPins(self, Width, "D_");
          protocolPins(self, Width, "MEM_", true);
      }), block_words_(block_words), pipelined_(pipelined), type_("TwoPortMemoryArbiter<" + std::to_string(Width) + ">") {
    static_assert(Width == 8 || Width == 16 || Width == 32);
    if (!std::has_single_bit(block_words_) || block_words_ > 16)
        throw std::invalid_argument("Arbiter burst block must be a power of two up to 16 words");
}

namespace {
using memory_detail::StructuralWiring;
using PinPtr = StructuralWiring::PinPtr;

// Who is offered to the lower memory next: D when it asks and either has
// priority or I is not asking; I otherwise. A request is offered only while
// the arbiter is idle, and accepted when the lower memory is ready.
class MemoryArbiterGrant : public IOComponent {
public:
    explicit MemoryArbiterGrant(std::string name)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"I_WANTS", "D_WANTS", "PREFER_D", "BUSY", "LOWER_READY"})
                  self->addPin(pin, PinType::INPUT);
              for (auto pin : {"SELECT_D", "OFFER", "I_READY", "D_READY", "ACCEPT"})
                  self->addPin(pin, PinType::OUTPUT);
          }) {}
    const char* getTypeName() const override { return "MemoryArbiterGrant"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto idle = w.invert("IDLE", getInputPin("BUSY"));
        auto d_wants = getInputPin("D_WANTS");
        auto i_wants = getInputPin("I_WANTS");
        auto select_d = w.both("SELECT_D", d_wants,
            w.either("D_PRIORITY_OR_I_IDLE", getInputPin("PREFER_D"), w.invert("I_IDLE", i_wants)));
        auto select_i = w.invert("SELECT_I", select_d);
        auto offered = w.both("LOWER_OFFER", idle, w.either("ANY_REQUEST", i_wants, d_wants));
        auto lower_ready = getInputPin("LOWER_READY");
        w.connect(select_d, getOutputPin("SELECT_D"));
        w.connect(offered, getOutputPin("OFFER"));
        w.connect(w.reduce("I_REQ_READY", {idle, select_i, lower_ready}, true), getOutputPin("I_READY"));
        w.connect(w.reduce("D_REQ_READY", {idle, select_d, lower_ready}, true), getOutputPin("D_READY"));
        w.connect(w.both("ACCEPT", offered, lower_ready), getOutputPin("ACCEPT"));
        w.finish();
    }
};

// Busy, owner and priority bits, and the burst count. Priority goes to the
// port that was not granted last (round robin); a granted burst read (op 4)
// keeps the arbiter busy until its last word.
class MemoryArbiterState : public IOComponent {
public:
    MemoryArbiterState(std::string name, size_t block_words)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "ACCEPT", "SELECT_D", "COMPLETE"})
                  self->addPin(pin, PinType::INPUT);
              self->addPin<3>("OP", PinType::INPUT);
              for (auto pin : {"BUSY", "OWNER_D", "PREFER_D"})
                  self->addPin(pin, PinType::OUTPUT);
          }), block_words_(block_words) {}
    const char* getTypeName() const override { return "MemoryArbiterState"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto clock = getInputPin("CLK");
        auto reset = getInputPin("RST");
        auto bit = [&](const std::string& name) {
            auto state = builder.add(circuit::families::ClockedStateBit, name);
            w.connect(clock, state->getInputPin("CLK"));
            w.connect(reset, state->getInputPin("RST"));
            return state;
        };
        auto busy = bit("BUSY");
        auto owner_d = bit("OWNER_D");
        auto prefer_d = bit("PREFER_D");
        auto burst = bit("BURST");
        auto accepted = getInputPin("ACCEPT");
        auto select_d = getInputPin("SELECT_D");
        auto select_i = w.invert("SELECT_I", select_d);
        auto complete = getInputPin("COMPLETE");
        auto active = busy->getOutputPin("Q");
        auto op_bits = builder.addNewComponent<BitSplitter<3>>("SELECTED_OP_BITS");
        w.connect(getInputPin<3>("OP"), op_bits->getInputPin<3>("IN"));
        auto burst_request = w.reduce("BURST_REQUEST", {w.invert("NOT_OP_0", op_bits->getOutputPin("OUT_0")),
            w.invert("NOT_OP_1", op_bits->getOutputPin("OUT_1")), op_bits->getOutputPin("OUT_2")}, true);
        std::vector<PinPtr> beat;
        std::vector<std::shared_ptr<IOComponent>> beat_bits;
        const size_t beat_width = std::countr_zero(block_words_);
        for (size_t i = 0; i < beat_width; ++i) {
            beat_bits.push_back(bit("BEAT_" + std::to_string(i)));
            beat.push_back(beat_bits.back()->getOutputPin("Q"));
        }
        auto last_beat = beat_width ? w.reduce("LAST_BEAT", beat, true) : w.constant("ONE_BEAT", true);
        auto final_complete = w.both("FINAL_COMPLETE", complete,
            w.either("SINGLE_OR_LAST", w.invert("NOT_BURST", burst->getOutputPin("Q")), last_beat));
        {
            auto hold = w.both("HOLD_BURST", burst->getOutputPin("Q"), w.invert("NOT_ACCEPTING", accepted));
            auto load = w.both("ACCEPT_BURST", accepted, burst_request);
            w.connect(w.either("NEXT_BURST", load, hold), burst->getInputPin("D"));
        }
        // BEAT counts completions of a burst and returns to 0 with its last word.
        PinPtr carry = w.both("BEAT_STEP", complete, burst->getOutputPin("Q"));
        for (size_t i = 0; i < beat_width; ++i) {
            const auto n = std::to_string(i);
            auto next = w.binary<XORGate>("BEAT_NEXT_" + n, beat[i], carry);
            carry = w.both("BEAT_CARRY_" + n, beat[i], carry);
            w.connect(w.both("BEAT_KEEP_" + n, next, w.invert("BEAT_NOT_ACCEPT_" + n, accepted)), beat_bits[i]->getInputPin("D"));
        }
        w.connect(w.either("NEXT_BUSY", accepted,
            w.both("HOLD_BUSY", active, w.invert("NOT_COMPLETE", final_complete))), busy->getInputPin("D"));
        {
            auto hold = w.both("HOLD_OWNER_D", owner_d->getOutputPin("Q"), w.invert("NOT_ACCEPT", accepted));
            auto load = w.both("ACCEPT_D", accepted, select_d);
            w.connect(w.either("NEXT_OWNER_D", load, hold), owner_d->getInputPin("D"));
        }
        {
            auto hold = w.both("HOLD_PREFER_D", prefer_d->getOutputPin("Q"), w.invert("NOT_ACCEPT_FOR_PRIORITY", accepted));
            auto load = w.both("ACCEPT_I", accepted, select_i);
            w.connect(w.either("NEXT_PREFER_D", load, hold), prefer_d->getInputPin("D"));
        }
        w.connect(active, getOutputPin("BUSY"));
        w.connect(owner_d->getOutputPin("Q"), getOutputPin("OWNER_D"));
        w.connect(prefer_d->getOutputPin("Q"), getOutputPin("PREFER_D"));
        w.finish();
    }
private:
    size_t block_words_;
};

// A response leaves the lower memory into its owner's buffer whenever that
// buffer is empty, whatever the requester does: a requester that is not
// taking responses (a core stalled on its other port) cannot hold the bus,
// and its RSP_READY does not reach the memory in the same cycle.
class MemoryArbiterResponseRoute : public IOComponent {
public:
    explicit MemoryArbiterResponseRoute(std::string name)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"BUSY", "OWNER_D", "MEM_RSP_VALID", "I_FULL", "D_FULL"})
                  self->addPin(pin, PinType::INPUT);
              for (auto pin : {"MEM_RSP_READY", "COMPLETE", "I_RESPONSE", "D_RESPONSE"})
                  self->addPin(pin, PinType::OUTPUT);
          }) {}
    const char* getTypeName() const override { return "MemoryArbiterResponseRoute"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto owner_d = getInputPin("OWNER_D");
        auto valid = getInputPin("MEM_RSP_VALID");
        auto d_empty = w.invert("D_BUFFER_EMPTY", getInputPin("D_FULL"));
        auto i_empty = w.invert("I_BUFFER_EMPTY", getInputPin("I_FULL"));
        auto owner_empty = memory_detail::payloadMux(builder, w, "OWNER_BUFFER_EMPTY", i_empty, d_empty, owner_d);
        auto ready = w.both("LOWER_RESPONSE_READY", getInputPin("BUSY"), owner_empty);
        w.connect(ready, getOutputPin("MEM_RSP_READY"));
        w.connect(w.both("COMPLETE", ready, valid), getOutputPin("COMPLETE"));
        w.connect(w.reduce("I_RESPONSE", {ready, w.invert("OWNER_I", owner_d), valid}, true), getOutputPin("I_RESPONSE"));
        w.connect(w.reduce("D_RESPONSE", {ready, owner_d, valid}, true), getOutputPin("D_RESPONSE"));
        w.finish();
    }
};
} // namespace

template<size_t Width>
void TwoPortMemoryArbiter<Width>::buildInternals(ComponentBuilder& builder) {
    if (pipelined_) { buildPipelined(builder); return; }
    StructuralWiring w(builder);
    // Both requesters run on one clock. The state is clocked straight from
    // I_CLK: a gate in the clock path (such as an AND of both clocks) would
    // delay this part's edge behind the requesters' (clock skew). D_CLK must
    // carry the same clock and is not read.
    auto shared_clock = getInputPin("I_CLK");
    auto reset = w.either("RESET", getInputPin("I_RST"), getInputPin("D_RST"));
    w.connect(memory_detail::bridge(builder, w, "LOWER_CLOCK", shared_clock), getOutputPin("MEM_CLK"));
    w.connect(memory_detail::bridge(builder, w, "LOWER_RESET", reset), getOutputPin("MEM_RST"));

    auto grant = builder.addNewComponent<MemoryArbiterGrant>("GRANT");
    auto state = builder.addNewComponent<MemoryArbiterState>("STATE", block_words_);
    auto route = builder.addNewComponent<MemoryArbiterResponseRoute>("RESPONSE_ROUTE");
    w.connect(getInputPin("I_REQ_VALID"), grant->getInputPin("I_WANTS"));
    w.connect(getInputPin("D_REQ_VALID"), grant->getInputPin("D_WANTS"));
    w.connect(getInputPin("MEM_REQ_READY"), grant->getInputPin("LOWER_READY"));
    w.connect(state->getOutputPin("PREFER_D"), grant->getInputPin("PREFER_D"));
    w.connect(state->getOutputPin("BUSY"), grant->getInputPin("BUSY"));
    w.connect(grant->getOutputPin("OFFER"), getOutputPin("MEM_REQ_VALID"));
    w.connect(grant->getOutputPin("I_READY"), getOutputPin("I_REQ_READY"));
    w.connect(grant->getOutputPin("D_READY"), getOutputPin("D_REQ_READY"));

    auto select_d = grant->getOutputPin("SELECT_D");
    for (auto field : {"REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK"}) {
        auto selected = memory_detail::payloadMux(builder, w, std::string("SELECT_") + field,
            getInputPinDynamic(std::string("I_") + field),
            getInputPinDynamic(std::string("D_") + field), select_d);
        w.connect(selected, getOutputPinDynamic(std::string("MEM_") + field));
        if (std::string(field) == "REQ_OP") w.connect(selected, state->template getInputPin<3>("OP"));
    }
    w.connect(shared_clock, state->getInputPin("CLK"));
    w.connect(reset, state->getInputPin("RST"));
    w.connect(grant->getOutputPin("ACCEPT"), state->getInputPin("ACCEPT"));
    w.connect(select_d, state->getInputPin("SELECT_D"));
    w.connect(route->getOutputPin("COMPLETE"), state->getInputPin("COMPLETE"));

    // Each port has a one-entry response buffer, bypassed when its requester
    // is ready.
    w.connect(state->getOutputPin("BUSY"), route->getInputPin("BUSY"));
    w.connect(state->getOutputPin("OWNER_D"), route->getInputPin("OWNER_D"));
    w.connect(getInputPin("MEM_RSP_VALID"), route->getInputPin("MEM_RSP_VALID"));
    w.connect(route->getOutputPin("MEM_RSP_READY"), getOutputPin("MEM_RSP_READY"));
    for (const std::string port : {"I", "D"}) {
        auto buffer = builder.addNewComponent<MemoryResponseBuffer>(port + "_RESPONSE", Width);
        w.connect(shared_clock, buffer->getInputPin("CLK"));
        w.connect(reset, buffer->getInputPin("RST"));
        w.connect(getInputPin(port + "_RSP_READY"), buffer->getInputPin("OUT_READY"));
        w.connect(route->getOutputPin(port + "_RESPONSE"), buffer->getInputPin("IN_VALID"));
        w.connect(buffer->getOutputPin("FULL"), route->getInputPin(port + "_FULL"));
        w.connect(getInputPinDynamic("MEM_RSP_READ_DATA"), buffer->getInputPinDynamic("IN_DATA"));
        w.connect(this->template getInputPin<3>("MEM_RSP_STATUS"), buffer->template getInputPin<3>("IN_STATUS"));
        w.connect(buffer->getOutputPin("OUT_VALID"), getOutputPin(port + "_RSP_VALID"));
        w.connect(buffer->getOutputPinDynamic("OUT_DATA"), getOutputPinDynamic(port + "_RSP_READ_DATA"));
        w.connect(buffer->template getOutputPin<3>("OUT_STATUS"), this->template getOutputPin<3>(port + "_RSP_STATUS"));
    }
    w.finish();
}

// The pipelined arbiter (§ 6.3). The same round-robin choice, burst hold and
// one response buffer per port. Differences:
//  - FREE = idle, or the request in flight completes (its last word) this
//    cycle: a new request is offered and accepted at that same edge;
//  - from idle, MEM_RSP_READY is also high for the request being accepted, so
//    a memory that answers in the same cycle is not held for an edge.
// The answer this cycle belongs to the request in flight when BUSY, else to
// the one being accepted. No signal here depends on itself in one cycle: the
// lower memory's REQ_READY comes from its registered state (§ 3.4).
template<size_t Width>
void TwoPortMemoryArbiter<Width>::buildPipelined(ComponentBuilder& builder) {
    StructuralWiring w(builder);
    auto shared_clock = getInputPin("I_CLK");
    auto reset = w.either("RESET", getInputPin("I_RST"), getInputPin("D_RST"));
    w.connect(memory_detail::bridge(builder, w, "LOWER_CLOCK", shared_clock), getOutputPin("MEM_CLK"));
    w.connect(memory_detail::bridge(builder, w, "LOWER_RESET", reset), getOutputPin("MEM_RST"));
    auto bit = [&](const std::string& name) {
        auto state = builder.add(circuit::families::ClockedStateBit, name);
        w.connect(shared_clock, state->getInputPin("CLK"));
        w.connect(reset, state->getInputPin("RST"));
        return state;
    };
    auto busy = bit("BUSY"), owner_d = bit("OWNER_D"), prefer_d = bit("PREFER_D"), burst = bit("BURST");
    const auto active = busy->getOutputPin("Q");
    const auto i_wants = getInputPin("I_REQ_VALID"), d_wants = getInputPin("D_REQ_VALID");
    const auto select_d = w.both("SELECT_D", d_wants,
        w.either("D_PRIORITY_OR_I_IDLE", prefer_d->getOutputPin("Q"), w.invert("I_IDLE", i_wants)));
    const auto select_i = w.invert("SELECT_I", select_d);
    // Payload of the chosen request.
    std::shared_ptr<PinBase> selected_op;
    for (auto field : {"REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK"}) {
        auto selected = memory_detail::payloadMux(builder, w, std::string("SELECT_") + field,
            getInputPinDynamic(std::string("I_") + field), getInputPinDynamic(std::string("D_") + field), select_d);
        w.connect(selected, getOutputPinDynamic(std::string("MEM_") + field));
        if (std::string(field) == "REQ_OP") selected_op = selected;
    }
    auto op_bits = builder.addNewComponent<BitSplitter<3>>("SELECTED_OP_BITS");
    w.connect(selected_op, op_bits->getInputPin<3>("IN"));
    const auto burst_request = w.reduce("BURST_REQUEST", {w.invert("NOT_OP_0", op_bits->getOutputPin("OUT_0")),
        w.invert("NOT_OP_1", op_bits->getOutputPin("OUT_1")), op_bits->getOutputPin("OUT_2")}, true);
    // Response buffers (filled below) and which one is empty.
    std::map<std::string, std::shared_ptr<IOComponent>> buffers;
    for (const std::string port : {"I", "D"}) {
        auto buffer = builder.addNewComponent<MemoryResponseBuffer>(port + "_RESPONSE", Width);
        w.connect(shared_clock, buffer->getInputPin("CLK"));
        w.connect(reset, buffer->getInputPin("RST"));
        w.connect(getInputPin(port + "_RSP_READY"), buffer->getInputPin("OUT_READY"));
        w.connect(getInputPinDynamic("MEM_RSP_READ_DATA"), buffer->getInputPinDynamic("IN_DATA"));
        w.connect(this->template getInputPin<3>("MEM_RSP_STATUS"), buffer->template getInputPin<3>("IN_STATUS"));
        w.connect(buffer->getOutputPin("OUT_VALID"), getOutputPin(port + "_RSP_VALID"));
        w.connect(buffer->getOutputPinDynamic("OUT_DATA"), getOutputPinDynamic(port + "_RSP_READ_DATA"));
        w.connect(buffer->template getOutputPin<3>("OUT_STATUS"), this->template getOutputPin<3>(port + "_RSP_STATUS"));
        buffers[port] = buffer;
    }
    const auto i_empty = w.invert("I_BUFFER_EMPTY", buffers["I"]->getOutputPin("FULL"));
    const auto d_empty = w.invert("D_BUFFER_EMPTY", buffers["D"]->getOutputPin("FULL"));
    const auto owner_empty = memory_detail::payloadMux(builder, w, "OWNER_BUFFER_EMPTY", i_empty, d_empty, owner_d->getOutputPin("Q"));
    const auto selected_empty = memory_detail::payloadMux(builder, w, "SELECTED_BUFFER_EMPTY", i_empty, d_empty, select_d);
    // Completion of the request in flight; a burst completes with its last word.
    std::vector<std::shared_ptr<PinBase>> beat;
    std::vector<std::shared_ptr<IOComponent>> beat_bits;
    const size_t beat_width = std::countr_zero(block_words_);
    for (size_t i = 0; i < beat_width; ++i) { beat_bits.push_back(bit("BEAT_" + std::to_string(i))); beat.push_back(beat_bits.back()->getOutputPin("Q")); }
    const auto last_beat = beat_width ? w.reduce("LAST_BEAT", beat, true) : w.constant("ONE_BEAT", true);
    const auto valid = getInputPin("MEM_RSP_VALID");
    const auto busy_ready = w.both("BUSY_RESPONSE_READY", active, owner_empty);
    const auto complete_busy = w.both("COMPLETE", busy_ready, valid);
    const auto final_busy = w.both("FINAL_COMPLETE", complete_busy,
        w.either("SINGLE_OR_LAST", w.invert("NOT_BURST", burst->getOutputPin("Q")), last_beat));
    const auto free = w.either("FREE", w.invert("IDLE", active), final_busy);
    const auto offered = w.both("LOWER_OFFER", free, w.either("ANY_REQUEST", i_wants, d_wants));
    const auto lower_ready = getInputPin("MEM_REQ_READY");
    const auto accept = w.both("ACCEPT", offered, lower_ready);
    w.connect(offered, getOutputPin("MEM_REQ_VALID"));
    w.connect(w.reduce("I_REQ_READY", {offered, select_i, lower_ready}, true), getOutputPin("I_REQ_READY"));
    w.connect(w.reduce("D_REQ_READY", {offered, select_d, lower_ready}, true), getOutputPin("D_REQ_READY"));
    // From idle: the accepted request's same-cycle answer (not a burst's).
    const auto new_ready = w.reduce("NEW_RESPONSE_READY", {w.invert("WAS_IDLE", active), accept, selected_empty,
        w.invert("NOT_BURST_REQUEST", burst_request)}, true);
    const auto complete_new = w.both("COMPLETE_NEW", new_ready, valid);
    w.connect(w.either("MEM_RESPONSE_READY", busy_ready, new_ready), getOutputPin("MEM_RSP_READY"));
    const auto response_owner_d = memory_detail::payloadMux(builder, w, "RESPONSE_OWNER", select_d, owner_d->getOutputPin("Q"), active);
    const auto answered = w.either("ANSWERED", complete_busy, complete_new);
    w.connect(w.both("I_ANSWER", answered, w.invert("RESPONSE_OWNER_I", response_owner_d)), buffers["I"]->getInputPin("IN_VALID"));
    w.connect(w.both("D_ANSWER", answered, response_owner_d), buffers["D"]->getInputPin("IN_VALID"));
    // State.
    const auto not_accept = w.invert("NOT_ACCEPT", accept);
    w.connect(w.either("NEXT_BUSY", w.both("STILL_NEW", accept, w.invert("NOT_COMPLETE_NEW", complete_new)),
        w.reduce("STILL_BUSY", {not_accept, active, w.invert("NOT_FINAL", final_busy)}, true)), busy->getInputPin("D"));
    w.connect(w.either("NEXT_OWNER_D", w.both("ACCEPT_D", accept, select_d), w.both("HOLD_OWNER_D", owner_d->getOutputPin("Q"), not_accept)),
              owner_d->getInputPin("D"));
    w.connect(w.either("NEXT_PREFER_D", w.both("ACCEPT_I", accept, select_i), w.both("HOLD_PREFER_D", prefer_d->getOutputPin("Q"), not_accept)),
              prefer_d->getInputPin("D"));
    w.connect(w.either("NEXT_BURST", w.both("ACCEPT_BURST", accept, burst_request), w.both("HOLD_BURST", burst->getOutputPin("Q"), not_accept)),
              burst->getInputPin("D"));
    std::shared_ptr<PinBase> carry = w.both("BEAT_STEP", complete_busy, burst->getOutputPin("Q"));
    for (size_t i = 0; i < beat_width; ++i) {
        const auto n = std::to_string(i);
        auto next = w.binary<XORGate>("BEAT_NEXT_" + n, beat[i], carry);
        carry = w.both("BEAT_CARRY_" + n, beat[i], carry);
        w.connect(w.both("BEAT_KEEP_" + n, next, w.invert("BEAT_NOT_ACCEPT_" + n, accept)), beat_bits[i]->getInputPin("D"));
    }
    w.finish();
}

template class TwoPortMemoryArbiter<8>;
template class TwoPortMemoryArbiter<16>;
template class TwoPortMemoryArbiter<32>;

void registerArbiters(ComponentCatalog& catalog) {
    for (size_t width : {8, 16, 32}) {
        const auto id = "memory.arbiter." + std::to_string(width);
        auto blockWords = [](const ParameterMap& parameters) {
            memory_device::keys(parameters, {"block_words", "pipelined"});
            if (const auto it = parameters.find("pipelined"); it != parameters.end() && it->second != "0" && it->second != "1")
                throw std::invalid_argument("Arbiter pipelined must be 0 or 1");
            const size_t words = memory_device::parameter(parameters, "block_words", 2);
            if (!std::has_single_bit(words) || words > 16)
                throw std::invalid_argument("Arbiter burst block must be a power of two up to 16 words");
            return words;
        };
        auto schema = [width, blockWords](const ParameterMap& parameters) {
            blockWords(parameters);
            return memory_device::describe([width](IOComponent* self) {
                protocolPins(self, width, "I_");
                protocolPins(self, width, "D_");
                protocolPins(self, width, "MEM_", true);
            });
        };
        const auto type = "TwoPortMemoryArbiter<" + std::to_string(width) + ">";
        catalog.registerContract({id, 1, type, schema({}), {}, "four-state", "transaction-completion-v2", schema});
        ImplementationDescriptor implementation;
        implementation.id = id + ".structural";
        implementation.contract_id = id;
        implementation.fidelity = Fidelity::Structural;
        implementation.concrete_type_name = type;
        implementation.supports = [schema](const ParameterMap& parameters) {
            try { schema(parameters); return true; }
            catch (const std::exception&) { return false; }
        };
        implementation.factory = [width, blockWords](const std::string& name, const ParameterMap& parameters,
                                         const std::shared_ptr<BuildContext>& context) -> std::shared_ptr<Component> {
            const size_t words = blockWords(parameters);
            const bool pipelined = parameters.contains("pipelined") && parameters.at("pipelined") == "1";
            if (width == 8) return Component::createWithContext<TwoPortMemoryArbiter<8>>(context, name, words, pipelined);
            if (width == 16) return Component::createWithContext<TwoPortMemoryArbiter<16>>(context, name, words, pipelined);
            return Component::createWithContext<TwoPortMemoryArbiter<32>>(context, name, words, pipelined);
        };
        implementation.evidence = {VerificationStatus::Characterized,
            {"structural round-robin grant, burst hold and one response buffer per port"},
            {width == 8 ? "MemoryArbiterTest" : "TwoPortMemoryArbiterTest"}, {},
            width == 8 ? std::vector<std::string>{"TwoPortMemoryArbiterTest"} : std::vector<std::string>{},
            "four-state", "transaction-completion-v2"};
        catalog.registerImplementation(std::move(implementation));
    }
}

} // namespace circuit::memory
