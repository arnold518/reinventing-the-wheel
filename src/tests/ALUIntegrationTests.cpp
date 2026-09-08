#include "tests/ALUIntegrationTests.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/ALU32.hpp"
#include <array>
#include <functional>
#include <map>
#include <stdexcept>

namespace {
std::vector<TestRow> control8Rows() {
    std::vector<TestRow> rows;
    for (unsigned op = 0; op < 32; ++op) {
        const bool compare = op >= 21 && op <= 27;
        rows.push_back({{{"OP", bits(op)}}, {
            {"LOCAL_OP", bits(op & 7)}, {"ARITH_OP", bits(compare ? 2 : op & 7)},
            {"COMPARE_OP", bits(compare ? op - 21 : 0)},
            {"ARITHMETIC_ENABLE", bit(op <= 6)}, {"LOGIC_ENABLE", bit(op >= 8 && op <= 14)},
            {"SHIFT_ENABLE", bit(op >= 16 && op <= 20)}, {"COMPARE_ENABLE", bit(compare)},
            {"PASS_A_ENABLE", bit(op == 28)}, {"PASS_B_ENABLE", bit(op == 29)},
            {"INVALID_OP", bit(op == 7 || op == 15 || op == 31)}}});
    }
    return rows;
}
constexpr std::array<const char*, 6> inputs8{
    "ARITHMETIC_RESULT", "LOGIC_RESULT", "SHIFT_RESULT", "COMPARE_RESULT", "A", "B"};
constexpr std::array<const char*, 6> enables8{
    "ARITHMETIC_ENABLE", "LOGIC_ENABLE", "SHIFT_ENABLE", "COMPARE_ENABLE", "PASS_A_ENABLE", "PASS_B_ENABLE"};
std::vector<TestRow> selector8Rows() {
    std::vector<TestRow> rows;
    constexpr std::array<unsigned, 6> words{0x81, 0x42, 0x24, 0x18, 0x55, 0xaa};
    for (unsigned mask = 0; mask < 64; ++mask) {
        TestRow row;
        unsigned expected = 0;
        for (unsigned i = 0; i < 6; ++i) {
            row.inputs[inputs8[i]] = bits(words[i]);
            row.inputs[enables8[i]] = bit(mask & (1u << i));
            if (mask & (1u << i)) expected |= words[i];
        }
        row.outputs["OUT"] = bits(expected);
        rows.push_back(row);
    }
    for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto row = rows.front();
        for (auto name : inputs8) row.inputs[name] = TestValue(std::vector<LogicValue>(8, unknown));
        rows.push_back(row); // Disabled inputs must not poison the output.
    }
    return rows;
}
std::vector<TestRow> control32Rows() {
    std::vector<TestRow> rows;
    for (unsigned op = 0; op < 32; ++op) {
        const bool arithmetic = op == 0 || op == 1 || op == 8 || op == 9;
        rows.push_back({{{"OP", bits(op)}}, {
            {"LOCAL_OP", bits(op & 7)}, {"HIGH_HALF", bit(op & 8)}, {"OUT_OF_RANGE", bit(op & 16)},
            {"ARITHMETIC_ENABLE", bit(arithmetic)}, {"SUB", bit(arithmetic && op != 0)},
            {"SHIFT_LEFT", bit(!(op & 2))}, {"SHIFT_ARITHMETIC", bit(op & 1)}}});
    }
    return rows;
}
std::vector<TestRow> compare32Rows() {
    std::vector<TestRow> rows;
    for (unsigned controls = 0; controls < 8; ++controls)
        for (unsigned diff : {0u, 1u, 0x80000000u, 0xffffffffu}) {
            const bool enable = controls & 1, carry = controls & 2, overflow = controls & 4;
            const bool lt = enable && (bool(diff & 0x80000000u) != overflow);
            const bool ltu = enable && !carry;
            rows.push_back({{{"DIFF", bits(diff)}, {"CARRY_IN", bit(carry)},
                {"OVERFLOW_IN", bit(overflow)}, {"ENABLE", bit(enable)}}, {
                {"EQ", bit(enable && diff == 0)}, {"LT_SIGNED", bit(lt)}, {"LT_UNSIGNED", bit(ltu)},
                {"SLT_RESULT", bits(lt)}, {"SLTU_RESULT", bits(ltu)}}});
        }
    for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto row = rows.front();
        row.inputs["DIFF"] = TestValue(std::vector<LogicValue>(32, unknown));
        row.inputs["CARRY_IN"] = unknown;
        row.inputs["OVERFLOW_IN"] = unknown;
        rows.push_back(row);
    }
    return rows;
}
std::vector<TestRow> selector32Rows() {
    constexpr std::array<const char*, 9> names{"ARITHMETIC_RESULT", "AND_RESULT", "OR_RESULT", "XOR_RESULT",
        "SHIFT_RESULT", "SLT_RESULT", "SLTU_RESULT", "A", "B"};
    constexpr std::array<int, 16> selected{0, 0, 1, 2, 3, 4, 4, 4, 5, 6, 7, 8, -1, -1, -1, -1};
    std::vector<TestRow> rows;
    for (unsigned pattern : {0x12345678u, 0xedcba987u}) for (unsigned op = 0; op < 32; ++op) {
        TestRow row;
        for (unsigned i = 0; i < names.size(); ++i) row.inputs[names[i]] = bits(pattern ^ (0x11111111u * i));
        row.inputs["LOCAL_OP"] = bits(op & 7);
        row.inputs["HIGH_HALF"] = bit(op & 8);
        row.inputs["OUT_OF_RANGE"] = bit(op & 16);
        row.outputs["OUT"] = op < 16 && selected[op] >= 0
            ? row.inputs.at(names[selected[op]]) : bits(0);
        rows.push_back(row);
    }
    for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto row = rows[16];
        for (auto name : names) row.inputs[name] = TestValue(std::vector<LogicValue>(32, unknown));
        rows.push_back(row);
    }
    return rows;
}
std::vector<TestRow> flags32Rows() {
    std::vector<TestRow> rows;
    for (unsigned controls = 0; controls < 8; ++controls)
        for (unsigned value : {0u, 1u, 0x80000000u, 0xffffffffu}) {
            const bool enable = controls & 1, carry = controls & 2, overflow = controls & 4;
            rows.push_back({{{"RESULT", bits(value)}, {"ARITHMETIC_ENABLE", bit(enable)},
                {"ARITH_CARRY", bit(carry)}, {"ARITH_OVERFLOW", bit(overflow)}}, {
                {"ZERO", bit(value == 0)}, {"NEGATIVE", bit(value & 0x80000000u)},
                {"CARRY_OUT", bit(enable && carry)}, {"OVERFLOW", bit(enable && overflow)}}});
        }
    for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto row = rows.front();
        row.inputs["ARITH_CARRY"] = unknown;
        row.inputs["ARITH_OVERFLOW"] = unknown;
        rows.push_back(row);
    }
    return rows;
}
void checkHierarchy(const std::shared_ptr<Component>& root,
                    const std::map<std::string, std::string>& expected, unsigned full_adders) {
    std::map<std::string, std::string> actual;
    for (const auto& child : root->getChildren()) actual[child->getName()] = child->getTypeName();
    if (actual != expected) throw std::runtime_error(root->getName() + ": ALU must expose seven named functional blocks");
    unsigned count = 0;
    std::function<void(const std::shared_ptr<Component>&)> visit = [&](const auto& component) {
        if (std::string(component->getTypeName()) == "FullAdder") ++count;
        for (const auto& child : component->getChildren()) visit(child);
    };
    visit(root);
    if (count != full_adders) throw std::runtime_error(root->getName() + ": arithmetic must use one shared carry chain");
}
} // namespace

ALU8ControlDecoderTest::ALU8ControlDecoderTest() : TruthTableComponentTest("ALU8ControlDecoderTest", "ALU8_CONTROL_ROOT", control8Rows()) {}
ALU8ResultSelectorTest::ALU8ResultSelectorTest() : TruthTableComponentTest("ALU8ResultSelectorTest", "ALU8_RESULT_SELECTOR_ROOT", selector8Rows()) {}
ALU32ControlDecoderTest::ALU32ControlDecoderTest() : TruthTableComponentTest("ALU32ControlDecoderTest", "ALU32_CONTROL_ROOT", control32Rows()) {}
ALU32CompareUnitTest::ALU32CompareUnitTest() : TruthTableComponentTest("ALU32CompareUnitTest", "ALU32_COMPARE_ROOT", compare32Rows()) {}
ALU32ResultSelectorTest::ALU32ResultSelectorTest() : TruthTableComponentTest("ALU32ResultSelectorTest", "ALU32_RESULT_SELECTOR_ROOT", selector32Rows()) {}
ALU32FlagUnitTest::ALU32FlagUnitTest() : TruthTableComponentTest("ALU32FlagUnitTest", "ALU32_FLAGS_ROOT", flags32Rows()) {}
void ALUHierarchyTest::verifyResults() {
    checkHierarchy(Component::create<ALU8>("ALU8"), {{"CONTROL", "ALU8ControlDecoder"}, {"ARITHMETIC", "ArithmeticUnit8"},
        {"LOGIC", "LogicUnit8"}, {"SHIFTER", "BarrelShifter8"}, {"COMPARE_SELECT", "CompareSelect8"},
        {"RESULT_SELECT", "ALU8ResultSelector"}, {"FLAGS", "ALU8FlagUnit"}}, 8);
    checkHierarchy(Component::create<ALU32>("ALU32"), {{"CONTROL", "ALU32ControlDecoder"}, {"ARITHMETIC", "AddSub32"},
        {"LOGIC", "Logic32"}, {"SHIFT", "Shifter32"}, {"COMPARE", "ALU32CompareUnit"},
        {"RESULT_SELECT", "ALU32ResultSelector"}, {"FLAGS", "ALU32FlagUnit"}}, 32);
}
