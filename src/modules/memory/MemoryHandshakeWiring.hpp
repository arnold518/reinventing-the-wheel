#pragma once
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"

namespace circuit::memory_detail {
// A bus mux is an explicit hierarchy group, not a behavioral storage element.
class PayloadMux : public IOComponent {
public:
    PayloadMux(std::string n, size_t width)
        : IOComponent(std::move(n), [width](IOComponent* s) {
            for (auto p : {"A", "B"}) s->addPinDynamic(p, PinType::INPUT, width);
            s->addPin("SEL", PinType::INPUT);
            s->addPinDynamic("OUT", PinType::OUTPUT, width);
        }), width_(width), type_("MemoryPayloadMux<"+std::to_string(width)+">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        if(width_==1) {
            auto left=w.both("A_BIT",w.invert("NOT_SELECT",getInputPin("SEL")),getInputPin("A"));
            auto right=w.both("B_BIT",getInputPin("SEL"),getInputPin("B"));
            w.connect(w.either("SELECT",left,right),getOutputPin("OUT"));w.finish();return;
        }
        auto a = memory_device::bitAdapter(b,"A_BITS",width_,true);
        auto d = memory_device::bitAdapter(b,"B_BITS",width_,true);
        auto q = memory_device::bitAdapter(b,"RESULT",width_,false);
        w.connect(getInputPinDynamic("A"),a->getInputPinDynamic("IN"));
        w.connect(getInputPinDynamic("B"),d->getInputPinDynamic("IN"));
        auto no = w.invert("NOT_SELECT",getInputPin("SEL"));
        for (size_t i=0;i<width_;++i) {
            auto bit=std::to_string(i);
            auto left=w.both("A_"+bit,no,a->getOutputPin("OUT_"+bit));
            auto right=w.both("B_"+bit,getInputPin("SEL"),d->getOutputPin("OUT_"+bit));
            w.connect(w.either("SELECT_"+bit,left,right),q->getInputPin("IN_"+bit));
        }
        w.connect(q->getOutputPinDynamic("OUT"),getOutputPinDynamic("OUT"));
        w.finish();
    }
private: size_t width_; std::string type_;
};
inline StructuralWiring::PinPtr payloadMux(ComponentBuilder& b, StructuralWiring& w,
    const std::string& name, StructuralWiring::PinPtr a, StructuralWiring::PinPtr d,
    StructuralWiring::PinPtr select) {
    auto mux=b.addNewComponent<PayloadMux>(name,a->getWidth());
    w.connect(a,mux->getInputPinDynamic("A"));
    w.connect(d,mux->getInputPinDynamic("B"));
    w.connect(select,mux->getInputPin("SEL"));
    return mux->getOutputPinDynamic("OUT");
}
inline StructuralWiring::PinPtr bridge(ComponentBuilder& b, StructuralWiring& w,
    const std::string& name, StructuralWiring::PinPtr source) {
    const auto width=source->getWidth();
    auto r=b.addNewComponent<Rewire>(name,std::vector<Rewire::WireSpec>{{"IN",width}},
        std::vector<Rewire::WireSpec>{{"OUT",width}},identity_mapping("IN",0,width,"OUT"));
    w.connect(source,r->getInputPinDynamic("IN"));
    return r->getOutputPinDynamic("OUT");
}
}
