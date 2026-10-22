#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordDatapath.hpp"
#include "modules/composite/WordBehavioral.hpp"
#include "modules/composite/WordSliceBuilder.hpp"
#include "modules/basic/Decoder.hpp"
namespace circuit::word {
namespace {
void controlExtended(IOComponent& root,ComponentBuilder& builder){
    using Net = circuit::detail::WordSliceBuilder;
    Net n(root, builder);
    auto decoder = n.add<Decoder<5>>("OPERATION_DECODER");
    n.connect(n.input("OP"), decoder->getInputPin<5>("ADDR"));
    n.connect(n.constant<1>("HIGH", 1), decoder->getInputPin("ENABLE"));
    auto decoded = [&](size_t i) -> Net::Signal { return decoder->getOutputPin("OUT" + std::to_string(i)); };
    auto group = [&](const std::string& name, size_t begin, size_t end) {
        std::vector<Net::Signal> bits;
        for (size_t i = begin; i <= end; ++i) bits.push_back(decoded(i));
        return n.any(name, bits);
    };
    auto arithmetic = group("ARITHMETIC_ENABLE", 0, 6);
    auto logic = group("LOGIC_ENABLE", 8, 14);
    auto shift = group("SHIFT_ENABLE", 16, 20);
    auto compare = group("COMPARE_ENABLE", 21, 27);
    n.connect(n.any("INVALID_DECODE", {decoded(7), decoded(15), decoded(31)}), n.output("INVALID_OP"));
    auto op_bits = n.split<5>("OP_SPLIT", n.input("OP"));
    std::array<Net::Signal, 3> low_op{op_bits[0], op_bits[1], op_bits[2]};
    auto local_op = n.join<3>("LOCAL_OP", low_op);

    // Compare operations reuse the same subtraction hardware as SUB/SBC.
    auto low = n.constant<1>("LOW", 0), high = n.constant<1>("SUB_BIT", 1);
    std::array<Net::Signal, 3> arithmetic_op;
    for (size_t i = 0; i < 3; ++i)
        arithmetic_op[i] = n.mux("ARITH_OP_" + std::to_string(i), low_op[i], i == 1 ? high : low, compare);
    n.connect(n.join<3>("ARITH_OP_JOIN", arithmetic_op), n.output("ARITH_OP"));
    std::array<Net::Signal, 3> compare_op;
    for (size_t bit = 0; bit < 3; ++bit) {
        std::vector<Net::Signal> sources;
        for (size_t i = 0; i < 7; ++i) if ((i >> bit) & 1) sources.push_back(decoded(21+i));
        compare_op[bit] = n.any("COMPARE_OP_" + std::to_string(bit), sources);
    }
    n.connect(n.join<3>("COMPARE_OP_JOIN", compare_op), n.output("COMPARE_OP"));
    n.connect(local_op, n.output("LOCAL_OP"));
    n.connect(arithmetic, n.output("ARITHMETIC_ENABLE"));
    n.connect(logic, n.output("LOGIC_ENABLE"));
    n.connect(shift, n.output("SHIFT_ENABLE"));
    n.connect(compare, n.output("COMPARE_ENABLE"));
    n.connect(decoded(28), n.output("PASS_A_ENABLE"));
    n.connect(decoded(29), n.output("PASS_B_ENABLE"));
    n.finish();
}
void controlIntegerDatapath(IOComponent& root,ComponentBuilder& builder){
    circuit::detail::WordSliceBuilder n(root, builder);
    auto op = n.split<5>("OP_SPLIT", n.input("OP"));
    std::array<circuit::detail::WordSliceBuilder::Signal, 3> low{op[0], op[1], op[2]};
    n.connect(n.join<3>("OP_LOW_JOIN", low), n.output("LOCAL_OP"));
    n.connect(op[3], n.output("HIGH_HALF"));
    n.connect(op[4], n.output("OUT_OF_RANGE"));
    auto not4 = n.invert("NOT_OP4", op[4]);
    auto not2 = n.invert("NOT_OP2", op[2]);
    auto not1 = n.invert("NOT_OP1", op[1]);
    auto selector = n.binary<ORGate>("SUB_SELECTOR_OR", op[3], op[0]);
    auto valid1 = n.binary<ANDGate>("ARITHMETIC_VALID_1", not4, not2);
    auto valid = n.binary<ANDGate>("ARITHMETIC_VALID_2", valid1, not1);
    n.connect(valid, n.output("ARITHMETIC_ENABLE"));
    n.connect(n.binary<ANDGate>("SUB_CONTROL", valid, selector), n.output("SUB"));
    n.connect(not1, n.output("SHIFT_LEFT"));
    n.connect(op[0], n.output("SHIFT_ARITHMETIC"));
    n.finish();
}
}
const ComponentFamily& controlFamily(Operations operations) {
    static const ComponentFamily extended{"word.control.extended","ALUControlDecoder<extended>",[](IOComponent* p){
        p->addPin<5>("OP",PinType::INPUT);
        for(auto name:{"LOCAL_OP","ARITH_OP","COMPARE_OP"})p->addPin<3>(name,PinType::OUTPUT);
        for(auto name:{"ARITHMETIC_ENABLE","LOGIC_ENABLE","SHIFT_ENABLE","COMPARE_ENABLE","PASS_A_ENABLE","PASS_B_ENABLE","INVALID_OP"})p->addPin(name,PinType::OUTPUT);
    },[](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<ControlDecoder>(c,n,Operations::Extended);},controlBehavioralFactory(Operations::Extended)};
    static const ComponentFamily integer{"word.control.integer","ALUControlDecoder<integer>",[](IOComponent* p){
        p->addPin<5>("OP",PinType::INPUT);p->addPin<3>("LOCAL_OP",PinType::OUTPUT);
        for(auto name:{"HIGH_HALF","OUT_OF_RANGE","ARITHMETIC_ENABLE","SUB","SHIFT_LEFT","SHIFT_ARITHMETIC"})p->addPin(name,PinType::OUTPUT);
    },[](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<ControlDecoder>(c,n,Operations::IntegerDatapath);}};
    if(operations==Operations::Extended)return extended;
    if(operations==Operations::IntegerDatapath)return integer;
    throw std::invalid_argument("Invalid operation decoding contract");
}
ControlDecoder::ControlDecoder(std::string n,Operations o):IOComponent(std::move(n),controlFamily(o).pinInitializer()),operations_(o),
    type_(o==Operations::Extended?"ALUControlDecoder<extended>":"ALUControlDecoder<integer>"){}
void ControlDecoder::buildInternals(ComponentBuilder& b){buildControl(*this,b,operations_);}
void buildControl(IOComponent& root,ComponentBuilder& b,Operations o){if(o==Operations::Extended)controlExtended(root,b);else if(o==Operations::IntegerDatapath)controlIntegerDatapath(root,b);else throw std::invalid_argument("Invalid operation decoding contract");}
}
