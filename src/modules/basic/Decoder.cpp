#include "components/StructuralNetBuilder.hpp"
#include "components/StructuralLogic.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include <map>
#include "modules/basic/Decoder.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
template<size_t ADDR_WIDTH>
constexpr size_t decoderOutputCount() {
    return size_t{1} << ADDR_WIDTH;
}

std::string outputPinName(size_t index) {
    return "OUT" + std::to_string(index);
}

std::string notGateName(size_t bit) {
    return "NOT_ADDR" + std::to_string(bit);
}

std::string matchGateName(size_t output, size_t stage) {
    return outputPinName(output) + "_MATCH_AND_" + std::to_string(stage);
}

std::string enableGateName(size_t output) {
    return outputPinName(output) + "_ENABLE_AND";
}

template<size_t ADDR_WIDTH>
void addDecoderPins(IOComponent* self) {
    self->addPin<ADDR_WIDTH>("ADDR", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    for (size_t output = 0; output < decoderOutputCount<ADDR_WIDTH>(); ++output) {
        self->addPin(outputPinName(output), PinType::OUTPUT);
    }
}

template<size_t ADDR_WIDTH>
void buildDecoder(IOComponent& component, ComponentBuilder& builder,bool zero_output_unused=false) {
    circuit::detail::StructuralWiring w(builder);
    static_assert(ADDR_WIDTH >= 2, "decoder helper expects at least two address bits");

    constexpr size_t OutputCount = decoderOutputCount<ADDR_WIDTH>();
    builder.addNewComponent<BitSplitter<ADDR_WIDTH>>("ADDR_SPLIT");

    if constexpr (ADDR_WIDTH == 5) {
        for (size_t bit = 0; bit < 5; ++bit)
            builder.addNewComponent<NOTGate>(notGateName(bit));
        for (size_t low = 0; low < 4; ++low)
            builder.addNewComponent<ANDGate>("LOW_" + std::to_string(low));
        for (size_t high = 0; high < 8; ++high) {
            builder.addNewComponent<ANDGate>("HIGH_PAIR_" + std::to_string(high));
            builder.addNewComponent<ANDGate>("HIGH_" + std::to_string(high));
        }
        for (size_t output = 0; output < OutputCount; ++output) {
            if (output==0 && zero_output_unused) continue;
            builder.addNewComponent<ANDGate>("MATCH_" + std::to_string(output));
            builder.addNewComponent<ANDGate>(enableGateName(output));
        }
        if (zero_output_unused) {
            auto zero=builder.addNewComponent<ConstantValue<1>>("OUT0_ZERO",0);
            w.connect(zero->getOutputPin("OUT"),{component.getOutputPin("OUT0")});
        }
        w.connect(component.getInputPin<5>("ADDR"),
            {builder.getInputPin<BitSplitter<5>, 5>("ADDR_SPLIT", "IN")});
        std::array<std::vector<std::shared_ptr<Pin<>>>, 5> positive_sinks, negative_sinks;
        for (size_t bit = 0; bit < 5; ++bit)
            positive_sinks[bit].push_back(builder.getInputPin<NOTGate>(notGateName(bit), "IN"));
        auto literal = [&](size_t bit, bool positive, std::shared_ptr<Pin<>> sink) {
            (positive ? positive_sinks[bit] : negative_sinks[bit]).push_back(std::move(sink));
        };
        for (size_t low = 0; low < 4; ++low) {
            const auto gate = "LOW_" + std::to_string(low);
            literal(0, low & 1U, builder.getInputPin<ANDGate>(gate, "A"));
            literal(1, low & 2U, builder.getInputPin<ANDGate>(gate, "B"));
        }
        for (size_t high = 0; high < 8; ++high) {
            const auto suffix = std::to_string(high);
            literal(2, high & 1U, builder.getInputPin<ANDGate>("HIGH_PAIR_" + suffix, "A"));
            literal(3, high & 2U, builder.getInputPin<ANDGate>("HIGH_PAIR_" + suffix, "B"));
            w.connect(builder.getOutputPin<ANDGate>("HIGH_PAIR_" + suffix, "OUT"),
                {builder.getInputPin<ANDGate>("HIGH_" + suffix, "A")});
            literal(4, high & 4U, builder.getInputPin<ANDGate>("HIGH_" + suffix, "B"));
        }
        for (size_t bit = 0; bit < 5; ++bit) {
            w.connect(builder.getOutputPin<BitSplitter<5>>("ADDR_SPLIT", "OUT_" + std::to_string(bit)),
                positive_sinks[bit]);
            w.connect(builder.getOutputPin<NOTGate>(notGateName(bit), "OUT"), negative_sinks[bit]);
        }
        std::vector<std::shared_ptr<Pin<>>> enable_sinks;
        std::array<std::vector<std::shared_ptr<Pin<>>>, 4> low_sinks;
        std::array<std::vector<std::shared_ptr<Pin<>>>, 8> high_sinks;
        for (size_t output = 0; output < OutputCount; ++output) {
            if (output==0 && zero_output_unused) continue;
            const auto suffix = std::to_string(output);
            low_sinks[output & 3U].push_back(builder.getInputPin<ANDGate>("MATCH_" + suffix, "A"));
            high_sinks[output >> 2U].push_back(builder.getInputPin<ANDGate>("MATCH_" + suffix, "B"));
            w.connect(builder.getOutputPin<ANDGate>("MATCH_" + suffix, "OUT"),
                {builder.getInputPin<ANDGate>(enableGateName(output), "B")});
            enable_sinks.push_back(builder.getInputPin<ANDGate>(enableGateName(output), "A"));
            w.connect(builder.getOutputPin<ANDGate>(enableGateName(output), "OUT"),
                {component.getOutputPin(outputPinName(output))});
        }
        for (size_t low = 0; low < low_sinks.size(); ++low)
            w.connect(builder.getOutputPin<ANDGate>("LOW_" + std::to_string(low), "OUT"), low_sinks[low]);
        for (size_t high = 0; high < high_sinks.size(); ++high)
            w.connect(builder.getOutputPin<ANDGate>("HIGH_" + std::to_string(high), "OUT"), high_sinks[high]);
        w.connect(component.getInputPin("ENABLE"), enable_sinks);
        w.finish();
        return;
    }

    for (size_t bit = 0; bit < ADDR_WIDTH; ++bit) {
        builder.addNewComponent<NOTGate>(notGateName(bit));
    }

    for (size_t output = 0; output < OutputCount; ++output) {
        for (size_t stage = 0; stage < ADDR_WIDTH - 1; ++stage) {
            builder.addNewComponent<ANDGate>(matchGateName(output, stage));
        }
        builder.addNewComponent<ANDGate>(enableGateName(output));
    }

    w.connect(component.getInputPin<ADDR_WIDTH>("ADDR"),
        {builder.getInputPin<BitSplitter<ADDR_WIDTH>, ADDR_WIDTH>("ADDR_SPLIT", "IN")});

    std::array<std::vector<std::shared_ptr<Pin<>>>, ADDR_WIDTH> high_sinks;
    std::array<std::vector<std::shared_ptr<Pin<>>>, ADDR_WIDTH> low_sinks;
    std::vector<std::shared_ptr<Pin<>>> enable_sinks;
    enable_sinks.reserve(OutputCount);

    for (size_t output = 0; output < OutputCount; ++output) {
        for (size_t bit = 0; bit < ADDR_WIDTH; ++bit) {
            std::shared_ptr<Pin<>> sink;
            if (bit == 0) {
                sink = builder.getInputPin<ANDGate>(matchGateName(output, 0), "A");
            } else if (bit == 1) {
                sink = builder.getInputPin<ANDGate>(matchGateName(output, 0), "B");
            } else {
                sink = builder.getInputPin<ANDGate>(matchGateName(output, bit - 1), "B");
            }

            if ((output >> bit) & 1U) {
                high_sinks[bit].push_back(sink);
            } else {
                low_sinks[bit].push_back(sink);
            }
        }

        for (size_t stage = 1; stage < ADDR_WIDTH - 1; ++stage) {
            w.connect(builder.getOutputPin<ANDGate>(matchGateName(output, stage - 1), "OUT"),
                {builder.getInputPin<ANDGate>(matchGateName(output, stage), "A")});
        }

        w.connect(builder.getOutputPin<ANDGate>(matchGateName(output, ADDR_WIDTH - 2), "OUT"),
            {builder.getInputPin<ANDGate>(enableGateName(output), "B")});

        enable_sinks.push_back(builder.getInputPin<ANDGate>(enableGateName(output), "A"));

        w.connect(builder.getOutputPin<ANDGate>(enableGateName(output), "OUT"),
            {component.getOutputPin(outputPinName(output))});
    }

    for (size_t bit = 0; bit < ADDR_WIDTH; ++bit) {
        const auto bit_text = std::to_string(bit);
        high_sinks[bit].push_back(builder.getInputPin<NOTGate>(notGateName(bit), "IN"));

        w.connect(builder.getOutputPin<BitSplitter<ADDR_WIDTH>>("ADDR_SPLIT", "OUT_" + bit_text),
            high_sinks[bit]);

        w.connect(builder.getOutputPin<NOTGate>(notGateName(bit), "OUT"),
            low_sinks[bit]);
    }

    w.connect(component.getInputPin("ENABLE"), enable_sinks);
    w.finish();
}

std::string decoderType(size_t bits,bool suppress) {
    return "Decoder<"+std::to_string(bits)+(suppress?",zero-disabled>":">");
}
void decoderPins(IOComponent* p,size_t bits) {
    if(bits<1||bits>5)throw std::invalid_argument("Decoder supports one through five address bits");
    p->addPinDynamic("ADDR",PinType::INPUT,bits);p->addPin("ENABLE",PinType::INPUT);
    for(size_t i=0;i<(size_t{1}<<bits);++i)p->addPin(outputPinName(i),PinType::OUTPUT);
}
class DecoderBehavioral final : public BasicComponent {
    size_t address_bits_;bool suppress_zero_;std::string type_;
public:
    DecoderBehavioral(std::string name,size_t bits,bool suppress)
        :BasicComponent(std::move(name),1,[bits](IOComponent* p){decoderPins(p,bits);}),
         address_bits_(bits),suppress_zero_(suppress),type_(decoderType(bits,suppress)){}
    const char* getTypeName()const override{return type_.c_str();}
    void evaluate(SimulationTime t,Simulator& s)override {
        using namespace circuit::logic_values;
        const auto address=getInputPinDynamic("ADDR")->getValueAsVector();
        for(size_t output=0;output<(size_t{1}<<address_bits_);++output) {
            auto value=getInputPin("ENABLE")->getValue();
            for(size_t bit=0;bit<address_bits_;++bit)
                value=andGate(value,(output>>bit)&1?address[bit]:notGate(address[bit]));
            if(suppress_zero_&&output==0)value=LogicValue::LOW;
            _updateOutputWire(s,outputPinName(output),value,t);
        }
    }
};
}
namespace circuit::logic {
WordDecoder::WordDecoder(std::string name,size_t bits,bool suppress)
    :IOComponent(std::move(name),[bits](IOComponent* p){decoderPins(p,bits);}),
     address_bits_(bits),suppress_zero_(suppress),type_(decoderType(bits,suppress)){}
void WordDecoder::buildInternals(ComponentBuilder& builder) {
    if(address_bits_==5){buildDecoder<5>(*this,builder,suppress_zero_);return;}
    circuit::structural::Logic net(builder);
    const auto address=net.split(getInputPinDynamic("ADDR"));
    structural::V inverse;
    for(const auto& bit:address)inverse.push_back(net.inv(bit));
    for(size_t output=0;output<(size_t{1}<<address_bits_);++output) {
        auto value=net.constant(1)[0];
        if(!suppress_zero_||output) {
            structural::V literals{getInputPin("ENABLE")};
            for(size_t bit=0;bit<address_bits_;++bit)literals.push_back((output>>bit)&1?address[bit]:inverse[bit]);
            value=net.all(literals);
        }
        net.wire(value,getOutputPin(outputPinName(output)));
    }
    net.finish();
}
void registerDecoders(ComponentCatalog& c) {
    for(size_t bits=1;bits<=5;++bits)for(bool suppress:{false,true}) {
        const auto& f=families::decoder(bits,suppress);
        const std::string test=bits==5?(suppress?"DecoderWidth5ZeroDisabledTest":"DecoderWidth5Test"):"WidthComponentsTest";
        registration::registerStructural(c,f,test);
        if(f.supports(Fidelity::Behavioral))registration::registerBehavioral(c,f,test,test,{},"stable-after-settle");
    }
}
}
namespace circuit::families {
const ComponentFamily& decoder(size_t bits,bool suppress) {
    if(bits<1||bits>5)throw std::invalid_argument("Invalid decoder address width");
    struct Entry {
        std::string id,type;ComponentFamily family;
        Entry(size_t bits,bool suppress):id("logic.decoder."+std::to_string(bits)+(suppress?".zero-disabled":".full")),
            type(decoderType(bits,suppress)),family(id,type,[bits](IOComponent* p){decoderPins(p,bits);},
            [bits,suppress](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<logic::WordDecoder>(c,n,bits,suppress);},
            bits==5?FamilyFactory([bits,suppress](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<DecoderBehavioral>(c,n,bits,suppress);}):FamilyFactory{}){}
    };
    static std::map<std::pair<size_t,bool>,std::unique_ptr<Entry>> entries;
    auto& e=entries[{bits,suppress}];if(!e)e=std::make_unique<Entry>(bits,suppress);return e->family;
}
}
