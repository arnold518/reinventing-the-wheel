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
        w.connect(w.either("NEXT_BURST", w.both("ACCEPT_BURST", accepted, burst_request),
            w.both("HOLD_BURST", burst->getOutputPin("Q"), w.invert("NOT_ACCEPTING", accepted))), burst->getInputPin("D"));
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
        w.connect(w.either("NEXT_OWNER_D", w.both("ACCEPT_D", accepted, select_d),
            w.both("HOLD_OWNER_D", owner_d->getOutputPin("Q"), w.invert("NOT_ACCEPT", accepted))), owner_d->getInputPin("D"));
        w.connect(w.either("NEXT_PREFER_D", w.both("ACCEPT_I", accepted, select_i),
            w.both("HOLD_PREFER_D", prefer_d->getOutputPin("Q"),
                   w.invert("NOT_ACCEPT_FOR_PRIORITY", accepted))), prefer_d->getInputPin("D"));
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
        auto owner_empty = memory_detail::payloadMux(builder, w, "OWNER_BUFFER_EMPTY",
            w.invert("I_BUFFER_EMPTY", getInputPin("I_FULL")),
            w.invert("D_BUFFER_EMPTY", getInputPin("D_FULL")), owner_d);
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

template class TwoPortMemoryArbiter<8>;
template class TwoPortMemoryArbiter<16>;
template class TwoPortMemoryArbiter<32>;

void registerArbiters(ComponentCatalog& catalog) {
    for (size_t width : {8, 16, 32}) {
        const auto id = "memory.arbiter." + std::to_string(width);
        auto blockWords = [](const ParameterMap& parameters) {
            memory_device::keys(parameters, {"block_words"});
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
            if (width == 8) return Component::createWithContext<TwoPortMemoryArbiter<8>>(context, name, words);
            if (width == 16) return Component::createWithContext<TwoPortMemoryArbiter<16>>(context, name, words);
            return Component::createWithContext<TwoPortMemoryArbiter<32>>(context, name, words);
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
