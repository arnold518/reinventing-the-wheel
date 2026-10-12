#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/composite/ALU32Behavioral.hpp"
#include "basic/LogicValueHelpers.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/storage/RegisterBehavioral.hpp"
#include "modules/rv32i/RegisterFile32x32Behavioral.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "RV32IPipelineStagesInternal.hpp"
#include "RV32IPipelineRules.hpp"

namespace {
using rv32i::pipeline_encoding::bits;

using circuit::logic_values::high;

using circuit::logic_values::logic;

uint32_t word(const BasicComponent& component, const char* pin) {
    return static_cast<uint32_t>(
        component.getInputPin<32>(pin)->getValueAsUInt64());
}

uint8_t address5(const BasicComponent& component, const char* pin) {
    return static_cast<uint8_t>(
        component.getInputPin<5>(pin)->getValueAsUInt64());
}

bool preterminal(
    uint32_t fetch_status,
    uint32_t execution_status,
    const rv32i::RV32IControlSignals& control) {
    using rv32i::pipeline_encoding::statusBit;
    return rv32i::pipeline_rules::preterminal(
        statusBit(fetch_status, 0), statusBit(fetch_status, 1), control, statusBit(execution_status, 0));
}

uint32_t writebackValue(
    const rv32i::RV32IControlSignals& control,
    uint32_t alu_result,
    uint32_t memory_data,
    uint32_t pc_plus_4) {
    return rv32i::pipeline_rules::writebackValue(control, alu_result, memory_data, pc_plus_4);
}

rv32i::RV32IExecutionTrapCause retirementCause(
    bool valid,
    uint32_t fetch_status,
    uint32_t execution_status,
    uint32_t memory_status,
    const rv32i::RV32IControlSignals& control) {
    using rv32i::pipeline_encoding::statusBit;
    return rv32i::pipeline_rules::retirementCause(
        valid, statusBit(fetch_status, 0), statusBit(fetch_status, 1), control,
        statusBit(memory_status, 0), statusBit(memory_status, 1), statusBit(execution_status, 0));
}

class RV32IFetchStageBehavioral : public BasicComponent {
public:
    explicit RV32IFetchStageBehavioral(std::string name, bool external_next_pc = false)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IFetchStage.pinInitializer()),
          external_next_pc_(external_next_pc) {}

    static constexpr const char* TypeName = "RV32IFetchStage";
    const char* getTypeName() const override { return TypeName; }

    // The gates are a register, an incrementer and two-input multiplexers;
    // each is evaluated by its own four-state rule.
    void evaluate(size_t time, Simulator& simulator) override {
        using namespace circuit::logic_values;
        using Bits = std::vector<LogicValue>;
        const auto select = [](const Bits& a, const Bits& b, LogicValue sel) {
            Bits out(32);
            for (size_t bit = 0; bit < 32; ++bit) out[bit] = mux2(sanitizeBit(a[bit]), sanitizeBit(b[bit]), sel);
            return out;
        };
        const Bits zero(32, LogicValue::LOW);
        const Bits redirect = getInputPin<32>("REDIRECT_PC")->getValueAsVector();
        registerStep(pc_, previous_clk_, getInputValue("RST"), getInputValue("CLK"), getInputValue("PC_WRITE"),
                     external_next_pc_ ? redirect : select(incrementBy4(pc_), redirect, getInputValue("PC_REDIRECT")));

        const auto valid = getInputValue("FETCH_VALID");
        Bits status(32, LogicValue::LOW);
        status[0] = orGate(pc_[0], pc_[1]);
        status[1] = getInputValue("IMEM_FAULT");
        _updateOutputWire<32>(simulator, "FETCH_PC", pc_, time);
        _updateOutputWire<32>(simulator, "IMEM_ADDR", pc_, time);
        _updateOutputWire(simulator, "OUT_VALID", valid, time);
        _updateOutputWire<32>(simulator, "OUT_PC", select(zero, pc_, valid), time);
        _updateOutputWire<32>(simulator, "OUT_INSTRUCTION",
                              select(zero, getInputPin<32>("IMEM_READ_DATA")->getValueAsVector(), valid), time);
        _updateOutputWire<32>(simulator, "OUT_FETCH_STATUS", select(zero, status, valid), time);
    }

private:
    bool external_next_pc_;
    std::vector<LogicValue> pc_ = std::vector<LogicValue>(32, LogicValue::UNKNOWN);
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};

class RV32IDecodeStageBehavioral
    : public BasicComponent,
      public RegisterStateView {
public:
    explicit RV32IDecodeStageBehavioral(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IDecodeStage.pinInitializer()) {
        for (auto& value : registers_) {
            value.fill(LogicValue::UNKNOWN);
        }
        registers_[0].fill(LogicValue::LOW);
        record(0);
    }

    static constexpr const char* TypeName = "RV32IDecodeStage";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t time, Simulator& simulator) override {
        const auto before = registers_;
        rv32i::register_file::step(
            registers_, previous_clk_, getInputValue("RST"), getInputValue("CLK"), getInputValue("WB_REGISTER_WRITE"),
            getInputPin<5>("WB_RD")->getValueAsVector(), getInputPin<32>("WB_VALUE")->getValueAsVector());
        if (registers_ != before) record(time);

        const uint32_t instruction = word(*this, "IN_INSTRUCTION");
        const auto decoded = rv32i::RV32IDecoder::decode(instruction);
        const auto control = rv32i::RV32IControl::fromDecoded(decoded);
        // A stored bit the gates do not know stays unknown on the way out.
        const auto read = [&](uint8_t address) {
            if (high(getInputValue("WB_VALID"))
                && high(getInputValue("WB_REG_WRITE"))
                && address != 0
                && address5(*this, "WB_RD") == address) {
                return getInputPin<32>("WB_VALUE")->getValueAsVector();
            }
            return std::vector<LogicValue>(registers_[address].begin(), registers_[address].end());
        };
        const uint32_t control_word =
            rv32i::pipeline_encoding::packControl(control);
        const uint32_t addresses =
            rv32i::pipeline_encoding::packAddresses(
                decoded.rs1, decoded.rs2, decoded.rd);
        const auto ex_control =
            rv32i::pipeline_encoding::unpackControl(
                word(*this, "EX_CONTROL"));
        const auto ex_addresses =
            word(*this, "EX_REGISTER_ADDRESSES");
        const bool hazard =
            high(getInputValue("IN_VALID"))
            && high(getInputValue("EX_VALID"))
            && ex_control.mem_read
            && rv32i::pipeline_encoding::rd(ex_addresses) != 0
            && ((control.uses_rs1
                 && decoded.rs1
                    == rv32i::pipeline_encoding::rd(ex_addresses))
                || (control.uses_rs2
                    && decoded.rs2
                       == rv32i::pipeline_encoding::rd(ex_addresses)));
        const uint32_t fetch_status = word(*this, "IN_FETCH_STATUS");
        const bool terminal =
            high(getInputValue("IN_VALID"))
            && (rv32i::pipeline_encoding::statusBit(fetch_status, 0)
                || rv32i::pipeline_encoding::statusBit(fetch_status, 1)
                || !control.legal || control.trap || control.halt);

        _updateOutputWire(
            simulator,
            "OUT_VALID",
            getInputValue("IN_VALID"),
            time);
        for (const auto& pin : {
                 "OUT_PC", "OUT_INSTRUCTION", "OUT_FETCH_STATUS"}) {
            const char* input = pin == std::string("OUT_PC")
                ? "IN_PC"
                : pin == std::string("OUT_INSTRUCTION")
                    ? "IN_INSTRUCTION"
                    : "IN_FETCH_STATUS";
            _updateOutputWire<32>(
                simulator, pin, word(*this, input), time);
        }
        _updateOutputWire<32>(
            simulator, "OUT_RS1_VALUE", read(decoded.rs1), time);
        _updateOutputWire<32>(
            simulator, "OUT_RS2_VALUE", read(decoded.rs2), time);
        _updateOutputWire<32>(
            simulator,
            "OUT_IMMEDIATE",
            static_cast<uint32_t>(decoded.immediate),
            time);
        _updateOutputWire<32>(
            simulator, "OUT_CONTROL", control_word, time);
        _updateOutputWire<32>(
            simulator, "OUT_REGISTER_ADDRESSES", addresses, time);
        _updateOutputWire(
            simulator, "LOAD_USE_HAZARD", logic(hazard), time);
        _updateOutputWire(
            simulator, "TERMINAL", logic(terminal), time);
    }

    std::vector<std::vector<LogicValue>>
    getRegisterStateAtTime(size_t target_time) const override {
        const RegisterSnapshot* snapshot = &registers_;
        const auto upper = std::upper_bound(
            history_.begin(),
            history_.end(),
            target_time,
            [](size_t time, const auto& entry) {
                return time < entry.first;
            });
        if (!history_.empty()) {
            snapshot = upper == history_.begin()
                ? &history_.front().second
                : &(upper - 1)->second;
        }
        std::vector<std::vector<LogicValue>> result;
        result.reserve(snapshot->size());
        for (const auto& value : *snapshot) {
            result.emplace_back(value.begin(), value.end());
        }
        return result;
    }

private:
    using RegisterSnapshot =
        std::array<std::array<LogicValue, 32>, 32>;
    RegisterSnapshot registers_{};
    std::vector<std::pair<size_t, RegisterSnapshot>> history_;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;

    void record(size_t time) {
        if (!history_.empty() && history_.back().first == time) {
            history_.back().second = registers_;
        } else if (
            history_.empty()
            || history_.back().second != registers_) {
            history_.emplace_back(time, registers_);
        }
    }
};

class RV32IExecuteStageBehavioral : public BasicComponent {
public:
    explicit RV32IExecuteStageBehavioral(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IExecuteStage.pinInitializer()) {}

    static constexpr const char* TypeName = "RV32IExecuteStage";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t time, Simulator& simulator) override {
        const auto control =
            rv32i::pipeline_encoding::unpackControl(
                word(*this, "IN_CONTROL"));
        const uint32_t addresses =
            word(*this, "IN_REGISTER_ADDRESSES");
        const rv32i::pipeline_rules::ForwardSource ex_mem{
            high(getInputValue("EX_MEM_VALID")), high(getInputValue("EX_MEM_REG_WRITE")),
            high(getInputValue("EX_MEM_RESULT_READY")), address5(*this, "EX_MEM_RD"), word(*this, "EX_MEM_VALUE")};
        const rv32i::pipeline_rules::ForwardSource mem_wb{
            high(getInputValue("MEM_WB_VALID")), high(getInputValue("MEM_WB_REG_WRITE")),
            true, address5(*this, "MEM_WB_RD"), word(*this, "MEM_WB_VALUE")};
        const uint32_t rs1 = rv32i::pipeline_rules::forwardOperand(
            rv32i::pipeline_encoding::rs1(addresses), word(*this, "IN_RS1_VALUE"), ex_mem, mem_wb);
        const uint32_t rs2 = rv32i::pipeline_rules::forwardOperand(
            rv32i::pipeline_encoding::rs2(addresses), word(*this, "IN_RS2_VALUE"), ex_mem, mem_wb);
        const uint32_t pc = word(*this, "IN_PC");
        const auto executed = rv32i::pipeline_rules::execute(control, pc, word(*this, "IN_IMMEDIATE"), rs1, rs2);
        const uint32_t result = executed.alu_result, pc_plus_4 = executed.pc_plus_4, next_pc = executed.next_pc;
        const bool raw_redirect = executed.control_taken;
        _updateOutputWire(simulator,"IS_BRANCH",logic(executed.is_branch),time);
        _updateOutputWire(simulator,"BRANCH_TAKEN",logic(executed.branch_taken),time);
        _updateOutputWire(simulator,"CONTROL_TAKEN",logic(raw_redirect),time);
        _updateOutputWire<32>(simulator,"CONTROL_TARGET",executed.target,time);
        const uint32_t execution_status =
            executed.target_misaligned ? 1U : 0U;
        const bool valid = high(getInputValue("IN_VALID"));
        const bool terminal = valid && preterminal(
            word(*this, "IN_FETCH_STATUS"),
            execution_status,
            control);
        const bool redirect = valid && raw_redirect && !terminal;

        _updateOutputWire(
            simulator, "OUT_VALID", getInputValue("IN_VALID"), time);
        const std::array<std::pair<const char*, uint32_t>, 10> outputs{{
            {"OUT_PC", pc},
            {"OUT_INSTRUCTION", word(*this, "IN_INSTRUCTION")},
            {"OUT_ALU_RESULT", result},
            {"OUT_STORE_DATA", rs2},
            {"OUT_PC_PLUS_4", pc_plus_4},
            {"OUT_NEXT_PC", next_pc},
            {"OUT_CONTROL", word(*this, "IN_CONTROL")},
            {"OUT_REGISTER_ADDRESSES", addresses},
            {"OUT_FETCH_STATUS", word(*this, "IN_FETCH_STATUS")},
            {"OUT_EXECUTION_STATUS", execution_status},
        }};
        for (const auto& [pin, value] : outputs) {
            _updateOutputWire<32>(simulator, pin, value, time);
        }
        _updateOutputWire(
            simulator, "REDIRECT", logic(redirect), time);
        _updateOutputWire<32>(
            simulator, "REDIRECT_PC", next_pc, time);
        _updateOutputWire(
            simulator, "PRETERMINAL", logic(terminal), time);
    }
};

class RV32IMemoryStageBehavioral : public BasicComponent {
public:
    explicit RV32IMemoryStageBehavioral(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IMemoryStage.pinInitializer()) {}

    static constexpr const char* TypeName = "RV32IMemoryStage";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t time, Simulator& simulator) override {
        const auto control =
            rv32i::pipeline_encoding::unpackControl(
                word(*this, "IN_CONTROL"));
        const bool valid = high(getInputValue("IN_VALID"));
        const uint32_t address = word(*this, "IN_ALU_RESULT");
        const bool stage_terminal = valid && preterminal(
            word(*this, "IN_FETCH_STATUS"),
            word(*this, "IN_EXECUTION_STATUS"),
            control);
        const bool memory_operation =
            control.mem_read || control.mem_write;
        // The size field as its two bits, as the alignment gates read it:
        // bit 0 asks for an even address, bit 1 for a multiple of four.
        const uint32_t size_code = rv32i::pipeline_encoding::bits(word(*this, "IN_CONTROL"), 11, 0x3U);
        const bool data_misaligned =
            memory_operation && (((size_code & 1U) && (address & 1U)) || ((size_code & 2U) && (address & 3U)));
        const bool data_fault =
            memory_operation
            && high(getInputValue("DMEM_FAULT"))
            && !data_misaligned;
        const uint32_t memory_status =
            (data_misaligned ? 1U : 0U)
            | (data_fault ? 2U : 0U);
        const bool memory_intent =
            valid && memory_operation && !stage_terminal;
        const bool load_intent = memory_intent && control.mem_read;
        const bool store_intent = memory_intent && control.mem_write;
        const bool load_request = load_intent && !data_misaligned;
        const bool store_request = store_intent && !data_misaligned;
        const bool load_fault =
            load_intent && (data_misaligned || data_fault);
        const bool store_fault =
            store_intent && (data_misaligned || data_fault);
        const uint32_t forward_value = writebackValue(
            control,
            address,
            0,
            word(*this, "IN_PC_PLUS_4"));
        const bool port_active =
            high(getInputValue("DMEM_READ_ENABLE"))
            || high(getInputValue("DMEM_WRITE_ENABLE"));

        _updateOutputWire(
            simulator, "OUT_VALID", getInputValue("IN_VALID"), time);
        const std::array<std::pair<const char*, uint32_t>, 12> outputs{{
            {"OUT_PC", word(*this, "IN_PC")},
            {"OUT_INSTRUCTION", word(*this, "IN_INSTRUCTION")},
            {"OUT_ALU_RESULT", address},
            {"OUT_MEMORY_DATA", word(*this, "DMEM_READ_DATA")},
            {"OUT_STORE_DATA", word(*this, "IN_STORE_DATA")},
            {"OUT_PC_PLUS_4", word(*this, "IN_PC_PLUS_4")},
            {"OUT_NEXT_PC", word(*this, "IN_NEXT_PC")},
            {"OUT_CONTROL", word(*this, "IN_CONTROL")},
            {"OUT_REGISTER_ADDRESSES", word(*this, "IN_REGISTER_ADDRESSES")},
            {"OUT_FETCH_STATUS", word(*this, "IN_FETCH_STATUS")},
            {"OUT_EXECUTION_STATUS", word(*this, "IN_EXECUTION_STATUS")},
            {"OUT_MEMORY_STATUS", memory_status},
        }};
        for (const auto& [pin, value] : outputs) {
            _updateOutputWire<32>(simulator, pin, value, time);
        }
        _updateOutputWire(
            simulator, "LOAD_REQUEST", logic(load_request), time);
        _updateOutputWire(
            simulator, "STORE_REQUEST", logic(store_request), time);
        _updateOutputWire(
            simulator, "LOAD_FAULT", logic(load_fault), time);
        _updateOutputWire(
            simulator, "STORE_FAULT", logic(store_fault), time);
        _updateOutputWire(
            simulator,
            "MEMORY_FAULT",
            logic(load_fault || store_fault),
            time);
        _updateOutputWire(
            simulator, "PRETERMINAL", logic(stage_terminal), time);
        _updateOutputWire<5>(
            simulator,
            "FORWARD_RD",
            rv32i::pipeline_encoding::rd(
                word(*this, "IN_REGISTER_ADDRESSES")),
            time);
        _updateOutputWire(
            simulator,
            "FORWARD_REG_WRITE",
            logic(control.reg_write),
            time);
        _updateOutputWire(
            simulator,
            "FORWARD_RESULT_READY",
            logic(!control.mem_read),
            time);
        _updateOutputWire<32>(
            simulator, "FORWARD_VALUE", forward_value, time);
        _updateOutputWire<32>(
            simulator,
            "DMEM_ADDR",
            address,
            time);
        _updateOutputWire<32>(
            simulator,
            "DMEM_WRITE_DATA",
            word(*this, "IN_STORE_DATA"),
            time);
        _updateOutputWire<2>(
            simulator,
            "DMEM_SIZE",
            port_active ? size_code : 0U,
            time);
        _updateOutputWire(
            simulator,
            "DMEM_SIGN_EXTEND",
            logic(
                high(getInputValue("DMEM_READ_ENABLE"))
                && control.load_sign_extend),
            time);
    }
};

class RV32IWritebackStageBehavioral : public BasicComponent {
public:
    explicit RV32IWritebackStageBehavioral(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IWritebackStage.pinInitializer()) {}

    static constexpr const char* TypeName = "RV32IWritebackStage";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t time, Simulator& simulator) override {
        const auto control =
            rv32i::pipeline_encoding::unpackControl(
                word(*this, "IN_CONTROL"));
        const bool valid = high(getInputValue("IN_VALID"));
        // Cause is unqualified; only TRAP_EVENT authorizes capturing it.
        const auto cause = retirementCause(
            true,
            word(*this, "IN_FETCH_STATUS"),
            word(*this, "IN_EXECUTION_STATUS"),
            word(*this, "IN_MEMORY_STATUS"),
            control);
        const bool terminal =
            valid
            && (cause != rv32i::RV32IExecutionTrapCause::None
                || control.halt);
        const bool commit =
            high(getInputValue("COMMIT_ENABLE")) && valid;
        const bool normal =
            commit && !terminal;
        const bool register_write =
            normal && control.reg_write;
        const uint32_t wb_value = writebackValue(
            control,
            word(*this, "IN_ALU_RESULT"),
            word(*this, "IN_MEMORY_DATA"),
            word(*this, "IN_PC_PLUS_4"));

        const bool trap = cause != rv32i::RV32IExecutionTrapCause::None;
        _updateOutputWire(simulator, "HALT_EVENT", logic(commit && !trap && control.halt), time);
        _updateOutputWire(simulator, "TRAP_EVENT", logic(commit && trap), time);
        _updateOutputWire<4>(simulator, "TRAP_CAUSE", static_cast<uint8_t>(cause), time);
        _updateOutputWire(
            simulator, "COMMIT_VALID", logic(commit), time);
        _updateOutputWire(
            simulator, "WB_VALID", getInputValue("IN_VALID"), time);
        _updateOutputWire(
            simulator,
            "WB_REG_WRITE",
            logic(control.reg_write),
            time);
        _updateOutputWire(
            simulator,
            "REGISTER_WRITE",
            logic(register_write),
            time);
        _updateOutputWire<5>(
            simulator,
            "WB_RD",
            rv32i::pipeline_encoding::rd(
                word(*this, "IN_REGISTER_ADDRESSES")),
            time);
        _updateOutputWire<32>(
            simulator, "WB_VALUE", wb_value, time);
        _updateOutputWire(
            simulator, "TERMINAL", logic(terminal), time);
        _updateOutputWire(
            simulator,
            "RETIRE_TERMINAL",
            logic(commit && terminal),
            time);
    }

};

class RV32IPipelineExecutionStatusBehavioral : public BasicComponent {
public:
    explicit RV32IPipelineExecutionStatusBehavioral(std::string name)
        : BasicComponent(std::move(name), 1,
              circuit::families::RV32IPipelineExecutionStatus.pinInitializer()) {}
    const char* getTypeName() const override { return "RV32IPipelineExecutionStatus"; }
    // Three registers sharing a clock and reset, each by the register's rule.
    void evaluate(size_t time, Simulator& sim) override {
        const auto clk = getInputValue("CLK"), rst = getInputValue("RST");
        auto clock = previous_clk_;
        registerStep(halted_, clock, rst, clk, getInputValue("HALT_EVENT"), {LogicValue::HIGH});
        clock = previous_clk_;
        registerStep(trapped_, clock, rst, clk, getInputValue("TRAP_EVENT"), {LogicValue::HIGH});
        registerStep(cause_, previous_clk_, rst, clk, getInputValue("TRAP_EVENT"),
                     getInputPin<4>("EVENT_TRAP_CAUSE")->getValueAsVector());
        _updateOutputWire(sim, "HALTED", halted_[0], time);
        _updateOutputWire(sim, "TRAPPED", trapped_[0], time);
        _updateOutputWire<4>(sim, "TRAP_CAUSE", cause_, time);
    }
private:
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
    std::vector<LogicValue> halted_{LogicValue::UNKNOWN}, trapped_{LogicValue::UNKNOWN},
        cause_ = std::vector<LogicValue>(4, LogicValue::UNKNOWN);
};

} // namespace

namespace rv32i::pipeline_stages {
std::shared_ptr<Component> directFetchStage(const std::string& name, const Context& context, bool external_next_pc) {
    return Component::createWithContext<RV32IFetchStageBehavioral>(context, name, external_next_pc);
}
std::shared_ptr<Component> directDecodeStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IDecodeStageBehavioral>(context, name);
}
std::shared_ptr<Component> directExecuteStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IExecuteStageBehavioral>(context, name);
}
std::shared_ptr<Component> directMemoryStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IMemoryStageBehavioral>(context, name);
}
std::shared_ptr<Component> directWritebackStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IWritebackStageBehavioral>(context, name);
}
std::shared_ptr<Component> directExecutionStatus(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IPipelineExecutionStatusBehavioral>(context, name);
}
} // namespace rv32i::pipeline_stages
