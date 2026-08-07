#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
void forwardingPins(IOComponent* self) {
    self->addPin<5>("ID_EX_RS1", PinType::INPUT);
    self->addPin<5>("ID_EX_RS2", PinType::INPUT);
    self->addPin<5>("EX_MEM_RD", PinType::INPUT);
    self->addPin("EX_MEM_VALID", PinType::INPUT);
    self->addPin("EX_MEM_REG_WRITE", PinType::INPUT);
    self->addPin("EX_MEM_RESULT_READY", PinType::INPUT);
    self->addPin<5>("MEM_WB_RD", PinType::INPUT);
    self->addPin("MEM_WB_VALID", PinType::INPUT);
    self->addPin("MEM_WB_REG_WRITE", PinType::INPUT);
    self->addPin<2>("FORWARD_A", PinType::OUTPUT);
    self->addPin<2>("FORWARD_B", PinType::OUTPUT);
}

void hazardPins(IOComponent* self) {
    self->addPin("ID_VALID", PinType::INPUT);
    self->addPin<5>("ID_RS1", PinType::INPUT);
    self->addPin<5>("ID_RS2", PinType::INPUT);
    self->addPin("ID_USES_RS1", PinType::INPUT);
    self->addPin("ID_USES_RS2", PinType::INPUT);
    self->addPin("EX_VALID", PinType::INPUT);
    self->addPin("EX_MEM_READ", PinType::INPUT);
    self->addPin<5>("EX_RD", PinType::INPUT);
    self->addPin("LOAD_USE_HAZARD", PinType::OUTPUT);
    self->addPin("STALL_FETCH", PinType::OUTPUT);
    self->addPin("STALL_IF_ID", PinType::OUTPUT);
    self->addPin("BUBBLE_ID_EX", PinType::OUTPUT);
}

void pipelineControlFlowPins(IOComponent* self) {
    self->addPin<32>("PC", PinType::INPUT);
    self->addPin<32>("RS1_VALUE", PinType::INPUT);
    self->addPin<32>("IMMEDIATE", PinType::INPUT);
    self->addPin<3>("BRANCH_TYPE", PinType::INPUT);
    self->addPin<2>("JUMP_TYPE", PinType::INPUT);
    self->addPin("EQ", PinType::INPUT);
    self->addPin("LT_SIGNED", PinType::INPUT);
    self->addPin("LT_UNSIGNED", PinType::INPUT);
    self->addPin<32>("PC_PLUS_4", PinType::OUTPUT);
    self->addPin<32>("NEXT_PC", PinType::OUTPUT);
    self->addPin("BRANCH_TAKEN", PinType::OUTPUT);
    self->addPin("REDIRECT", PinType::OUTPUT);
    self->addPin("TARGET_MISALIGNED", PinType::OUTPUT);
}

void memoryAlignmentPins(IOComponent* self) {
    self->addPin<32>("ADDRESS", PinType::INPUT);
    self->addPin<2>("SIZE", PinType::INPUT);
    self->addPin("MISALIGNED", PinType::OUTPUT);
}

void pipelineRetirementPins(IOComponent* self) {
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin("MEM_WB_VALID", PinType::INPUT);
    self->addPin("PC_MISALIGNED", PinType::INPUT);
    self->addPin("INSTRUCTION_FAULT", PinType::INPUT);
    self->addPin("LEGAL", PinType::INPUT);
    self->addPin("TRAP_REQUEST", PinType::INPUT);
    self->addPin<4>("DECODE_TRAP_CAUSE", PinType::INPUT);
    self->addPin("MEM_READ", PinType::INPUT);
    self->addPin("MEM_WRITE", PinType::INPUT);
    self->addPin("DATA_MISALIGNED", PinType::INPUT);
    self->addPin("DATA_FAULT", PinType::INPUT);
    self->addPin("TARGET_MISALIGNED", PinType::INPUT);
    self->addPin("HALT_REQUEST", PinType::INPUT);
    self->addPin("REG_WRITE_REQUEST", PinType::INPUT);

    self->addPin("COMMIT", PinType::OUTPUT);
    self->addPin("NORMAL_COMMIT", PinType::OUTPUT);
    self->addPin("REGISTER_WRITE", PinType::OUTPUT);
    self->addPin("HALT_EVENT", PinType::OUTPUT);
    self->addPin("TRAP_EVENT", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
}

void pipelineCoordinatorPins(IOComponent* self) {
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin("HALTED", PinType::INPUT);
    self->addPin("TRAPPED", PinType::INPUT);
    self->addPin("IMEM_READY", PinType::INPUT);
    self->addPin("DMEM_READY", PinType::INPUT);
    self->addPin("LOAD_USE_HAZARD", PinType::INPUT);
    self->addPin("TERMINAL_PENDING", PinType::INPUT);
    self->addPin("EX_PRETERMINAL", PinType::INPUT);
    self->addPin("MEMORY_TERMINAL", PinType::INPUT);
    self->addPin("WB_TERMINAL", PinType::INPUT);
    self->addPin("EX_REDIRECT", PinType::INPUT);
    self->addPin("MEMORY_FAULT", PinType::INPUT);
    self->addPin("MEM_LOAD", PinType::INPUT);
    self->addPin("MEM_STORE", PinType::INPUT);
    self->addPin("RETIRE_TERMINAL", PinType::INPUT);

    self->addPin("ACTIVE", PinType::OUTPUT);
    self->addPin("IMEM_READ_ENABLE", PinType::OUTPUT);
    self->addPin("DMEM_READ_ENABLE", PinType::OUTPUT);
    self->addPin("DMEM_WRITE_ENABLE", PinType::OUTPUT);
    self->addPin("FETCH_VALID", PinType::OUTPUT);
    self->addPin("FETCH_PC_WRITE", PinType::OUTPUT);
    self->addPin("FETCH_PC_REDIRECT", PinType::OUTPUT);
    self->addPin("IF_ID_WRITE", PinType::OUTPUT);
    self->addPin("IF_ID_FLUSH", PinType::OUTPUT);
    self->addPin("ID_EX_WRITE", PinType::OUTPUT);
    self->addPin("ID_EX_FLUSH", PinType::OUTPUT);
    self->addPin("EX_MEM_WRITE", PinType::OUTPUT);
    self->addPin("EX_MEM_FLUSH", PinType::OUTPUT);
    self->addPin("MEM_WB_WRITE", PinType::OUTPUT);
    self->addPin("MEM_WB_FLUSH", PinType::OUTPUT);
    self->addPin("COMMIT_ENABLE", PinType::OUTPUT);
    self->addPin("PIPELINE_STALL", PinType::OUTPUT);
    self->addPin("LOAD_USE_STALL", PinType::OUTPUT);
    self->addPin("MEMORY_STALL", PinType::OUTPUT);
    self->addPin("DATA_PORT_STALL", PinType::OUTPUT);
    self->addPin("PIPELINE_FLUSH", PinType::OUTPUT);
}

bool high(LogicValue value) {
    return value == LogicValue::HIGH;
}

class RV32IForwardingDirect : public BasicComponent {
public:
    explicit RV32IForwardingDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IForwardingUnit.pinInitializer()) {}

    static constexpr const char* TypeName = "RV32IForwardingUnit";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto rs1 =
            getInputPin<5>("ID_EX_RS1")->getValueAsUInt64();
        const auto rs2 =
            getInputPin<5>("ID_EX_RS2")->getValueAsUInt64();
        const auto ex_rd =
            getInputPin<5>("EX_MEM_RD")->getValueAsUInt64();
        const auto mem_rd =
            getInputPin<5>("MEM_WB_RD")->getValueAsUInt64();

        const bool ex_common =
            high(getInputValue("EX_MEM_VALID"))
            && high(getInputValue("EX_MEM_REG_WRITE"))
            && ex_rd != 0;
        const bool mem_common =
            high(getInputValue("MEM_WB_VALID"))
            && high(getInputValue("MEM_WB_REG_WRITE"))
            && mem_rd != 0;
        const bool ex_ready =
            high(getInputValue("EX_MEM_RESULT_READY"));

        const auto select = [&](uint64_t source) {
            const bool ex_blocks = ex_common && ex_rd == source;
            if (ex_blocks) {
                return static_cast<uint64_t>(ex_ready ? 1 : 0);
            }
            if (mem_common && mem_rd == source) {
                return uint64_t{2};
            }
            return uint64_t{0};
        };

        _updateOutputWire<2>(
            simulator, "FORWARD_A", select(rs1), current_time);
        _updateOutputWire<2>(
            simulator, "FORWARD_B", select(rs2), current_time);
    }
};

class RV32IHazardDetectionDirect : public BasicComponent {
public:
    explicit RV32IHazardDetectionDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IHazardDetectionUnit.pinInitializer()) {}

    static constexpr const char* TypeName = "RV32IHazardDetectionUnit";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto ex_rd =
            getInputPin<5>("EX_RD")->getValueAsUInt64();
        const auto id_rs1 =
            getInputPin<5>("ID_RS1")->getValueAsUInt64();
        const auto id_rs2 =
            getInputPin<5>("ID_RS2")->getValueAsUInt64();
        const bool hazard =
            high(getInputValue("ID_VALID"))
            && high(getInputValue("EX_VALID"))
            && high(getInputValue("EX_MEM_READ"))
            && ex_rd != 0
            && ((high(getInputValue("ID_USES_RS1"))
                 && ex_rd == id_rs1)
                || (high(getInputValue("ID_USES_RS2"))
                    && ex_rd == id_rs2));
        const auto value =
            hazard ? LogicValue::HIGH : LogicValue::LOW;
        for (const auto* output : {
                 "LOAD_USE_HAZARD",
                 "STALL_FETCH",
                 "STALL_IF_ID",
                 "BUBBLE_ID_EX"}) {
            _updateOutputWire(
                simulator, output, value, current_time);
        }
    }
};

class RV32IPipelineControlFlowDirect : public BasicComponent {
public:
    explicit RV32IPipelineControlFlowDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IPipelineControlFlowUnit
                  .pinInitializer()) {}

    static constexpr const char* TypeName =
        "RV32IPipelineControlFlowUnit";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto pc =
            static_cast<uint32_t>(
                getInputPin<32>("PC")->getValueAsUInt64());
        const auto rs1 =
            static_cast<uint32_t>(
                getInputPin<32>("RS1_VALUE")->getValueAsUInt64());
        const auto immediate =
            static_cast<uint32_t>(
                getInputPin<32>("IMMEDIATE")->getValueAsUInt64());
        const auto branch =
            static_cast<uint8_t>(
                getInputPin<3>("BRANCH_TYPE")->getValueAsUInt64());
        const auto jump =
            static_cast<uint8_t>(
                getInputPin<2>("JUMP_TYPE")->getValueAsUInt64());
        const bool eq = high(getInputValue("EQ"));
        const bool lt_signed = high(getInputValue("LT_SIGNED"));
        const bool lt_unsigned = high(getInputValue("LT_UNSIGNED"));

        bool branch_taken = false;
        switch (branch) {
            case 1: branch_taken = eq; break;
            case 2: branch_taken = !eq; break;
            case 3: branch_taken = lt_signed; break;
            case 4: branch_taken = !lt_signed; break;
            case 5: branch_taken = lt_unsigned; break;
            case 6: branch_taken = !lt_unsigned; break;
            default: break;
        }

        const auto pc_plus_4 = pc + 4U;
        uint32_t next_pc = branch_taken
            ? pc + immediate
            : pc_plus_4;
        if (jump == 1) {
            next_pc = pc + immediate;
        } else if (jump == 2) {
            next_pc = (rs1 + immediate) & ~uint32_t{1};
        } else if (jump == 3) {
            next_pc = 0;
        }
        const bool redirect = branch_taken || jump != 0;
        const bool misaligned =
            redirect && (next_pc & 0x3U) != 0;

        _updateOutputWire<32>(
            simulator, "PC_PLUS_4", pc_plus_4, current_time);
        _updateOutputWire<32>(
            simulator, "NEXT_PC", next_pc, current_time);
        _updateOutputWire(
            simulator,
            "BRANCH_TAKEN",
            branch_taken ? LogicValue::HIGH : LogicValue::LOW,
            current_time);
        _updateOutputWire(
            simulator,
            "REDIRECT",
            redirect ? LogicValue::HIGH : LogicValue::LOW,
            current_time);
        _updateOutputWire(
            simulator,
            "TARGET_MISALIGNED",
            misaligned ? LogicValue::HIGH : LogicValue::LOW,
            current_time);
    }
};

class RV32IMemoryAlignmentDirect : public BasicComponent {
public:
    explicit RV32IMemoryAlignmentDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IMemoryAlignmentUnit
                  .pinInitializer()) {}

    static constexpr const char* TypeName =
        "RV32IMemoryAlignmentUnit";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto address =
            getInputPin<32>("ADDRESS")->getValueAsUInt64();
        const auto size =
            getInputPin<2>("SIZE")->getValueAsUInt64();
        const bool misaligned =
            (size == 1 && (address & 0x1U) != 0)
            || (size == 2 && (address & 0x3U) != 0);
        _updateOutputWire(
            simulator,
            "MISALIGNED",
            misaligned ? LogicValue::HIGH : LogicValue::LOW,
            current_time);
    }
};

class RV32IPipelineRetirementDirect : public BasicComponent {
public:
    explicit RV32IPipelineRetirementDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IPipelineRetirementUnit
                  .pinInitializer()) {}

    static constexpr const char* TypeName =
        "RV32IPipelineRetirementUnit";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const bool commit =
            high(getInputValue("ENABLE"))
            && high(getInputValue("MEM_WB_VALID"));
        uint8_t cause = 0;
        if (high(getInputValue("PC_MISALIGNED"))) {
            cause = 3;
        } else if (high(getInputValue("INSTRUCTION_FAULT"))) {
            cause = 4;
        } else if (!high(getInputValue("LEGAL"))) {
            cause = 1;
        } else if (high(getInputValue("TRAP_REQUEST"))) {
            cause = static_cast<uint8_t>(
                getInputPin<4>("DECODE_TRAP_CAUSE")
                    ->getValueAsUInt64());
        } else if (high(getInputValue("DATA_MISALIGNED"))) {
            cause = high(getInputValue("MEM_WRITE")) ? 7 : 5;
        } else if (high(getInputValue("DATA_FAULT"))) {
            cause = high(getInputValue("MEM_WRITE")) ? 8 : 6;
        } else if (high(getInputValue("TARGET_MISALIGNED"))) {
            cause = 3;
        }
        const bool trap = commit && cause != 0;
        const bool halt =
            commit && !trap && high(getInputValue("HALT_REQUEST"));
        const bool normal = commit && !trap && !halt;
        const auto bit = [](bool value) {
            return value ? LogicValue::HIGH : LogicValue::LOW;
        };
        _updateOutputWire(
            simulator, "COMMIT", bit(commit), current_time);
        _updateOutputWire(
            simulator, "NORMAL_COMMIT", bit(normal), current_time);
        _updateOutputWire(
            simulator,
            "REGISTER_WRITE",
            bit(normal
                && high(getInputValue("REG_WRITE_REQUEST"))),
            current_time);
        _updateOutputWire(
            simulator, "HALT_EVENT", bit(halt), current_time);
        _updateOutputWire(
            simulator, "TRAP_EVENT", bit(trap), current_time);
        _updateOutputWire<4>(
            simulator, "TRAP_CAUSE", cause, current_time);
    }
};

class RV32IPipelineCoordinatorDirect : public BasicComponent {
public:
    explicit RV32IPipelineCoordinatorDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IPipelineCoordinator
                  .pinInitializer()) {}

    static constexpr const char* TypeName =
        "RV32IPipelineCoordinator";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const bool active =
            high(getInputValue("ENABLE"))
            && !high(getInputValue("HALTED"))
            && !high(getInputValue("TRAPPED"));
        const bool mem_load = high(getInputValue("MEM_LOAD"));
        const bool mem_store = high(getInputValue("MEM_STORE"));
        const bool wb_terminal = high(getInputValue("WB_TERMINAL"));
        const bool memory_access =
            (mem_load || mem_store) && !wb_terminal;
        const bool memory_wait =
            active && memory_access
            && !high(getInputValue("DMEM_READY"));
        const bool port_conflict =
            active && mem_store && mem_load && !wb_terminal
            && high(getInputValue("DMEM_READY"));
        const bool terminal_pending =
            high(getInputValue("TERMINAL_PENDING"))
            || high(getInputValue("EX_PRETERMINAL"))
            || high(getInputValue("MEMORY_TERMINAL"))
            || high(getInputValue("WB_TERMINAL"));
        const bool retire_terminal =
            active && !memory_wait
            && high(getInputValue("RETIRE_TERMINAL"));
        const bool normal_step =
            active && !memory_wait
            && !port_conflict && !retire_terminal;
        const bool hazard =
            high(getInputValue("LOAD_USE_HAZARD"));
        const bool kill =
            high(getInputValue("EX_PRETERMINAL"))
            || high(getInputValue("EX_REDIRECT"))
            || high(getInputValue("MEMORY_FAULT"));
        const bool fetch_valid =
            normal_step && !kill && !hazard
            && high(getInputValue("IMEM_READY"))
            && !terminal_pending;
        const bool redirect =
            normal_step && high(getInputValue("EX_REDIRECT"));
        const auto bit = [](bool value) {
            return value ? LogicValue::HIGH : LogicValue::LOW;
        };

        const bool ifid_write =
            retire_terminal || (normal_step && !hazard);
        const bool idex_write = retire_terminal || normal_step;
        const bool exmem_write = retire_terminal || normal_step;
        const bool memwb_write =
            retire_terminal || port_conflict || normal_step;

        const std::array<std::pair<const char*, bool>, 21> outputs{{
            {"ACTIVE", active},
            {"IMEM_READ_ENABLE",
             active && !terminal_pending},
            {"DMEM_READ_ENABLE",
             active && mem_load && !mem_store && !wb_terminal},
            {"DMEM_WRITE_ENABLE",
             active && mem_store && !wb_terminal},
            {"FETCH_VALID", fetch_valid},
            {"FETCH_PC_WRITE", fetch_valid || redirect},
            {"FETCH_PC_REDIRECT", redirect},
            {"IF_ID_WRITE", ifid_write},
            {"IF_ID_FLUSH", retire_terminal || (normal_step && kill)},
            {"ID_EX_WRITE", idex_write},
            {"ID_EX_FLUSH",
             retire_terminal || (normal_step && (kill || hazard))},
            {"EX_MEM_WRITE", exmem_write},
            {"EX_MEM_FLUSH",
             retire_terminal
                 || (normal_step
                     && high(getInputValue("MEMORY_FAULT")))},
            {"MEM_WB_WRITE", memwb_write},
            {"MEM_WB_FLUSH", retire_terminal || port_conflict},
            {"COMMIT_ENABLE", active && !memory_wait},
            {"PIPELINE_STALL",
             memory_wait || port_conflict || hazard},
            {"LOAD_USE_STALL", normal_step && hazard},
            {"MEMORY_STALL", memory_wait},
            {"DATA_PORT_STALL", port_conflict},
            {"PIPELINE_FLUSH", normal_step && kill},
        }};
        for (const auto& [name, value] : outputs) {
            _updateOutputWire(
                simulator, name, bit(value), current_time);
        }
    }
};

using NetId = size_t;

struct LogicNet {
    std::string name;
    std::shared_ptr<Pin<>> source;
    std::vector<std::shared_ptr<Pin<>>> sinks;
};

class LogicNetwork {
public:
    explicit LogicNetwork(ComponentBuilder& builder)
        : builder_(builder) {}

    NetId source(std::string name, std::shared_ptr<Pin<>> pin) {
        nets_.push_back({std::move(name), std::move(pin), {}});
        return nets_.size() - 1;
    }

    void sink(NetId net, std::shared_ptr<Pin<>> pin) {
        nets_.at(net).sinks.push_back(std::move(pin));
    }

    NetId logicalNot(const std::string& name, NetId input) {
        builder_.addNewComponent<NOTGate>(name);
        sink(input, builder_.getInputPin<NOTGate>(name, "IN"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<NOTGate>(name, "OUT"));
    }

    NetId logicalAnd(
        const std::string& name,
        NetId left,
        NetId right) {
        builder_.addNewComponent<ANDGate>(name);
        sink(left, builder_.getInputPin<ANDGate>(name, "A"));
        sink(right, builder_.getInputPin<ANDGate>(name, "B"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<ANDGate>(name, "OUT"));
    }

    NetId logicalOr(
        const std::string& name,
        NetId left,
        NetId right) {
        builder_.addNewComponent<ORGate>(name);
        sink(left, builder_.getInputPin<ORGate>(name, "A"));
        sink(right, builder_.getInputPin<ORGate>(name, "B"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<ORGate>(name, "OUT"));
    }

    NetId andAll(
        const std::string& prefix,
        const std::vector<NetId>& inputs) {
        auto current = inputs.front();
        for (size_t index = 1; index < inputs.size(); ++index) {
            current = logicalAnd(
                prefix + "_" + std::to_string(index),
                current,
                inputs[index]);
        }
        return current;
    }

    NetId orAll(
        const std::string& prefix,
        const std::vector<NetId>& inputs) {
        auto current = inputs.front();
        for (size_t index = 1; index < inputs.size(); ++index) {
            current = logicalOr(
                prefix + "_" + std::to_string(index),
                current,
                inputs[index]);
        }
        return current;
    }

    NetId equal5(
        const std::string& prefix,
        const std::array<NetId, 5>& left,
        const std::array<NetId, 5>& right) {
        std::vector<NetId> equal_bits;
        equal_bits.reserve(5);
        for (size_t bit = 0; bit < 5; ++bit) {
            const auto xor_net = [&] {
                const auto name =
                    prefix + "_XOR_" + std::to_string(bit);
                builder_.addNewComponent<XORGate>(name);
                sink(
                    left[bit],
                    builder_.getInputPin<XORGate>(name, "A"));
                sink(
                    right[bit],
                    builder_.getInputPin<XORGate>(name, "B"));
                return source(
                    name + "_OUT",
                    builder_.getOutputPin<XORGate>(name, "OUT"));
            }();
            equal_bits.push_back(logicalNot(
                prefix + "_XNOR_" + std::to_string(bit),
                xor_net));
        }
        return andAll(prefix + "_ALL", equal_bits);
    }

    NetId nonZero5(
        const std::string& prefix,
        const std::array<NetId, 5>& value) {
        return orAll(
            prefix,
            std::vector<NetId>(value.begin(), value.end()));
    }

    void materialize() {
        for (auto& net : nets_) {
            builder_.addNewWire(
                net.name,
                net.source,
                net.sinks);
        }
    }

private:
    ComponentBuilder& builder_;
    std::vector<LogicNet> nets_;
};

std::array<NetId, 5> split5(
    ComponentBuilder& builder,
    LogicNetwork& logic,
    IOComponent& owner,
    const std::string& pin_name,
    const std::string& child_name) {
    builder.addNewComponent<BitSplitter<5>>(child_name);
    builder.addNewWire<5>(
        child_name + "_BUS",
        owner.getInputPin<5>(pin_name),
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
    return result;
}
} // namespace

namespace circuit::families {
const ComponentFamily RV32IForwardingUnit{
    "rv32i.pipeline.forwarding",
    "RV32IForwardingUnit",
    forwardingPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IForwardingUnit>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IForwardingDirect>(
            context, name);
    }};

const ComponentFamily RV32IHazardDetectionUnit{
    "rv32i.pipeline.hazard-detection",
    "RV32IHazardDetectionUnit",
    hazardPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IHazardDetectionUnit>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IHazardDetectionDirect>(
            context, name);
    }};

const ComponentFamily RV32IPipelineControlFlowUnit{
    "rv32i.pipeline.control-flow",
    "RV32IPipelineControlFlowUnit",
    pipelineControlFlowPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IPipelineControlFlowUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            RV32IPipelineControlFlowDirect>(context, name);
    }};

const ComponentFamily RV32IMemoryAlignmentUnit{
    "rv32i.pipeline.memory-alignment",
    "RV32IMemoryAlignmentUnit",
    memoryAlignmentPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IMemoryAlignmentUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            RV32IMemoryAlignmentDirect>(context, name);
    }};

const ComponentFamily RV32IPipelineRetirementUnit{
    "rv32i.pipeline.retirement",
    "RV32IPipelineRetirementUnit",
    pipelineRetirementPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IPipelineRetirementUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            RV32IPipelineRetirementDirect>(context, name);
    }};

const ComponentFamily RV32IPipelineCoordinator{
    "rv32i.pipeline.coordinator",
    "RV32IPipelineCoordinator",
    pipelineCoordinatorPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IPipelineCoordinator>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            RV32IPipelineCoordinatorDirect>(context, name);
    }};
} // namespace circuit::families

RV32IForwardingUnit::RV32IForwardingUnit(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IForwardingUnit.pinInitializer()) {}

void RV32IForwardingUnit::buildInternals(ComponentBuilder& builder) {
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

    builder.addNewWire<2>(
        "FORWARD_A_to_output",
        builder.getOutputPin<BitJoiner<2>, 2>(
            "FORWARD_A_JOIN", "OUT"),
        {getOutputPin<2>("FORWARD_A")});
    builder.addNewWire<2>(
        "FORWARD_B_to_output",
        builder.getOutputPin<BitJoiner<2>, 2>(
            "FORWARD_B_JOIN", "OUT"),
        {getOutputPin<2>("FORWARD_B")});
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

RV32IPipelineControlFlowUnit::RV32IPipelineControlFlowUnit(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IPipelineControlFlowUnit
              .pinInitializer()) {}

void RV32IPipelineControlFlowUnit::buildInternals(
    ComponentBuilder& builder) {
    builder.addNewComponent<Adder32>("PC_PLUS_4_ADD");
    builder.addNewComponent<Adder32>("PC_IMMEDIATE_ADD");
    builder.addNewComponent<Adder32>("RS1_IMMEDIATE_ADD");
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
    builder.addNewComponent<BitSplitter<32>>("TARGET_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("JUMP_SPLIT");
    builder.addNewComponent<ORGate>("JUMP_ACTIVE_OR");
    builder.addNewComponent<ORGate>("REDIRECT_OR");
    builder.addNewComponent<ORGate>("TARGET_LOW_OR");
    builder.addNewComponent<ANDGate>("TARGET_MISALIGNED_AND");
    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<32>>("CONST_FOUR", 4);
    builder.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);

    builder.addNewWire<32>(
        "PC_fanout",
        getInputPin<32>("PC"),
        {builder.getInputPin<Adder32, 32>("PC_PLUS_4_ADD", "A"),
         builder.getInputPin<Adder32, 32>("PC_IMMEDIATE_ADD", "A")});
    builder.addNewWire<32>(
        "IMMEDIATE_fanout",
        getInputPin<32>("IMMEDIATE"),
        {builder.getInputPin<Adder32, 32>("PC_IMMEDIATE_ADD", "B"),
         builder.getInputPin<Adder32, 32>("RS1_IMMEDIATE_ADD", "B")});
    builder.addNewWire<32>(
        "RS1_to_target",
        getInputPin<32>("RS1_VALUE"),
        {builder.getInputPin<Adder32, 32>("RS1_IMMEDIATE_ADD", "A")});
    builder.addNewWire<32>(
        "FOUR_to_plus4",
        builder.getOutputPin<ConstantValue<32>, 32>("CONST_FOUR", "OUT"),
        {builder.getInputPin<Adder32, 32>("PC_PLUS_4_ADD", "B")});
    builder.addNewWire(
        "LOW_fanout",
        builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Adder32>("PC_PLUS_4_ADD", "Cin"),
         builder.getInputPin<Adder32>("PC_IMMEDIATE_ADD", "Cin"),
         builder.getInputPin<Adder32>("RS1_IMMEDIATE_ADD", "Cin"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN0"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN7")});

    builder.addNewWire<32>(
        "PC_PLUS_4_fanout",
        builder.getOutputPin<Adder32, 32>("PC_PLUS_4_ADD", "Sum"),
        {getOutputPin<32>("PC_PLUS_4"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "BRANCH_NEXT_MUX", "A")});
    builder.addNewWire<32>(
        "PC_TARGET_to_branch_mux",
        builder.getOutputPin<Adder32, 32>("PC_IMMEDIATE_ADD", "Sum"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
             "BRANCH_NEXT_MUX", "B"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "JUMP_NEXT_MUX", "IN1")});
    builder.addNewWire<32>(
        "RS1_TARGET_to_mask",
        builder.getOutputPin<Adder32, 32>(
            "RS1_IMMEDIATE_ADD", "Sum"),
        {builder.getInputPin<Rewire, 32>("JALR_MASK", "IN")});
    builder.addNewWire<32>(
        "JALR_TARGET_to_jump_mux",
        builder.getOutputPin<Rewire, 32>("JALR_MASK", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "JUMP_NEXT_MUX", "IN2")});
    builder.addNewWire<32>(
        "ZERO_to_reserved_jump",
        builder.getOutputPin<ConstantValue<32>, 32>(
            "CONST_ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "JUMP_NEXT_MUX", "IN3")});

    builder.addNewWire(
        "EQ_fanout",
        getInputPin("EQ"),
        {builder.getInputPin<NOTGate>("NOT_EQ", "IN"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN1")});
    builder.addNewWire(
        "LT_SIGNED_fanout",
        getInputPin("LT_SIGNED"),
        {builder.getInputPin<NOTGate>("NOT_LT_SIGNED", "IN"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN3")});
    builder.addNewWire(
        "LT_UNSIGNED_fanout",
        getInputPin("LT_UNSIGNED"),
        {builder.getInputPin<NOTGate>("NOT_LT_UNSIGNED", "IN"),
         builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN5")});
    builder.addNewWire(
        "NOT_EQ_to_branch",
        builder.getOutputPin<NOTGate>("NOT_EQ", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN2")});
    builder.addNewWire(
        "NOT_LT_SIGNED_to_branch",
        builder.getOutputPin<NOTGate>("NOT_LT_SIGNED", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN4")});
    builder.addNewWire(
        "NOT_LT_UNSIGNED_to_branch",
        builder.getOutputPin<NOTGate>("NOT_LT_UNSIGNED", "OUT"),
        {builder.getInputPin<Mux8to1>("BRANCH_MUX", "IN6")});
    builder.addNewWire<3>(
        "BRANCH_TYPE_to_mux",
        getInputPin<3>("BRANCH_TYPE"),
        {builder.getInputPin<Mux8to1, 3>("BRANCH_MUX", "SEL")});
    builder.addNewWire(
        "BRANCH_TAKEN_fanout",
        builder.getOutputPin<Mux8to1>("BRANCH_MUX", "OUT"),
        {getOutputPin("BRANCH_TAKEN"),
         builder.getInputPin<Mux2to1_32bit>(
             "BRANCH_NEXT_MUX", "SEL"),
         builder.getInputPin<ORGate>("REDIRECT_OR", "A")});
    builder.addNewWire<32>(
        "BRANCH_NEXT_to_jump_mux",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "BRANCH_NEXT_MUX", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "JUMP_NEXT_MUX", "IN0")});
    builder.addNewWire<2>(
        "JUMP_TYPE_fanout",
        getInputPin<2>("JUMP_TYPE"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
             "JUMP_NEXT_MUX", "SEL"),
         builder.getInputPin<BitSplitter<2>, 2>(
             "JUMP_SPLIT", "IN")});
    builder.addNewWire(
        "JUMP_bit0_to_active",
        builder.getOutputPin<BitSplitter<2>>(
            "JUMP_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("JUMP_ACTIVE_OR", "A")});
    builder.addNewWire(
        "JUMP_bit1_to_active",
        builder.getOutputPin<BitSplitter<2>>(
            "JUMP_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("JUMP_ACTIVE_OR", "B")});
    builder.addNewWire(
        "JUMP_ACTIVE_to_redirect",
        builder.getOutputPin<ORGate>("JUMP_ACTIVE_OR", "OUT"),
        {builder.getInputPin<ORGate>("REDIRECT_OR", "B")});
    builder.addNewWire(
        "REDIRECT_fanout",
        builder.getOutputPin<ORGate>("REDIRECT_OR", "OUT"),
        {getOutputPin("REDIRECT"),
         builder.getInputPin<ANDGate>(
             "TARGET_MISALIGNED_AND", "B")});
    builder.addNewWire<32>(
        "NEXT_PC_fanout",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "JUMP_NEXT_MUX", "OUT"),
        {getOutputPin<32>("NEXT_PC"),
         builder.getInputPin<BitSplitter<32>, 32>(
             "TARGET_SPLIT", "IN")});
    builder.addNewWire(
        "TARGET_bit0_to_low",
        builder.getOutputPin<BitSplitter<32>>(
            "TARGET_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("TARGET_LOW_OR", "A")});
    builder.addNewWire(
        "TARGET_bit1_to_low",
        builder.getOutputPin<BitSplitter<32>>(
            "TARGET_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("TARGET_LOW_OR", "B")});
    builder.addNewWire(
        "TARGET_LOW_to_misaligned",
        builder.getOutputPin<ORGate>("TARGET_LOW_OR", "OUT"),
        {builder.getInputPin<ANDGate>(
            "TARGET_MISALIGNED_AND", "A")});
    builder.addNewWire(
        "TARGET_MISALIGNED_to_output",
        builder.getOutputPin<ANDGate>(
            "TARGET_MISALIGNED_AND", "OUT"),
        {getOutputPin("TARGET_MISALIGNED")});
}

RV32IMemoryAlignmentUnit::RV32IMemoryAlignmentUnit(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IMemoryAlignmentUnit
              .pinInitializer()) {}

void RV32IMemoryAlignmentUnit::buildInternals(
    ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("ADDRESS_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("SIZE_SPLIT");
    builder.addNewComponent<ORGate>("LOW_ADDRESS_OR");
    builder.addNewComponent<ANDGate>("HALF_MISALIGNED");
    builder.addNewComponent<ANDGate>("WORD_MISALIGNED");
    builder.addNewComponent<ORGate>("MISALIGNED_OR");

    builder.addNewWire<32>(
        "ADDRESS_to_split",
        getInputPin<32>("ADDRESS"),
        {builder.getInputPin<BitSplitter<32>, 32>(
            "ADDRESS_SPLIT", "IN")});
    builder.addNewWire<2>(
        "SIZE_to_split",
        getInputPin<2>("SIZE"),
        {builder.getInputPin<BitSplitter<2>, 2>(
            "SIZE_SPLIT", "IN")});
    builder.addNewWire(
        "ADDRESS_bit0_fanout",
        builder.getOutputPin<BitSplitter<32>>(
            "ADDRESS_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("LOW_ADDRESS_OR", "A"),
         builder.getInputPin<ANDGate>(
             "HALF_MISALIGNED", "B")});
    builder.addNewWire(
        "ADDRESS_bit1_to_low_or",
        builder.getOutputPin<BitSplitter<32>>(
            "ADDRESS_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("LOW_ADDRESS_OR", "B")});
    builder.addNewWire(
        "SIZE_bit0_to_half",
        builder.getOutputPin<BitSplitter<2>>(
            "SIZE_SPLIT", "OUT_0"),
        {builder.getInputPin<ANDGate>(
            "HALF_MISALIGNED", "A")});
    builder.addNewWire(
        "SIZE_bit1_to_word",
        builder.getOutputPin<BitSplitter<2>>(
            "SIZE_SPLIT", "OUT_1"),
        {builder.getInputPin<ANDGate>(
            "WORD_MISALIGNED", "A")});
    builder.addNewWire(
        "LOW_ADDRESS_to_word",
        builder.getOutputPin<ORGate>(
            "LOW_ADDRESS_OR", "OUT"),
        {builder.getInputPin<ANDGate>(
            "WORD_MISALIGNED", "B")});
    builder.addNewWire(
        "HALF_to_misaligned_or",
        builder.getOutputPin<ANDGate>(
            "HALF_MISALIGNED", "OUT"),
        {builder.getInputPin<ORGate>("MISALIGNED_OR", "A")});
    builder.addNewWire(
        "WORD_to_misaligned_or",
        builder.getOutputPin<ANDGate>(
            "WORD_MISALIGNED", "OUT"),
        {builder.getInputPin<ORGate>("MISALIGNED_OR", "B")});
    builder.addNewWire(
        "MISALIGNED_to_output",
        builder.getOutputPin<ORGate>(
            "MISALIGNED_OR", "OUT"),
        {getOutputPin("MISALIGNED")});
}

RV32IPipelineRetirementUnit::RV32IPipelineRetirementUnit(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IPipelineRetirementUnit
              .pinInitializer()) {}

void RV32IPipelineRetirementUnit::buildInternals(
    ComponentBuilder& builder) {
    LogicNetwork logic(builder);
    const auto enable =
        logic.source("ENABLE", getInputPin("ENABLE"));
    const auto valid =
        logic.source("VALID", getInputPin("MEM_WB_VALID"));
    const auto pc_misaligned = logic.source(
        "PC_MISALIGNED", getInputPin("PC_MISALIGNED"));
    const auto instruction_fault = logic.source(
        "INSTRUCTION_FAULT",
        getInputPin("INSTRUCTION_FAULT"));
    const auto legal =
        logic.source("LEGAL", getInputPin("LEGAL"));
    const auto not_legal =
        logic.logicalNot("NOT_LEGAL", legal);
    const auto trap_request = logic.source(
        "TRAP_REQUEST", getInputPin("TRAP_REQUEST"));
    const auto data_misaligned = logic.source(
        "DATA_MISALIGNED", getInputPin("DATA_MISALIGNED"));
    const auto data_fault =
        logic.source("DATA_FAULT", getInputPin("DATA_FAULT"));
    const auto target_misaligned = logic.source(
        "TARGET_MISALIGNED",
        getInputPin("TARGET_MISALIGNED"));
    const auto halt_request = logic.source(
        "HALT_REQUEST", getInputPin("HALT_REQUEST"));
    const auto reg_write_request = logic.source(
        "REG_WRITE_REQUEST",
        getInputPin("REG_WRITE_REQUEST"));

    const auto trap_any = logic.orAll(
        "TRAP_ANY",
        {pc_misaligned,
         instruction_fault,
         not_legal,
         trap_request,
         data_misaligned,
         data_fault,
         target_misaligned});
    const auto commit =
        logic.logicalAnd("COMMIT", enable, valid);
    const auto trap_event =
        logic.logicalAnd("TRAP_EVENT", commit, trap_any);
    const auto not_trap =
        logic.logicalNot("NOT_TRAP", trap_any);
    const auto commit_without_trap = logic.logicalAnd(
        "COMMIT_WITHOUT_TRAP", commit, not_trap);
    const auto halt_event = logic.logicalAnd(
        "HALT_EVENT", commit_without_trap, halt_request);
    const auto not_halt =
        logic.logicalNot("NOT_HALT", halt_request);
    const auto normal_commit = logic.andAll(
        "NORMAL_COMMIT",
        {commit, not_trap, not_halt});
    const auto register_write = logic.logicalAnd(
        "REGISTER_WRITE",
        normal_commit,
        reg_write_request);

    logic.sink(commit, getOutputPin("COMMIT"));
    logic.sink(
        normal_commit, getOutputPin("NORMAL_COMMIT"));
    logic.sink(
        register_write, getOutputPin("REGISTER_WRITE"));
    logic.sink(halt_event, getOutputPin("HALT_EVENT"));
    logic.sink(trap_event, getOutputPin("TRAP_EVENT"));

    builder.addNewComponent<ConstantValue<4>>("CAUSE_NONE", 0);
    builder.addNewComponent<ConstantValue<4>>("CAUSE_ILLEGAL", 1);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_TARGET_MISALIGNED", 3);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_PC_MISALIGNED", 3);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_INSTRUCTION_FAULT", 4);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_LOAD_MISALIGNED", 5);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_LOAD_FAULT", 6);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_STORE_MISALIGNED", 7);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_STORE_FAULT", 8);
    builder.addNewComponent<Mux2to1_4bit>(
        "DATA_MISALIGNED_CAUSE");
    builder.addNewComponent<Mux2to1_4bit>(
        "DATA_FAULT_CAUSE");

    builder.addNewWire(
        "MEM_WRITE_to_data_cause_muxes",
        getInputPin("MEM_WRITE"),
        {builder.getInputPin<Mux2to1_4bit>(
             "DATA_MISALIGNED_CAUSE", "SEL"),
         builder.getInputPin<Mux2to1_4bit>(
             "DATA_FAULT_CAUSE", "SEL")});
    builder.addNewWire<4>(
        "LOAD_MISALIGNED_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_LOAD_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_MISALIGNED_CAUSE", "A")});
    builder.addNewWire<4>(
        "STORE_MISALIGNED_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_STORE_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_MISALIGNED_CAUSE", "B")});
    builder.addNewWire<4>(
        "LOAD_FAULT_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_LOAD_FAULT", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_FAULT_CAUSE", "A")});
    builder.addNewWire<4>(
        "STORE_FAULT_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_STORE_FAULT", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_FAULT_CAUSE", "B")});

    struct CauseStage {
        const char* name;
        NetId select;
        std::shared_ptr<Pin<4>> cause;
    };
    const std::array<CauseStage, 7> stages{{
        {"TARGET_CAUSE", target_misaligned,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_TARGET_MISALIGNED", "OUT")},
        {"DATA_FAULT_SELECT", data_fault,
         builder.getOutputPin<Mux2to1_4bit, 4>(
             "DATA_FAULT_CAUSE", "OUT")},
        {"DATA_MISALIGNED_SELECT", data_misaligned,
         builder.getOutputPin<Mux2to1_4bit, 4>(
             "DATA_MISALIGNED_CAUSE", "OUT")},
        {"DECODE_TRAP_SELECT", trap_request,
         getInputPin<4>("DECODE_TRAP_CAUSE")},
        {"ILLEGAL_SELECT", not_legal,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_ILLEGAL", "OUT")},
        {"INSTRUCTION_FAULT_SELECT", instruction_fault,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_INSTRUCTION_FAULT", "OUT")},
        {"PC_MISALIGNED_SELECT", pc_misaligned,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_PC_MISALIGNED", "OUT")},
    }};

    auto previous =
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_NONE", "OUT");
    for (const auto& stage : stages) {
        const auto mux_name =
            std::string(stage.name) + "_MUX";
        builder.addNewComponent<Mux2to1_4bit>(mux_name);
        builder.addNewWire<4>(
            mux_name + "_previous",
            previous,
            {builder.getInputPin<Mux2to1_4bit, 4>(
                mux_name, "A")});
        builder.addNewWire<4>(
            mux_name + "_cause",
            stage.cause,
            {builder.getInputPin<Mux2to1_4bit, 4>(
                mux_name, "B")});
        logic.sink(
            stage.select,
            builder.getInputPin<Mux2to1_4bit>(
                mux_name, "SEL"));
        previous =
            builder.getOutputPin<Mux2to1_4bit, 4>(
                mux_name, "OUT");
    }
    builder.addNewWire<4>(
        "TRAP_CAUSE_to_output",
        previous,
        {getOutputPin<4>("TRAP_CAUSE")});
    logic.materialize();
}

RV32IPipelineCoordinator::RV32IPipelineCoordinator(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IPipelineCoordinator
              .pinInitializer()) {}

void RV32IPipelineCoordinator::buildInternals(
    ComponentBuilder& builder) {
    LogicNetwork logic(builder);
    const auto input = [&](const char* name) {
        return logic.source(name, getInputPin(name));
    };
    const auto enable = input("ENABLE");
    const auto halted = input("HALTED");
    const auto trapped = input("TRAPPED");
    const auto imem_ready = input("IMEM_READY");
    const auto dmem_ready = input("DMEM_READY");
    const auto hazard = input("LOAD_USE_HAZARD");
    const auto terminal_pending = input("TERMINAL_PENDING");
    const auto ex_preterminal = input("EX_PRETERMINAL");
    const auto memory_terminal = input("MEMORY_TERMINAL");
    const auto wb_terminal = input("WB_TERMINAL");
    const auto ex_redirect = input("EX_REDIRECT");
    const auto memory_fault = input("MEMORY_FAULT");
    const auto mem_load = input("MEM_LOAD");
    const auto mem_store = input("MEM_STORE");
    const auto retire_terminal = input("RETIRE_TERMINAL");

    const auto active = logic.andAll(
        "ACTIVE",
        {enable,
         logic.logicalNot("NOT_HALTED", halted),
         logic.logicalNot("NOT_TRAPPED", trapped)});
    const auto not_wb_terminal =
        logic.logicalNot("NOT_WB_TERMINAL", wb_terminal);
    const auto memory_access = logic.logicalAnd(
        "MEMORY_ACCESS",
        logic.logicalOr("MEMORY_ACCESS_RAW", mem_load, mem_store),
        not_wb_terminal);
    const auto not_dmem_ready =
        logic.logicalNot("NOT_DMEM_READY", dmem_ready);
    const auto memory_wait = logic.andAll(
        "MEMORY_WAIT",
        {active, memory_access, not_dmem_ready});
    const auto port_conflict = logic.andAll(
        "DATA_PORT_CONFLICT",
        {active, mem_store, mem_load, dmem_ready,
         not_wb_terminal});
    const auto not_memory_wait =
        logic.logicalNot("NOT_MEMORY_WAIT", memory_wait);
    const auto not_port_conflict =
        logic.logicalNot("NOT_PORT_CONFLICT", port_conflict);
    const auto not_retire_terminal =
        logic.logicalNot(
            "NOT_RETIRE_TERMINAL", retire_terminal);
    const auto terminal_step = logic.andAll(
        "TERMINAL_STEP",
        {active, not_memory_wait, retire_terminal});
    const auto normal_step = logic.andAll(
        "NORMAL_STEP",
        {active,
         not_memory_wait,
         not_port_conflict,
         not_retire_terminal});
    const auto kill = logic.orAll(
        "KILL_YOUNGER",
        {ex_preterminal, ex_redirect, memory_fault});
    const auto not_kill =
        logic.logicalNot("NOT_KILL", kill);
    const auto not_hazard =
        logic.logicalNot("NOT_HAZARD", hazard);
    const auto any_terminal_pending = logic.orAll(
        "ANY_TERMINAL_PENDING",
        {terminal_pending,
         ex_preterminal,
         memory_terminal,
         wb_terminal});
    const auto not_terminal_pending = logic.logicalNot(
        "NOT_TERMINAL_PENDING", any_terminal_pending);
    const auto fetch_valid = logic.andAll(
        "FETCH_VALID",
        {normal_step,
         not_kill,
         not_hazard,
         imem_ready,
         not_terminal_pending});
    const auto redirect_step = logic.logicalAnd(
        "REDIRECT_STEP", normal_step, ex_redirect);

    const auto output = [&](NetId net, const char* name) {
        logic.sink(net, getOutputPin(name));
    };
    output(active, "ACTIVE");
    output(
        logic.logicalAnd(
            "IMEM_READ_ENABLE",
            active,
            not_terminal_pending),
        "IMEM_READ_ENABLE");
    output(
        logic.andAll(
            "DMEM_READ_ENABLE",
            {active,
             mem_load,
             logic.logicalNot("NOT_MEM_STORE", mem_store),
             not_wb_terminal}),
        "DMEM_READ_ENABLE");
    output(
        logic.andAll(
            "DMEM_WRITE_ENABLE",
            {active, mem_store, not_wb_terminal}),
        "DMEM_WRITE_ENABLE");
    output(fetch_valid, "FETCH_VALID");
    output(
        logic.logicalOr(
            "FETCH_PC_WRITE", fetch_valid, redirect_step),
        "FETCH_PC_WRITE");
    output(redirect_step, "FETCH_PC_REDIRECT");
    output(
        logic.logicalOr(
            "IF_ID_WRITE",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_IF_ID_WRITE",
                normal_step,
                not_hazard)),
        "IF_ID_WRITE");
    output(
        logic.logicalOr(
            "IF_ID_FLUSH",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_IF_ID_FLUSH", normal_step, kill)),
        "IF_ID_FLUSH");
    output(
        logic.logicalOr(
            "ID_EX_WRITE", terminal_step, normal_step),
        "ID_EX_WRITE");
    output(
        logic.logicalOr(
            "ID_EX_FLUSH",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_ID_EX_FLUSH",
                normal_step,
                logic.logicalOr(
                    "KILL_OR_HAZARD", kill, hazard))),
        "ID_EX_FLUSH");
    output(
        logic.logicalOr(
            "EX_MEM_WRITE", terminal_step, normal_step),
        "EX_MEM_WRITE");
    output(
        logic.logicalOr(
            "EX_MEM_FLUSH",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_EX_MEM_FLUSH",
                normal_step,
                memory_fault)),
        "EX_MEM_FLUSH");
    output(
        logic.orAll(
            "MEM_WB_WRITE",
            {terminal_step, port_conflict, normal_step}),
        "MEM_WB_WRITE");
    output(
        logic.logicalOr(
            "MEM_WB_FLUSH", terminal_step, port_conflict),
        "MEM_WB_FLUSH");
    output(
        logic.logicalAnd(
            "COMMIT_ENABLE", active, not_memory_wait),
        "COMMIT_ENABLE");
    output(
        logic.orAll(
            "PIPELINE_STALL",
            {memory_wait, port_conflict, hazard}),
        "PIPELINE_STALL");
    output(
        logic.logicalAnd(
            "LOAD_USE_STALL", normal_step, hazard),
        "LOAD_USE_STALL");
    output(memory_wait, "MEMORY_STALL");
    output(port_conflict, "DATA_PORT_STALL");
    output(
        logic.logicalAnd(
            "PIPELINE_FLUSH", normal_step, kill),
        "PIPELINE_FLUSH");
    logic.materialize();
}
