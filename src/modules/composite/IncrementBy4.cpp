#include "modules/composite/IncrementBy4.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <array>
#include <utility>

IncrementBy4::IncrementBy4(std::string name)
    : IOComponent(std::move(name),[](IOComponent* self){
        self->addPin<32>("A",PinType::INPUT);
        self->addPin<32>("Sum",PinType::OUTPUT);
    }) {}

void IncrementBy4::buildInternals(ComponentBuilder& builder) {
    using circuit::memory_detail::StructuralWiring;
    StructuralWiring w(builder);
    auto split=builder.addNewComponent<BitSplitter<32>>("INPUT_BITS");
    auto join=builder.addNewComponent<BitJoiner<32>>("SUM_BITS");
    w.connect(getInputPin<32>("A"),split->getInputPin<32>("IN"));
    std::array<StructuralWiring::PinPtr,32> prefix{};
    for (size_t i=2;i<32;++i) prefix[i]=split->getOutputPin("OUT_"+std::to_string(i));
    for (size_t distance=1;distance<32;distance*=2) {
        auto next=prefix;
        for (size_t i=2+distance;i<32;++i)
            next[i]=w.both("PREFIX_"+std::to_string(distance)+"_"+std::to_string(i),
                prefix[i],prefix[i-distance]);
        prefix=std::move(next);
    }
    for (size_t i=0;i<32;++i) {
        StructuralWiring::PinPtr bit=split->getOutputPin("OUT_"+std::to_string(i));
        if (i==2) bit=w.invert("INCREMENT_BIT_2",bit);
        else if (i>2) bit=w.binary<XORGate>("INCREMENT_BIT_"+std::to_string(i),bit,prefix[i-1]);
        w.connect(bit,join->getInputPin("IN_"+std::to_string(i)));
    }
    w.connect(join->getOutputPin<32>("OUT"),getOutputPin<32>("Sum"));
    w.finish();
}
