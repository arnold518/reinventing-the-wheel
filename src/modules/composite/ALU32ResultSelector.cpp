#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"
#include "components/StructuralNetBuilder.hpp"

namespace {
// A visible bit slice keeps the selection network expandable without putting
// hundreds of gates in the selector's top-level view.
class ALU32ResultBit : public IOComponent {
public:
    explicit ALU32ResultBit(std::string name) : IOComponent(std::move(name), [](IOComponent* s) {
        for (unsigned i=0;i<9;++i) {
            s->addPin("DATA_"+std::to_string(i),PinType::INPUT);
            s->addPin("SELECT_"+std::to_string(i),PinType::INPUT);
        }
        s->addPin("OUT",PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override { return "ALU32ResultBit"; }
    void buildInternals(ComponentBuilder& b) override {
        circuit::detail::StructuralWiring w(b);
        std::vector<decltype(w)::PinPtr> terms;
        for (unsigned i=0;i<9;++i) {
            const auto id=std::to_string(i);
            terms.push_back(w.both("MASK_"+id,getInputPin("DATA_"+id),getInputPin("SELECT_"+id)));
        }
        w.connect(w.reduce("RESULT",terms,false),getOutputPin("OUT"));w.finish();
    }
};
}

BEGIN_PINS(ALU32ResultSelector, IOComponent)
    INPUT_PIN_WIDTH("ARITHMETIC_RESULT", 32)
    INPUT_PIN_WIDTH("AND_RESULT", 32)
    INPUT_PIN_WIDTH("OR_RESULT", 32)
    INPUT_PIN_WIDTH("XOR_RESULT", 32)
    INPUT_PIN_WIDTH("SHIFT_RESULT", 32)
    INPUT_PIN_WIDTH("SLT_RESULT", 32)
    INPUT_PIN_WIDTH("SLTU_RESULT", 32)
    INPUT_PIN_WIDTH("A", 32)
    INPUT_PIN_WIDTH("B", 32)
    INPUT_PIN_WIDTH("LOCAL_OP", 3)
    INPUT_PIN("HIGH_HALF")
    INPUT_PIN("OUT_OF_RANGE")
    OUTPUT_PIN_WIDTH("OUT", 32)
END_PINS()

void ALU32ResultSelector::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralBuilder n(*this, builder);
    auto local=n.split<3>("LOCAL_BITS",n.input("LOCAL_OP"));
    std::array<circuit::detail::StructuralBuilder::Signal,4> op{
        local[0],local[1],local[2],n.input("HIGH_HALF")}, inverse;
    for(unsigned i=0;i<4;++i) inverse[i]=n.invert("NOT_OP_"+std::to_string(i),op[i]);
    auto valid=n.invert("IN_RANGE",n.input("OUT_OF_RANGE"));
    std::array<circuit::detail::StructuralBuilder::Signal,12> terms;
    for(unsigned code=0;code<terms.size();++code) {
        std::vector<circuit::detail::StructuralBuilder::Signal> factors;
        for(unsigned bit=0;bit<4;++bit) factors.push_back(code&(1u<<bit)?op[bit]:inverse[bit]);
        factors.push_back(valid);
        terms[code]=n.reduceBalanced("OP_"+std::to_string(code),factors,true);
    }
    // Keep every minterm, including repeated arithmetic/shift inputs. Dropping
    // (s | !s) would change the original gate mux's X/Z select semantics.
    std::array<circuit::detail::StructuralBuilder::Signal,9> selects{
        n.binary<ORGate>("ARITHMETIC_SELECT",terms[0],terms[1]),terms[2],terms[3],terms[4],
        n.reduceBalanced("SHIFT_SELECT",{terms[5],terms[6],terms[7]},false),
        terms[8],terms[9],terms[10],terms[11]};
    const std::array<const char*,9> names{"ARITHMETIC_RESULT","AND_RESULT","OR_RESULT",
        "XOR_RESULT","SHIFT_RESULT","SLT_RESULT","SLTU_RESULT","A","B"};
    std::array<std::array<circuit::detail::StructuralBuilder::Signal,32>,9> data;
    for(unsigned i=0;i<names.size();++i) data[i]=n.split<32>(std::string(names[i])+"_BITS",n.input(names[i]));
    std::array<circuit::detail::StructuralBuilder::Signal,32> result;
    for(unsigned bit=0;bit<32;++bit) {
        auto slice=n.add<ALU32ResultBit>("BIT_"+std::to_string(bit));
        for(unsigned i=0;i<names.size();++i) {
            n.connect(data[i][bit],slice->getInputPin("DATA_"+std::to_string(i)));
            n.connect(selects[i],slice->getInputPin("SELECT_"+std::to_string(i)));
        }
        result[bit]=slice->getOutputPin("OUT");
    }
    n.connect(n.join<32>("RESULT",result),n.output("OUT"));
    n.finish();
}
