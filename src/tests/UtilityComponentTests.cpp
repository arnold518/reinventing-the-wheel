#include "tests/UtilityComponentTests.hpp"

#include "components/Component.hpp"
#include "modules/utility/Rewire.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using circuit::ParameterMap;
using circuit::test::ActionScenario;
using circuit::test::CheckpointKind;
using circuit::test::ComponentTestSpec;
using circuit::test::LogicVector;
using circuit::test::ScenarioAction;
using circuit::test::actionScenarioFromRows;
using circuit::test::logicBit;
using circuit::test::logicBits;

void expect(bool condition, const char* test_name) {
    if (!condition) {
        std::cerr << test_name << " failed" << std::endl;
        assert(false && "Utility component test failed");
    }
}

template<typename Fn>
void expectInvalidArgument(Fn&& fn, const char* test_name) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, test_name);
}

uint64_t widthMask(size_t width) {
    return width == 32
        ? 0xffffffffULL
        : ((uint64_t{1} << width) - 1);
}

std::map<std::string, TestValue> indexedBits(
    const std::string& prefix,
    size_t width,
    uint64_t value) {
    std::map<std::string, TestValue> result;
    for (size_t index = 0; index < width; ++index) {
        result.emplace(
            prefix + std::to_string(index),
            bit(((value >> index) & 1U) != 0));
    }
    return result;
}

LogicVector fourStatePattern(size_t width, size_t offset) {
    constexpr std::array<LogicValue, 4> Values{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    LogicVector result;
    result.reserve(width);
    for (size_t index = 0; index < width; ++index) {
        result.push_back(Values[(index + offset) % Values.size()]);
    }
    return result;
}

ParameterMap rewireParameters(
    const std::vector<Rewire::WireSpec>& inputs,
    const std::vector<Rewire::WireSpec>& outputs,
    const std::vector<Rewire::BitMap>& mappings,
    Rewire::UnmappedBitValue unmapped =
        Rewire::UnmappedBitValue::UNKNOWN) {
    const auto wireSpecs = [](const auto& specs) {
        std::string result;
        for (const auto& spec : specs) {
            if (!result.empty()) {
                result += ',';
            }
            result += spec.name + ':' + std::to_string(spec.width);
        }
        return result;
    };
    std::string mapping_text;
    for (const auto& mapping : mappings) {
        if (!mapping_text.empty()) {
            mapping_text += ',';
        }
        mapping_text += mapping.src_wire + ':'
            + std::to_string(mapping.src_bit) + '>'
            + mapping.dst_wire + ':'
            + std::to_string(mapping.dst_bit);
    }
    const char* unmapped_text = "unknown";
    if (unmapped == Rewire::UnmappedBitValue::LOW) {
        unmapped_text = "low";
    } else if (unmapped == Rewire::UnmappedBitValue::HIGH) {
        unmapped_text = "high";
    }
    return {
        {"inputs", wireSpecs(inputs)},
        {"outputs", wireSpecs(outputs)},
        {"mappings", std::move(mapping_text)},
        {"unmapped", unmapped_text},
    };
}

ActionScenario rowScenario(
    std::string id,
    ParameterMap parameters,
    std::vector<TestRow> rows) {
    return actionScenarioFromRows(
        std::move(id),
        "utility.rewire",
        rows,
        {100'000, 1'000'000},
        parameters);
}

ComponentTestSpec rewireSpec() {
    const std::vector<std::string> byte_bits{
        "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"};
    std::vector<ActionScenario> scenarios;
    scenarios.push_back(rowScenario(
        "unpack",
        rewireParameters(
            {{"BUS", 8}},
            {{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"B4"}, {"B5"}, {"B6"}, {"B7"}},
            unpack_mapping("BUS", 8, byte_bits)),
        {
            {{{"BUS", bits(0xa5)}}, indexedBits("B", 8, 0xa5)},
            {{{"BUS", bits(0x00)}}, indexedBits("B", 8, 0x00)},
            {{{"BUS", bits(0xff)}}, indexedBits("B", 8, 0xff)},
        }));
    scenarios.push_back(rowScenario(
        "pack",
        rewireParameters(
            {{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"B4"}, {"B5"}, {"B6"}, {"B7"}},
            {{"BUS", 8}},
            pack_mapping(byte_bits, "BUS")),
        {
            {indexedBits("B", 8, 0xa5), {{"BUS", bits(0xa5)}}},
            {indexedBits("B", 8, 0x00), {{"BUS", bits(0x00)}}},
            {indexedBits("B", 8, 0xff), {{"BUS", bits(0xff)}}},
        }));
    scenarios.push_back(rowScenario(
        "slice",
        rewireParameters(
            {{"DATA", 16}},
            {{"BYTE", 8}},
            slice_mapping("DATA", 4, 8, "BYTE")),
        {
            {{{"DATA", bits(0xabcd)}}, {{"BYTE", bits(0xbc)}}},
            {{{"DATA", bits(0x1234)}}, {{"BYTE", bits(0x23)}}},
        }));
    scenarios.push_back(rowScenario(
        "zero-extend",
        rewireParameters(
            {{"BYTE", 8}},
            {{"WORD", 32}},
            identity_mapping("BYTE", 0, 8, "WORD", 0),
            Rewire::UnmappedBitValue::LOW),
        {
            {{{"BYTE", bits(0xf2)}}, {{"WORD", bits(0x000000f2)}}},
            {{{"BYTE", bits(0xff)}}, {{"WORD", bits(0x000000ff)}}},
        }));
    scenarios.push_back(rowScenario(
        "sign-extend",
        rewireParameters(
            {{"BYTE", 8}},
            {{"WORD", 16}},
            sign_extend_mapping("BYTE", 8, "WORD", 16)),
        {
            {{{"BYTE", bits(0x80)}}, {{"WORD", bits(0xff80)}}},
            {{{"BYTE", bits(0x7f)}}, {{"WORD", bits(0x007f)}}},
        }));
    scenarios.push_back(rowScenario(
        "unmapped-high",
        rewireParameters(
            {{"NIBBLE", 4}},
            {{"BYTE", 8}},
            identity_mapping("NIBBLE", 0, 4, "BYTE", 0),
            Rewire::UnmappedBitValue::HIGH),
        {
            {{{"NIBBLE", bits(0x0a)}}, {{"BYTE", bits(0xfa)}}},
            {{{"NIBBLE", bits(0x00)}}, {{"BYTE", bits(0xf0)}}},
        }));
    scenarios.push_back(rowScenario(
        "width-5",
        rewireParameters(
            {{"OP", 5}},
            {{"COPY", 5}},
            identity_mapping("OP", 0, 5, "COPY", 0)),
        {
            {{{"OP", bits(0x15)}}, {{"COPY", bits(0x15)}}},
            {{{"OP", bits(0x1f)}}, {{"COPY", bits(0x1f)}}},
        }));
    scenarios.push_back(rowScenario(
        "four-state-copy",
        rewireParameters(
            {{"SOURCE", 4}},
            {{"COPY", 4}},
            identity_mapping("SOURCE", 0, 4, "COPY", 0)),
        {
            {
                {{"SOURCE", TestValue(fourStatePattern(4, 0))}},
                {{"COPY", TestValue(fourStatePattern(4, 0))}},
            },
            {
                {{"SOURCE", TestValue(fourStatePattern(4, 2))}},
                {{"COPY", TestValue(fourStatePattern(4, 2))}},
            },
        }));

    auto unknown_parameters = rewireParameters(
        {{"NIBBLE", 4}},
        {{"BYTE", 8}},
        identity_mapping("NIBBLE", 0, 4, "BYTE", 0));
    LogicVector unknown_byte(8, LogicValue::UNKNOWN);
    const auto low_nibble = logicBits(4, 0x0a);
    std::copy(
        low_nibble.begin(),
        low_nibble.end(),
        unknown_byte.begin());
    scenarios.push_back({
        "unmapped-unknown",
        std::move(unknown_parameters),
        {
            {
                "unknown-fill",
                CheckpointKind::Settled,
                {{"NIBBLE", low_nibble}},
                {{"BYTE", unknown_byte}},
                true,
                "Unmapped output bits retain the four-state unknown value.",
            },
        },
        {100'000, 1'000'000},
    });

    return {
        "RewireTest",
        "utility.rewire",
        "REWIRE_ROOT",
        {},
        std::move(scenarios),
    };
}

ComponentTestSpec splitterSpec(
    size_t width,
    std::string test_id,
    std::string contract_id) {
    const auto mask = widthMask(width);
    const auto pattern = 0xa5a55a5aULL & mask;
    ActionScenario scenario;
    scenario.id = "rows";
    scenario.limits = {100'000, 1'000'000};
    for (const auto value :
         std::array<uint64_t, 3>{uint64_t{0}, pattern, mask}) {
        circuit::test::NamedValues outputs;
        for (size_t bit_index = 0; bit_index < width; ++bit_index) {
            outputs.emplace(
                "OUT_" + std::to_string(bit_index),
                logicBit(((value >> bit_index) & 1U) != 0));
        }
        scenario.actions.push_back({
            "value-" + std::to_string(value),
            CheckpointKind::Settled,
            {{"IN", logicBits(width, value)}},
            std::move(outputs),
            true,
            "Split every input bit onto its indexed output.",
        });
    }
    for (const auto offset : {size_t{2}, size_t{3}}) {
        const auto input = fourStatePattern(width, offset);
        circuit::test::NamedValues outputs;
        for (size_t bit_index = 0; bit_index < width; ++bit_index) {
            outputs.emplace(
                "OUT_" + std::to_string(bit_index),
                logicBit(input[bit_index]));
        }
        scenario.actions.push_back({
            offset == 2 ? "four-state-unknown" : "four-state-high-z",
            CheckpointKind::Settled,
            {{"IN", input}},
            std::move(outputs),
            true,
            "Splitting preserves LOW, HIGH, UNKNOWN, and HIGH_Z exactly.",
        });
    }
    return {
        std::move(test_id),
        std::move(contract_id),
        "BIT_SPLITTER_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec joinerSpec(
    size_t width,
    std::string test_id,
    std::string contract_id) {
    const auto mask = widthMask(width);
    const auto pattern = 0xa5a55a5aULL & mask;
    ActionScenario scenario;
    scenario.id = "rows";
    scenario.limits = {100'000, 1'000'000};
    if (width == 8) {
        LogicVector expected(8, LogicValue::UNKNOWN);
        expected[0] = LogicValue::HIGH;
        scenario.actions.push_back({
            "partial-unknown",
            CheckpointKind::Settled,
            {{"IN_0", logicBit(true)}},
            {{"OUT", std::move(expected)}},
            true,
            "Undriven input bits remain unknown in the joined bus.",
        });
    }
    for (const auto value :
         std::array<uint64_t, 3>{uint64_t{0}, pattern, mask}) {
        circuit::test::NamedValues inputs;
        for (size_t bit_index = 0; bit_index < width; ++bit_index) {
            inputs.emplace(
                "IN_" + std::to_string(bit_index),
                logicBit(((value >> bit_index) & 1U) != 0));
        }
        scenario.actions.push_back({
            "value-" + std::to_string(value),
            CheckpointKind::Settled,
            std::move(inputs),
            {{"OUT", logicBits(width, value)}},
            true,
            "Join every indexed input into the output bus.",
        });
    }
    for (const auto offset : {size_t{2}, size_t{3}}) {
        const auto expected = fourStatePattern(width, offset);
        circuit::test::NamedValues inputs;
        for (size_t bit_index = 0; bit_index < width; ++bit_index) {
            inputs.emplace(
                "IN_" + std::to_string(bit_index),
                logicBit(expected[bit_index]));
        }
        scenario.actions.push_back({
            offset == 2 ? "four-state-unknown" : "four-state-high-z",
            CheckpointKind::Settled,
            std::move(inputs),
            {{"OUT", expected}},
            true,
            "Joining preserves LOW, HIGH, UNKNOWN, and HIGH_Z exactly.",
        });
    }
    return {
        std::move(test_id),
        std::move(contract_id),
        "BIT_JOINER_ROOT",
        {},
        {std::move(scenario)},
    };
}

ComponentTestSpec constantSpec(
    size_t width,
    std::string test_id,
    std::string contract_id) {
    const auto mask = widthMask(width);
    const auto pattern = 0xdeadbeefULL & mask;
    std::vector<ActionScenario> scenarios;
    for (const auto [id, value] :
         std::vector<std::pair<std::string, uint64_t>>{
             {"representative", pattern},
             {"zero", 0},
             {"maximum", mask},
         }) {
        scenarios.push_back({
            id,
            {{"value", std::to_string(value)}},
            {
                {
                    "output",
                    CheckpointKind::Settled,
                    {},
                    {{"OUT", logicBits(width, value)}},
                    true,
                    "The source drives its configured value at startup.",
                },
            },
            {100'000, 1'000'000},
        });
    }
    return {
        std::move(test_id),
        std::move(contract_id),
        "CONSTANT_VALUE_ROOT",
        {},
        std::move(scenarios),
    };
}
} // namespace

RewireTest::RewireTest()
    : ComponentScenarioTest(rewireSpec()) {}

std::string RewireValidationTest::getTestName() const {
    return "RewireValidationTest";
}

void RewireValidationTest::verifyResults() {
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 4, "B", 0}});
        },
        "RewireValidationTest invalid source bit");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 4}});
        },
        "RewireValidationTest invalid destination bit");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 7}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 0}});
        },
        "RewireValidationTest invalid width");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"MISSING", 0, "B", 0}});
        },
        "RewireValidationTest invalid source name");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "MISSING", 0}});
        },
        "RewireValidationTest invalid destination name");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 0}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{});
        },
        "RewireValidationTest zero input width");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 0}},
                std::vector<Rewire::BitMap>{});
        },
        "RewireValidationTest zero output width");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 64}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 0}});
        },
        "RewireValidationTest unsupported output width");
    expectInvalidArgument(
        [] {
            (void)Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{
                    {"A", 0, "B", 0},
                    {"A", 1, "B", 0}});
        },
        "RewireValidationTest duplicate destination bit");
    expectInvalidArgument(
        [] {
            (void)unpack_mapping("BUS", 4, {"B0", "B1"});
        },
        "RewireValidationTest invalid unpack helper");
    expectInvalidArgument(
        [] {
            (void)sign_extend_mapping("BYTE", 8, "NIBBLE", 4);
        },
        "RewireValidationTest invalid sign extend helper");
}

#define DEFINE_SPLITTER_TEST(WIDTH) \
    BitSplitter##WIDTH##Test::BitSplitter##WIDTH##Test() \
        : ComponentScenarioTest(splitterSpec( \
              WIDTH, \
              "BitSplitter" #WIDTH "Test", \
              "utility.bit-splitter.width" #WIDTH)) {}

DEFINE_SPLITTER_TEST(1)
DEFINE_SPLITTER_TEST(2)
DEFINE_SPLITTER_TEST(3)
DEFINE_SPLITTER_TEST(4)
DEFINE_SPLITTER_TEST(5)
DEFINE_SPLITTER_TEST(8)
DEFINE_SPLITTER_TEST(16)
DEFINE_SPLITTER_TEST(32)
#undef DEFINE_SPLITTER_TEST

#define DEFINE_JOINER_TEST(WIDTH) \
    BitJoiner##WIDTH##Test::BitJoiner##WIDTH##Test() \
        : ComponentScenarioTest(joinerSpec( \
              WIDTH, \
              "BitJoiner" #WIDTH "Test", \
              "utility.bit-joiner.width" #WIDTH)) {}

DEFINE_JOINER_TEST(1)
DEFINE_JOINER_TEST(2)
DEFINE_JOINER_TEST(3)
DEFINE_JOINER_TEST(4)
DEFINE_JOINER_TEST(5)
DEFINE_JOINER_TEST(8)
DEFINE_JOINER_TEST(16)
DEFINE_JOINER_TEST(32)
#undef DEFINE_JOINER_TEST

#define DEFINE_CONSTANT_TEST(WIDTH) \
    ConstantValue##WIDTH##Test::ConstantValue##WIDTH##Test() \
        : ComponentScenarioTest(constantSpec( \
              WIDTH, \
              "ConstantValue" #WIDTH "Test", \
              "utility.constant.width" #WIDTH)) {}

DEFINE_CONSTANT_TEST(1)
DEFINE_CONSTANT_TEST(2)
DEFINE_CONSTANT_TEST(4)
DEFINE_CONSTANT_TEST(8)
DEFINE_CONSTANT_TEST(32)
#undef DEFINE_CONSTANT_TEST
