#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "basic/LogicValueHelpers.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "RV32ILogicNetwork.hpp"
#include "RV32IPipelineControlInternal.hpp"

namespace {
using circuit::logic_values::high;

class RV32IForwardingBehavioral : public BasicComponent {
public:
    explicit RV32IForwardingBehavioral(std::string name)
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

class RV32IHazardDetectionBehavioral : public BasicComponent {
public:
    explicit RV32IHazardDetectionBehavioral(std::string name)
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

class RV32IMemoryAlignmentBehavioral : public BasicComponent {
public:
    explicit RV32IMemoryAlignmentBehavioral(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IMemoryAlignmentUnit
                  .pinInitializer()) {}

    static constexpr const char* TypeName =
        "RV32IMemoryAlignmentUnit";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto address = getInputPin<32>("ADDRESS")->getValueAsVector();
        const auto size = getInputPin<2>("SIZE")->getValueAsVector();
        // Mirror the structural gates, including reserved SIZE=3 and X/Z.
        const auto and_bit = [](LogicValue a, LogicValue b) {
            if (a == LogicValue::LOW || b == LogicValue::LOW) return LogicValue::LOW;
            if (a == LogicValue::HIGH && b == LogicValue::HIGH) return LogicValue::HIGH;
            return LogicValue::UNKNOWN;
        };
        const auto or_bit = [](LogicValue a, LogicValue b) {
            if (a == LogicValue::HIGH || b == LogicValue::HIGH) return LogicValue::HIGH;
            if (a == LogicValue::LOW && b == LogicValue::LOW) return LogicValue::LOW;
            return LogicValue::UNKNOWN;
        };
        const auto misaligned = or_bit(and_bit(size[0], address[0]),
            and_bit(size[1], or_bit(address[0], address[1])));
        _updateOutputWire(
            simulator,
            "MISALIGNED",
            misaligned,
            current_time);
    }
};

class RV32IPipelineRetirementBehavioral : public BasicComponent {
public:
    explicit RV32IPipelineRetirementBehavioral(std::string name)
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

class RV32IPipelineCoordinatorBehavioral : public BasicComponent {
public:
    explicit RV32IPipelineCoordinatorBehavioral(std::string name)
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

        const bool dmem_read = active && mem_load && !mem_store && !wb_terminal;
        const bool dmem_write = active && mem_store && !wb_terminal;
        const bool ifid_flush = retire_terminal || (normal_step && kill);
        const bool stopped = high(getInputValue("HALTED")) || high(getInputValue("TRAPPED"));
        const std::array<std::pair<const char*, bool>, 23> outputs{{
            {"ACTIVE", active},
            {"FETCH_RESPONSE_TAKE", fetch_valid || (stopped || ifid_flush)},
            {"DATA_REQUEST_ISSUE", dmem_read || dmem_write},
            {"IMEM_READ_ENABLE",
             active && !terminal_pending},
            {"DMEM_READ_ENABLE",
             dmem_read},
            {"DMEM_WRITE_ENABLE",
             dmem_write},
            {"FETCH_VALID", fetch_valid},
            {"FETCH_PC_WRITE", fetch_valid || redirect},
            {"FETCH_PC_REDIRECT", redirect},
            {"IF_ID_WRITE", ifid_write},
            {"IF_ID_FLUSH", ifid_flush},
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
} // namespace

namespace rv32i::pipeline_control {
std::shared_ptr<Component> directForwardingUnit(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IForwardingBehavioral>(context, name);
}
std::shared_ptr<Component> directHazardDetectionUnit(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IHazardDetectionBehavioral>(context, name);
}
std::shared_ptr<Component> directMemoryAlignmentUnit(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IMemoryAlignmentBehavioral>(context, name);
}
std::shared_ptr<Component> directPipelineRetirementUnit(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IPipelineRetirementBehavioral>(context, name);
}
std::shared_ptr<Component> directPipelineCoordinator(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IPipelineCoordinatorBehavioral>(context, name);
}
} // namespace rv32i::pipeline_control
