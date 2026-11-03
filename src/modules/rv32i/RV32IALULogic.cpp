#include "components/ComponentBuilder.tpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/composite/WordSliceBuilder.hpp"
namespace rv32i::alu_detail {
// Three word gates on the whole operands: each is 32 one-bit gates.
void integerLogic(IOComponent& root,ComponentBuilder& builder) {
    using circuit::word::GateKind;
    circuit::detail::WordSliceBuilder n(root, builder);
    const auto a = n.input("A"), b = n.input("B");
    n.connect(n.wordGate(GateKind::And, "AND", a, b), n.output("AND_OUT"));
    n.connect(n.wordGate(GateKind::Or, "OR", a, b), n.output("OR_OUT"));
    n.connect(n.wordGate(GateKind::Xor, "XOR", a, b), n.output("XOR_OUT"));
    n.finish();
}
}
void RV32IALULogic::buildInternals(ComponentBuilder& b) { rv32i::alu_detail::integerLogic(*this,b); }
