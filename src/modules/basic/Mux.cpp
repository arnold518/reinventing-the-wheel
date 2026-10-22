#include "components/selection/FamilyRegistration.hpp"
#include <map>
#include "modules/basic/WordMux.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Mux.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/selection/FamilyFactories.hpp"
#include <string>
#include <vector>

namespace {
std::string muxInputName(size_t input_count, size_t index) {
    if (input_count == 2) {
        return index == 0 ? "A" : "B";
    }
    return "IN" + std::to_string(index);
}

void buildMux2FromGates(IOComponent& component, ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOTGate>("NOT_SEL");
    builder.addNewComponent<ANDGate>("AND_A");
    builder.addNewComponent<ANDGate>("AND_B");
    builder.addNewComponent<ORGate>("OR_OUT");

    w.connect(component.getInputPin("SEL"),
        {builder.getInputPin<NOTGate>("NOT_SEL", "IN"), builder.getInputPin<ANDGate>("AND_B", "B")});
    w.connect(component.getInputPin("A"), {builder.getInputPin<ANDGate>("AND_A", "A")});
    w.connect(component.getInputPin("B"), {builder.getInputPin<ANDGate>("AND_B", "A")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_SEL", "OUT"), {builder.getInputPin<ANDGate>("AND_A", "B")});
    w.connect(builder.getOutputPin<ANDGate>("AND_A", "OUT"), {builder.getInputPin<ORGate>("OR_OUT", "A")});
    w.connect(builder.getOutputPin<ANDGate>("AND_B", "OUT"), {builder.getInputPin<ORGate>("OR_OUT", "B")});
    w.connect(builder.getOutputPin<ORGate>("OR_OUT", "OUT"), {component.getOutputPin("OUT")});
    w.finish();
}

template<size_t SEL_WIDTH>
void buildOneBitMuxTree(IOComponent& component, ComponentBuilder& builder, size_t input_count) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<SEL_WIDTH>>("SEL_SPLIT");
    w.connect(component.getInputPin<SEL_WIDTH>("SEL"),
        {builder.getInputPin<BitSplitter<SEL_WIDTH>, SEL_WIDTH>("SEL_SPLIT", "IN")});

    std::vector<std::string> previous_level;
    size_t level_input_count = input_count;
    size_t level = 0;

    while (level_input_count > 1) {
        std::vector<std::string> current_level;
        std::vector<std::shared_ptr<Pin<>>> select_sinks;
        const size_t mux_count = level_input_count / 2;

        for (size_t i = 0; i < mux_count; ++i) {
            const auto mux_name = "MUX_L" + std::to_string(level) + "_" + std::to_string(i);
            builder.addNewComponent<Mux<2,1>>(mux_name);
            current_level.push_back(mux_name);
            select_sinks.push_back(builder.getInputPin<Mux<2,1>>(mux_name, "SEL"));

            if (level == 0) {
                w.connect(component.getInputPin(muxInputName(input_count, 2 * i)),
                    {builder.getInputPin<Mux<2,1>>(mux_name, "A")});
                w.connect(component.getInputPin(muxInputName(input_count, 2 * i + 1)),
                    {builder.getInputPin<Mux<2,1>>(mux_name, "B")});
            } else {
                w.connect(builder.getOutputPin<Mux<2,1>>(previous_level[2 * i], "OUT"),
                    {builder.getInputPin<Mux<2,1>>(mux_name, "A")});
                w.connect(builder.getOutputPin<Mux<2,1>>(previous_level[2 * i + 1], "OUT"),
                    {builder.getInputPin<Mux<2,1>>(mux_name, "B")});
            }
        }

        w.connect(builder.getOutputPin<BitSplitter<SEL_WIDTH>>("SEL_SPLIT", "OUT_" + std::to_string(level)),
            select_sinks);

        previous_level = std::move(current_level);
        level_input_count = mux_count;
        ++level;
    }

    w.connect(builder.getOutputPin<Mux<2,1>>(previous_level.front(), "OUT"),
        {component.getOutputPin("OUT")});
    w.finish();
}

template<typename OneBitMuxT, size_t INPUT_COUNT, size_t SEL_WIDTH, size_t BUS_WIDTH>
void buildBusMux(IOComponent& component, ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    for (size_t input_index = 0; input_index < INPUT_COUNT; ++input_index) {
        const auto input_name = muxInputName(INPUT_COUNT, input_index);
        const auto splitter_name = input_name + "_SPLIT";
        builder.addNewComponent<BitSplitter<BUS_WIDTH>>(splitter_name);
        w.connect(component.getInputPin<BUS_WIDTH>(input_name),
            {builder.getInputPin<BitSplitter<BUS_WIDTH>, BUS_WIDTH>(splitter_name, "IN")});
    }

    builder.addNewComponent<BitJoiner<BUS_WIDTH>>("OUT_JOIN");

    if constexpr (INPUT_COUNT == 2 && SEL_WIDTH == 1) {
        builder.addNewComponent<NOTGate>("NOT_SEL");
        std::vector<std::shared_ptr<Pin<>>> selected_sinks{
            builder.getInputPin<NOTGate>("NOT_SEL", "IN")};
        std::vector<std::shared_ptr<Pin<>>> unselected_sinks;
        for (size_t bit = 0; bit < BUS_WIDTH; ++bit) {
            const auto suffix = std::to_string(bit);
            const auto a = "SELECT_A_" + suffix;
            const auto b = "SELECT_B_" + suffix;
            const auto out = "COMBINE_" + suffix;
            builder.addNewComponent<ANDGate>(a);
            builder.addNewComponent<ANDGate>(b);
            builder.addNewComponent<ORGate>(out);
            w.connect(builder.getOutputPin<BitSplitter<BUS_WIDTH>>("A_SPLIT", "OUT_" + suffix),
                {builder.getInputPin<ANDGate>(a, "A")});
            w.connect(builder.getOutputPin<BitSplitter<BUS_WIDTH>>("B_SPLIT", "OUT_" + suffix),
                {builder.getInputPin<ANDGate>(b, "A")});
            selected_sinks.push_back(builder.getInputPin<ANDGate>(b, "B"));
            unselected_sinks.push_back(builder.getInputPin<ANDGate>(a, "B"));
            w.connect(builder.getOutputPin<ANDGate>(a, "OUT"),
                {builder.getInputPin<ORGate>(out, "A")});
            w.connect(builder.getOutputPin<ANDGate>(b, "OUT"),
                {builder.getInputPin<ORGate>(out, "B")});
            w.connect(builder.getOutputPin<ORGate>(out, "OUT"),
                {builder.getInputPin<BitJoiner<BUS_WIDTH>>("OUT_JOIN", "IN_" + suffix)});
        }
        w.connect(component.getInputPin("SEL"), selected_sinks);
        w.connect(builder.getOutputPin<NOTGate>("NOT_SEL", "OUT"),
            unselected_sinks);
        w.connect(builder.getOutputPin<BitJoiner<BUS_WIDTH>, BUS_WIDTH>("OUT_JOIN", "OUT"),
            {component.getOutputPin<BUS_WIDTH>("OUT")});
        w.finish();
        return;
    }

    std::vector<std::shared_ptr<Pin<SEL_WIDTH>>> select_sinks;
    select_sinks.reserve(BUS_WIDTH);

    for (size_t bit = 0; bit < BUS_WIDTH; ++bit) {
        const auto bit_text = std::to_string(bit);
        const auto mux_name = "MUX_BIT_" + bit_text;
        builder.addNewComponent<OneBitMuxT>(mux_name);
        select_sinks.push_back(builder.getInputPin<OneBitMuxT, SEL_WIDTH>(mux_name, "SEL"));

        for (size_t input_index = 0; input_index < INPUT_COUNT; ++input_index) {
            const auto input_name = muxInputName(INPUT_COUNT, input_index);
            const auto splitter_name = input_name + "_SPLIT";
            w.connect(builder.getOutputPin<BitSplitter<BUS_WIDTH>>(splitter_name, "OUT_" + bit_text),
                {builder.getInputPin<OneBitMuxT>(mux_name, muxInputName(INPUT_COUNT, input_index))});
        }

        w.connect(builder.getOutputPin<OneBitMuxT>(mux_name, "OUT"),
            {builder.getInputPin<BitJoiner<BUS_WIDTH>>("OUT_JOIN", "IN_" + bit_text)});
    }

    w.connect(component.getInputPin<SEL_WIDTH>("SEL"),
        select_sinks);

    w.connect(builder.getOutputPin<BitJoiner<BUS_WIDTH>, BUS_WIDTH>("OUT_JOIN", "OUT"),
        {component.getOutputPin<BUS_WIDTH>("OUT")});
    w.finish();
}

template<size_t INPUT_COUNT, size_t SEL_WIDTH, size_t BUS_WIDTH>
void defineBusMuxPins(IOComponent* self) {
    for (size_t input = 0; input < INPUT_COUNT; ++input)
        self->addPin<BUS_WIDTH>(muxInputName(INPUT_COUNT, input), PinType::INPUT);
    self->addPin<SEL_WIDTH>("SEL", PinType::INPUT);
    self->addPin<BUS_WIDTH>("OUT", PinType::OUTPUT);
}

// Each output bit is the same tree of Mux<2,1> the structural form builds:
// level 0 pairs neighbouring inputs on SEL bit 0, level 1 on SEL bit 1, ...
template<size_t INPUT_COUNT, size_t SEL_WIDTH, size_t BUS_WIDTH>
class BusMuxBehavioral final : public BasicComponent {
public:
    BusMuxBehavioral(std::string name, std::string type_name)
        : BasicComponent(std::move(name), 1, defineBusMuxPins<INPUT_COUNT, SEL_WIDTH, BUS_WIDTH>),
          type_name_(type_name) {}
    const char* getTypeName() const override { return type_name_.c_str(); }
    void evaluate(SimulationTime current_time, Simulator& simulator) override {
        const auto select = getInputPin<SEL_WIDTH>("SEL")->getValueAsVector();
        std::vector<std::vector<LogicValue>> inputs;
        inputs.reserve(INPUT_COUNT);
        for (size_t input = 0; input < INPUT_COUNT; ++input)
            inputs.push_back(getInputPin<BUS_WIDTH>(muxInputName(INPUT_COUNT, input))->getValueAsVector());
        std::vector<LogicValue> out(BUS_WIDTH);
        for (size_t bit = 0; bit < BUS_WIDTH; ++bit) {
            std::vector<LogicValue> level(INPUT_COUNT);
            for (size_t input = 0; input < INPUT_COUNT; ++input) level[input] = inputs[input][bit];
            for (size_t stage = 0; level.size() > 1; ++stage) {
                std::vector<LogicValue> next(level.size() / 2);
                for (size_t pair = 0; pair < next.size(); ++pair)
                    next[pair] = circuit::logic_values::mux2(level[2 * pair], level[2 * pair + 1], select[stage]);
                level = std::move(next);
            }
            out[bit] = level.front();
        }
        _updateOutputWire<BUS_WIDTH>(simulator, "OUT", out, current_time);
    }
private:
    std::string type_name_;
};
}

namespace circuit::word {
namespace {
template<size_t W> void buildWordMux(IOComponent& self, ComponentBuilder& builder, size_t inputs) {
    switch(inputs) {
    case 2: buildBusMux<Mux<2,1>,2,1,W>(self,builder); break;
    case 4: buildBusMux<Mux<4,1>,4,2,W>(self,builder); break;
    case 8: buildBusMux<Mux<8,1>,8,3,W>(self,builder); break;
    case 16: buildBusMux<Mux<16,1>,16,4,W>(self,builder); break;
    case 32: buildBusMux<Mux<32,1>,32,5,W>(self,builder); break;
    }
}
}
WordMux::WordMux(std::string name,size_t inputs,size_t width)
    : IOComponent(std::move(name),[inputs,width](IOComponent* p){
        if(inputs!=2 && inputs!=4 && inputs!=8 && inputs!=16 && inputs!=32) throw std::invalid_argument("Invalid mux input count");
        if(width!=1 && width!=4 && width!=8 && width!=16 && width!=32) throw std::invalid_argument("Invalid mux width");
        for(size_t i=0;i<inputs;++i)p->addPinDynamic(muxInputName(inputs,i),PinType::INPUT,width);
        p->addPinDynamic("SEL",PinType::INPUT,std::countr_zero(inputs));
        p->addPinDynamic("OUT",PinType::OUTPUT,width);
    }),inputs_(inputs),width_(width),type_("Mux<"+std::to_string(inputs)+","+std::to_string(width)+">") {}
void WordMux::buildInternals(ComponentBuilder& b) {
    switch(width_) {
    case 1:
        if(inputs_==2)buildMux2FromGates(*this,b);
        else if(inputs_==4)buildOneBitMuxTree<2>(*this,b,inputs_);
        else if(inputs_==8)buildOneBitMuxTree<3>(*this,b,inputs_);
        else if(inputs_==16)buildOneBitMuxTree<4>(*this,b,inputs_);
        else buildOneBitMuxTree<5>(*this,b,inputs_);
        break;
    case 4: buildWordMux<4>(*this,b,inputs_); break;
    case 8: buildWordMux<8>(*this,b,inputs_); break;
    case 16: buildWordMux<16>(*this,b,inputs_); break;
    case 32: buildWordMux<32>(*this,b,inputs_); break;
    }
}
}

namespace circuit::word {
const ComponentFamily& mux(size_t inputs,size_t width){
    if((inputs!=2 && inputs!=4 && inputs!=8 && inputs!=16 && inputs!=32)||(width!=1 && width!=4 && width!=8 && width!=16 && width!=32))
        throw std::invalid_argument("Invalid mux geometry");
    struct Entry {
        std::string id,type;
        ComponentFamily family;
        Entry(size_t n,size_t w):id("logic.mux."+std::to_string(n)+"."+std::to_string(w)),
            type("Mux<"+std::to_string(n)+","+std::to_string(w)+">"),
            family(id,type,[n,w](IOComponent*p){
                for(size_t i=0;i<n;++i)p->addPinDynamic(muxInputName(n,i),PinType::INPUT,w);
                p->addPinDynamic("SEL",PinType::INPUT,std::countr_zero(n));
                p->addPinDynamic("OUT",PinType::OUTPUT,w);
            },[n,w](const std::string& name,const std::shared_ptr<BuildContext>& c){
                return Component::createWithContext<WordMux>(c,name,n,w);
            },w==32?FamilyFactory([n](const std::string& name,const std::shared_ptr<BuildContext>& c)->std::shared_ptr<Component>{
                const auto type=std::string(mux(n,32).typeName());
                switch(n){
                case 2:return Component::createWithContext<BusMuxBehavioral<2,1,32>>(c,name,type);
                case 4:return Component::createWithContext<BusMuxBehavioral<4,2,32>>(c,name,type);
                case 8:return Component::createWithContext<BusMuxBehavioral<8,3,32>>(c,name,type);
                case 16:return Component::createWithContext<BusMuxBehavioral<16,4,32>>(c,name,type);
                default:return Component::createWithContext<BusMuxBehavioral<32,5,32>>(c,name,type);
                }
            }):FamilyFactory{}) {}
    };
    static std::map<std::pair<size_t,size_t>,std::unique_ptr<Entry>> entries;
    auto& e=entries[{inputs,width}];if(!e)e=std::make_unique<Entry>(inputs,width);return e->family;
}
void registerMuxes(ComponentCatalog& c) {
    for(size_t w:{1u,4u,8u,16u,32u})for(size_t n:{2u,4u,8u,16u,32u}) {
        const auto& f=mux(n,w);
        const bool recorded=n!=16 && (w==1 || w==32 || (w==4&&n==2) || (w==8&&n==8));
        const auto test=recorded?"Mux"+std::to_string(n)+"Width"+std::to_string(w)+"Test":"WidthComponentsTest";
        circuit::registration::registerStructural(c,f,test);
        if(f.supports(Fidelity::Behavioral)) {
            circuit::registration::registerBehavioral(c,f,test,test,{},"stable-after-settle");
        }
    }
}
}
