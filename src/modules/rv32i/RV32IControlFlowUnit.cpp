#include "modules/rv32i/RV32IControlFlowUnit.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"

RV32IControlFlowUnit::RV32IControlFlowUnit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
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
      }) {}

void RV32IControlFlowUnit::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Register32>("PC_STATE");
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

    builder.addNewComponent<ConstantValue<1, 32>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<32, 32>>("CONST_FOUR", 4);
    builder.addNewComponent<ConstantValue<32, 32>>("CONST_ZERO32", 0);

    builder.addNewWire(
        "CLK_to_PC",
        getInputPin("CLK"),
        {builder.getInputPin<Register32>("PC_STATE", "CLK")});
    builder.addNewWire(
        "RST_to_PC",
        getInputPin("RST"),
        {builder.getInputPin<Register32>("PC_STATE", "RST")});
    builder.addNewWire(
        "PC_WRITE_to_PC",
        getInputPin("PC_WRITE"),
        {builder.getInputPin<Register32>("PC_STATE", "WE")});

    builder.addNewWire<32>(
        "PC_fanout",
        builder.getOutputPin<Register32, 32>("PC_STATE", "Q"),
        {getOutputPin<32>("PC"),
         builder.getInputPin<Adder32, 32>("PC_PLUS_4_ADD", "A"),
         builder.getInputPin<Adder32, 32>("PC_IMM_ADD", "A"),
         builder.getInputPin<BitSplitter<32>, 32>("PC_SPLIT", "IN")});
    builder.addNewWire<32>(
        "IMM_fanout",
        getInputPin<32>("IMM"),
        {builder.getInputPin<Adder32, 32>("PC_IMM_ADD", "B"),
         builder.getInputPin<Adder32, 32>("RS1_IMM_ADD", "B")});
    builder.addNewWire<32>(
        "RS1_to_JALR_ADD",
        getInputPin<32>("RS1_VALUE"),
        {builder.getInputPin<Adder32, 32>("RS1_IMM_ADD", "A")});
    builder.addNewWire<32>(
        "CONST_FOUR_to_ADD",
        builder.getOutputPin<ConstantValue<32, 32>, 32>("CONST_FOUR", "OUT"),
        {builder.getInputPin<Adder32, 32>("PC_PLUS_4_ADD", "B")});

    builder.addNewWire(
        "CONST_LOW_fanout",
        builder.getOutputPin<ConstantValue<1, 32>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder32>("PC_PLUS_4_ADD", "Cin"),
         builder.getInputPin<Adder32>("PC_IMM_ADD", "Cin"),
         builder.getInputPin<Adder32>("RS1_IMM_ADD", "Cin"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN0"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN7")});

    builder.addNewWire<32>(
        "PC_PLUS_4_fanout",
        builder.getOutputPin<Adder32, 32>("PC_PLUS_4_ADD", "Sum"),
        {getOutputPin<32>("PC_PLUS_4"),
         builder.getInputPin<Mux2to1_32bit, 32>("BRANCH_NEXT_MUX", "A")});
    builder.addNewWire<32>(
        "PC_IMM_fanout",
        builder.getOutputPin<Adder32, 32>("PC_IMM_ADD", "Sum"),
        {builder.getInputPin<Mux2to1_32bit, 32>("BRANCH_NEXT_MUX", "B"),
         builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN1")});
    builder.addNewWire<32>(
        "RS1_IMM_to_JALR_MASK",
        builder.getOutputPin<Adder32, 32>("RS1_IMM_ADD", "Sum"),
        {builder.getInputPin<Rewire, 32>("JALR_MASK", "IN")});
    builder.addNewWire<32>(
        "JALR_MASK_to_JUMP_MUX",
        builder.getOutputPin<Rewire, 32>("JALR_MASK", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN2")});
    builder.addNewWire<32>(
        "CONST_ZERO_to_JUMP_MUX",
        builder.getOutputPin<ConstantValue<32, 32>, 32>("CONST_ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN3")});

    builder.addNewWire("EQ_fanout", getInputPin("EQ"),
        {builder.getInputPin<NOTGate>("NOT_EQ", "IN"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN1")});
    builder.addNewWire("LT_SIGNED_fanout", getInputPin("LT_SIGNED"),
        {builder.getInputPin<NOTGate>("NOT_LT_SIGNED", "IN"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN3")});
    builder.addNewWire("LT_UNSIGNED_fanout", getInputPin("LT_UNSIGNED"),
        {builder.getInputPin<NOTGate>("NOT_LT_UNSIGNED", "IN"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN5")});
    builder.addNewWire("NOT_EQ_to_branch", builder.getOutputPin<NOTGate>("NOT_EQ", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN2")});
    builder.addNewWire("NOT_LT_SIGNED_to_branch", builder.getOutputPin<NOTGate>("NOT_LT_SIGNED", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN4")});
    builder.addNewWire("NOT_LT_UNSIGNED_to_branch", builder.getOutputPin<NOTGate>("NOT_LT_UNSIGNED", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN6")});
    builder.addNewWire<3>(
        "BRANCH_TYPE_to_mux",
        getInputPin<3>("BRANCH_TYPE"),
        {builder.getInputPin<Mux8to1, 3>("BRANCH_MUX", "SEL")});
    builder.addNewWire(
        "BRANCH_TAKEN_fanout",
        builder.getOutputPin<Mux8to1>("BRANCH_MUX", "OUT"),
        {getOutputPin("BRANCH_TAKEN"),
         builder.getInputPin<Mux2to1_32bit>("BRANCH_NEXT_MUX", "SEL"),
         builder.getInputPin<ORGate>("CONTROL_TRANSFER_OR", "A")});

    builder.addNewWire<32>(
        "BRANCH_NEXT_to_JUMP_MUX",
        builder.getOutputPin<Mux2to1_32bit, 32>("BRANCH_NEXT_MUX", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "IN0")});
    builder.addNewWire<2>(
        "JUMP_TYPE_fanout",
        getInputPin<2>("JUMP_TYPE"),
        {builder.getInputPin<Mux4to1_32bit, 2>("JUMP_NEXT_MUX", "SEL"),
         builder.getInputPin<BitSplitter<2>, 2>("JUMP_TYPE_SPLIT", "IN")});
    builder.addNewWire<32>(
        "NEXT_PC_fanout",
        builder.getOutputPin<Mux4to1_32bit, 32>("JUMP_NEXT_MUX", "OUT"),
        {getOutputPin<32>("NEXT_PC_CANDIDATE"),
         builder.getInputPin<Register32, 32>("PC_STATE", "D"),
         builder.getInputPin<BitSplitter<32>, 32>("TARGET_SPLIT", "IN")});

    builder.addNewWire("PC_bit0_to_align", builder.getOutputPin<BitSplitter<32>>("PC_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("PC_LOW_OR", "A")});
    builder.addNewWire("PC_bit1_to_align", builder.getOutputPin<BitSplitter<32>>("PC_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("PC_LOW_OR", "B")});
    builder.addNewWire("PC_align_to_output", builder.getOutputPin<ORGate>("PC_LOW_OR", "OUT"),
        {getOutputPin("PC_MISALIGNED")});

    builder.addNewWire("TARGET_bit0_to_align", builder.getOutputPin<BitSplitter<32>>("TARGET_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("TARGET_LOW_OR", "A")});
    builder.addNewWire("TARGET_bit1_to_align", builder.getOutputPin<BitSplitter<32>>("TARGET_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("TARGET_LOW_OR", "B")});
    builder.addNewWire("JUMP_bit0_to_active", builder.getOutputPin<BitSplitter<2>>("JUMP_TYPE_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("JUMP_ACTIVE_OR", "A")});
    builder.addNewWire("JUMP_bit1_to_active", builder.getOutputPin<BitSplitter<2>>("JUMP_TYPE_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("JUMP_ACTIVE_OR", "B")});
    builder.addNewWire("JUMP_active_to_transfer", builder.getOutputPin<ORGate>("JUMP_ACTIVE_OR", "OUT"),
        {builder.getInputPin<ORGate>("CONTROL_TRANSFER_OR", "B")});
    builder.addNewWire("TARGET_low_to_final", builder.getOutputPin<ORGate>("TARGET_LOW_OR", "OUT"),
        {builder.getInputPin<ANDGate>("TARGET_MISALIGNED_AND", "A")});
    builder.addNewWire("TRANSFER_active_to_final", builder.getOutputPin<ORGate>("CONTROL_TRANSFER_OR", "OUT"),
        {builder.getInputPin<ANDGate>("TARGET_MISALIGNED_AND", "B")});
    builder.addNewWire("TARGET_misaligned_to_output", builder.getOutputPin<ANDGate>("TARGET_MISALIGNED_AND", "OUT"),
        {getOutputPin("TARGET_MISALIGNED")});
}
