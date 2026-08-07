#include "modules/rv32i/RV32IPipelineStages.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
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

namespace {
using rv32i::pipeline_encoding::bits;

void fetchStagePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("PC_WRITE", PinType::INPUT);
    self->addPin("PC_REDIRECT", PinType::INPUT);
    self->addPin("FETCH_VALID", PinType::INPUT);
    self->addPin<32>("REDIRECT_PC", PinType::INPUT);
    self->addPin<32>("IMEM_READ_DATA", PinType::INPUT);
    self->addPin("IMEM_FAULT", PinType::INPUT);

    self->addPin<32>("FETCH_PC", PinType::OUTPUT);
    self->addPin<32>("IMEM_ADDR", PinType::OUTPUT);
    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
}

void decodeStagePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin<32>("EX_CONTROL", PinType::INPUT);
    self->addPin<32>("EX_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin("EX_VALID", PinType::INPUT);
    self->addPin("WB_VALID", PinType::INPUT);
    self->addPin("WB_REG_WRITE", PinType::INPUT);
    self->addPin("WB_REGISTER_WRITE", PinType::INPUT);
    self->addPin<5>("WB_RD", PinType::INPUT);
    self->addPin<32>("WB_VALUE", PinType::INPUT);

    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_RS1_VALUE", PinType::OUTPUT);
    self->addPin<32>("OUT_RS2_VALUE", PinType::OUTPUT);
    self->addPin<32>("OUT_IMMEDIATE", PinType::OUTPUT);
    self->addPin<32>("OUT_CONTROL", PinType::OUTPUT);
    self->addPin<32>("OUT_REGISTER_ADDRESSES", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
    self->addPin("LOAD_USE_HAZARD", PinType::OUTPUT);
    self->addPin("TERMINAL", PinType::OUTPUT);
}

void executeStagePins(IOComponent* self) {
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_RS1_VALUE", PinType::INPUT);
    self->addPin<32>("IN_RS2_VALUE", PinType::INPUT);
    self->addPin<32>("IN_IMMEDIATE", PinType::INPUT);
    self->addPin<32>("IN_CONTROL", PinType::INPUT);
    self->addPin<32>("IN_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin("EX_MEM_VALID", PinType::INPUT);
    self->addPin<5>("EX_MEM_RD", PinType::INPUT);
    self->addPin("EX_MEM_REG_WRITE", PinType::INPUT);
    self->addPin("EX_MEM_RESULT_READY", PinType::INPUT);
    self->addPin<32>("EX_MEM_VALUE", PinType::INPUT);
    self->addPin("MEM_WB_VALID", PinType::INPUT);
    self->addPin<5>("MEM_WB_RD", PinType::INPUT);
    self->addPin("MEM_WB_REG_WRITE", PinType::INPUT);
    self->addPin<32>("MEM_WB_VALUE", PinType::INPUT);

    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_ALU_RESULT", PinType::OUTPUT);
    self->addPin<32>("OUT_STORE_DATA", PinType::OUTPUT);
    self->addPin<32>("OUT_PC_PLUS_4", PinType::OUTPUT);
    self->addPin<32>("OUT_NEXT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_CONTROL", PinType::OUTPUT);
    self->addPin<32>("OUT_REGISTER_ADDRESSES", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
    self->addPin<32>("OUT_EXECUTION_STATUS", PinType::OUTPUT);
    self->addPin("REDIRECT", PinType::OUTPUT);
    self->addPin<32>("REDIRECT_PC", PinType::OUTPUT);
    self->addPin("PRETERMINAL", PinType::OUTPUT);
}

void memoryStagePins(IOComponent* self) {
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_ALU_RESULT", PinType::INPUT);
    self->addPin<32>("IN_STORE_DATA", PinType::INPUT);
    self->addPin<32>("IN_PC_PLUS_4", PinType::INPUT);
    self->addPin<32>("IN_NEXT_PC", PinType::INPUT);
    self->addPin<32>("IN_CONTROL", PinType::INPUT);
    self->addPin<32>("IN_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin<32>("IN_EXECUTION_STATUS", PinType::INPUT);
    self->addPin<32>("DMEM_READ_DATA", PinType::INPUT);
    self->addPin("DMEM_FAULT", PinType::INPUT);
    self->addPin("DMEM_READ_ENABLE", PinType::INPUT);
    self->addPin("DMEM_WRITE_ENABLE", PinType::INPUT);
    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_ALU_RESULT", PinType::OUTPUT);
    self->addPin<32>("OUT_MEMORY_DATA", PinType::OUTPUT);
    self->addPin<32>("OUT_STORE_DATA", PinType::OUTPUT);
    self->addPin<32>("OUT_PC_PLUS_4", PinType::OUTPUT);
    self->addPin<32>("OUT_NEXT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_CONTROL", PinType::OUTPUT);
    self->addPin<32>("OUT_REGISTER_ADDRESSES", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
    self->addPin<32>("OUT_EXECUTION_STATUS", PinType::OUTPUT);
    self->addPin<32>("OUT_MEMORY_STATUS", PinType::OUTPUT);

    self->addPin("LOAD_REQUEST", PinType::OUTPUT);
    self->addPin("STORE_REQUEST", PinType::OUTPUT);
    self->addPin("LOAD_FAULT", PinType::OUTPUT);
    self->addPin("STORE_FAULT", PinType::OUTPUT);
    self->addPin("MEMORY_FAULT", PinType::OUTPUT);
    self->addPin("PRETERMINAL", PinType::OUTPUT);
    self->addPin<5>("FORWARD_RD", PinType::OUTPUT);
    self->addPin("FORWARD_REG_WRITE", PinType::OUTPUT);
    self->addPin("FORWARD_RESULT_READY", PinType::OUTPUT);
    self->addPin<32>("FORWARD_VALUE", PinType::OUTPUT);

    self->addPin<32>("DMEM_ADDR", PinType::OUTPUT);
    self->addPin<32>("DMEM_WRITE_DATA", PinType::OUTPUT);
    self->addPin<2>("DMEM_SIZE", PinType::OUTPUT);
    self->addPin("DMEM_SIGN_EXTEND", PinType::OUTPUT);
}

void writebackStagePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("COMMIT_ENABLE", PinType::INPUT);
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_ALU_RESULT", PinType::INPUT);
    self->addPin<32>("IN_MEMORY_DATA", PinType::INPUT);
    self->addPin<32>("IN_STORE_DATA", PinType::INPUT);
    self->addPin<32>("IN_PC_PLUS_4", PinType::INPUT);
    self->addPin<32>("IN_NEXT_PC", PinType::INPUT);
    self->addPin<32>("IN_CONTROL", PinType::INPUT);
    self->addPin<32>("IN_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin<32>("IN_EXECUTION_STATUS", PinType::INPUT);
    self->addPin<32>("IN_MEMORY_STATUS", PinType::INPUT);

    self->addPin<32>("PC", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("COMMIT_VALID", PinType::OUTPUT);
    self->addPin<32>("RETIRED_COUNT", PinType::OUTPUT);
    self->addPin<32>("RETIRED_PC", PinType::OUTPUT);
    self->addPin<32>("RETIRED_INSTRUCTION", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_READ", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_WRITE", PinType::OUTPUT);
    self->addPin<32>("RETIRED_MEM_ADDR", PinType::OUTPUT);
    self->addPin<32>("RETIRED_MEM_WRITE_DATA", PinType::OUTPUT);
    self->addPin<32>("RETIRED_MEM_READ_DATA", PinType::OUTPUT);
    self->addPin<2>("RETIRED_MEM_SIZE", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_SIGN_EXTEND", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_FAULT", PinType::OUTPUT);

    self->addPin("WB_VALID", PinType::OUTPUT);
    self->addPin("WB_REG_WRITE", PinType::OUTPUT);
    self->addPin("REGISTER_WRITE", PinType::OUTPUT);
    self->addPin<5>("WB_RD", PinType::OUTPUT);
    self->addPin<32>("WB_VALUE", PinType::OUTPUT);
    self->addPin("TERMINAL", PinType::OUTPUT);
    self->addPin("RETIRE_TERMINAL", PinType::OUTPUT);
}

bool high(LogicValue value) {
    return value == LogicValue::HIGH;
}

LogicValue logic(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

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

uint32_t alu(uint8_t operation, uint32_t a, uint32_t b) {
    switch (operation) {
        case ALU32Op::ADD: return a + b;
        case ALU32Op::SUB: return a - b;
        case ALU32Op::AND: return a & b;
        case ALU32Op::OR: return a | b;
        case ALU32Op::XOR: return a ^ b;
        case ALU32Op::SLL: return a << (b & 31U);
        case ALU32Op::SRL: return a >> (b & 31U);
        case ALU32Op::SRA:
            return static_cast<uint32_t>(
                static_cast<int32_t>(a) >> (b & 31U));
        case ALU32Op::SLT:
            return static_cast<int32_t>(a)
                       < static_cast<int32_t>(b)
                ? 1U
                : 0U;
        case ALU32Op::SLTU: return a < b ? 1U : 0U;
        case ALU32Op::PASS_A: return a;
        case ALU32Op::PASS_B: return b;
        default: return 0;
    }
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
    explicit RV32IFetchStageDirect(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::RV32IFetchStage.pinInitializer()) {}

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
            pc_ = high(getInputValue("PC_REDIRECT"))
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
            if (source == 0) return uint32_t{0};
            if (high(getInputValue("EX_MEM_VALID"))
                && high(getInputValue("EX_MEM_REG_WRITE"))
                && high(getInputValue("EX_MEM_RESULT_READY"))
                && address5(*this, "EX_MEM_RD") == source
                && source != 0) {
                return word(*this, "EX_MEM_VALUE");
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
        const uint32_t result = alu(control.alu_op, alu_a, alu_b);
        const uint32_t pc_plus_4 = pc + 4U;
        const bool branch = branchTaken(control.branch, rs1, rs2);
        uint32_t next_pc = pc_plus_4;
        if (branch || control.jump == rv32i::RV32IJumpType::JAL) {
            next_pc = pc + immediate;
        } else if (control.jump == rv32i::RV32IJumpType::JALR) {
            next_pc = (rs1 + immediate) & ~uint32_t{1};
        }
        const bool raw_redirect =
            branch || control.jump != rv32i::RV32IJumpType::None;
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
        const auto clk = getInputValue("CLK");
        const auto rst = getInputValue("RST");
        const auto control =
            rv32i::pipeline_encoding::unpackControl(
                word(*this, "IN_CONTROL"));
        const bool valid = high(getInputValue("IN_VALID"));
        const uint32_t address = word(*this, "IN_ALU_RESULT");
        const auto cause = retirementCause(
            valid,
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

        if (rst == LogicValue::HIGH) {
            pc_ = 0;
            halted_ = false;
            trapped_ = false;
            trap_cause_ = 0;
            retired_count_ = 0;
            retired_pc_ = 0;
            retired_instruction_ = 0;
            retired_memory_ = {};
        } else if (
            rst == LogicValue::LOW
            && previous_clk_ == LogicValue::LOW
            && clk == LogicValue::HIGH
            && commit) {
            ++retired_count_;
            retired_pc_ = word(*this, "IN_PC");
            retired_instruction_ = word(*this, "IN_INSTRUCTION");
            retired_memory_ = {};
            if (control.mem_read) {
                retired_memory_.read = true;
                retired_memory_.address = address;
                retired_memory_.read_data =
                    word(*this, "IN_MEMORY_DATA");
                retired_memory_.size =
                    rv32i::component_encoding::memorySize(
                        control.mem_size);
                retired_memory_.sign_extend =
                    control.load_sign_extend;
                retired_memory_.fault =
                    rv32i::pipeline_encoding::statusBit(
                        word(*this, "IN_MEMORY_STATUS"), 0)
                    || rv32i::pipeline_encoding::statusBit(
                        word(*this, "IN_MEMORY_STATUS"), 1);
            } else if (control.mem_write) {
                retired_memory_.write = true;
                retired_memory_.address = address;
                retired_memory_.write_data =
                    word(*this, "IN_STORE_DATA");
                retired_memory_.size =
                    rv32i::component_encoding::memorySize(
                        control.mem_size);
                retired_memory_.fault =
                    rv32i::pipeline_encoding::statusBit(
                        word(*this, "IN_MEMORY_STATUS"), 0)
                    || rv32i::pipeline_encoding::statusBit(
                        word(*this, "IN_MEMORY_STATUS"), 1);
            }
            if (cause != rv32i::RV32IExecutionTrapCause::None) {
                trapped_ = true;
                trap_cause_ = static_cast<uint8_t>(cause);
            } else if (control.halt) {
                halted_ = true;
            } else {
                pc_ = word(*this, "IN_NEXT_PC");
            }
        }
        previous_clk_ = clk;

        _updateOutputWire<32>(simulator, "PC", pc_, time);
        _updateOutputWire(
            simulator, "HALTED", logic(halted_), time);
        _updateOutputWire(
            simulator, "TRAPPED", logic(trapped_), time);
        _updateOutputWire<4>(
            simulator, "TRAP_CAUSE", trap_cause_, time);
        _updateOutputWire(
            simulator, "COMMIT_VALID", logic(commit), time);
        _updateOutputWire<32>(
            simulator, "RETIRED_COUNT", retired_count_, time);
        _updateOutputWire<32>(
            simulator, "RETIRED_PC", retired_pc_, time);
        _updateOutputWire<32>(
            simulator,
            "RETIRED_INSTRUCTION",
            retired_instruction_,
            time);
        _updateOutputWire(
            simulator,
            "RETIRED_MEM_READ",
            logic(retired_memory_.read),
            time);
        _updateOutputWire(
            simulator,
            "RETIRED_MEM_WRITE",
            logic(retired_memory_.write),
            time);
        _updateOutputWire<32>(
            simulator,
            "RETIRED_MEM_ADDR",
            retired_memory_.address,
            time);
        _updateOutputWire<32>(
            simulator,
            "RETIRED_MEM_WRITE_DATA",
            retired_memory_.write_data,
            time);
        _updateOutputWire<32>(
            simulator,
            "RETIRED_MEM_READ_DATA",
            retired_memory_.read_data,
            time);
        _updateOutputWire<2>(
            simulator,
            "RETIRED_MEM_SIZE",
            retired_memory_.size,
            time);
        _updateOutputWire(
            simulator,
            "RETIRED_MEM_SIGN_EXTEND",
            logic(retired_memory_.sign_extend),
            time);
        _updateOutputWire(
            simulator,
            "RETIRED_MEM_FAULT",
            logic(retired_memory_.fault),
            time);
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

private:
    struct RetiredMemory {
        bool read = false;
        bool write = false;
        uint32_t address = 0;
        uint32_t write_data = 0;
        uint32_t read_data = 0;
        uint8_t size = 0;
        bool sign_extend = false;
        bool fault = false;
    };

    uint32_t pc_ = 0;
    bool halted_ = false;
    bool trapped_ = false;
    uint8_t trap_cause_ = 0;
    uint32_t retired_count_ = 0;
    uint32_t retired_pc_ = 0;
    uint32_t retired_instruction_ = 0;
    RetiredMemory retired_memory_{};
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};

} // namespace

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
} // namespace

namespace circuit::families {
const ComponentFamily RV32IFetchStage{
    "rv32i.pipeline.stage.fetch",
    "RV32IFetchStage",
    fetchStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IFetchStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IFetchStageDirect>(
            context, name);
    }};

const ComponentFamily RV32IDecodeStage{
    "rv32i.pipeline.stage.decode",
    "RV32IDecodeStage",
    decodeStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IDecodeStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IDecodeStageDirect>(
            context, name);
    }};

const ComponentFamily RV32IExecuteStage{
    "rv32i.pipeline.stage.execute",
    "RV32IExecuteStage",
    executeStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IExecuteStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IExecuteStageDirect>(
            context, name);
    }};

const ComponentFamily RV32IMemoryStage{
    "rv32i.pipeline.stage.memory",
    "RV32IMemoryStage",
    memoryStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IMemoryStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IMemoryStageDirect>(
            context, name);
    }};

const ComponentFamily RV32IWritebackStage{
    "rv32i.pipeline.stage.writeback",
    "RV32IWritebackStage",
    writebackStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IWritebackStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<RV32IWritebackStageDirect>(
            context, name);
    }};
} // namespace circuit::families

RV32IFetchStage::RV32IFetchStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IFetchStage.pinInitializer()) {}

void RV32IFetchStage::buildInternals(ComponentBuilder& builder) {
    const auto pc = builder.add(
        circuit::families::Register32, "PC_STATE");
    const auto alignment = builder.add(
        circuit::families::RV32IMemoryAlignmentUnit, "ALIGNMENT");
    builder.addNewComponent<Adder32>("PC_PLUS_4");
    builder.addNewComponent<Mux2to1_32bit>("NEXT_PC_MUX");
    builder.addNewComponent<Mux2to1_32bit>("PC_OUTPUT_MUX");
    builder.addNewComponent<Mux2to1_32bit>("INSTRUCTION_OUTPUT_MUX");
    builder.addNewComponent<Mux2to1_32bit>("STATUS_OUTPUT_MUX");
    builder.addNewComponent<ConstantValue<1>>("LOW", 0);
    builder.addNewComponent<ConstantValue<2>>("WORD_SIZE", 2);
    builder.addNewComponent<ConstantValue<32>>("ZERO", 0);
    builder.addNewComponent<ConstantValue<32>>("FOUR", 4);
    addPassThrough(builder, "VALID_PASS", {{"VALID", 1}});
    addStatusPack(
        builder,
        "STATUS_PACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});

    builder.addNewWire(
        "CLK", getInputPin("CLK"), {pc->getInputPin("CLK")});
    builder.addNewWire(
        "RST", getInputPin("RST"), {pc->getInputPin("RST")});
    builder.addNewWire(
        "PC_WRITE",
        getInputPin("PC_WRITE"),
        {pc->getInputPin("WE")});
    builder.addNewWire(
        "PC_REDIRECT",
        getInputPin("PC_REDIRECT"),
        {builder.getInputPin<Mux2to1_32bit>(
            "NEXT_PC_MUX", "SEL")});
    builder.addNewWire(
        "FETCH_VALID",
        getInputPin("FETCH_VALID"),
        {builder.getInputPin<Mux2to1_32bit>(
             "PC_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Mux2to1_32bit>(
             "INSTRUCTION_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Mux2to1_32bit>(
             "STATUS_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Rewire>(
             "VALID_PASS", "IN_VALID")});
    builder.addNewWire<32>(
        "REDIRECT_PC",
        getInputPin<32>("REDIRECT_PC"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
            "NEXT_PC_MUX", "B")});
    builder.addNewWire<32>(
        "IMEM_READ_DATA",
        getInputPin<32>("IMEM_READ_DATA"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
            "INSTRUCTION_OUTPUT_MUX", "B")});
    builder.addNewWire(
        "IMEM_FAULT",
        getInputPin("IMEM_FAULT"),
        {builder.getInputPin<Rewire>(
            "STATUS_PACK", "INSTRUCTION_FAULT")});
    builder.addNewWire(
        "LOW",
        builder.getOutputPin<ConstantValue<1>>("LOW", "OUT"),
        {builder.getInputPin<Adder32>("PC_PLUS_4", "Cin")});
    builder.addNewWire<2>(
        "WORD_SIZE",
        builder.getOutputPin<ConstantValue<2>, 2>(
            "WORD_SIZE", "OUT"),
        {alignment->getInputPin<2>("SIZE")});
    builder.addNewWire<32>(
        "ZERO",
        builder.getOutputPin<ConstantValue<32>, 32>("ZERO", "OUT"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
             "PC_OUTPUT_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "INSTRUCTION_OUTPUT_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "STATUS_OUTPUT_MUX", "A")});
    builder.addNewWire<32>(
        "FOUR",
        builder.getOutputPin<ConstantValue<32>, 32>("FOUR", "OUT"),
        {builder.getInputPin<Adder32, 32>("PC_PLUS_4", "B")});
    builder.addNewWire<32>(
        "PC",
        pc->getOutputPin<32>("Q"),
        {builder.getInputPin<Adder32, 32>("PC_PLUS_4", "A"),
         alignment->getInputPin<32>("ADDRESS"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "PC_OUTPUT_MUX", "B"),
         getOutputPin<32>("FETCH_PC"),
         getOutputPin<32>("IMEM_ADDR")});
    builder.addNewWire<32>(
        "PC_PLUS_4",
        builder.getOutputPin<Adder32, 32>("PC_PLUS_4", "Sum"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
            "NEXT_PC_MUX", "A")});
    builder.addNewWire<32>(
        "NEXT_PC",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "NEXT_PC_MUX", "OUT"),
        {pc->getInputPin<32>("D")});
    builder.addNewWire(
        "MISALIGNED",
        alignment->getOutputPin("MISALIGNED"),
        {builder.getInputPin<Rewire>(
            "STATUS_PACK", "PC_MISALIGNED")});
    builder.addNewWire<32>(
        "STATUS",
        builder.getOutputPin<Rewire, 32>("STATUS_PACK", "STATUS"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
            "STATUS_OUTPUT_MUX", "B")});
    builder.addNewWire<32>(
        "OUT_PC",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "PC_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_PC")});
    builder.addNewWire<32>(
        "OUT_INSTRUCTION",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "INSTRUCTION_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_INSTRUCTION")});
    builder.addNewWire<32>(
        "OUT_STATUS",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "STATUS_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_FETCH_STATUS")});
    builder.addNewWire(
        "OUT_VALID",
        builder.getOutputPin<Rewire>(
            "VALID_PASS", "OUT_VALID"),
        {getOutputPin("OUT_VALID")});
}

RV32IDecodeStage::RV32IDecodeStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IDecodeStage.pinInitializer()) {}

void RV32IDecodeStage::buildInternals(ComponentBuilder& builder) {
    const auto decode = builder.add(
        circuit::families::RV32IDecodeControl, "CONTROL");
    register_file_ = builder.add(
        circuit::families::RegisterFile32x32, "REGISTER_FILE");
    const auto hazard = builder.add(
        circuit::families::RV32IHazardDetectionUnit, "HAZARD");
    const auto bypass = builder.add(
        circuit::families::RV32IForwardingUnit, "WB_BYPASS");
    builder.addNewComponent<Mux4to1_32bit>("RS1_MUX");
    builder.addNewComponent<Mux4to1_32bit>("RS2_MUX");
    builder.addNewComponent<ConstantValue<1>>("LOW", 0);
    builder.addNewComponent<ConstantValue<5>>("ZERO5", 0);
    builder.addNewComponent<ConstantValue<32>>("ZERO32", 0);
    addControlPack(builder, "CONTROL_PACK");
    addControlUnpack(builder, "EX_CONTROL_UNPACK");
    addAddressPack(builder, "ADDRESSES_PACK");
    addAddressUnpack(builder, "EX_ADDRESSES_UNPACK");
    addStatusUnpack(
        builder,
        "FETCH_STATUS_UNPACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});
    addPassThrough(
        builder,
        "PASS",
        {{"VALID", 1},
         {"PC", 32},
         {"INSTRUCTION", 32},
         {"FETCH_STATUS", 32}});
    builder.addNewComponent<NOTGate>("NOT_LEGAL");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_1");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_2");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_3");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_4");
    builder.addNewComponent<ANDGate>("TERMINAL");

    builder.addNewWire(
        "CLK",
        getInputPin("CLK"),
        {register_file_->getInputPin("CLK")});
    builder.addNewWire(
        "RST",
        getInputPin("RST"),
        {register_file_->getInputPin("RST")});
    builder.addNewWire(
        "IN_VALID",
        getInputPin("IN_VALID"),
        {builder.getInputPin<Rewire>("PASS", "IN_VALID"),
         hazard->getInputPin("ID_VALID"),
         builder.getInputPin<ANDGate>("TERMINAL", "A")});
    builder.addNewWire<32>(
        "IN_PC",
        getInputPin<32>("IN_PC"),
        {builder.getInputPin<Rewire, 32>("PASS", "IN_PC")});
    builder.addNewWire<32>(
        "IN_INSTRUCTION",
        getInputPin<32>("IN_INSTRUCTION"),
        {builder.getInputPin<Rewire, 32>(
             "PASS", "IN_INSTRUCTION"),
         decode->getInputPin<32>("INSTRUCTION")});
    builder.addNewWire<32>(
        "IN_FETCH_STATUS",
        getInputPin<32>("IN_FETCH_STATUS"),
        {builder.getInputPin<Rewire, 32>(
             "PASS", "IN_FETCH_STATUS"),
         builder.getInputPin<Rewire, 32>(
             "FETCH_STATUS_UNPACK", "STATUS")});
    builder.addNewWire<32>(
        "EX_CONTROL",
        getInputPin<32>("EX_CONTROL"),
        {builder.getInputPin<Rewire, 32>(
            "EX_CONTROL_UNPACK", "CONTROL")});
    builder.addNewWire<32>(
        "EX_ADDRESSES",
        getInputPin<32>("EX_REGISTER_ADDRESSES"),
        {builder.getInputPin<Rewire, 32>(
            "EX_ADDRESSES_UNPACK", "ADDRESSES")});
    builder.addNewWire(
        "EX_VALID",
        getInputPin("EX_VALID"),
        {hazard->getInputPin("EX_VALID")});
    builder.addNewWire(
        "WB_VALID",
        getInputPin("WB_VALID"),
        {bypass->getInputPin("MEM_WB_VALID")});
    builder.addNewWire(
        "WB_REG_WRITE",
        getInputPin("WB_REG_WRITE"),
        {bypass->getInputPin("MEM_WB_REG_WRITE")});
    builder.addNewWire(
        "WB_REGISTER_WRITE",
        getInputPin("WB_REGISTER_WRITE"),
        {register_file_->getInputPin("REG_WRITE")});
    builder.addNewWire<5>(
        "WB_RD",
        getInputPin<5>("WB_RD"),
        {bypass->getInputPin<5>("MEM_WB_RD"),
         register_file_->getInputPin<5>("RD_ADDR")});
    builder.addNewWire<32>(
        "WB_VALUE",
        getInputPin<32>("WB_VALUE"),
        {builder.getInputPin<Mux4to1_32bit, 32>("RS1_MUX", "IN2"),
         builder.getInputPin<Mux4to1_32bit, 32>("RS2_MUX", "IN2"),
         register_file_->getInputPin<32>("WRITE_DATA")});
    builder.addNewWire(
        "LOW",
        builder.getOutputPin<ConstantValue<1>>("LOW", "OUT"),
        {bypass->getInputPin("EX_MEM_VALID"),
         bypass->getInputPin("EX_MEM_REG_WRITE"),
         bypass->getInputPin("EX_MEM_RESULT_READY")});
    builder.addNewWire<5>(
        "ZERO5",
        builder.getOutputPin<ConstantValue<5>, 5>("ZERO5", "OUT"),
        {bypass->getInputPin<5>("EX_MEM_RD")});
    builder.addNewWire<32>(
        "ZERO32",
        builder.getOutputPin<ConstantValue<32>, 32>("ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("RS1_MUX", "IN1"),
         builder.getInputPin<Mux4to1_32bit, 32>("RS1_MUX", "IN3"),
         builder.getInputPin<Mux4to1_32bit, 32>("RS2_MUX", "IN1"),
         builder.getInputPin<Mux4to1_32bit, 32>("RS2_MUX", "IN3")});

    builder.addNewWire<5>(
        "RS1",
        decode->getOutputPin<5>("RS1_ADDR"),
        {register_file_->getInputPin<5>("RS1_ADDR"),
         hazard->getInputPin<5>("ID_RS1"),
         bypass->getInputPin<5>("ID_EX_RS1"),
         builder.getInputPin<Rewire, 5>(
             "ADDRESSES_PACK", "RS1")});
    builder.addNewWire<5>(
        "RS2",
        decode->getOutputPin<5>("RS2_ADDR"),
        {register_file_->getInputPin<5>("RS2_ADDR"),
         hazard->getInputPin<5>("ID_RS2"),
         bypass->getInputPin<5>("ID_EX_RS2"),
         builder.getInputPin<Rewire, 5>(
             "ADDRESSES_PACK", "RS2")});
    builder.addNewWire<5>(
        "RD",
        decode->getOutputPin<5>("RD_ADDR"),
        {builder.getInputPin<Rewire, 5>(
            "ADDRESSES_PACK", "RD")});
    builder.addNewWire<5>(
        "EX_RD",
        builder.getOutputPin<Rewire, 5>(
            "EX_ADDRESSES_UNPACK", "RD"),
        {hazard->getInputPin<5>("EX_RD")});
    builder.addNewWire(
        "EX_MEM_READ",
        builder.getOutputPin<Rewire>(
            "EX_CONTROL_UNPACK", "MEM_READ"),
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
        builder.addNewWireDynamic(
            "CONTROL_" + std::string(field_name),
            width,
            decode->getOutputPinDynamic(field_name),
            sinks);
    }
    builder.addNewWire<32>(
        "IMMEDIATE",
        decode->getOutputPin<32>("IMM"),
        {getOutputPin<32>("OUT_IMMEDIATE")});
    builder.addNewWire<32>(
        "CONTROL_WORD",
        builder.getOutputPin<Rewire, 32>(
            "CONTROL_PACK", "CONTROL"),
        {getOutputPin<32>("OUT_CONTROL")});
    builder.addNewWire<32>(
        "ADDRESSES",
        builder.getOutputPin<Rewire, 32>(
            "ADDRESSES_PACK", "ADDRESSES"),
        {getOutputPin<32>("OUT_REGISTER_ADDRESSES")});
    builder.addNewWire<32>(
        "RS1_RAW",
        register_file_->getOutputPin<32>("RS1_DATA"),
        {builder.getInputPin<Mux4to1_32bit, 32>("RS1_MUX", "IN0")});
    builder.addNewWire<32>(
        "RS2_RAW",
        register_file_->getOutputPin<32>("RS2_DATA"),
        {builder.getInputPin<Mux4to1_32bit, 32>("RS2_MUX", "IN0")});
    builder.addNewWire<2>(
        "BYPASS_A",
        bypass->getOutputPin<2>("FORWARD_A"),
        {builder.getInputPin<Mux4to1_32bit, 2>("RS1_MUX", "SEL")});
    builder.addNewWire<2>(
        "BYPASS_B",
        bypass->getOutputPin<2>("FORWARD_B"),
        {builder.getInputPin<Mux4to1_32bit, 2>("RS2_MUX", "SEL")});
    builder.addNewWire<32>(
        "RS1_VALUE",
        builder.getOutputPin<Mux4to1_32bit, 32>("RS1_MUX", "OUT"),
        {getOutputPin<32>("OUT_RS1_VALUE")});
    builder.addNewWire<32>(
        "RS2_VALUE",
        builder.getOutputPin<Mux4to1_32bit, 32>("RS2_MUX", "OUT"),
        {getOutputPin<32>("OUT_RS2_VALUE")});
    builder.addNewWire(
        "HAZARD",
        hazard->getOutputPin("LOAD_USE_HAZARD"),
        {getOutputPin("LOAD_USE_HAZARD")});

    const auto pass = builder.getComponent<Rewire>("PASS");
    const std::array<std::pair<const char*, size_t>, 4> pass_fields{{
        {"VALID", 1}, {"PC", 32},
        {"INSTRUCTION", 32}, {"FETCH_STATUS", 32},
    }};
    for (const auto& [field_name, width] : pass_fields) {
        builder.addNewWireDynamic(
            "PASS_" + std::string(field_name),
            width,
            pass->getOutputPinDynamic("OUT_" + std::string(field_name)),
            {getOutputPinDynamic("OUT_" + std::string(field_name))});
    }

    builder.addNewWire(
        "STATUS_PC",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "PC_MISALIGNED"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_1", "A")});
    builder.addNewWire(
        "STATUS_FAULT",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "INSTRUCTION_FAULT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_1", "B")});
    builder.addNewWire(
        "NOT_LEGAL",
        builder.getOutputPin<NOTGate>("NOT_LEGAL", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_2", "B")});
    builder.addNewWire(
        "REASON_1",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_1", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_2", "A")});
    builder.addNewWire(
        "REASON_2",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_2", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_3", "A")});
    builder.addNewWire(
        "REASON_3",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_3", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_4", "A")});
    builder.addNewWire(
        "REASON",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_4", "OUT"),
        {builder.getInputPin<ANDGate>("TERMINAL", "B")});
    builder.addNewWire(
        "TERMINAL",
        builder.getOutputPin<ANDGate>("TERMINAL", "OUT"),
        {getOutputPin("TERMINAL")});
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

RV32IExecuteStage::RV32IExecuteStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IExecuteStage.pinInitializer()) {}

void RV32IExecuteStage::buildInternals(ComponentBuilder& builder) {
    const auto forwarding = builder.add(
        circuit::families::RV32IForwardingUnit, "FORWARDING");
    const auto alu_component = builder.add(
        circuit::families::ALU32, "ALU");
    const auto control_flow = builder.add(
        circuit::families::RV32IPipelineControlFlowUnit,
        "CONTROL_FLOW");
    builder.addNewComponent<Mux4to1_32bit>("FORWARD_A_MUX");
    builder.addNewComponent<Mux4to1_32bit>("FORWARD_B_MUX");
    builder.addNewComponent<Mux4to1_32bit>("ALU_A_MUX");
    builder.addNewComponent<Mux4to1_32bit>("ALU_B_MUX");
    builder.addNewComponent<ConstantValue<32>>("ZERO32", 0);
    addControlUnpack(builder, "CONTROL_UNPACK");
    addAddressUnpack(builder, "ADDRESSES_UNPACK");
    addStatusUnpack(
        builder,
        "FETCH_STATUS_UNPACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});
    addStatusPack(
        builder,
        "EXECUTION_STATUS_PACK",
        {"TARGET_MISALIGNED"});
    addPassThrough(
        builder,
        "PASS",
        {{"VALID", 1},
         {"PC", 32},
         {"INSTRUCTION", 32},
         {"CONTROL", 32},
         {"REGISTER_ADDRESSES", 32},
         {"FETCH_STATUS", 32}});
    builder.addNewComponent<NOTGate>("NOT_LEGAL");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_1");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_2");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_3");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_4");
    builder.addNewComponent<ORGate>("TERMINAL_REASON_5");
    builder.addNewComponent<ANDGate>("PRETERMINAL");
    builder.addNewComponent<NOTGate>("NOT_PRETERMINAL");
    builder.addNewComponent<ANDGate>("REDIRECT_VALID");
    builder.addNewComponent<ANDGate>("REDIRECT");

    builder.addNewWire(
        "IN_VALID",
        getInputPin("IN_VALID"),
        {builder.getInputPin<Rewire>("PASS", "IN_VALID"),
         builder.getInputPin<ANDGate>("PRETERMINAL", "A"),
         builder.getInputPin<ANDGate>("REDIRECT_VALID", "A")});
    const std::array<const char*, 5> pass_input_fields{
        "PC", "INSTRUCTION", "CONTROL",
        "REGISTER_ADDRESSES", "FETCH_STATUS"};
    for (const auto* field_name : pass_input_fields) {
        std::vector<std::shared_ptr<PinBase>> sinks{
            builder.getInputPin<Rewire, 32>(
                "PASS", "IN_" + std::string(field_name))};
        if (std::string(field_name) == "PC") {
            sinks.push_back(
                builder.getInputPin<Mux4to1_32bit, 32>(
                    "ALU_A_MUX", "IN1"));
            sinks.push_back(control_flow->getInputPin<32>("PC"));
        } else if (std::string(field_name) == "CONTROL") {
            sinks.push_back(
                builder.getInputPin<Rewire, 32>(
                    "CONTROL_UNPACK", "CONTROL"));
        } else if (
            std::string(field_name) == "REGISTER_ADDRESSES") {
            sinks.push_back(
                builder.getInputPin<Rewire, 32>(
                    "ADDRESSES_UNPACK", "ADDRESSES"));
        } else if (
            std::string(field_name) == "FETCH_STATUS") {
            sinks.push_back(
                builder.getInputPin<Rewire, 32>(
                    "FETCH_STATUS_UNPACK", "STATUS"));
        }
        builder.addNewWireDynamic(
            "IN_" + std::string(field_name),
            32,
            getInputPinDynamic(
                "IN_" + std::string(field_name)),
            sinks);
    }
    builder.addNewWire<32>(
        "RS1_VALUE",
        getInputPin<32>("IN_RS1_VALUE"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "FORWARD_A_MUX", "IN0")});
    builder.addNewWire<32>(
        "RS2_VALUE",
        getInputPin<32>("IN_RS2_VALUE"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "FORWARD_B_MUX", "IN0")});
    builder.addNewWire<32>(
        "IMMEDIATE",
        getInputPin<32>("IN_IMMEDIATE"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_B_MUX", "IN1"),
         control_flow->getInputPin<32>("IMMEDIATE")});
    builder.addNewWire(
        "EX_MEM_VALID",
        getInputPin("EX_MEM_VALID"),
        {forwarding->getInputPin("EX_MEM_VALID")});
    builder.addNewWire<5>(
        "EX_MEM_RD",
        getInputPin<5>("EX_MEM_RD"),
        {forwarding->getInputPin<5>("EX_MEM_RD")});
    builder.addNewWire(
        "EX_MEM_REG_WRITE",
        getInputPin("EX_MEM_REG_WRITE"),
        {forwarding->getInputPin("EX_MEM_REG_WRITE")});
    builder.addNewWire(
        "EX_MEM_RESULT_READY",
        getInputPin("EX_MEM_RESULT_READY"),
        {forwarding->getInputPin("EX_MEM_RESULT_READY")});
    builder.addNewWire<32>(
        "EX_MEM_VALUE",
        getInputPin<32>("EX_MEM_VALUE"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_A_MUX", "IN1"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_B_MUX", "IN1")});
    builder.addNewWire(
        "MEM_WB_VALID",
        getInputPin("MEM_WB_VALID"),
        {forwarding->getInputPin("MEM_WB_VALID")});
    builder.addNewWire<5>(
        "MEM_WB_RD",
        getInputPin<5>("MEM_WB_RD"),
        {forwarding->getInputPin<5>("MEM_WB_RD")});
    builder.addNewWire(
        "MEM_WB_REG_WRITE",
        getInputPin("MEM_WB_REG_WRITE"),
        {forwarding->getInputPin("MEM_WB_REG_WRITE")});
    builder.addNewWire<32>(
        "MEM_WB_VALUE",
        getInputPin<32>("MEM_WB_VALUE"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_A_MUX", "IN2"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_B_MUX", "IN2")});
    builder.addNewWire<32>(
        "ZERO32",
        builder.getOutputPin<ConstantValue<32>, 32>("ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_A_MUX", "IN3"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "FORWARD_B_MUX", "IN3"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_A_MUX", "IN2"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_A_MUX", "IN3"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_B_MUX", "IN2"),
         builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_B_MUX", "IN3")});

    builder.addNewWire<5>(
        "RS1",
        builder.getOutputPin<Rewire, 5>(
            "ADDRESSES_UNPACK", "RS1"),
        {forwarding->getInputPin<5>("ID_EX_RS1")});
    builder.addNewWire<5>(
        "RS2",
        builder.getOutputPin<Rewire, 5>(
            "ADDRESSES_UNPACK", "RS2"),
        {forwarding->getInputPin<5>("ID_EX_RS2")});
    builder.addNewWire<2>(
        "FORWARD_A",
        forwarding->getOutputPin<2>("FORWARD_A"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
            "FORWARD_A_MUX", "SEL")});
    builder.addNewWire<2>(
        "FORWARD_B",
        forwarding->getOutputPin<2>("FORWARD_B"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
            "FORWARD_B_MUX", "SEL")});
    builder.addNewWire<32>(
        "FORWARDED_RS1",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "FORWARD_A_MUX", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_A_MUX", "IN0"),
         control_flow->getInputPin<32>("RS1_VALUE")});
    builder.addNewWire<32>(
        "FORWARDED_RS2",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "FORWARD_B_MUX", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "ALU_B_MUX", "IN0"),
         getOutputPin<32>("OUT_STORE_DATA")});
    builder.addNewWire<5>(
        "ALU_OP",
        builder.getOutputPin<Rewire, 5>(
            "CONTROL_UNPACK", "ALU_OP"),
        {alu_component->getInputPin<5>("OP")});
    builder.addNewWire<2>(
        "ALU_A_SEL",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "ALU_A_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
            "ALU_A_MUX", "SEL")});
    builder.addNewWire<2>(
        "ALU_B_SEL",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "ALU_B_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
            "ALU_B_MUX", "SEL")});
    builder.addNewWire<3>(
        "BRANCH_TYPE",
        builder.getOutputPin<Rewire, 3>(
            "CONTROL_UNPACK", "BRANCH_TYPE"),
        {control_flow->getInputPin<3>("BRANCH_TYPE")});
    builder.addNewWire<2>(
        "JUMP_TYPE",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "JUMP_TYPE"),
        {control_flow->getInputPin<2>("JUMP_TYPE")});
    builder.addNewWire<32>(
        "ALU_A",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "ALU_A_MUX", "OUT"),
        {alu_component->getInputPin<32>("A")});
    builder.addNewWire<32>(
        "ALU_B",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "ALU_B_MUX", "OUT"),
        {alu_component->getInputPin<32>("B")});
    builder.addNewWire<32>(
        "ALU_RESULT",
        alu_component->getOutputPin<32>("OUT"),
        {getOutputPin<32>("OUT_ALU_RESULT")});
    builder.addNewWire(
        "EQ",
        alu_component->getOutputPin("EQ"),
        {control_flow->getInputPin("EQ")});
    builder.addNewWire(
        "LT_SIGNED",
        alu_component->getOutputPin("LT_SIGNED"),
        {control_flow->getInputPin("LT_SIGNED")});
    builder.addNewWire(
        "LT_UNSIGNED",
        alu_component->getOutputPin("LT_UNSIGNED"),
        {control_flow->getInputPin("LT_UNSIGNED")});
    builder.addNewWire<32>(
        "PC_PLUS_4",
        control_flow->getOutputPin<32>("PC_PLUS_4"),
        {getOutputPin<32>("OUT_PC_PLUS_4")});
    builder.addNewWire<32>(
        "NEXT_PC",
        control_flow->getOutputPin<32>("NEXT_PC"),
        {getOutputPin<32>("OUT_NEXT_PC"),
         getOutputPin<32>("REDIRECT_PC")});
    builder.addNewWire(
        "TARGET_MISALIGNED",
        control_flow->getOutputPin("TARGET_MISALIGNED"),
        {builder.getInputPin<Rewire>(
             "EXECUTION_STATUS_PACK", "TARGET_MISALIGNED"),
         builder.getInputPin<ORGate>("TERMINAL_REASON_5", "B")});
    builder.addNewWire<32>(
        "EXECUTION_STATUS",
        builder.getOutputPin<Rewire, 32>(
            "EXECUTION_STATUS_PACK", "STATUS"),
        {getOutputPin<32>("OUT_EXECUTION_STATUS")});

    builder.addNewWire(
        "FETCH_PC_MISALIGNED",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "PC_MISALIGNED"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_1", "A")});
    builder.addNewWire(
        "FETCH_FAULT",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "INSTRUCTION_FAULT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_1", "B")});
    builder.addNewWire(
        "LEGAL",
        builder.getOutputPin<Rewire>("CONTROL_UNPACK", "LEGAL"),
        {builder.getInputPin<NOTGate>("NOT_LEGAL", "IN")});
    builder.addNewWire(
        "NOT_LEGAL",
        builder.getOutputPin<NOTGate>("NOT_LEGAL", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_2", "B")});
    builder.addNewWire(
        "TRAP",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "TRAP_REQUEST"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_3", "B")});
    builder.addNewWire(
        "HALT",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "HALT_REQUEST"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_4", "B")});
    builder.addNewWire(
        "REASON_1",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_1", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_2", "A")});
    builder.addNewWire(
        "REASON_2",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_2", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_3", "A")});
    builder.addNewWire(
        "REASON_3",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_3", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_4", "A")});
    builder.addNewWire(
        "REASON_4",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_4", "OUT"),
        {builder.getInputPin<ORGate>("TERMINAL_REASON_5", "A")});
    builder.addNewWire(
        "PRETERMINAL_REASON",
        builder.getOutputPin<ORGate>("TERMINAL_REASON_5", "OUT"),
        {builder.getInputPin<ANDGate>("PRETERMINAL", "B")});
    builder.addNewWire(
        "PRETERMINAL",
        builder.getOutputPin<ANDGate>("PRETERMINAL", "OUT"),
        {getOutputPin("PRETERMINAL"),
         builder.getInputPin<NOTGate>("NOT_PRETERMINAL", "IN")});
    builder.addNewWire(
        "RAW_REDIRECT",
        control_flow->getOutputPin("REDIRECT"),
        {builder.getInputPin<ANDGate>("REDIRECT_VALID", "B")});
    builder.addNewWire(
        "REDIRECT_VALID",
        builder.getOutputPin<ANDGate>("REDIRECT_VALID", "OUT"),
        {builder.getInputPin<ANDGate>("REDIRECT", "A")});
    builder.addNewWire(
        "NOT_PRETERMINAL",
        builder.getOutputPin<NOTGate>("NOT_PRETERMINAL", "OUT"),
        {builder.getInputPin<ANDGate>("REDIRECT", "B")});
    builder.addNewWire(
        "REDIRECT",
        builder.getOutputPin<ANDGate>("REDIRECT", "OUT"),
        {getOutputPin("REDIRECT")});

    const auto pass = builder.getComponent<Rewire>("PASS");
    for (const auto* field_name : {
             "VALID", "PC", "INSTRUCTION", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS"}) {
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

namespace {
using NetId = size_t;

struct StageNet {
    std::string name;
    std::shared_ptr<Pin<>> source;
    std::vector<std::shared_ptr<Pin<>>> sinks;
};

class StageLogic {
public:
    explicit StageLogic(ComponentBuilder& builder)
        : builder_(builder) {}

    NetId source(std::string name, std::shared_ptr<Pin<>> pin) {
        nets_.push_back({std::move(name), std::move(pin), {}});
        return nets_.size() - 1;
    }

    void sink(NetId net, std::shared_ptr<Pin<>> pin) {
        nets_.at(net).sinks.push_back(std::move(pin));
    }

    NetId logicalNot(std::string name, NetId input) {
        builder_.addNewComponent<NOTGate>(name);
        sink(input, builder_.getInputPin<NOTGate>(name, "IN"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<NOTGate>(name, "OUT"));
    }

    NetId logicalAnd(std::string name, NetId left, NetId right) {
        builder_.addNewComponent<ANDGate>(name);
        sink(left, builder_.getInputPin<ANDGate>(name, "A"));
        sink(right, builder_.getInputPin<ANDGate>(name, "B"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<ANDGate>(name, "OUT"));
    }

    NetId logicalOr(std::string name, NetId left, NetId right) {
        builder_.addNewComponent<ORGate>(name);
        sink(left, builder_.getInputPin<ORGate>(name, "A"));
        sink(right, builder_.getInputPin<ORGate>(name, "B"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<ORGate>(name, "OUT"));
    }

    NetId andAll(std::string prefix, std::vector<NetId> inputs) {
        auto result = inputs.front();
        for (size_t index = 1; index < inputs.size(); ++index) {
            result = logicalAnd(
                prefix + "_" + std::to_string(index),
                result,
                inputs[index]);
        }
        return result;
    }

    NetId orAll(std::string prefix, std::vector<NetId> inputs) {
        auto result = inputs.front();
        for (size_t index = 1; index < inputs.size(); ++index) {
            result = logicalOr(
                prefix + "_" + std::to_string(index),
                result,
                inputs[index]);
        }
        return result;
    }

    void materialize() {
        for (auto& net : nets_) {
            builder_.addNewWire(net.name, net.source, net.sinks);
        }
    }

private:
    ComponentBuilder& builder_;
    std::vector<StageNet> nets_;
};

void addCauseExpand(ComponentBuilder& builder, const char* name) {
    builder.addNewComponent<Rewire>(
        name,
        std::vector<Rewire::WireSpec>{{"CAUSE", 4}},
        std::vector<Rewire::WireSpec>{{"WORD", 32}},
        identity_mapping("CAUSE", 0, 4, "WORD"),
        Rewire::UnmappedBitValue::LOW);
}

void addCauseSlice(ComponentBuilder& builder, const char* name) {
    builder.addNewComponent<Rewire>(
        name,
        std::vector<Rewire::WireSpec>{{"WORD", 32}},
        std::vector<Rewire::WireSpec>{{"CAUSE", 4}},
        identity_mapping("WORD", 0, 4, "CAUSE"),
        Rewire::UnmappedBitValue::LOW);
}
} // namespace

RV32IWritebackStage::RV32IWritebackStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IWritebackStage.pinInitializer()) {}

void RV32IWritebackStage::buildInternals(ComponentBuilder& builder) {
    const auto retirement = builder.add(
        circuit::families::RV32IPipelineRetirementUnit,
        "RETIREMENT");
    const auto committed_pc = builder.add(
        circuit::families::Register32, "COMMITTED_PC_STATE");
    const auto halted = builder.add(
        circuit::families::MemoryBit, "HALTED_STATE");
    const auto trapped = builder.add(
        circuit::families::MemoryBit, "TRAPPED_STATE");
    const auto trap_cause = builder.add(
        circuit::families::Register32, "TRAP_CAUSE_STATE");
    const auto retired_count = builder.add(
        circuit::families::Register32, "RETIRED_COUNT_STATE");
    const auto retired_pc = builder.add(
        circuit::families::Register32, "RETIRED_PC_STATE");
    const auto retired_instruction = builder.add(
        circuit::families::Register32, "RETIRED_INSTRUCTION_STATE");
    const auto retired_mem_addr = builder.add(
        circuit::families::Register32, "RETIRED_MEM_ADDR_STATE");
    const auto retired_mem_write_data = builder.add(
        circuit::families::Register32,
        "RETIRED_MEM_WRITE_DATA_STATE");
    const auto retired_mem_read_data = builder.add(
        circuit::families::Register32,
        "RETIRED_MEM_READ_DATA_STATE");
    const auto retired_mem_meta = builder.add(
        circuit::families::Register32, "RETIRED_MEM_META_STATE");

    builder.addNewComponent<Adder32>("RETIRED_COUNT_PLUS_1");
    builder.addNewComponent<Mux4to1_32bit>("WRITEBACK_MUX");
    builder.addNewComponent<Mux2to1_32bit>("RETIRED_MEM_ADDR_MUX");
    builder.addNewComponent<Mux2to1_32bit>(
        "RETIRED_MEM_WRITE_DATA_MUX");
    builder.addNewComponent<Mux2to1_32bit>(
        "RETIRED_MEM_READ_DATA_MUX");
    builder.addNewComponent<Mux2to1_32bit>("RETIRED_MEM_META_MUX");
    builder.addNewComponent<ConstantValue<1>>("LOW", 0);
    builder.addNewComponent<ConstantValue<1>>("HIGH", 1);
    builder.addNewComponent<ConstantValue<32>>("ZERO32", 0);
    builder.addNewComponent<ConstantValue<32>>("ONE32", 1);
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
    addStatusUnpack(
        builder,
        "MEMORY_STATUS_UNPACK",
        {"DATA_MISALIGNED", "DATA_FAULT"});
    addCauseExpand(builder, "TRAP_CAUSE_EXPAND");
    addCauseSlice(builder, "TRAP_CAUSE_SLICE");
    addPassThrough(builder, "VALID_PASS", {{"VALID", 1}});
    builder.addNewComponent<Rewire>(
        "RETIRED_MEMORY_META_PACK",
        std::vector<Rewire::WireSpec>{
            {"READ", 1}, {"WRITE", 1}, {"SIZE", 2},
            {"SIGN_EXTEND", 1}, {"FAULT", 1}},
        std::vector<Rewire::WireSpec>{{"STATUS", 32}},
        std::vector<Rewire::BitMap>{
            {"READ", 0, "STATUS", 0},
            {"WRITE", 0, "STATUS", 1},
            {"SIZE", 0, "STATUS", 2},
            {"SIZE", 1, "STATUS", 3},
            {"SIGN_EXTEND", 0, "STATUS", 4},
            {"FAULT", 0, "STATUS", 5}},
        Rewire::UnmappedBitValue::LOW);
    builder.addNewComponent<Rewire>(
        "RETIRED_MEMORY_META",
        std::vector<Rewire::WireSpec>{{"STATUS", 32}},
        std::vector<Rewire::WireSpec>{
            {"READ", 1}, {"WRITE", 1}, {"SIZE", 2},
            {"SIGN_EXTEND", 1}, {"FAULT", 1}},
        std::vector<Rewire::BitMap>{
            {"STATUS", 0, "READ", 0},
            {"STATUS", 1, "WRITE", 0},
            {"STATUS", 2, "SIZE", 0},
            {"STATUS", 3, "SIZE", 1},
            {"STATUS", 4, "SIGN_EXTEND", 0},
            {"STATUS", 5, "FAULT", 0}},
        Rewire::UnmappedBitValue::LOW);

    std::vector<std::shared_ptr<Pin<>>> clocks;
    std::vector<std::shared_ptr<Pin<>>> resets;
    for (const auto& state : {
             committed_pc, halted, trapped, trap_cause,
             retired_count, retired_pc, retired_instruction,
             retired_mem_addr, retired_mem_write_data,
             retired_mem_read_data, retired_mem_meta}) {
        clocks.push_back(state->getInputPin("CLK"));
        resets.push_back(state->getInputPin("RST"));
    }
    builder.addNewWire("CLK", getInputPin("CLK"), clocks);
    builder.addNewWire("RST", getInputPin("RST"), resets);
    builder.addNewWire(
        "COMMIT_ENABLE",
        getInputPin("COMMIT_ENABLE"),
        {retirement->getInputPin("ENABLE")});
    builder.addNewWire(
        "IN_VALID",
        getInputPin("IN_VALID"),
        {retirement->getInputPin("MEM_WB_VALID"),
         builder.getInputPin<Rewire>(
             "VALID_PASS", "IN_VALID")});
    builder.addNewWire<32>(
        "IN_PC",
        getInputPin<32>("IN_PC"),
        {retired_pc->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "IN_INSTRUCTION",
        getInputPin<32>("IN_INSTRUCTION"),
        {retired_instruction->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "IN_ALU_RESULT",
        getInputPin<32>("IN_ALU_RESULT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "WRITEBACK_MUX", "IN1"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_ADDR_MUX", "B")});
    builder.addNewWire<32>(
        "IN_MEMORY_DATA",
        getInputPin<32>("IN_MEMORY_DATA"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "WRITEBACK_MUX", "IN2"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_READ_DATA_MUX", "B")});
    builder.addNewWire<32>(
        "IN_STORE_DATA",
        getInputPin<32>("IN_STORE_DATA"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_WRITE_DATA_MUX", "B")});
    builder.addNewWire<32>(
        "IN_PC_PLUS_4",
        getInputPin<32>("IN_PC_PLUS_4"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
            "WRITEBACK_MUX", "IN3")});
    builder.addNewWire<32>(
        "IN_NEXT_PC",
        getInputPin<32>("IN_NEXT_PC"),
        {committed_pc->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "IN_CONTROL",
        getInputPin<32>("IN_CONTROL"),
        {builder.getInputPin<Rewire, 32>(
             "CONTROL_UNPACK", "CONTROL")});
    builder.addNewWire<32>(
        "IN_ADDRESSES",
        getInputPin<32>("IN_REGISTER_ADDRESSES"),
        {builder.getInputPin<Rewire, 32>(
            "ADDRESSES_UNPACK", "ADDRESSES")});
    builder.addNewWire<32>(
        "IN_FETCH_STATUS",
        getInputPin<32>("IN_FETCH_STATUS"),
        {builder.getInputPin<Rewire, 32>(
            "FETCH_STATUS_UNPACK", "STATUS")});
    builder.addNewWire<32>(
        "IN_EXECUTION_STATUS",
        getInputPin<32>("IN_EXECUTION_STATUS"),
        {builder.getInputPin<Rewire, 32>(
            "EXECUTION_STATUS_UNPACK", "STATUS")});
    builder.addNewWire<32>(
        "IN_MEMORY_STATUS",
        getInputPin<32>("IN_MEMORY_STATUS"),
        {builder.getInputPin<Rewire, 32>(
            "MEMORY_STATUS_UNPACK", "STATUS")});
    builder.addNewWire(
        "LOW",
        builder.getOutputPin<ConstantValue<1>>("LOW", "OUT"),
        {builder.getInputPin<Adder32>(
            "RETIRED_COUNT_PLUS_1", "Cin")});
    builder.addNewWire(
        "HIGH",
        builder.getOutputPin<ConstantValue<1>>("HIGH", "OUT"),
        {halted->getInputPin("D"), trapped->getInputPin("D")});
    builder.addNewWire<32>(
        "ZERO32",
        builder.getOutputPin<ConstantValue<32>, 32>("ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>(
             "WRITEBACK_MUX", "IN0"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_ADDR_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_WRITE_DATA_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_READ_DATA_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>(
             "RETIRED_MEM_META_MUX", "A")});
    builder.addNewWire<32>(
        "ONE32",
        builder.getOutputPin<ConstantValue<32>, 32>("ONE32", "OUT"),
        {builder.getInputPin<Adder32, 32>(
            "RETIRED_COUNT_PLUS_1", "B")});
    builder.addNewWire<32>(
        "RETIRED_COUNT_Q",
        retired_count->getOutputPin<32>("Q"),
        {builder.getInputPin<Adder32, 32>(
             "RETIRED_COUNT_PLUS_1", "A"),
         getOutputPin<32>("RETIRED_COUNT")});
    builder.addNewWire<32>(
        "RETIRED_COUNT_NEXT",
        builder.getOutputPin<Adder32, 32>(
            "RETIRED_COUNT_PLUS_1", "Sum"),
        {retired_count->getInputPin<32>("D")});

    builder.addNewWire<2>(
        "WRITEBACK_SEL",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "WRITEBACK_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>(
            "WRITEBACK_MUX", "SEL")});
    builder.addNewWire<32>(
        "WRITEBACK_VALUE",
        builder.getOutputPin<Mux4to1_32bit, 32>(
            "WRITEBACK_MUX", "OUT"),
        {getOutputPin<32>("WB_VALUE")});
    builder.addNewWire<5>(
        "WB_RD",
        builder.getOutputPin<Rewire, 5>(
            "ADDRESSES_UNPACK", "RD"),
        {getOutputPin<5>("WB_RD")});
    builder.addNewWire<2>(
        "MEM_SIZE",
        builder.getOutputPin<Rewire, 2>(
            "CONTROL_UNPACK", "MEM_SIZE"),
        {builder.getInputPin<Rewire, 2>(
             "RETIRED_MEMORY_META_PACK", "SIZE")});

    StageLogic logic_network(builder);
    const auto valid = logic_network.source(
        "VALID_BUFFERED",
        builder.getOutputPin<Rewire>(
            "VALID_PASS", "OUT_VALID"));
    const auto pc_misaligned = logic_network.source(
        "PC_MISALIGNED",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "PC_MISALIGNED"));
    const auto instruction_fault = logic_network.source(
        "INSTRUCTION_FAULT",
        builder.getOutputPin<Rewire>(
            "FETCH_STATUS_UNPACK", "INSTRUCTION_FAULT"));
    const auto legal = logic_network.source(
        "LEGAL",
        builder.getOutputPin<Rewire>("CONTROL_UNPACK", "LEGAL"));
    const auto trap_request = logic_network.source(
        "TRAP_REQUEST",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "TRAP_REQUEST"));
    const auto halt_request = logic_network.source(
        "HALT_REQUEST",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "HALT_REQUEST"));
    const auto mem_read = logic_network.source(
        "MEM_READ",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "MEM_READ"));
    const auto mem_write = logic_network.source(
        "MEM_WRITE",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "MEM_WRITE"));
    const auto reg_write = logic_network.source(
        "REG_WRITE",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "REG_WRITE"));
    const auto load_sign = logic_network.source(
        "LOAD_SIGN_EXTEND",
        builder.getOutputPin<Rewire>(
            "CONTROL_UNPACK", "LOAD_SIGN_EXTEND"));
    const auto target_misaligned = logic_network.source(
        "TARGET_MISALIGNED",
        builder.getOutputPin<Rewire>(
            "EXECUTION_STATUS_UNPACK", "TARGET_MISALIGNED"));
    const auto stored_misaligned = logic_network.source(
        "STORED_MISALIGNED",
        builder.getOutputPin<Rewire>(
            "MEMORY_STATUS_UNPACK", "DATA_MISALIGNED"));
    const auto stored_fault = logic_network.source(
        "STORED_FAULT",
        builder.getOutputPin<Rewire>(
            "MEMORY_STATUS_UNPACK", "DATA_FAULT"));
    const auto retire_misaligned = stored_misaligned;
    const auto retire_fault = stored_fault;
    const auto preterminal_reason = logic_network.orAll(
        "PRETERMINAL_REASON",
        {pc_misaligned,
         instruction_fault,
         logic_network.logicalNot("NOT_LEGAL", legal),
         trap_request,
         halt_request,
         target_misaligned});
    const auto terminal_reason = logic_network.orAll(
        "TERMINAL_REASON",
        {preterminal_reason, retire_misaligned, retire_fault});
    const auto terminal = logic_network.logicalAnd(
        "TERMINAL", valid, terminal_reason);
    const auto memory_access = logic_network.logicalOr(
        "MEMORY_ACCESS", mem_read, mem_write);

    logic_network.sink(valid, getOutputPin("WB_VALID"));
    logic_network.sink(terminal, getOutputPin("TERMINAL"));
    logic_network.sink(
        memory_access,
        builder.getInputPin<Mux2to1_32bit>(
            "RETIRED_MEM_ADDR_MUX", "SEL"));
    logic_network.sink(
        memory_access,
        builder.getInputPin<Mux2to1_32bit>(
            "RETIRED_MEM_WRITE_DATA_MUX", "SEL"));
    logic_network.sink(
        memory_access,
        builder.getInputPin<Mux2to1_32bit>(
            "RETIRED_MEM_READ_DATA_MUX", "SEL"));
    logic_network.sink(
        memory_access,
        builder.getInputPin<Mux2to1_32bit>(
            "RETIRED_MEM_META_MUX", "SEL"));

    logic_network.sink(
        pc_misaligned,
        retirement->getInputPin("PC_MISALIGNED"));
    logic_network.sink(
        instruction_fault,
        retirement->getInputPin("INSTRUCTION_FAULT"));
    logic_network.sink(legal, retirement->getInputPin("LEGAL"));
    logic_network.sink(
        trap_request,
        retirement->getInputPin("TRAP_REQUEST"));
    logic_network.sink(
        mem_read, retirement->getInputPin("MEM_READ"));
    logic_network.sink(
        mem_write, retirement->getInputPin("MEM_WRITE"));
    logic_network.sink(
        retire_misaligned,
        retirement->getInputPin("DATA_MISALIGNED"));
    logic_network.sink(
        retire_fault,
        retirement->getInputPin("DATA_FAULT"));
    logic_network.sink(
        target_misaligned,
        retirement->getInputPin("TARGET_MISALIGNED"));
    logic_network.sink(
        halt_request,
        retirement->getInputPin("HALT_REQUEST"));
    logic_network.sink(
        reg_write,
        retirement->getInputPin("REG_WRITE_REQUEST"));
    logic_network.sink(
        reg_write, getOutputPin("WB_REG_WRITE"));
    logic_network.sink(
        mem_read,
        builder.getInputPin<Rewire>(
            "RETIRED_MEMORY_META_PACK", "READ"));
    logic_network.sink(
        mem_write,
        builder.getInputPin<Rewire>(
            "RETIRED_MEMORY_META_PACK", "WRITE"));
    logic_network.sink(
        load_sign,
        builder.getInputPin<Rewire>(
            "RETIRED_MEMORY_META_PACK", "SIGN_EXTEND"));
    logic_network.sink(
        logic_network.logicalOr(
            "RETIRED_MEMORY_FAULT",
            retire_misaligned,
            retire_fault),
        builder.getInputPin<Rewire>(
            "RETIRED_MEMORY_META_PACK", "FAULT"));

    const auto commit = logic_network.source(
        "COMMIT", retirement->getOutputPin("COMMIT"));
    const auto normal_commit = logic_network.source(
        "NORMAL_COMMIT",
        retirement->getOutputPin("NORMAL_COMMIT"));
    const auto register_write = logic_network.source(
        "REGISTER_WRITE",
        retirement->getOutputPin("REGISTER_WRITE"));
    const auto halt_event = logic_network.source(
        "HALT_EVENT", retirement->getOutputPin("HALT_EVENT"));
    const auto trap_event = logic_network.source(
        "TRAP_EVENT", retirement->getOutputPin("TRAP_EVENT"));
    logic_network.sink(commit, getOutputPin("COMMIT_VALID"));
    for (const auto& state : {
             retired_count,
             retired_pc,
             retired_instruction,
             retired_mem_addr,
             retired_mem_write_data,
             retired_mem_read_data,
             retired_mem_meta}) {
        logic_network.sink(commit, state->getInputPin("WE"));
    }
    logic_network.sink(
        normal_commit, committed_pc->getInputPin("WE"));
    logic_network.sink(
        register_write, getOutputPin("REGISTER_WRITE"));
    logic_network.sink(halt_event, halted->getInputPin("WE"));
    logic_network.sink(trap_event, trapped->getInputPin("WE"));
    logic_network.sink(trap_event, trap_cause->getInputPin("WE"));
    logic_network.sink(
        logic_network.logicalOr(
            "RETIRE_TERMINAL", halt_event, trap_event),
        getOutputPin("RETIRE_TERMINAL"));
    logic_network.materialize();

    builder.addNewWire<4>(
        "DECODE_TRAP_CAUSE",
        builder.getOutputPin<Rewire, 4>(
            "CONTROL_UNPACK", "DECODE_TRAP_CAUSE"),
        {retirement->getInputPin<4>("DECODE_TRAP_CAUSE")});
    builder.addNewWire<4>(
        "RETIREMENT_CAUSE",
        retirement->getOutputPin<4>("TRAP_CAUSE"),
        {builder.getInputPin<Rewire, 4>(
            "TRAP_CAUSE_EXPAND", "CAUSE")});
    builder.addNewWire<32>(
        "TRAP_CAUSE_WORD",
        builder.getOutputPin<Rewire, 32>(
            "TRAP_CAUSE_EXPAND", "WORD"),
        {trap_cause->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "TRAP_CAUSE_Q",
        trap_cause->getOutputPin<32>("Q"),
        {builder.getInputPin<Rewire, 32>(
            "TRAP_CAUSE_SLICE", "WORD")});
    builder.addNewWire<4>(
        "TRAP_CAUSE",
        builder.getOutputPin<Rewire, 4>(
            "TRAP_CAUSE_SLICE", "CAUSE"),
        {getOutputPin<4>("TRAP_CAUSE")});
    builder.addNewWire<32>(
        "PC",
        committed_pc->getOutputPin<32>("Q"),
        {getOutputPin<32>("PC")});
    builder.addNewWire(
        "HALTED",
        halted->getOutputPin("Q"),
        {getOutputPin("HALTED")});
    builder.addNewWire(
        "TRAPPED",
        trapped->getOutputPin("Q"),
        {getOutputPin("TRAPPED")});
    builder.addNewWire<32>(
        "RETIRED_PC",
        retired_pc->getOutputPin<32>("Q"),
        {getOutputPin<32>("RETIRED_PC")});
    builder.addNewWire<32>(
        "RETIRED_INSTRUCTION",
        retired_instruction->getOutputPin<32>("Q"),
        {getOutputPin<32>("RETIRED_INSTRUCTION")});
    builder.addNewWire<32>(
        "RETIRED_META_Q",
        retired_mem_meta->getOutputPin<32>("Q"),
        {builder.getInputPin<Rewire, 32>(
            "RETIRED_MEMORY_META", "STATUS")});
    builder.addNewWire<32>(
        "RETIRED_MEM_ADDR_Q",
        retired_mem_addr->getOutputPin<32>("Q"),
        {getOutputPin<32>("RETIRED_MEM_ADDR")});
    builder.addNewWire<32>(
        "RETIRED_MEM_WRITE_DATA_Q",
        retired_mem_write_data->getOutputPin<32>("Q"),
        {getOutputPin<32>("RETIRED_MEM_WRITE_DATA")});
    builder.addNewWire<32>(
        "RETIRED_MEM_READ_DATA_Q",
        retired_mem_read_data->getOutputPin<32>("Q"),
        {getOutputPin<32>("RETIRED_MEM_READ_DATA")});
    builder.addNewWire(
        "RETIRED_MEM_READ",
        builder.getOutputPin<Rewire>(
            "RETIRED_MEMORY_META", "READ"),
        {getOutputPin("RETIRED_MEM_READ")});
    builder.addNewWire(
        "RETIRED_MEM_WRITE",
        builder.getOutputPin<Rewire>(
            "RETIRED_MEMORY_META", "WRITE"),
        {getOutputPin("RETIRED_MEM_WRITE")});
    builder.addNewWire<2>(
        "RETIRED_MEM_SIZE",
        builder.getOutputPin<Rewire, 2>(
            "RETIRED_MEMORY_META", "SIZE"),
        {getOutputPin<2>("RETIRED_MEM_SIZE")});
    builder.addNewWire(
        "RETIRED_MEM_SIGN",
        builder.getOutputPin<Rewire>(
            "RETIRED_MEMORY_META", "SIGN_EXTEND"),
        {getOutputPin("RETIRED_MEM_SIGN_EXTEND")});
    builder.addNewWire(
        "RETIRED_MEM_FAULT",
        builder.getOutputPin<Rewire>(
            "RETIRED_MEMORY_META", "FAULT"),
        {getOutputPin("RETIRED_MEM_FAULT")});
    builder.addNewWire<32>(
        "RETIRED_MEMORY_META_WORD",
        builder.getOutputPin<Rewire, 32>(
            "RETIRED_MEMORY_META_PACK", "STATUS"),
        {builder.getInputPin<Mux2to1_32bit, 32>(
            "RETIRED_MEM_META_MUX", "B")});
    builder.addNewWire<32>(
        "RETIRED_MEM_ADDR_NEXT",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "RETIRED_MEM_ADDR_MUX", "OUT"),
        {retired_mem_addr->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "RETIRED_MEM_WRITE_DATA_NEXT",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "RETIRED_MEM_WRITE_DATA_MUX", "OUT"),
        {retired_mem_write_data->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "RETIRED_MEM_READ_DATA_NEXT",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "RETIRED_MEM_READ_DATA_MUX", "OUT"),
        {retired_mem_read_data->getInputPin<32>("D")});
    builder.addNewWire<32>(
        "RETIRED_MEM_META_NEXT",
        builder.getOutputPin<Mux2to1_32bit, 32>(
            "RETIRED_MEM_META_MUX", "OUT"),
        {retired_mem_meta->getInputPin<32>("D")});
}
