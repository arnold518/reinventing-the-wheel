#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include <string>
#include <vector>
#include <stdexcept>
#include "modules/rv32i/RV32IControlFlowBehavioral.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/memory/Register.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"

namespace {
void defineControlFlowPins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("PC_WRITE", PinType::INPUT);
    self->addPin<32>("RS1_VALUE", PinType::INPUT);
    self->addPin<32>("IMM", PinType::INPUT);
    self->addPin<3>("BRANCH_TYPE", PinType::INPUT);
    self->addPin<2>("JUMP_TYPE", PinType::INPUT);
    self->addPin("EQ", PinType::INPUT);
    self->addPin("LT_SIGNED", PinType::INPUT);
    self->addPin("LT_UNSIGNED", PinType::INPUT);
    self->addPin<32>("PC", PinType::OUTPUT);
    self->addPin<32>("PC_PLUS_4", PinType::OUTPUT);
    self->addPin<32>("NEXT_PC_CANDIDATE", PinType::OUTPUT);
    self->addPin("BRANCH_TAKEN", PinType::OUTPUT);
    self->addPin("PC_MISALIGNED", PinType::OUTPUT);
    self->addPin("TARGET_MISALIGNED", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily RV32IControlFlow{
    "rv32i.control-flow",
    "RV32IControlFlowUnit",
    defineControlFlowPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IControlFlowUnit>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IControlFlowBehavioral>(context, name);
    }};
}

RV32IControlFlowUnit::RV32IControlFlowUnit(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::RV32IControlFlow.pinInitializer()) {}

void RV32IControlFlowUnit::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.add(circuit::families::registerWord(32), "PC_STATE");
    builder.addNewComponent<Adder32>("PC_PLUS_4_ADD");
    builder.addNewComponent<Adder32>("PC_IMM_ADD");
    builder.addNewComponent<Adder32>("RS1_IMM_ADD");
    builder.addNewComponent<Rewire>(
        "JALR_MASK",
        std::vector<Rewire::WireSpec>{{"IN", 32}},
        std::vector<Rewire::WireSpec>{{"OUT", 32}},
        identity_mapping("IN", 1, 31, "OUT", 1),
        Rewire::UnmappedBitValue::LOW);

    builder.addNewComponent<NOTGate>("NOT_EQ");
    builder.addNewComponent<NOTGate>("NOT_LT_SIGNED");
    builder.addNewComponent<NOTGate>("NOT_LT_UNSIGNED");
    builder.addNewComponent<Mux8to1>("BRANCH_MUX");
    builder.addNewComponent<Mux2to1_32bit>("BRANCH_NEXT_MUX");
    builder.addNewComponent<Mux4to1_32bit>("JUMP_NEXT_MUX");

    builder.addNewComponent<BitSplitter<32>>("PC_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("TARGET_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("JUMP_TYPE_SPLIT");
    builder.addNewComponent<ORGate>("PC_LOW_OR");
    builder.addNewComponent<ORGate>("TARGET_LOW_OR");
    builder.addNewComponent<ORGate>("JUMP_ACTIVE_OR");
    builder.addNewComponent<ORGate>("CONTROL_TRANSFER_OR");
    builder.addNewComponent<ANDGate>("TARGET_MISALIGNED_AND");

    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<32>>("CONST_FOUR", 4);
    builder.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);

    w.connect(getInputPin("CLK"), {builder.getInputPin<IOComponent>("PC_STATE", "CLK")});
    w.connect(getInputPin("RST"), {builder.getInputPin<IOComponent>("PC_STATE", "RST")});
    w.connect(getInputPin("PC_WRITE"), {builder.getInputPin<IOComponent>("PC_STATE", "WE")});

    w.connect(builder.getOutputPin<IOComponent, 32>("PC_STATE", "Q"),
        {getOutputPin<32>("PC"), builder.getInputPin<Adder32, 32>("PC_PLUS_4_ADD", "A"),
         builder.getInputPin<Adder32, 32>("PC_IMM_ADD", "A"),
         builder.getInputPin<BitSplitter<32>, 32>("PC_SPLIT", "IN")});
    w.connect(getInputPin<32>("IMM"),
        {builder.getInputPin<Adder32, 32>("PC_IMM_ADD", "B"), builder.getInputPin<Adder32, 32>("RS1_IMM_ADD", "B")});
    w.connect(getInputPin<32>("RS1_VALUE"),
        {builder.getInputPin<Adder32, 32>("RS1_IMM_ADD", "A")});
    w.connect(builder.getOutputPin<ConstantValue<32>, 32>("CONST_FOUR", "OUT"),
        {builder.getInputPin<Adder32, 32>("PC_PLUS_4_ADD", "B")});

    w.connect(builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder32>("PC_PLUS_4_ADD", "Cin"),
         builder.getInputPin<Adder32>("PC_IMM_ADD", "Cin"), builder.getInputPin<Adder32>("RS1_IMM_ADD", "Cin"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN0"), builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN7")});

    w.connect(builder.getOutputPin<Adder32, 32>("PC_PLUS_4_ADD", "Sum"),
        {getOutputPin<32>("PC_PLUS_4"), builder.getInputPin<Mux2to1_32bit, 32>("BRANCH_NEXT_MUX", "A")});
    w.connect(builder.getOutputPin<Adder32, 32>("PC_IMM_ADD", "Sum"),
        {builder.getInputPin<Mux2to1_32bit, 32>("BRANCH_NEXT_MUX", "B"),
         builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN1")});
    w.connect(builder.getOutputPin<Adder32, 32>("RS1_IMM_ADD", "Sum"),
        {builder.getInputPin<Rewire, 32>("JALR_MASK", "IN")});
    w.connect(builder.getOutputPin<Rewire, 32>("JALR_MASK", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN2")});
    w.connect(builder.getOutputPin<ConstantValue<32>, 32>("CONST_ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN3")});

    w.connect(getInputPin("EQ"),
        {builder.getInputPin<NOTGate>("NOT_EQ", "IN"), builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN1")});
    w.connect(getInputPin("LT_SIGNED"),
        {builder.getInputPin<NOTGate>("NOT_LT_SIGNED", "IN"), builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN3")});
    w.connect(getInputPin("LT_UNSIGNED"),
        {builder.getInputPin<NOTGate>("NOT_LT_UNSIGNED", "IN"), builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN5")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_EQ", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN2")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_LT_SIGNED", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN4")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_LT_UNSIGNED", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN6")});
    w.connect(getInputPin<3>("BRANCH_TYPE"),
        {builder.getInputPin<Mux8to1, 3>("BRANCH_MUX", "SEL")});
    w.connect(builder.getOutputPin<Mux8to1>("BRANCH_MUX", "OUT"),
        {getOutputPin("BRANCH_TAKEN"), builder.getInputPin<Mux2to1_32bit>("BRANCH_NEXT_MUX", "SEL"),
         builder.getInputPin<ORGate>("CONTROL_TRANSFER_OR", "A")});

    w.connect(builder.getOutputPin<Mux2to1_32bit, 32>("BRANCH_NEXT_MUX", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN0")});
    w.connect(getInputPin<2>("JUMP_TYPE"),
        {builder.getInputPin<Mux4to1_32bit, 2>("JUMP_NEXT_MUX", "SEL"),
         builder.getInputPin<BitSplitter<2>, 2>("JUMP_TYPE_SPLIT", "IN")});
    w.connect(builder.getOutputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "OUT"),
        {getOutputPin<32>("NEXT_PC_CANDIDATE"), builder.getInputPin<IOComponent, 32>("PC_STATE", "D"),
         builder.getInputPin<BitSplitter<32>, 32>("TARGET_SPLIT", "IN")});

    w.connect(builder.getOutputPin<BitSplitter<32>>("PC_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("PC_LOW_OR", "A")});
    w.connect(builder.getOutputPin<BitSplitter<32>>("PC_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("PC_LOW_OR", "B")});
    w.connect(builder.getOutputPin<ORGate>("PC_LOW_OR", "OUT"),
        {getOutputPin("PC_MISALIGNED")});

    w.connect(builder.getOutputPin<BitSplitter<32>>("TARGET_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("TARGET_LOW_OR", "A")});
    w.connect(builder.getOutputPin<BitSplitter<32>>("TARGET_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("TARGET_LOW_OR", "B")});
    w.connect(builder.getOutputPin<BitSplitter<2>>("JUMP_TYPE_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("JUMP_ACTIVE_OR", "A")});
    w.connect(builder.getOutputPin<BitSplitter<2>>("JUMP_TYPE_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("JUMP_ACTIVE_OR", "B")});
    w.connect(builder.getOutputPin<ORGate>("JUMP_ACTIVE_OR", "OUT"),
        {builder.getInputPin<ORGate>("CONTROL_TRANSFER_OR", "B")});
    w.connect(builder.getOutputPin<ORGate>("TARGET_LOW_OR", "OUT"),
        {builder.getInputPin<ANDGate>("TARGET_MISALIGNED_AND", "A")});
    w.connect(builder.getOutputPin<ORGate>("CONTROL_TRANSFER_OR", "OUT"),
        {builder.getInputPin<ANDGate>("TARGET_MISALIGNED_AND", "B")});
    w.connect(builder.getOutputPin<ANDGate>("TARGET_MISALIGNED_AND", "OUT"),
        {getOutputPin("TARGET_MISALIGNED")});
    w.finish();
}
