#include "modules/composite/Incrementer.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "basic/LogicValueHelpers.hpp"
#include <array>
#include <bit>

namespace circuit::word {
namespace {
template<size_t W>void buildIncrementer(IOComponent& root,ComponentBuilder& builder,uint64_t step) {
    using Wiring=circuit::detail::StructuralWiring;
    Wiring wires(builder);
    auto split=builder.addNewComponent<BitSplitter<W>>("INPUT_BITS");
    auto join=builder.addNewComponent<BitJoiner<W>>("SUM_BITS");
    wires.connect(root.getInputPin<W>("A"),split->template getInputPin<W>("IN"));
    std::array<typename Wiring::PinPtr,W> bits;
    for(size_t i=0;i<W;++i)bits[i]=split->getOutputPin("OUT_"+std::to_string(i));
    if(step && std::has_single_bit(step)) {
        const size_t first=std::countr_zero(step);
        // Each prefix level and the sum are rows of independent gates, built
        // as word gates (PREFIX_<distance>, INCREMENT).
        using circuit::word::GateKind;
        auto prefix=bits;
        for(size_t distance=1;distance<W;distance*=2) {
            if(first+distance>=W)break;
            std::vector<typename Wiring::PinPtr> high,low;
            for(size_t i=first+distance;i<W;++i){high.push_back(prefix[i]);low.push_back(prefix[i-distance]);}
            const auto level=wires.gateBits(GateKind::And,"PREFIX_"+std::to_string(distance),high,low);
            for(size_t i=first+distance;i<W;++i)prefix[i]=level[i-first-distance];
        }
        std::vector<typename Wiring::PinPtr> upper,carries;
        for(size_t i=first+1;i<W;++i){upper.push_back(bits[i]);carries.push_back(prefix[i-1]);}
        bits[first]=wires.invert("INCREMENT_BIT_"+std::to_string(first),bits[first]);
        const auto sum=wires.gateBits(GateKind::Xor,"INCREMENT",upper,carries);
        for(size_t i=first+1;i<W;++i)bits[i]=sum[i-first-1];
    } else if(step) {
        const size_t first=std::countr_zero(step);
        auto carry=bits[first];
        bits[first]=wires.invert("INCREMENT_BIT_"+std::to_string(first),bits[first]);
        for(size_t i=first+1;i<W;++i) {
            const auto id=std::to_string(i);const auto input=bits[i];
            bits[i]=wires.binary<XORGate>("SUM_"+id,input,carry);
            if((step>>i)&1) {
                bits[i]=wires.invert("INVERT_SUM_"+id,bits[i]);
                carry=wires.either("CARRY_"+id,input,carry);
            } else carry=wires.both("CARRY_"+id,input,carry);
        }
    }
    for(size_t i=0;i<W;++i)wires.connect(bits[i],join->getInputPin("IN_"+std::to_string(i)));
    wires.connect(join->template getOutputPin<W>("OUT"),root.getOutputPin<W>("Sum"));wires.finish();
}
}
WordIncrementer::WordIncrementer(std::string name,size_t width,uint64_t step)
    :IOComponent(std::move(name),[width](IOComponent* p){
        p->addPinDynamic("A",PinType::INPUT,width);p->addPinDynamic("Sum",PinType::OUTPUT,width);
    }),width_(width),increment_(step),type_("Incrementer<"+std::to_string(width)+","+std::to_string(step)+">") {
    if((width!=8&&width!=16&&width!=32)||step>=(uint64_t{1}<<width))throw std::invalid_argument("Invalid incrementer configuration");
}
void WordIncrementer::buildInternals(ComponentBuilder& b) {
    if(width_==8)buildIncrementer<8>(*this,b,increment_);
    else if(width_==16)buildIncrementer<16>(*this,b,increment_);
    else buildIncrementer<32>(*this,b,increment_);
}
std::vector<LogicValue> incrementWord(const std::vector<LogicValue>& value,uint64_t step) {
    using namespace circuit::logic_values;
    const auto width=value.size();
    if((width!=8&&width!=16&&width!=32)||step>=(uint64_t{1}<<width))throw std::invalid_argument("Invalid increment word");
    auto sum=value;if(!step)return sum;
    const size_t first=std::countr_zero(step);
    auto carry=value[first];sum[first]=notGate(value[first]);
    for(size_t i=first+1;i<width;++i) {
        sum[i]=xorGate(value[i],carry);
        if((step>>i)&1){sum[i]=notGate(sum[i]);carry=orGate(value[i],carry);}
        else carry=andGate(value[i],carry);
    }
    return sum;
}
}
