#include "modules/rv32i/RV32IControlFlowDirect.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"

#include "simulator/Simulator.hpp"
#include <cstdint>
#include <utility>

namespace {
bool isKnown(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH;
}

bool allKnown(const std::vector<LogicValue>& values) {
    for (const auto value : values) {
        if (!isKnown(value)) {
            return false;
        }
    }
    return true;
}

uint32_t toUInt32(const std::vector<LogicValue>& values) {
    uint32_t result = 0;
    for (size_t bit = 0; bit < values.size() && bit < 32; ++bit) {
        if (values[bit] == LogicValue::HIGH) {
            result |= uint32_t{1} << bit;
        }
    }
    return result;
}

std::vector<LogicValue> bits32(uint32_t value) {
    std::vector<LogicValue> result(32, LogicValue::LOW);
    for (size_t bit = 0; bit < 32; ++bit) {
        result[bit] = ((value >> bit) & 1U) ? LogicValue::HIGH : LogicValue::LOW;
    }
    return result;
}

LogicValue bit(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}
}

RV32IControlFlowDirect::RV32IControlFlowDirect(std::string name)
    : BasicComponent(std::move(name), 1,
                     circuit::families::RV32IControlFlow.pinInitializer()),
      pc_(32, LogicValue::UNKNOWN) {}

void RV32IControlFlowDirect::evaluate(size_t current_time, Simulator& simulator) {
    const auto clk = getInputValue("CLK");
    const auto rst = getInputValue("RST");
    const auto pc_write = getInputValue("PC_WRITE");

    auto compute = [&]() {
        struct Result {
            std::vector<LogicValue> plus4{32, LogicValue::UNKNOWN};
            std::vector<LogicValue> next{32, LogicValue::UNKNOWN};
            LogicValue branch = LogicValue::UNKNOWN;
            LogicValue pc_misaligned = LogicValue::UNKNOWN;
            LogicValue target_misaligned = LogicValue::UNKNOWN;
        } result;

        const auto rs1 = getInputPin<32>("RS1_VALUE")->getValueAsVector();
        const auto imm = getInputPin<32>("IMM")->getValueAsVector();
        const auto branch_type = getInputPin<3>("BRANCH_TYPE")->getValueAsVector();
        const auto jump_type = getInputPin<2>("JUMP_TYPE")->getValueAsVector();
        const auto eq = getInputValue("EQ");
        const auto lt_signed = getInputValue("LT_SIGNED");
        const auto lt_unsigned = getInputValue("LT_UNSIGNED");

        if (!allKnown(pc_)) {
            return result;
        }

        const uint32_t pc = toUInt32(pc_);
        const uint32_t plus4 = pc + 4U;
        result.plus4 = bits32(plus4);
        result.pc_misaligned = bit((pc & 0x3U) != 0);

        if (!allKnown(rs1) || !allKnown(imm) || !allKnown(branch_type) || !allKnown(jump_type)
            || !isKnown(eq) || !isKnown(lt_signed) || !isKnown(lt_unsigned)) {
            return result;
        }

        const auto branch = static_cast<uint8_t>(toUInt32(branch_type));
        bool branch_taken = false;
        switch (branch) {
            case 0: branch_taken = false; break;
            case 1: branch_taken = eq == LogicValue::HIGH; break;
            case 2: branch_taken = eq == LogicValue::LOW; break;
            case 3: branch_taken = lt_signed == LogicValue::HIGH; break;
            case 4: branch_taken = lt_signed == LogicValue::LOW; break;
            case 5: branch_taken = lt_unsigned == LogicValue::HIGH; break;
            case 6: branch_taken = lt_unsigned == LogicValue::LOW; break;
            default: branch_taken = false; break;
        }

        const uint32_t immediate = toUInt32(imm);
        const uint32_t pc_target = pc + immediate;
        const uint32_t jalr_target = (toUInt32(rs1) + immediate) & ~uint32_t{1};
        const auto jump = static_cast<uint8_t>(toUInt32(jump_type));
        uint32_t next = branch_taken ? pc_target : plus4;
        if (jump == 1) {
            next = pc_target;
        } else if (jump == 2) {
            next = jalr_target;
        } else if (jump == 3) {
            next = 0;
        }

        const bool active_transfer = branch_taken || jump != 0;
        result.next = bits32(next);
        result.branch = bit(branch_taken);
        result.target_misaligned = bit(active_transfer && (next & 0x3U) != 0);
        return result;
    };

    if (rst == LogicValue::HIGH) {
        pc_ = bits32(0);
    } else if (rst != LogicValue::LOW) {
        pc_.assign(32, LogicValue::UNKNOWN);
    } else if (previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH) {
        if (pc_write == LogicValue::HIGH) {
            pc_ = compute().next;
        } else if (pc_write != LogicValue::LOW) {
            pc_.assign(32, LogicValue::UNKNOWN);
        }
    }

    previous_clk_ = clk;
    const auto result = compute();
    _updateOutputWire<32>(simulator, "PC", pc_, current_time);
    _updateOutputWire<32>(simulator, "PC_PLUS_4", result.plus4, current_time);
    _updateOutputWire<32>(simulator, "NEXT_PC_CANDIDATE", result.next, current_time);
    _updateOutputWire(simulator, "BRANCH_TAKEN", result.branch, current_time);
    _updateOutputWire(simulator, "PC_MISALIGNED", result.pc_misaligned, current_time);
    _updateOutputWire(simulator, "TARGET_MISALIGNED", result.target_misaligned, current_time);
}
