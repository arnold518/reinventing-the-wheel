#pragma once

#include "basic/LogicValue.hpp"
#include "rv32i/RV32IState.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace rv32i {

/**
 * Read-only architectural observation that preserves X/Z values.
 *
 * Tests may convert this to RV32IState only after every required bit has
 * become binary. Instruction ordinal is deliberately absent: it belongs to
 * the execution trace, not to circuit state.
 */
struct RV32IArchitecturalState {
    std::vector<LogicValue> pc =
        std::vector<LogicValue>(32, LogicValue::UNKNOWN);
    std::array<std::vector<LogicValue>, 32> x{};
    LogicValue halted = LogicValue::UNKNOWN;
    LogicValue trapped = LogicValue::UNKNOWN;
    std::vector<LogicValue> trap_cause =
        std::vector<LogicValue>(4, LogicValue::UNKNOWN);

    RV32IArchitecturalState() {
        for (auto& word : x) {
            word.assign(32, LogicValue::UNKNOWN);
        }
    }

    static RV32IArchitecturalState fromKnown(
        const RV32IState& state) {
        RV32IArchitecturalState result;
        result.pc = bits(32, state.pc);
        for (size_t index = 0; index < result.x.size(); ++index) {
            result.x[index] = bits(32, state.x[index]);
        }
        result.halted =
            state.halted ? LogicValue::HIGH : LogicValue::LOW;
        result.trapped =
            state.trapped ? LogicValue::HIGH : LogicValue::LOW;
        result.trap_cause = bits(
            4,
            static_cast<uint64_t>(state.trap_cause));
        return result;
    }

    RV32IState toKnownState() const {
        RV32IState result;
        result.pc = static_cast<uint32_t>(
            knownValue(pc, "PC"));
        for (size_t index = 0; index < result.x.size(); ++index) {
            result.x[index] = static_cast<uint32_t>(
                knownValue(
                    x[index],
                    "x" + std::to_string(index)));
        }
        result.halted = knownBit(halted, "HALTED");
        result.trapped = knownBit(trapped, "TRAPPED");
        result.trap_cause =
            static_cast<RV32IExecutionTrapCause>(
                knownValue(trap_cause, "TRAP_CAUSE"));
        result.forceX0();
        return result;
    }

private:
    static std::vector<LogicValue> bits(
        size_t width,
        uint64_t value) {
        std::vector<LogicValue> result(width, LogicValue::LOW);
        for (size_t bit = 0; bit < width; ++bit) {
            if (((value >> bit) & 1U) != 0) {
                result[bit] = LogicValue::HIGH;
            }
        }
        return result;
    }

    static uint64_t knownValue(
        const std::vector<LogicValue>& value,
        const std::string& name) {
        if (value.size() > 64) {
            throw std::logic_error(
                name + " exceeds the observation integer width");
        }
        uint64_t result = 0;
        for (size_t bit = 0; bit < value.size(); ++bit) {
            if (value[bit] != LogicValue::LOW
                && value[bit] != LogicValue::HIGH) {
                throw std::logic_error(
                    name + " contains an unknown or high-Z bit");
            }
            if (value[bit] == LogicValue::HIGH) {
                result |= uint64_t{1} << bit;
            }
        }
        return result;
    }

    static bool knownBit(
        LogicValue value,
        const std::string& name) {
        if (value == LogicValue::LOW) {
            return false;
        }
        if (value == LogicValue::HIGH) {
            return true;
        }
        throw std::logic_error(
            name + " contains an unknown or high-Z bit");
    }
};

} // namespace rv32i
