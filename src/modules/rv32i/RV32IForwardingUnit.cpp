#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/utility/BitAdapter.hpp"
#include "RV32ILogicNetwork.hpp"

namespace {
using rv32i::detail::LogicNetwork;
using rv32i::detail::NetId;

std::array<NetId, 5> split5(
    ComponentBuilder& builder,
    LogicNetwork& logic,
    IOComponent& owner,
    const std::string& pin_name,
    const std::string& child_name) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<5>>(child_name);
    w.connect(owner.getInputPin<5>(pin_name),
        {builder.getInputPin<BitSplitter<5>, 5>(
            child_name, "IN")});
    std::array<NetId, 5> result{};
    for (size_t bit = 0; bit < 5; ++bit) {
        result[bit] = logic.source(
            child_name + "_BIT_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<5>>(
                child_name,
                "OUT_" + std::to_string(bit)));
    }
    w.finish();
    return result;
}
} // namespace

RV32IForwardingUnit::RV32IForwardingUnit(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IForwardingUnit.pinInitializer()) {}

void RV32IForwardingUnit::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    LogicNetwork logic(builder);
    const auto rs1 =
        split5(builder, logic, *this, "ID_EX_RS1", "RS1_SPLIT");
    const auto rs2 =
        split5(builder, logic, *this, "ID_EX_RS2", "RS2_SPLIT");
    const auto ex_rd =
        split5(builder, logic, *this, "EX_MEM_RD", "EX_RD_SPLIT");
    const auto mem_rd =
        split5(builder, logic, *this, "MEM_WB_RD", "MEM_RD_SPLIT");

    const auto ex_valid = logic.source(
        "EX_VALID", getInputPin("EX_MEM_VALID"));
    const auto ex_write = logic.source(
        "EX_WRITE", getInputPin("EX_MEM_REG_WRITE"));
    const auto ex_ready = logic.source(
        "EX_READY", getInputPin("EX_MEM_RESULT_READY"));
    const auto mem_valid = logic.source(
        "MEM_VALID", getInputPin("MEM_WB_VALID"));
    const auto mem_write = logic.source(
        "MEM_WRITE", getInputPin("MEM_WB_REG_WRITE"));

    const auto ex_nonzero =
        logic.nonZero5("EX_RD_NONZERO", ex_rd);
    const auto mem_nonzero =
        logic.nonZero5("MEM_RD_NONZERO", mem_rd);
    const auto ex_common = logic.andAll(
        "EX_COMMON",
        {ex_valid, ex_write, ex_nonzero});
    const auto mem_common = logic.andAll(
        "MEM_COMMON",
        {mem_valid, mem_write, mem_nonzero});

    builder.addNewComponent<BitJoiner<2>>("FORWARD_A_JOIN");
    builder.addNewComponent<BitJoiner<2>>("FORWARD_B_JOIN");

    const auto connect_select = [&](const std::string& suffix,
                                    const std::array<NetId, 5>& source,
                                    const std::string& join_name) {
        const auto ex_equal =
            logic.equal5("EX_EQ_" + suffix, ex_rd, source);
        const auto mem_equal =
            logic.equal5("MEM_EQ_" + suffix, mem_rd, source);
        const auto ex_blocks = logic.logicalAnd(
            "EX_BLOCKS_" + suffix, ex_common, ex_equal);
        const auto ex_select = logic.logicalAnd(
            "EX_SELECT_" + suffix, ex_blocks, ex_ready);
        const auto mem_candidate = logic.logicalAnd(
            "MEM_CANDIDATE_" + suffix, mem_common, mem_equal);
        const auto not_ex_blocks = logic.logicalNot(
            "NOT_EX_BLOCKS_" + suffix, ex_blocks);
        const auto mem_select = logic.logicalAnd(
            "MEM_SELECT_" + suffix,
            mem_candidate,
            not_ex_blocks);
        logic.sink(
            ex_select,
            builder.getInputPin<BitJoiner<2>>(
                join_name, "IN_0"));
        logic.sink(
            mem_select,
            builder.getInputPin<BitJoiner<2>>(
                join_name, "IN_1"));
    };

    connect_select("A", rs1, "FORWARD_A_JOIN");
    connect_select("B", rs2, "FORWARD_B_JOIN");
    logic.materialize();

    w.connect(builder.getOutputPin<BitJoiner<2>, 2>(
            "FORWARD_A_JOIN", "OUT"),
        {getOutputPin<2>("FORWARD_A")});
    w.connect(builder.getOutputPin<BitJoiner<2>, 2>(
            "FORWARD_B_JOIN", "OUT"),
        {getOutputPin<2>("FORWARD_B")});
    w.finish();
}

RV32IWritebackBypass::RV32IWritebackBypass(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (const auto* pin : {"RS1", "RS2", "WB_RD"}) self->addPin<5>(pin, PinType::INPUT);
        for (const auto* pin : {"WB_VALID", "WB_REG_WRITE"}) self->addPin(pin, PinType::INPUT);
        for (const auto* pin : {"SELECT_A", "SELECT_B"}) self->addPin(pin, PinType::OUTPUT);
    }) {}

void RV32IWritebackBypass::buildInternals(ComponentBuilder& builder) {
    LogicNetwork logic(builder);
    const auto rs1 = split5(builder, logic, *this, "RS1", "RS1_SPLIT");
    const auto rs2 = split5(builder, logic, *this, "RS2", "RS2_SPLIT");
    const auto rd = split5(builder, logic, *this, "WB_RD", "RD_SPLIT");
    const auto eligible = logic.andAll("WB_ELIGIBLE", {
        logic.source("WB_VALID", getInputPin("WB_VALID")),
        logic.source("WB_REG_WRITE", getInputPin("WB_REG_WRITE")),
        logic.nonZero5("RD_NONZERO", rd)});
    logic.sink(logic.logicalAnd("SELECT_A", eligible, logic.equal5("RS1_MATCH", rd, rs1)),
        getOutputPin("SELECT_A"));
    logic.sink(logic.logicalAnd("SELECT_B", eligible, logic.equal5("RS2_MATCH", rd, rs2)),
        getOutputPin("SELECT_B"));
    logic.materialize();
}

RV32IHazardDetectionUnit::RV32IHazardDetectionUnit(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IHazardDetectionUnit.pinInitializer()) {}

void RV32IHazardDetectionUnit::buildInternals(
    ComponentBuilder& builder) {
    LogicNetwork logic(builder);
    const auto rs1 =
        split5(builder, logic, *this, "ID_RS1", "ID_RS1_SPLIT");
    const auto rs2 =
        split5(builder, logic, *this, "ID_RS2", "ID_RS2_SPLIT");
    const auto ex_rd =
        split5(builder, logic, *this, "EX_RD", "EX_RD_SPLIT");

    const auto id_valid =
        logic.source("ID_VALID", getInputPin("ID_VALID"));
    const auto uses_rs1 =
        logic.source("USES_RS1", getInputPin("ID_USES_RS1"));
    const auto uses_rs2 =
        logic.source("USES_RS2", getInputPin("ID_USES_RS2"));
    const auto ex_valid =
        logic.source("EX_VALID", getInputPin("EX_VALID"));
    const auto ex_mem_read =
        logic.source("EX_MEM_READ", getInputPin("EX_MEM_READ"));
    const auto rd_nonzero =
        logic.nonZero5("EX_RD_NONZERO", ex_rd);
    const auto rs1_equal =
        logic.equal5("EX_RD_EQ_RS1", ex_rd, rs1);
    const auto rs2_equal =
        logic.equal5("EX_RD_EQ_RS2", ex_rd, rs2);
    const auto rs1_dependency = logic.andAll(
        "RS1_DEPENDENCY",
        {uses_rs1, rs1_equal});
    const auto rs2_dependency = logic.andAll(
        "RS2_DEPENDENCY",
        {uses_rs2, rs2_equal});
    const auto any_dependency = logic.logicalOr(
        "ANY_DEPENDENCY",
        rs1_dependency,
        rs2_dependency);
    const auto hazard = logic.andAll(
        "LOAD_USE_HAZARD",
        {id_valid, ex_valid, ex_mem_read, rd_nonzero, any_dependency});

    for (const auto* output : {
             "LOAD_USE_HAZARD",
             "STALL_FETCH",
             "STALL_IF_ID",
             "BUBBLE_ID_EX"}) {
        logic.sink(hazard, getOutputPin(output));
    }
    logic.materialize();
}

