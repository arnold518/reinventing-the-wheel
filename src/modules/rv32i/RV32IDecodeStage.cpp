#include "modules/rv32i/RV32IPipelineStages.hpp"
#include <string>
#include <vector>
#include <stdexcept>

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "RV32IPipelineStagesInternal.hpp"

using namespace rv32i::pipeline_stages;

RV32IDecodeStage::RV32IDecodeStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IDecodeStage.pinInitializer()) {}

void RV32IDecodeStage::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    const auto decode = builder.add(
        circuit::families::RV32IDecodeControl, "CONTROL");
    register_file_ = builder.add(
        circuit::families::RegisterFile32x32, "REGISTER_FILE");
    const auto hazard = builder.add(
        circuit::families::RV32IHazardDetectionUnit, "HAZARD");
    const auto bypass = builder.addNewComponent<RV32IWritebackBypass>("WB_BYPASS");
    builder.addNewComponent<Mux2to1_32bit>("RS1_MUX");
    builder.addNewComponent<Mux2to1_32bit>("RS2_MUX");
    addControlPack(builder, "CONTROL_PACK");
    addControlUnpack(builder, "EX_CONTROL_UNPACK");
    addAddressPack(builder, "ADDRESSES_PACK");
    addAddressUnpack(builder, "EX_ADDRESSES_UNPACK");
    addStatusUnpack(
        builder,
        "FETCH_STATUS_UNPACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});
    builder.addNewComponent<NOTGate>("NOT_LEGAL");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_1");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_2");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_3");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_4");
    builder.addNewComponent<ANDGate>("TERMINAL");

    w.connect(getInputPin("CLK"), {register_file_->getInputPin("CLK")});
    w.connect(getInputPin("RST"), {register_file_->getInputPin("RST")});
    w.connect(getInputPin("IN_VALID"),
        {getOutputPin("OUT_VALID"), hazard->getInputPin("ID_VALID"),
         builder.getInputPin<ANDGate>("TERMINAL", "A")});
    w.connect(getInputPin<32>("IN_PC"), {getOutputPin<32>("OUT_PC")});
    w.connect(getInputPin<32>("IN_INSTRUCTION"),
        {getOutputPin<32>("OUT_INSTRUCTION"), decode->getInputPin<32>("INSTRUCTION")});
    w.connect(getInputPin<32>("IN_FETCH_STATUS"),
        {getOutputPin<32>("OUT_FETCH_STATUS"),
         builder.getInputPin<Rewire, 32>("FETCH_STATUS_UNPACK", "STATUS")});
    w.connect(getInputPin<32>("EX_CONTROL"),
        {builder.getInputPin<Rewire, 32>("EX_CONTROL_UNPACK", "CONTROL")});
    w.connect(getInputPin<32>("EX_REGISTER_ADDRESSES"),
        {builder.getInputPin<Rewire, 32>("EX_ADDRESSES_UNPACK", "ADDRESSES")});
    w.connect(getInputPin("EX_VALID"), {hazard->getInputPin("EX_VALID")});
    w.connect(getInputPin("WB_VALID"), {bypass->getInputPin("WB_VALID")});
    w.connect(getInputPin("WB_REG_WRITE"), {bypass->getInputPin("WB_REG_WRITE")});
    w.connect(getInputPin("WB_REGISTER_WRITE"), {register_file_->getInputPin("REG_WRITE")});
    w.connect(getInputPin<5>("WB_RD"),
        {bypass->getInputPin<5>("WB_RD"), register_file_->getInputPin<5>("RD_ADDR")});
    w.connect(getInputPin<32>("WB_VALUE"),
        {builder.getInputPin<Mux2to1_32bit, 32>("RS1_MUX", "B"),
         builder.getInputPin<Mux2to1_32bit, 32>("RS2_MUX", "B"),
         register_file_->getInputPin<32>("WRITE_DATA")});

    w.connect(decode->getOutputPin<5>("RS1_ADDR"),
        {register_file_->getInputPin<5>("RS1_ADDR"), hazard->getInputPin<5>("ID_RS1"),
         bypass->getInputPin<5>("RS1"), builder.getInputPin<Rewire, 5>("ADDRESSES_PACK", "RS1")});
    w.connect(decode->getOutputPin<5>("RS2_ADDR"),
        {register_file_->getInputPin<5>("RS2_ADDR"), hazard->getInputPin<5>("ID_RS2"),
         bypass->getInputPin<5>("RS2"), builder.getInputPin<Rewire, 5>("ADDRESSES_PACK", "RS2")});
    w.connect(decode->getOutputPin<5>("RD_ADDR"), {builder.getInputPin<Rewire, 5>("ADDRESSES_PACK", "RD")});
    w.connect(builder.getOutputPin<Rewire, 5>("EX_ADDRESSES_UNPACK", "RD"),
        {hazard->getInputPin<5>("EX_RD")});
    w.connect(builder.getOutputPin<Rewire>("EX_CONTROL_UNPACK", "MEM_READ"),
        {hazard->getInputPin("EX_MEM_READ")});

    const std::array<std::pair<const char*, size_t>, 17> fields{{
        {"LEGAL", 1}, {"REG_WRITE", 1}, {"MEM_READ", 1},
        {"MEM_WRITE", 1}, {"LOAD_SIGN_EXTEND", 1},
        {"HALT_REQUEST", 1}, {"TRAP_REQUEST", 1},
        {"USES_RS1", 1}, {"USES_RS2", 1},
        {"WRITEBACK_SEL", 2}, {"MEM_SIZE", 2},
        {"BRANCH_TYPE", 3}, {"JUMP_TYPE", 2},
        {"DECODE_TRAP_CAUSE", 4}, {"ALU_OP", 5},
        {"ALU_A_SEL", 2}, {"ALU_B_SEL", 2},
    }};
    const auto pack = builder.getComponent<Rewire>("CONTROL_PACK");
    for (const auto& [field_name, width] : fields) {
        std::vector<std::shared_ptr<PinBase>> sinks{
            pack->getInputPinDynamic(field_name)};
        if (std::string(field_name) == "USES_RS1") {
            sinks.push_back(hazard->getInputPin("ID_USES_RS1"));
        } else if (std::string(field_name) == "USES_RS2") {
            sinks.push_back(hazard->getInputPin("ID_USES_RS2"));
        }
        if (std::string(field_name) == "LEGAL") {
            sinks.push_back(
                builder.getInputPin<NOTGate>("NOT_LEGAL", "IN"));
        } else if (std::string(field_name) == "TRAP_REQUEST") {
            sinks.push_back(
                builder.getInputPin<ORGate>(
                    "TERMINAL_REASON_3", "B"));
        } else if (std::string(field_name) == "HALT_REQUEST") {
            sinks.push_back(
                builder.getInputPin<ORGate>(
                    "TERMINAL_REASON_4", "B"));
        }
        w.connect(decode->getOutputPinDynamic(field_name),
            sinks);
    }
    w.connect(decode->getOutputPin<32>("IMM"), {getOutputPin<32>("OUT_IMMEDIATE")});
    w.connect(builder.getOutputPin<Rewire, 32>("CONTROL_PACK", "CONTROL"),
        {getOutputPin<32>("OUT_CONTROL")});
    w.connect(builder.getOutputPin<Rewire, 32>("ADDRESSES_PACK", "ADDRESSES"),
        {getOutputPin<32>("OUT_REGISTER_ADDRESSES")});
    w.connect(register_file_->getOutputPin<32>("RS1_DATA"),
        {builder.getInputPin<Mux2to1_32bit, 32>("RS1_MUX", "A")});
    w.connect(register_file_->getOutputPin<32>("RS2_DATA"),
        {builder.getInputPin<Mux2to1_32bit, 32>("RS2_MUX", "A")});
    w.connect(bypass->getOutputPin("SELECT_A"),
        {builder.getInputPin<Mux2to1_32bit>("RS1_MUX", "SEL")});
    w.connect(bypass->getOutputPin("SELECT_B"),
        {builder.getInputPin<Mux2to1_32bit>("RS2_MUX", "SEL")});
    w.connect(builder.getOutputPin<Mux2to1_32bit, 32>("RS1_MUX", "OUT"),
        {getOutputPin<32>("OUT_RS1_VALUE")});
    w.connect(builder.getOutputPin<Mux2to1_32bit, 32>("RS2_MUX", "OUT"),
        {getOutputPin<32>("OUT_RS2_VALUE")});
    w.connect(hazard->getOutputPin("LOAD_USE_HAZARD"), {getOutputPin("LOAD_USE_HAZARD")});

    w.connect(builder.getOutputPin<Rewire>("FETCH_STATUS_UNPACK", "PC_MISALIGNED"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_1", "A")});
    w.connect(builder.getOutputPin<Rewire>("FETCH_STATUS_UNPACK", "INSTRUCTION_FAULT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_1", "B")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_LEGAL", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_2", "B")});
    w.connect(builder.getOutputPin<ORGate>("TERMINAL_REASON_1", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_2", "A")});
    w.connect(builder.getOutputPin<ORGate>("TERMINAL_REASON_2", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_3", "A")});
    w.connect(builder.getOutputPin<ORGate>("TERMINAL_REASON_3", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_4", "A")});
    w.connect(builder.getOutputPin<ORGate>("TERMINAL_REASON_4", "OUT"),
        {builder.getInputPin<ANDGate>("TERMINAL", "B")});
    w.connect(builder.getOutputPin<ANDGate>("TERMINAL", "OUT"), {getOutputPin("TERMINAL")});
    w.finish();
}

std::vector<std::vector<LogicValue>>
RV32IDecodeStage::getRegisterStateAtTime(size_t time) const {
    const auto view =
        std::dynamic_pointer_cast<RegisterStateView>(register_file_);
    if (!view) {
        throw std::logic_error(
            "Selected decode-stage register file lacks observation");
    }
    return view->getRegisterStateAtTime(time);
}
