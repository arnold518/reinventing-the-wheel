#include "modules/rv32i/RV32IPipelineStages.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "RV32IPipelineStagesInternal.hpp"

using namespace rv32i::pipeline_stages;

RV32IMemoryStage::RV32IMemoryStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IMemoryStage.pinInitializer()) {}

void RV32IMemoryStage::buildInternals(ComponentBuilder& builder) {
    const auto alignment = builder.add(
        circuit::families::RV32IMemoryAlignmentUnit, "ALIGNMENT");
    builder.addNewComponent<Mux4to1_32bit>("FORWARD_VALUE_MUX");
    builder.addNewComponent<BitSplitter<2>>("DMEM_SIZE_SPLIT");
    builder.addNewComponent<BitJoiner<2>>("DMEM_SIZE_JOIN");
    builder.addNewComponent<ConstantValue<32>>("ZERO32", 0);
    addControlUnpack(builder, "CONTROL_UNPACK");
    addAddressUnpack(builder, "ADDRESSES_UNPACK");
    addStatusUnpack(
        builder,
        "FETCH_STATUS_UNPACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});
    addStatusUnpack(
        builder,
        "EXECUTION_STATUS_UNPACK",
        {"TARGET_MISALIGNED"});
    addStatusPack(
        builder,
        "MEMORY_STATUS_PACK",
        {"DATA_MISALIGNED", "DATA_FAULT"});
    addPassThrough(
        builder,
        "PASS",
        {{"VALID", 1},
         {"PC", 32},
         {"INSTRUCTION", 32},
         {"ALU_RESULT", 32},
         {"MEMORY_DATA", 32},
         {"STORE_DATA", 32},
         {"PC_PLUS_4", 32},
         {"NEXT_PC", 32},
         {"CONTROL", 32},
         {"REGISTER_ADDRESSES", 32},
         {"FETCH_STATUS", 32},
         {"EXECUTION_STATUS", 32}});
    // Keep the externally visible data-memory buses on ordinary component
    // outputs.  A composite boundary input cannot itself drive a boundary
    // output reliably because both pins are sinks in the parent scope.  This
    // zero-delay rewire is only a fan-out/boundary adapter; it is not an
    // additional datapath resource.
    addPassThrough(
        builder,
        "DMEM_PORT_PASS",
        {{"ADDRESS", 32}, {"WRITE_DATA", 32}});
    builder.addNewComponent<NOTGate>("NOT_LEGAL");
    builder.addNewComponent<NOTGate>("NOT_PRETERMINAL");
    builder.addNewComponent<NOTGate>("NOT_MEM_READ");
    builder.addNewComponent<NOTGate>("NOT_MISALIGNED");
    builder.addNewComponent<ORGate>("PRETERMINAL_REASON_1");
    builder.addNewComponent<ORGate>("PRETERMINAL_REASON_2");
    builder.addNewComponent<ORGate>("PRETERMINAL_REASON_3");
    builder.addNewComponent<ORGate>("PRETERMINAL_REASON_4");
    builder.addNewComponent<ORGate>("PRETERMINAL_REASON_5");
    builder.addNewComponent<ANDGate>("PRETERMINAL");
    builder.addNewComponent<ANDGate>("LOAD_REQUEST_1");
    builder.addNewComponent<ANDGate>("LOAD_REQUEST_2");
    builder.addNewComponent<ANDGate>("LOAD_REQUEST");
    builder.addNewComponent<ANDGate>("STORE_REQUEST_1");
    builder.addNewComponent<ANDGate>("STORE_REQUEST_2");
    builder.addNewComponent<ANDGate>("STORE_REQUEST");
    builder.addNewComponent<ORGate>("MEMORY_OPERATION");
    builder.addNewComponent<ANDGate>("MISALIGNED_ACTIVE");
    builder.addNewComponent<ANDGate>("MEMORY_FAULT");
    builder.addNewComponent<ANDGate>("MEMORY_FAULT_ACTIVE");
    builder.addNewComponent<ORGate>("MEMORY_ERROR");
    builder.addNewComponent<ANDGate>("LOAD_FAULT");
    builder.addNewComponent<ANDGate>("STORE_FAULT");
    builder.addNewComponent<ORGate>("ANY_MEMORY_FAULT");
    builder.addNewComponent<ORGate>("DMEM_ACTIVE");
    builder.addNewComponent<ANDGate>("DMEM_SIZE_BIT0");
    builder.addNewComponent<ANDGate>("DMEM_SIZE_BIT1");
    builder.addNewComponent<ANDGate>("DMEM_SIGN_EXTEND_ENABLE");

    const auto pass = builder.getComponent<Rewire>("PASS");
    const auto connect_input = [&](const char* field_name,
                                   std::vector<std::shared_ptr<PinBase>> extra = {}) {
        const size_t width =
            std::string(field_name) == "VALID" ? 1 : 32;
        std::vector<std::shared_ptr<PinBase>> sinks{
            pass->getInputPinDynamic("IN_" + std::string(field_name))};
        sinks.insert(sinks.end(), extra.begin(), extra.end());
        builder.addNewWireDynamic(
            "IN_" + std::string(field_name),
            width,
            getInputPinDynamic("IN_" + std::string(field_name)),
            sinks);
    };
    connect_input(
        "VALID",
        {builder.getInputPin<ANDGate>("PRETERMINAL", "A"),
         builder.getInputPin<ANDGate>("LOAD_REQUEST_1", "A"),
         builder.getInputPin<ANDGate>("STORE_REQUEST_1", "A")});
    connect_input("PC");
    connect_input("INSTRUCTION");
    connect_input(
        "ALU_RESULT",
        {alignment->getInputPin<32>("ADDRESS"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_VALUE_MUX", "IN1"),
         builder.getInputPin<Rewire, 32>(
             "DMEM_PORT_PASS", "IN_ADDRESS")});
    builder.addNewWire<32>(
        "IN_MEMORY_DATA",
        getInputPin<32>("DMEM_READ_DATA"),
        {builder.getInputPin<Rewire, 32>(
            "PASS", "IN_MEMORY_DATA")});
    connect_input(
        "STORE_DATA",
        {builder.getInputPin<Rewire, 32>(
            "DMEM_PORT_PASS", "IN_WRITE_DATA")});
    connect_input(
        "PC_PLUS_4",
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "FORWARD_VALUE_MUX", "IN3")});
    connect_input("NEXT_PC");
    connect_input(
        "CONTROL",
        {builder.getInputPin<Rewire, 32>(
             "CONTROL_UNPACK", "CONTROL")});
    connect_input(
        "REGISTER_ADDRESSES",
        {builder.getInputPin<Rewire, 32>(
            "ADDRESSES_UNPACK", "ADDRESSES")});
    connect_input(
        "FETCH_STATUS",
        {builder.getInputPin<Rewire, 32>(
            "FETCH_STATUS_UNPACK", "STATUS")});
    connect_input(
        "EXECUTION_STATUS",
        {builder.getInputPin<Rewire, 32>(
            "EXECUTION_STATUS_UNPACK", "STATUS")});
    builder.addNewWire<32>(
        "DMEM_ADDR",
        builder.getOutputPin<Rewire, 32>(
            "DMEM_PORT_PASS", "OUT_ADDRESS"),
        {getOutputPin<32>("DMEM_ADDR")});
    builder.addNewWire<32>(
        "DMEM_WRITE_DATA",
        builder.getOutputPin<Rewire, 32>(
            "DMEM_PORT_PASS", "OUT_WRITE_DATA"),
        {getOutputPin<32>("DMEM_WRITE_DATA")});
    builder.addNewWire(
        "DMEM_FAULT",
        getInputPin("DMEM_FAULT"),
        {builder.getInputPin<ANDGate>("MEMORY_FAULT", "A")});
    builder.addNewWire(
        "DMEM_READ_ENABLE",
        getInputPin("DMEM_READ_ENABLE"),
        {builder.getInputPin<ORGate>("DMEM_ACTIVE", "A"),
         builder.getInputPin<ANDGate>(
             "DMEM_SIGN_EXTEND_ENABLE", "A")});
    builder.addNewWire(
        "DMEM_WRITE_ENABLE",
        getInputPin("DMEM_WRITE_ENABLE"),
        {builder.getInputPin<ORGate>("DMEM_ACTIVE", "B")});
    builder.addNewWire<32>(
        "ZERO32",
        builder.getOutputPin<ConstantValue<32>, 32>("ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_VALUE_MUX", "IN0"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_VALUE_MUX", "IN2")});
    builder.addNewWire<2>(
        "WRITEBACK_SEL",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "WRITEBACK_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
            "FORWARD_VALUE_MUX", "SEL")});
    builder.addNewWire<2>(
        "MEM_SIZE_RAW",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "MEM_SIZE"),
        {alignment->getInputPin<2>("SIZE"),
         builder.getInputPin<BitSplitter<2>, 2>(
             "DMEM_SIZE_SPLIT", "IN")});
    builder.addNewWire<5>(
        "FORWARD_RD",
        builder.getOutputPin<Rewire, 5>(
            "ADDRESSES_UNPACK", "RD"),
        {getOutputPin<5>("FORWARD_RD")});
    builder.addNewWire(
        "FORWARD_REG_WRITE",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "REG_WRITE"),
        {getOutputPin("FORWARD_REG_WRITE")});
    builder.addNewWire(
        "MEM_READ",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "MEM_READ"),
        {builder.getInputPin<NOTGate>("NOT_MEM_READ", "IN"),
         builder.getInputPin<ANDGate>("LOAD_REQUEST_1", "B"),
         builder.getInputPin<ORGate>("MEMORY_OPERATION", "A")});
    builder.addNewWire(
        "MEM_WRITE",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "MEM_WRITE"),
        {builder.getInputPin<ANDGate>("STORE_REQUEST_1", "B"),
         builder.getInputPin<ORGate>("MEMORY_OPERATION", "B")});
    builder.addNewWire(
        "RESULT_READY",
        builder.getOutputPin<NOTGate>("NOT_MEM_READ", "OUT"),
        {getOutputPin("FORWARD_RESULT_READY")});
    builder.addNewWire<32>(
        "FORWARD_VALUE",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "FORWARD_VALUE_MUX", "OUT"),
        {getOutputPin<32>("FORWARD_VALUE")});

    builder.addNewWire(
        "PC_MISALIGNED",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "PC_MISALIGNED"),
        {builder.getInputPin<ORGate>(
            "PRETERMINAL_REASON_1", "A")});
    builder.addNewWire(
        "INSTRUCTION_FAULT",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "INSTRUCTION_FAULT"),
        {builder.getInputPin<ORGate>(
            "PRETERMINAL_REASON_1", "B")});
    builder.addNewWire(
        "LEGAL",
        builder.getOutputPin<Rewire>("CONTROL_UNPACK", "LEGAL"),
        {builder.getInputPin<NOTGate>("NOT_LEGAL", "IN")});
    builder.addNewWire(
        "NOT_LEGAL",
        builder.getOutputPin<NOTGate>("NOT_LEGAL", "OUT"),
        {builder.getInputPin<ORGate>(
            "PRETERMINAL_REASON_2", "B")});
    builder.addNewWire(
        "TRAP",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "TRAP_REQUEST"),
        {builder.getInputPin<ORGate>(
            "PRETERMINAL_REASON_3", "B")});
    builder.addNewWire(
        "HALT",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "HALT_REQUEST"),
        {builder.getInputPin<ORGate>(
            "PRETERMINAL_REASON_4", "B")});
    builder.addNewWire(
        "TARGET_MISALIGNED",
        builder.getOutputPin<Rewire>(
            "EXECUTION_STATUS_UNPACK", "TARGET_MISALIGNED"),
        {builder.getInputPin<ORGate>(
            "PRETERMINAL_REASON_5", "B")});
    for (size_t index = 1; index <= 4; ++index) {
        builder.addNewWire(
            "PRETERMINAL_REASON_" + std::to_string(index),
            builder.getOutputPin<ORGate>(
                "PRETERMINAL_REASON_" + std::to_string(index),
                "OUT"),
            {builder.getInputPin<ORGate>(
                "PRETERMINAL_REASON_" + std::to_string(index + 1),
                "A")});
    }
    builder.addNewWire(
        "PRETERMINAL_REASON",
        builder.getOutputPin<ORGate>(
            "PRETERMINAL_REASON_5", "OUT"),
        {builder.getInputPin<ANDGate>("PRETERMINAL", "B")});
    builder.addNewWire(
        "PRETERMINAL",
        builder.getOutputPin<ANDGate>("PRETERMINAL", "OUT"),
        {getOutputPin("PRETERMINAL"),
         builder.getInputPin<NOTGate>("NOT_PRETERMINAL", "IN")});
    builder.addNewWire(
        "NOT_PRETERMINAL",
        builder.getOutputPin<NOTGate>("NOT_PRETERMINAL", "OUT"),
        {builder.getInputPin<ANDGate>("LOAD_REQUEST_2", "B"),
         builder.getInputPin<ANDGate>("STORE_REQUEST_2", "B")});
    builder.addNewWire(
        "LOAD_REQUEST_1",
        builder.getOutputPin<ANDGate>("LOAD_REQUEST_1", "OUT"),
        {builder.getInputPin<ANDGate>("LOAD_REQUEST_2", "A")});
    builder.addNewWire(
        "LOAD_INTENT",
        builder.getOutputPin<ANDGate>("LOAD_REQUEST_2", "OUT"),
        {builder.getInputPin<ANDGate>("LOAD_REQUEST", "A"),
         builder.getInputPin<ANDGate>("LOAD_FAULT", "A")});
    builder.addNewWire(
        "STORE_REQUEST_1",
        builder.getOutputPin<ANDGate>("STORE_REQUEST_1", "OUT"),
        {builder.getInputPin<ANDGate>("STORE_REQUEST_2", "A")});
    builder.addNewWire(
        "STORE_INTENT",
        builder.getOutputPin<ANDGate>("STORE_REQUEST_2", "OUT"),
        {builder.getInputPin<ANDGate>("STORE_REQUEST", "A"),
         builder.getInputPin<ANDGate>("STORE_FAULT", "A")});
    builder.addNewWire(
        "MEMORY_OPERATION",
        builder.getOutputPin<ORGate>("MEMORY_OPERATION", "OUT"),
        {builder.getInputPin<ANDGate>("MISALIGNED_ACTIVE", "B"),
         builder.getInputPin<ANDGate>("MEMORY_FAULT_ACTIVE", "B")});

    builder.addNewWire(
        "MISALIGNED",
        alignment->getOutputPin("MISALIGNED"),
        {builder.getInputPin<ANDGate>("MISALIGNED_ACTIVE", "A"),
         builder.getInputPin<NOTGate>("NOT_MISALIGNED", "IN")});
    builder.addNewWire(
        "NOT_MISALIGNED",
        builder.getOutputPin<NOTGate>("NOT_MISALIGNED", "OUT"),
        {builder.getInputPin<ANDGate>("MEMORY_FAULT", "B"),
         builder.getInputPin<ANDGate>("LOAD_REQUEST", "B"),
         builder.getInputPin<ANDGate>("STORE_REQUEST", "B")});
    builder.addNewWire(
        "MISALIGNED_ACTIVE",
        builder.getOutputPin<ANDGate>("MISALIGNED_ACTIVE", "OUT"),
        {builder.getInputPin<Rewire>(
             "MEMORY_STATUS_PACK", "DATA_MISALIGNED"),
         builder.getInputPin<ORGate>("MEMORY_ERROR", "A")});
    builder.addNewWire(
        "MEMORY_FAULT_RAW",
        builder.getOutputPin<ANDGate>("MEMORY_FAULT", "OUT"),
        {builder.getInputPin<ANDGate>("MEMORY_FAULT_ACTIVE", "A")});
    builder.addNewWire(
        "MEMORY_FAULT_ACTIVE",
        builder.getOutputPin<ANDGate>("MEMORY_FAULT_ACTIVE", "OUT"),
        {builder.getInputPin<Rewire>(
             "MEMORY_STATUS_PACK", "DATA_FAULT"),
         builder.getInputPin<ORGate>("MEMORY_ERROR", "B")});
    builder.addNewWire(
        "MEMORY_ERROR",
        builder.getOutputPin<ORGate>("MEMORY_ERROR", "OUT"),
        {builder.getInputPin<ANDGate>("LOAD_FAULT", "B"),
         builder.getInputPin<ANDGate>("STORE_FAULT", "B")});
    builder.addNewWire(
        "LOAD_REQUEST",
        builder.getOutputPin<ANDGate>("LOAD_REQUEST", "OUT"),
        {getOutputPin("LOAD_REQUEST")});
    builder.addNewWire(
        "STORE_REQUEST",
        builder.getOutputPin<ANDGate>("STORE_REQUEST", "OUT"),
        {getOutputPin("STORE_REQUEST")});
    builder.addNewWire(
        "LOAD_FAULT",
        builder.getOutputPin<ANDGate>("LOAD_FAULT", "OUT"),
        {getOutputPin("LOAD_FAULT"),
         builder.getInputPin<ORGate>("ANY_MEMORY_FAULT", "A")});
    builder.addNewWire(
        "STORE_FAULT",
        builder.getOutputPin<ANDGate>("STORE_FAULT", "OUT"),
        {getOutputPin("STORE_FAULT"),
         builder.getInputPin<ORGate>("ANY_MEMORY_FAULT", "B")});
    builder.addNewWire(
        "ANY_MEMORY_FAULT",
        builder.getOutputPin<ORGate>("ANY_MEMORY_FAULT", "OUT"),
        {getOutputPin("MEMORY_FAULT")});
    builder.addNewWire<32>(
        "MEMORY_STATUS",
        builder.getOutputPin<Rewire, 32>(
            "MEMORY_STATUS_PACK", "STATUS"),
        {getOutputPin<32>("OUT_MEMORY_STATUS")});

    builder.addNewWire(
        "DMEM_ACTIVE",
        builder.getOutputPin<ORGate>("DMEM_ACTIVE", "OUT"),
        {builder.getInputPin<ANDGate>("DMEM_SIZE_BIT0", "B"),
         builder.getInputPin<ANDGate>("DMEM_SIZE_BIT1", "B")});
    for (size_t bit_index = 0; bit_index < 2; ++bit_index) {
        builder.addNewWire(
            "DMEM_SIZE_RAW_" + std::to_string(bit_index),
            builder.getOutputPin<BitSplitter<2>>(
                "DMEM_SIZE_SPLIT",
                "OUT_" + std::to_string(bit_index)),
            {builder.getInputPin<ANDGate>(
                "DMEM_SIZE_BIT" + std::to_string(bit_index),
                "A")});
        builder.addNewWire(
            "DMEM_SIZE_" + std::to_string(bit_index),
            builder.getOutputPin<ANDGate>(
                "DMEM_SIZE_BIT" + std::to_string(bit_index),
                "OUT"),
            {builder.getInputPin<BitJoiner<2>>(
                "DMEM_SIZE_JOIN",
                "IN_" + std::to_string(bit_index))});
    }
    builder.addNewWire<2>(
        "DMEM_SIZE",
        builder.getOutputPin<BitJoiner<2>, 2>(
            "DMEM_SIZE_JOIN", "OUT"),
        {getOutputPin<2>("DMEM_SIZE")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND_RAW",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "LOAD_SIGN_EXTEND"),
        {builder.getInputPin<ANDGate>(
            "DMEM_SIGN_EXTEND_ENABLE", "B")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND",
        builder.getOutputPin<ANDGate>(
            "DMEM_SIGN_EXTEND_ENABLE", "OUT"),
        {getOutputPin("DMEM_SIGN_EXTEND")});

    for (const auto* field_name : {
             "VALID", "PC", "INSTRUCTION", "ALU_RESULT",
             "MEMORY_DATA", "STORE_DATA", "PC_PLUS_4", "NEXT_PC",
             "CONTROL", "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS"}) {
        const size_t width =
            std::string(field_name) == "VALID" ? 1 : 32;
        builder.addNewWireDynamic(
            "PASS_" + std::string(field_name),
            width,
            pass->getOutputPinDynamic(
                "OUT_" + std::string(field_name)),
            {getOutputPinDynamic(
                "OUT_" + std::string(field_name))});
    }
}
