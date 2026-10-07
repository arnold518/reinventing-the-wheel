#pragma once

#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <array>

namespace circuit::arithmetic_detail {
// Composition of two carry transformations c' = G | (P & c).
// AND/OR distributivity preserves the ripple circuit's four-state behavior.
class CarryCombine : public IOComponent {
public:
    explicit CarryCombine(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
        for (auto p : {"G_HIGH", "P_HIGH", "G_LOW", "P_LOW"}) s->addPin(p, PinType::INPUT);
        for (auto p : {"G", "P"}) s->addPin(p, PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override { return "CarryCombine"; }
    void buildInternals(ComponentBuilder& b) override {
        memory_detail::StructuralWiring w(b);
        w.connect(w.either("GENERATE", getInputPin("G_HIGH"),
            w.both("PROPAGATE_GENERATE", getInputPin("P_HIGH"), getInputPin("G_LOW"))), getOutputPin("G"));
        w.connect(w.both("PROPAGATE", getInputPin("P_HIGH"), getInputPin("P_LOW")), getOutputPin("P"));
        w.finish();
    }
};

class CarryPrefixLevel : public IOComponent {
public:
    CarryPrefixLevel(std::string name, size_t distance) : IOComponent(std::move(name), [](IOComponent* s) {
        for (auto p : {"G", "P"}) s->addPin<32>(p, PinType::INPUT);
        for (auto p : {"GROUP_G", "GROUP_P"}) s->addPin<32>(p, PinType::OUTPUT);
    }), distance_(distance), type_("CarryPrefixLevel<" + std::to_string(distance) + ">") {
        if (distance == 0 || distance > 16 || (distance & (distance - 1)))
            throw std::invalid_argument("Invalid carry prefix distance");
    }
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        memory_detail::StructuralWiring w(b);
        auto g = b.addNewComponent<BitSplitter<32>>("GENERATE_BITS");
        auto p = b.addNewComponent<BitSplitter<32>>("PROPAGATE_BITS");
        auto go = b.addNewComponent<BitJoiner<32>>("GENERATES");
        auto po = b.addNewComponent<BitJoiner<32>>("PROPAGATES");
        w.connect(getInputPin<32>("G"), g->getInputPin<32>("IN"));
        w.connect(getInputPin<32>("P"), p->getInputPin<32>("IN"));
        for (size_t i = 0; i < 32; ++i) {
            const auto n = std::to_string(i);
            auto gs = g->getOutputPin("OUT_" + n), ps = p->getOutputPin("OUT_" + n);
            if (i >= distance_) {
                auto cell = b.addNewComponent<CarryCombine>("GROUP_" + n);
                w.connect(gs, cell->getInputPin("G_HIGH")); w.connect(ps, cell->getInputPin("P_HIGH"));
                w.connect(g->getOutputPin("OUT_" + std::to_string(i-distance_)), cell->getInputPin("G_LOW"));
                w.connect(p->getOutputPin("OUT_" + std::to_string(i-distance_)), cell->getInputPin("P_LOW"));
                gs = cell->getOutputPin("G"); ps = cell->getOutputPin("P");
            }
            w.connect(gs, go->getInputPin("IN_" + n)); w.connect(ps, po->getInputPin("IN_" + n));
        }
        w.connect(go->getOutputPin<32>("OUT"), getOutputPin<32>("GROUP_G"));
        w.connect(po->getOutputPin<32>("OUT"), getOutputPin<32>("GROUP_P")); w.finish();
    }
private:
    size_t distance_;
    std::string type_;
};
}

// Gate-built five-level parallel-prefix adder. The public wrappers retain
// their original pins; C31 is exposed here for AddSub32 signed overflow.
class ParallelPrefixAdder32 : public IOComponent {
public:
    explicit ParallelPrefixAdder32(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
        for (auto p : {"A", "B"}) s->addPin<32>(p, PinType::INPUT);
        s->addPin("Cin", PinType::INPUT); s->addPin<32>("Sum", PinType::OUTPUT);
        for (auto p : {"Cout", "C31"}) s->addPin(p, PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override { return "ParallelPrefixAdder32"; }
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit;
        memory_detail::StructuralWiring w(b);
        auto a = b.addNewComponent<BitSplitter<32>>("A_BITS");
        auto d = b.addNewComponent<BitSplitter<32>>("B_BITS");
        auto gj = b.addNewComponent<BitJoiner<32>>("BIT_GENERATES");
        auto pj = b.addNewComponent<BitJoiner<32>>("BIT_PROPAGATES");
        auto result = b.addNewComponent<BitJoiner<32>>("SUM");
        w.connect(getInputPin<32>("A"), a->getInputPin<32>("IN"));
        w.connect(getInputPin<32>("B"), d->getInputPin<32>("IN"));
        std::array<memory_detail::StructuralWiring::PinPtr,32> propagate;
        for (size_t i=0; i<32; ++i) {
            const auto n=std::to_string(i);
            auto bit=b.addNewComponent<HalfAdder>("BIT_"+n);
            w.connect(a->getOutputPin("OUT_"+n),bit->getInputPin("A"));
            w.connect(d->getOutputPin("OUT_"+n),bit->getInputPin("B"));
            propagate[i]=bit->getOutputPin("Sum");
            w.connect(propagate[i],pj->getInputPin("IN_"+n));
            w.connect(bit->getOutputPin("Carry"),gj->getInputPin("IN_"+n));
        }
        auto g=gj->getOutputPin<32>("OUT"), p=pj->getOutputPin<32>("OUT");
        for (size_t distance=1; distance<32; distance*=2) {
            auto level=b.addNewComponent<arithmetic_detail::CarryPrefixLevel>("PREFIX_"+std::to_string(distance),distance);
            w.connect(g,level->getInputPin<32>("G")); w.connect(p,level->getInputPin<32>("P"));
            g=level->getOutputPin<32>("GROUP_G"); p=level->getOutputPin<32>("GROUP_P");
        }
        auto gs=b.addNewComponent<BitSplitter<32>>("GROUP_GENERATES");
        auto ps=b.addNewComponent<BitSplitter<32>>("GROUP_PROPAGATES");
        w.connect(g,gs->getInputPin<32>("IN")); w.connect(p,ps->getInputPin<32>("IN"));
        auto carry=std::static_pointer_cast<PinBase>(getInputPin("Cin"));
        for (size_t i=0; i<32; ++i) {
            const auto n=std::to_string(i);
            w.connect(w.binary<XORGate>("SUM_"+n,propagate[i],carry),result->getInputPin("IN_"+n));
            carry=w.either("CARRY_"+n,gs->getOutputPin("OUT_"+n),
                w.both("CIN_"+n,ps->getOutputPin("OUT_"+n),getInputPin("Cin")));
            if (i==30) w.connect(carry,getOutputPin("C31"));
        }
        w.connect(carry,getOutputPin("Cout"));
        w.connect(result->getOutputPin<32>("OUT"),getOutputPin<32>("Sum")); w.finish();
    }
};
