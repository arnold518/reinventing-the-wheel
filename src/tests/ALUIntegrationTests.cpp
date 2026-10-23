#include "tests/ALUIntegrationTests.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/ALU.hpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/utility/Rewire.hpp"
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
void checkHierarchy(const std::shared_ptr<Component>& root,
                    const std::map<std::string, std::string>& expected,
                    const std::string& prefix_type, const std::string& add_sub_type, unsigned width) {
    std::map<std::string, std::string> actual;
    for (const auto& child : root->getChildren()) actual[child->getName()] = child->getTypeName();
    if (actual != expected) throw std::runtime_error(root->getName() + ": ALU must expose its named functional blocks");
    unsigned count = 0, prefixes = 0, half_adders = 0, add_sub_units = 0;
    std::function<void(const std::shared_ptr<Component>&)> visit = [&](const auto& component) {
        if (std::string(component->getTypeName()) == "FullAdder") ++count;
        if (std::string(component->getTypeName()) == "HalfAdder") ++half_adders;
        if (std::string(component->getTypeName()) == prefix_type) ++prefixes;
        if (std::string(component->getTypeName()) == add_sub_type) ++add_sub_units;
        for (const auto& child : component->getChildren()) visit(child);
    };
    visit(root);
    if (count != 0 || prefixes != 1 || half_adders != width || add_sub_units != 1)
        throw std::runtime_error(root->getName() + ": arithmetic must share one prefix add/subtract unit");
}
} // namespace

ALUControlDecoderTest::ALUControlDecoderTest() : TruthTableComponentTest("ALUControlDecoderTest", "ALU_CONTROL_DECODER_ROOT", control8Rows()) {}
ALUResultSelectorWidth8Test::ALUResultSelectorWidth8Test() : TruthTableComponentTest("ALUResultSelectorWidth8Test", "ALU_RESULT_SELECTOR_W8_ROOT", selector8Rows()) {}
void ALUHierarchyTest::verifyResults() {
    checkHierarchy(Component::create<ALU<8>>("ALU"), {{"CONTROL", "ALUControlDecoder"}, {"ARITHMETIC", "ArithmeticUnit<8>"},
        {"LOGIC", "LogicUnit<8>"}, {"SHIFTER", "BarrelShifter<8>"}, {"COMPARE_SELECT", "CompareSelect<8>"},
        {"RESULT_SELECT", "ALUResultSelector<8>"}, {"FLAGS", "ALUFlagUnit<8>"}}, "ParallelPrefixAdder<8>", "AddSub<8>", 8);
    checkHierarchy(Component::create<RV32IALU>("ALU"), {{"CONTROL", "RV32IALUControl"}, {"ARITHMETIC", "RV32IALUAddSub"},
        {"LOGIC", "RV32IALULogic"}, {"SHIFT", "RV32IALUShifter"}, {"COMPARE", "RV32IALUCompare"},
        {"RESULT_SELECT", "RV32IALUResultSelect"}}, "ParallelPrefixAdder<32>", "RV32IALUAddSub", 32);
}
