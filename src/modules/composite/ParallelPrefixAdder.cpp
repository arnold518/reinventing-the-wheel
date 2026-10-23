#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordRecipes.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include <array>

#include <stdexcept>
#include <string>

namespace circuit::arithmetic_detail {
CarryCombine::CarryCombine(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
    for (auto p : {"G_HIGH", "P_HIGH", "G_LOW", "P_LOW"}) s->addPin(p, PinType::INPUT);
    for (auto p : {"G", "P"}) s->addPin(p, PinType::OUTPUT);
}) {}

void CarryCombine::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    w.connect(w.either("GENERATE", getInputPin("G_HIGH"),
        w.both("PROPAGATE_GENERATE", getInputPin("P_HIGH"), getInputPin("G_LOW"))), getOutputPin("G"));
    w.connect(w.both("PROPAGATE", getInputPin("P_HIGH"), getInputPin("P_LOW")), getOutputPin("P"));
    w.finish();
}

CarryPrefixLevel::CarryPrefixLevel(std::string name, size_t distance,size_t width) : IOComponent(std::move(name), [width](IOComponent* s) {
    for (auto p : {"G", "P"}) s->addPinDynamic(p, PinType::INPUT,width);
    for (auto p : {"GROUP_G", "GROUP_P"}) s->addPinDynamic(p, PinType::OUTPUT,width);
}), distance_(distance),width_(width), type_("CarryPrefixLevel<" + (width==32?"":std::to_string(width)+",") + std::to_string(distance) + ">") {
    if ((width!=8 && width!=16 && width!=32) || distance == 0 || distance >= width || (distance & (distance - 1)))
        throw std::invalid_argument("Invalid carry prefix distance");
}

namespace {
template<size_t W>void buildPrefixLevel(IOComponent& root,ComponentBuilder& b,size_t distance){
    circuit::detail::StructuralWiring w(b);
    auto g = b.addNewComponent<BitSplitter<W>>("GENERATE_BITS");
    auto p = b.addNewComponent<BitSplitter<W>>("PROPAGATE_BITS");
    auto go = b.addNewComponent<BitJoiner<W>>("GENERATES");
    auto po = b.addNewComponent<BitJoiner<W>>("PROPAGATES");
    w.connect(root.getInputPin<W>("G"), g->template getInputPin<W>("IN"));
    w.connect(root.getInputPin<W>("P"), p->template getInputPin<W>("IN"));
    for (size_t i = 0; i < W; ++i) {
        const auto n = std::to_string(i);
        auto gs = g->getOutputPin("OUT_" + n), ps = p->getOutputPin("OUT_" + n);
        if (i >= distance) {
            auto cell = b.addNewComponent<CarryCombine>("GROUP_" + n);
            w.connect(gs, cell->getInputPin("G_HIGH")); w.connect(ps, cell->getInputPin("P_HIGH"));
            w.connect(g->getOutputPin("OUT_" + std::to_string(i-distance)), cell->getInputPin("G_LOW"));
            w.connect(p->getOutputPin("OUT_" + std::to_string(i-distance)), cell->getInputPin("P_LOW"));
            gs = cell->getOutputPin("G"); ps = cell->getOutputPin("P");
        }
        w.connect(gs, go->getInputPin("IN_" + n)); w.connect(ps, po->getInputPin("IN_" + n));
    }
    w.connect(go->template getOutputPin<W>("OUT"), root.getOutputPin<W>("GROUP_G"));
    w.connect(po->template getOutputPin<W>("OUT"), root.getOutputPin<W>("GROUP_P")); w.finish();
}
}
void CarryPrefixLevel::buildInternals(ComponentBuilder& b){
    if(width_==8)buildPrefixLevel<8>(*this,b,distance_);
    else if(width_==16)buildPrefixLevel<16>(*this,b,distance_);else buildPrefixLevel<32>(*this,b,distance_);
}

} // namespace circuit::arithmetic_detail

namespace {
template<size_t W>void buildPrefixWord(IOComponent& root,ComponentBuilder& b){
    using namespace circuit;
    circuit::detail::StructuralWiring w(b);
    auto a = b.addNewComponent<BitSplitter<W>>("A_BITS");
    auto d = b.addNewComponent<BitSplitter<W>>("B_BITS");
    auto gj = b.addNewComponent<BitJoiner<W>>("BIT_GENERATES");
    auto pj = b.addNewComponent<BitJoiner<W>>("BIT_PROPAGATES");
    auto result = b.addNewComponent<BitJoiner<W>>("SUM");
    w.connect(root.getInputPin<W>("A"), a->template getInputPin<W>("IN"));
    w.connect(root.getInputPin<W>("B"), d->template getInputPin<W>("IN"));
    std::array<circuit::detail::StructuralWiring::PinPtr,W> propagate;
    for (size_t i=0; i<W; ++i) {
        const auto n=std::to_string(i);
        auto bit=b.addNewComponent<HalfAdder>("BIT_"+n);
        w.connect(a->getOutputPin("OUT_"+n),bit->getInputPin("A"));
        w.connect(d->getOutputPin("OUT_"+n),bit->getInputPin("B"));
        propagate[i]=bit->getOutputPin("Sum");
        w.connect(propagate[i],pj->getInputPin("IN_"+n));
        w.connect(bit->getOutputPin("Carry"),gj->getInputPin("IN_"+n));
    }
    auto g=gj->template getOutputPin<W>("OUT"), p=pj->template getOutputPin<W>("OUT");
    for (size_t distance=1; distance<W; distance*=2) {
        auto level=b.addNewComponent<arithmetic_detail::CarryPrefixLevel>("PREFIX_"+std::to_string(distance),distance,W);
        w.connect(g,level->template getInputPin<W>("G")); w.connect(p,level->template getInputPin<W>("P"));
        g=level->template getOutputPin<W>("GROUP_G"); p=level->template getOutputPin<W>("GROUP_P");
    }
    auto gs=b.addNewComponent<BitSplitter<W>>("GROUP_GENERATES");
    auto ps=b.addNewComponent<BitSplitter<W>>("GROUP_PROPAGATES");
    w.connect(g,gs->template getInputPin<W>("IN")); w.connect(p,ps->template getInputPin<W>("IN"));
    auto carry=std::static_pointer_cast<PinBase>(root.getInputPin("C_IN"));
    for (size_t i=0; i<W; ++i) {
        const auto n=std::to_string(i);
        w.connect(w.binary<XORGate>("SUM_"+n,propagate[i],carry),result->getInputPin("IN_"+n));
        carry=w.either("CARRY_"+n,gs->getOutputPin("OUT_"+n),
            w.both("CIN_"+n,ps->getOutputPin("OUT_"+n),root.getInputPin("C_IN")));
        if (i==W-2) w.connect(carry,root.getOutputPin("C_SIGN"));
    }
    w.connect(carry,root.getOutputPin("C_OUT"));
    w.connect(result->template getOutputPin<W>("OUT"),root.getOutputPin<W>("OUT")); w.finish();
}
}
namespace circuit::word {
void buildPrefixAdder(IOComponent& root,ComponentBuilder& b,size_t w){
    if(w==8)buildPrefixWord<8>(root,b);else if(w==16)buildPrefixWord<16>(root,b);else buildPrefixWord<32>(root,b);
}
}
