#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/composite/ALU32Direct.hpp"
#include "basic/LogicValueHelpers.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "RV32IPipelineStagesInternal.hpp"

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

bool misaligned(uint32_t address, rv32i::RV32IMemorySize size) {
    switch (size) {
        case rv32i::RV32IMemorySize::Byte: return false;
        case rv32i::RV32IMemorySize::Halfword:
            return (address & 1U) != 0;
        case rv32i::RV32IMemorySize::Word:
            return (address & 3U) != 0;
        case rv32i::RV32IMemorySize::None: return true;
    }
    return true;
}

bool preterminal(
    uint32_t fetch_status,
    uint32_t execution_status,
    const rv32i::RV32IControlSignals& control) {
    return rv32i::pipeline_encoding::statusBit(fetch_status, 0)
        || rv32i::pipeline_encoding::statusBit(fetch_status, 1)
        || !control.legal
        || control.trap
        || control.halt
        || rv32i::pipeline_encoding::statusBit(execution_status, 0);
}

bool branchTaken(
    rv32i::RV32IBranchType branch,
    uint32_t left,
    uint32_t right) {
    switch (branch) {
        case rv32i::RV32IBranchType::None: return false;
        case rv32i::RV32IBranchType::BEQ: return left == right;
        case rv32i::RV32IBranchType::BNE: return left != right;
        case rv32i::RV32IBranchType::BLT:
            return static_cast<int32_t>(left)
                < static_cast<int32_t>(right);
        case rv32i::RV32IBranchType::BGE:
            return static_cast<int32_t>(left)
                >= static_cast<int32_t>(right);
        case rv32i::RV32IBranchType::BLTU: return left < right;
        case rv32i::RV32IBranchType::BGEU: return left >= right;
    }
    return false;
}

uint32_t writebackValue(
    const rv32i::RV32IControlSignals& control,
    uint32_t alu_result,
    uint32_t memory_data,
    uint32_t pc_plus_4) {
    switch (control.writeback) {
        case rv32i::RV32IWritebackSource::ALU: return alu_result;
        case rv32i::RV32IWritebackSource::Memory: return memory_data;
        case rv32i::RV32IWritebackSource::PCPlus4: return pc_plus_4;
        case rv32i::RV32IWritebackSource::None: return 0;
    }
    return 0;
}

rv32i::RV32IExecutionTrapCause decodeTrap(
    rv32i::RV32ITrapCause cause) {
    using Result = rv32i::RV32IExecutionTrapCause;
    switch (cause) {
        case rv32i::RV32ITrapCause::EnvironmentCall:
            return Result::EnvironmentCall;
        case rv32i::RV32ITrapCause::IllegalInstruction:
            return Result::IllegalInstruction;
        case rv32i::RV32ITrapCause::None:
            return Result::None;
    }
    return Result::IllegalInstruction;
}

rv32i::RV32IExecutionTrapCause retirementCause(
    bool valid,
    uint32_t fetch_status,
    uint32_t execution_status,
    uint32_t memory_status,
    const rv32i::RV32IControlSignals& control) {
    using Cause = rv32i::RV32IExecutionTrapCause;
    if (!valid) return Cause::None;
    if (rv32i::pipeline_encoding::statusBit(fetch_status, 0)) {
        return Cause::InstructionAddressMisaligned;
    }
    if (rv32i::pipeline_encoding::statusBit(fetch_status, 1)) {
        return Cause::InstructionAccessFault;
    }
    if (!control.legal) return Cause::IllegalInstruction;
    if (control.trap) return decodeTrap(control.trap_cause);
    const bool data_misaligned =
        rv32i::pipeline_encoding::statusBit(memory_status, 0);
    const bool data_fault =
        rv32i::pipeline_encoding::statusBit(memory_status, 1);
    if (data_misaligned) {
        return control.mem_write
            ? Cause::StoreAddressMisaligned
            : Cause::LoadAddressMisaligned;
    }
    if (data_fault) {
        return control.mem_write
            ? Cause::StoreAccessFault
            : Cause::LoadAccessFault;
    }
    if (rv32i::pipeline_encoding::statusBit(execution_status, 0)) {
        return Cause::InstructionAddressMisaligned;
    }
    return Cause::None;
}

class RV32IFetchStageDirect : public BasicComponent {
public:
    explicit RV32IFetchStageDirect(std::string name, bool external_next_pc = false)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IFetchStage.pinInitializer()),
          external_next_pc_(external_next_pc) {}

    static constexpr const char* TypeName = "RV32IFetchStage";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t time, Simulator& simulator) override {
        const auto clk = getInputValue("CLK");
        const auto rst = getInputValue("RST");
        if (rst == LogicValue::HIGH) {
            pc_ = 0;
        } else if (
            rst == LogicValue::LOW
            && previous_clk_ == LogicValue::LOW
            && clk == LogicValue::HIGH
            && high(getInputValue("PC_WRITE"))) {
            pc_ = (external_next_pc_ || high(getInputValue("PC_REDIRECT")))
                ? word(*this, "REDIRECT_PC")
                : pc_ + 4U;
        }
        previous_clk_ = clk;

        const bool valid = high(getInputValue("FETCH_VALID"));
        const uint32_t status =
            ((pc_ & 3U) != 0 ? 1U : 0U)
            | (high(getInputValue("IMEM_FAULT")) ? 2U : 0U);
        _updateOutputWire<32>(simulator, "FETCH_PC", pc_, time);
        _updateOutputWire<32>(simulator, "IMEM_ADDR", pc_, time);
        _updateOutputWire(simulator, "OUT_VALID", logic(valid), time);
        _updateOutputWire<32>(
            simulator, "OUT_PC", valid ? pc_ : 0U, time);
        _updateOutputWire<32>(
            simulator,
            "OUT_INSTRUCTION",
            valid ? word(*this, "IMEM_READ_DATA") : 0U,
            time);
        _updateOutputWire<32>(
            simulator,
            "OUT_FETCH_STATUS",
            valid ? status : 0U,
            time);
    }

private:
    bool external_next_pc_;
    uint32_t pc_ = 0;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};

class RV32IDecodeStageDirect
    : public BasicComponent,
      public RegisterStateView {
public:
    explicit RV32IDecodeStageDirect(std::string name)
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
        const auto clk = getInputValue("CLK");
        const auto rst = getInputValue("RST");
        if (rst == LogicValue::HIGH) {
            for (auto& value : registers_) {
                value.fill(LogicValue::LOW);
            }
            record(time);
        } else if (
            rst == LogicValue::LOW
            && previous_clk_ == LogicValue::LOW
            && clk == LogicValue::HIGH
            && high(getInputValue("WB_REGISTER_WRITE"))) {
            const auto rd = address5(*this, "WB_RD");
            if (rd != 0) {
                const auto data =
                    getInputPin<32>("WB_VALUE")->getValueAsVector();
                std::copy(data.begin(), data.end(), registers_[rd].begin());
                record(time);
            }
        }
        registers_[0].fill(LogicValue::LOW);
        previous_clk_ = clk;

        const uint32_t instruction = word(*this, "IN_INSTRUCTION");
        const auto decoded = rv32i::RV32IDecoder::decode(instruction);
        const auto control = rv32i::RV32IControl::fromDecoded(decoded);
        const auto read = [&](uint8_t address) {
            if (address == 0) return uint32_t{0};
            uint32_t value = 0;
            for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
                if (registers_[address][bit_index] == LogicValue::HIGH) {
                    value |= uint32_t{1} << bit_index;
                }
            }
            if (high(getInputValue("WB_VALID"))
                && high(getInputValue("WB_REG_WRITE"))
                && address5(*this, "WB_RD") == address) {
                return word(*this, "WB_VALUE");
            }
            return value;
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

class RV32IExecuteStageDirect : public BasicComponent {
public:
    explicit RV32IExecuteStageDirect(std::string name)
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
        const auto forward = [&](uint8_t source, uint32_t original) {
            if (high(getInputValue("EX_MEM_VALID"))
                && high(getInputValue("EX_MEM_REG_WRITE"))
                && address5(*this, "EX_MEM_RD") == source
                && source != 0) {
                // A younger unready producer hides an older WB value.
                return high(getInputValue("EX_MEM_RESULT_READY"))
                    ? word(*this, "EX_MEM_VALUE") : original;
            }
            if (high(getInputValue("MEM_WB_VALID"))
                && high(getInputValue("MEM_WB_REG_WRITE"))
                && address5(*this, "MEM_WB_RD") == source
                && source != 0) {
                return word(*this, "MEM_WB_VALUE");
            }
            return original;
        };
        const uint32_t rs1 = forward(
            rv32i::pipeline_encoding::rs1(addresses),
            word(*this, "IN_RS1_VALUE"));
        const uint32_t rs2 = forward(
            rv32i::pipeline_encoding::rs2(addresses),
            word(*this, "IN_RS2_VALUE"));
        const uint32_t pc = word(*this, "IN_PC");
        const uint32_t immediate = word(*this, "IN_IMMEDIATE");
        const uint32_t alu_a =
            control.alu_a == rv32i::RV32IALUSourceA::RS1
            ? rs1
            : control.alu_a == rv32i::RV32IALUSourceA::PC
                ? pc
                : 0U;
        const uint32_t alu_b =
            control.alu_b == rv32i::RV32IALUSourceB::RS2
            ? rs2
            : control.alu_b == rv32i::RV32IALUSourceB::Immediate
                ? immediate
                : 0U;
        const uint32_t result = alu32Result(control.alu_op, alu_a, alu_b);
        const uint32_t pc_plus_4 = pc + 4U;
        const bool compare=control.alu_op==ALU32Op::SUB||control.alu_op==ALU32Op::SLT||control.alu_op==ALU32Op::SLTU;
        const bool eq=compare&&alu_a==alu_b,lt=compare&&int32_t(alu_a)<int32_t(alu_b),ltu=compare&&alu_a<alu_b;
        const bool conditions[]{false,eq,!eq,lt,!lt,ltu,!ltu,false};
        const bool branch = conditions[unsigned(control.branch)&7];
        _updateOutputWire(simulator,"IS_BRANCH",logic(control.branch!=rv32i::RV32IBranchType::None),time);
        _updateOutputWire(simulator,"BRANCH_TAKEN",logic(branch),time);
        uint32_t next_pc = pc_plus_4;
        if (control.jump != rv32i::RV32IJumpType::None) {
            next_pc=control.jump==rv32i::RV32IJumpType::JAL ? result
                : control.jump==rv32i::RV32IJumpType::JALR ? result&~uint32_t{1} : 0;
        } else if (branch) {
            next_pc = pc + immediate;
        }
        const bool raw_redirect =
            branch || control.jump != rv32i::RV32IJumpType::None;
        const auto target=control.jump==rv32i::RV32IJumpType::None ? pc+immediate
            : control.jump==rv32i::RV32IJumpType::JAL ? result
            : control.jump==rv32i::RV32IJumpType::JALR ? result&~uint32_t{1} : 0;
        _updateOutputWire(simulator,"CONTROL_TAKEN",logic(raw_redirect),time);
        _updateOutputWire<32>(simulator,"CONTROL_TARGET",target,time);
        const bool target_misaligned =
            raw_redirect && (next_pc & 3U) != 0;
        const uint32_t execution_status =
            target_misaligned ? 1U : 0U;
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

class RV32IMemoryStageDirect : public BasicComponent {
public:
    explicit RV32IMemoryStageDirect(std::string name)
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
        const bool data_misaligned =
            memory_operation && misaligned(address, control.mem_size);
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
            port_active
                ? rv32i::component_encoding::memorySize(
                      control.mem_size)
                : 0U,
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

class RV32IWritebackStageDirect : public BasicComponent {
public:
    explicit RV32IWritebackStageDirect(std::string name)
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

class RV32IPipelineExecutionStatusDirect : public BasicComponent {
public:
    explicit RV32IPipelineExecutionStatusDirect(std::string name)
        : BasicComponent(std::move(name), 1,
              circuit::families::RV32IPipelineExecutionStatus.pinInitializer()) {}
    const char* getTypeName() const override { return "RV32IPipelineExecutionStatus"; }
    void evaluate(size_t time, Simulator& sim) override {
        const auto clk = getInputValue("CLK");
        const auto rst = getInputValue("RST");
        if (rst == LogicValue::HIGH) {
            halted_ = trapped_ = false;
            cause_ = 0;
        } else if (rst == LogicValue::LOW && previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH) {
            if (high(getInputValue("HALT_EVENT"))) halted_ = true;
            if (high(getInputValue("TRAP_EVENT"))) {
                trapped_ = true;
                cause_ = getInputPin<4>("EVENT_TRAP_CAUSE")->getValueAsUInt64();
            }
        }
        previous_clk_ = clk;
        _updateOutputWire(sim, "HALTED", logic(halted_), time);
        _updateOutputWire(sim, "TRAPPED", logic(trapped_), time);
        _updateOutputWire<4>(sim, "TRAP_CAUSE", cause_, time);
    }
private:
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
    bool halted_ = false, trapped_ = false;
    uint8_t cause_ = 0;
};

} // namespace

namespace rv32i::pipeline_stages {
std::shared_ptr<Component> directFetchStage(const std::string& name, const Context& context, bool external_next_pc) {
    return Component::createWithContext<RV32IFetchStageDirect>(context, name, external_next_pc);
}
std::shared_ptr<Component> directDecodeStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IDecodeStageDirect>(context, name);
}
std::shared_ptr<Component> directExecuteStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IExecuteStageDirect>(context, name);
}
std::shared_ptr<Component> directMemoryStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IMemoryStageDirect>(context, name);
}
std::shared_ptr<Component> directWritebackStage(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IWritebackStageDirect>(context, name);
}
std::shared_ptr<Component> directExecutionStatus(const std::string& name, const Context& context) {
    return Component::createWithContext<RV32IPipelineExecutionStatusDirect>(context, name);
}
} // namespace rv32i::pipeline_stages
