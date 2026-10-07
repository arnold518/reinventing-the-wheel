#include "modules/rv32i/RV32IPipelineStages.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/Register.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "RV32IPipelineStagesInternal.hpp"

namespace {
void appendMappings(
    std::vector<Rewire::BitMap>& destination,
    std::vector<Rewire::BitMap> source) {
    destination.insert(
        destination.end(), source.begin(), source.end());
}

std::vector<Rewire::WireSpec> controlFieldSpecs() {
    return {
        {"LEGAL", 1},
        {"REG_WRITE", 1},
        {"MEM_READ", 1},
        {"MEM_WRITE", 1},
        {"LOAD_SIGN_EXTEND", 1},
        {"HALT_REQUEST", 1},
        {"TRAP_REQUEST", 1},
        {"USES_RS1", 1},
        {"USES_RS2", 1},
        {"WRITEBACK_SEL", 2},
        {"MEM_SIZE", 2},
        {"BRANCH_TYPE", 3},
        {"JUMP_TYPE", 2},
        {"DECODE_TRAP_CAUSE", 4},
        {"ALU_OP", 5},
        {"ALU_A_SEL", 2},
        {"ALU_B_SEL", 2},
    };
}

std::vector<Rewire::BitMap> controlPackMappings() {
    std::vector<Rewire::BitMap> mappings;
    const auto add = [&](const char* name, size_t width, size_t offset) {
        appendMappings(
            mappings,
            identity_mapping(name, 0, width, "CONTROL", offset));
    };
    add("LEGAL", 1, 0);
    add("REG_WRITE", 1, 1);
    add("MEM_READ", 1, 2);
    add("MEM_WRITE", 1, 3);
    add("LOAD_SIGN_EXTEND", 1, 4);
    add("HALT_REQUEST", 1, 5);
    add("TRAP_REQUEST", 1, 6);
    add("USES_RS1", 1, 7);
    add("USES_RS2", 1, 8);
    add("WRITEBACK_SEL", 2, 9);
    add("MEM_SIZE", 2, 11);
    add("BRANCH_TYPE", 3, 13);
    add("JUMP_TYPE", 2, 16);
    add("DECODE_TRAP_CAUSE", 4, 18);
    add("ALU_OP", 5, 22);
    add("ALU_A_SEL", 2, 27);
    add("ALU_B_SEL", 2, 29);
    return mappings;
}
} // namespace

namespace rv32i::pipeline_stages {
void addControlPack(ComponentBuilder& builder, const char* name) {
    builder.addNewComponent<Rewire>(
        name,
        controlFieldSpecs(),
        std::vector<Rewire::WireSpec>{{"CONTROL", 32}},
        controlPackMappings(),
        Rewire::UnmappedBitValue::LOW);
}

void addControlUnpack(ComponentBuilder& builder, const char* name) {
    std::vector<Rewire::BitMap> mappings;
    for (const auto& mapping : controlPackMappings()) {
        mappings.emplace_back(
            "CONTROL",
            mapping.dst_bit,
            mapping.src_wire,
            mapping.src_bit);
    }
    builder.addNewComponent<Rewire>(
        name,
        std::vector<Rewire::WireSpec>{{"CONTROL", 32}},
        controlFieldSpecs(),
        std::move(mappings),
        Rewire::UnmappedBitValue::LOW);
}

void addAddressPack(ComponentBuilder& builder, const char* name) {
    std::vector<Rewire::BitMap> mappings;
    appendMappings(
        mappings,
        identity_mapping("RS1", 0, 5, "ADDRESSES", 0));
    appendMappings(
        mappings,
        identity_mapping("RS2", 0, 5, "ADDRESSES", 5));
    appendMappings(
        mappings,
        identity_mapping("RD", 0, 5, "ADDRESSES", 10));
    builder.addNewComponent<Rewire>(
        name,
        std::vector<Rewire::WireSpec>{
            {"RS1", 5}, {"RS2", 5}, {"RD", 5}},
        std::vector<Rewire::WireSpec>{{"ADDRESSES", 32}},
        std::move(mappings),
        Rewire::UnmappedBitValue::LOW);
}

void addAddressUnpack(ComponentBuilder& builder, const char* name) {
    std::vector<Rewire::BitMap> mappings;
    appendMappings(
        mappings,
        identity_mapping("ADDRESSES", 0, 5, "RS1"));
    appendMappings(
        mappings,
        identity_mapping("ADDRESSES", 5, 5, "RS2"));
    appendMappings(
        mappings,
        identity_mapping("ADDRESSES", 10, 5, "RD"));
    builder.addNewComponent<Rewire>(
        name,
        std::vector<Rewire::WireSpec>{{"ADDRESSES", 32}},
        std::vector<Rewire::WireSpec>{
            {"RS1", 5}, {"RS2", 5}, {"RD", 5}},
        std::move(mappings),
        Rewire::UnmappedBitValue::LOW);
}

void addStatusPack(
    ComponentBuilder& builder,
    const char* name,
    const std::vector<std::string>& fields) {
    std::vector<Rewire::WireSpec> inputs;
    std::vector<Rewire::BitMap> mappings;
    for (size_t index = 0; index < fields.size(); ++index) {
        inputs.emplace_back(fields[index], 1);
        mappings.emplace_back(fields[index], 0, "STATUS", index);
    }
    builder.addNewComponent<Rewire>(
        name,
        std::move(inputs),
        std::vector<Rewire::WireSpec>{{"STATUS", 32}},
        std::move(mappings),
        Rewire::UnmappedBitValue::LOW);
}

void addStatusUnpack(
    ComponentBuilder& builder,
    const char* name,
    const std::vector<std::string>& fields) {
    std::vector<Rewire::WireSpec> outputs;
    std::vector<Rewire::BitMap> mappings;
    for (size_t index = 0; index < fields.size(); ++index) {
        outputs.emplace_back(fields[index], 1);
        mappings.emplace_back("STATUS", index, fields[index], 0);
    }
    builder.addNewComponent<Rewire>(
        name,
        std::vector<Rewire::WireSpec>{{"STATUS", 32}},
        std::move(outputs),
        std::move(mappings),
        Rewire::UnmappedBitValue::LOW);
}

std::shared_ptr<Rewire> addPassThrough(
    ComponentBuilder& builder,
    const char* name,
    std::vector<Rewire::WireSpec> fields) {
    std::vector<Rewire::WireSpec> outputs = fields;
    std::vector<Rewire::BitMap> mappings;
    for (const auto& [field_name, width] : fields) {
        appendMappings(
            mappings,
            identity_mapping(
                "IN_" + field_name,
                0,
                width,
                "OUT_" + field_name));
    }
    for (auto& [field_name, width] : fields) {
        field_name = "IN_" + field_name;
    }
    for (auto& [field_name, width] : outputs) {
        field_name = "OUT_" + field_name;
    }
    return builder.addNewComponent<Rewire>(
        name,
        std::move(fields),
        std::move(outputs),
        std::move(mappings),
        Rewire::UnmappedBitValue::LOW);
}
} // namespace rv32i::pipeline_stages
