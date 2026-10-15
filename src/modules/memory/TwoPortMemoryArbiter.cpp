#include "modules/memory/TwoPortMemoryArbiter.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/utility/Constant.hpp"
#include "MemoryHandshakeWiring.hpp"

namespace circuit::memory {

template<size_t Width>
TwoPortMemoryArbiter<Width>::TwoPortMemoryArbiter(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          protocolPins(self, Width, "I_");
          protocolPins(self, Width, "D_");
          protocolPins(self, Width, "MEM_", true);
      }), type_("TwoPortMemoryArbiter<" + std::to_string(Width) + ">") {
    static_assert(Width == 8 || Width == 16 || Width == 32);
}

template<size_t Width>
void TwoPortMemoryArbiter<Width>::buildInternals(ComponentBuilder& builder) {
    memory_detail::StructuralWiring w(builder);
    auto busy = builder.add(circuit::families::ClockedStateBit, "BUSY");
    auto owner_d = builder.add(circuit::families::ClockedStateBit, "OWNER_D");
    auto prefer_d = builder.add(circuit::families::ClockedStateBit, "PREFER_D");
    // The simulator has one clock domain; both requester clocks advance together.
    auto shared_clock = w.both("SHARED_CLOCK", getInputPin("I_CLK"), getInputPin("D_CLK"));
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
    for (auto field : {"REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK"}) {
        auto selected = memory_detail::payloadMux(builder, w, std::string("SELECT_") + field,
            getInputPinDynamic(std::string("I_") + field),
            getInputPinDynamic(std::string("D_") + field), select_d);
        w.connect(selected, getOutputPinDynamic(std::string("MEM_") + field));
    }
    auto lower_ready = getInputPin("MEM_REQ_READY");
    w.connect(w.reduce("I_REQ_READY", {idle, select_i, lower_ready}, true),
              getOutputPin("I_REQ_READY"));
    w.connect(w.reduce("D_REQ_READY", {idle, select_d, lower_ready}, true),
              getOutputPin("D_REQ_READY"));
    auto accepted = w.both("ACCEPT", offered, lower_ready);
    auto response_ready = memory_detail::payloadMux(builder, w, "SELECT_RESPONSE_READY",
        getInputPin("I_RSP_READY"), getInputPin("D_RSP_READY"), owner_d->getOutputPin("Q"));
    auto lower_response_ready = w.both("LOWER_RESPONSE_READY", active, response_ready);
    w.connect(lower_response_ready, getOutputPin("MEM_RSP_READY"));
    auto complete = w.both("COMPLETE", lower_response_ready, getInputPin("MEM_RSP_VALID"));
    w.connect(w.either("NEXT_BUSY", accepted,
        w.both("HOLD_BUSY", active, w.invert("NOT_COMPLETE", complete))),
        busy->getInputPin("D"));
    w.connect(w.either("NEXT_OWNER_D", w.both("ACCEPT_D", accepted, select_d),
        w.both("HOLD_OWNER_D", owner_d->getOutputPin("Q"),
               w.invert("NOT_ACCEPT", accepted))), owner_d->getInputPin("D"));
    w.connect(w.either("NEXT_PREFER_D", w.both("ACCEPT_I", accepted, select_i),
        w.both("HOLD_PREFER_D", prefer_d->getOutputPin("Q"),
               w.invert("NOT_ACCEPT_FOR_PRIORITY", accepted))), prefer_d->getInputPin("D"));

    auto i_response = w.reduce("I_RESPONSE", {active,
        w.invert("OWNER_I", owner_d->getOutputPin("Q")), getInputPin("MEM_RSP_VALID")}, true);
    auto d_response = w.reduce("D_RESPONSE", {active, owner_d->getOutputPin("Q"),
        getInputPin("MEM_RSP_VALID")}, true);
    w.connect(i_response, getOutputPin("I_RSP_VALID"));
    w.connect(d_response, getOutputPin("D_RSP_VALID"));
    auto zero_data = builder.addNewComponent<ConstantValue<Width>>("ZERO_DATA", 0)
                         ->template getOutputPin<Width>("OUT");
    auto zero_status = builder.addNewComponent<ConstantValue<3>>("ZERO_STATUS", 0)
                           ->getOutputPin<3>("OUT");
    for (auto field : {"RSP_READ_DATA", "RSP_STATUS"}) {
        memory_detail::StructuralWiring::PinPtr zero_pin;
        if (std::string(field) == "RSP_STATUS") zero_pin = zero_status;
        else zero_pin = zero_data;
        auto lower = getInputPinDynamic(std::string("MEM_") + field);
        w.connect(memory_detail::payloadMux(builder, w, std::string("I_") + field,
            zero_pin, lower, i_response), getOutputPinDynamic(std::string("I_") + field));
        w.connect(memory_detail::payloadMux(builder, w, std::string("D_") + field,
            zero_pin, lower, d_response), getOutputPinDynamic(std::string("D_") + field));
    }
    w.finish();
}

template class TwoPortMemoryArbiter<8>;
template class TwoPortMemoryArbiter<16>;
template class TwoPortMemoryArbiter<32>;

} // namespace circuit::memory
