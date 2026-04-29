#include "tests/UtilityComponentTests.hpp"

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "tests/TestHelpers.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {
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
}

std::string RewireTest::getTestName() const {
    return "RewireTest";
}

void RewireTest::verifyResults() {
    {
        std::vector<Rewire::WireSpec> inputs{{"BUS", 8}};
        std::vector<Rewire::WireSpec> outputs{{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"B4"}, {"B5"}, {"B6"}, {"B7"}};
        auto mappings = unpack_mapping("BUS", 8, {"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"});
        expect(runRows<Rewire>({
            {{{"BUS", bits(0xA5)}},
             {{"B0", bit(true)}, {"B1", bit(false)}, {"B2", bit(true)}, {"B3", bit(false)},
              {"B4", bit(false)}, {"B5", bit(true)}, {"B6", bit(false)}, {"B7", bit(true)}}}
        }, inputs, outputs, mappings), "RewireTest unpack");
    }
    {
        std::vector<Rewire::WireSpec> inputs{{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"B4"}, {"B5"}, {"B6"}, {"B7"}};
        std::vector<Rewire::WireSpec> outputs{{"BUS", 8}};
        auto mappings = pack_mapping({"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"}, "BUS");
        expect(runRows<Rewire>({
            {{{"B0", bit(true)}, {"B1", bit(false)}, {"B2", bit(true)}, {"B3", bit(false)},
              {"B4", bit(false)}, {"B5", bit(true)}, {"B6", bit(false)}, {"B7", bit(true)}},
             {{"BUS", bits(0xA5)}}}
        }, inputs, outputs, mappings), "RewireTest pack");
    }
    {
        std::vector<Rewire::WireSpec> inputs{{"DATA", 16}};
        std::vector<Rewire::WireSpec> outputs{{"BYTE", 8}};
        auto mappings = slice_mapping("DATA", 4, 8, "BYTE");
        expect(runRows<Rewire>({
            {{{"DATA", bits(0xABCD)}}, {{"BYTE", bits(0xBC)}}}
        }, inputs, outputs, mappings), "RewireTest slice");
    }
    {
        std::vector<Rewire::WireSpec> inputs{{"BYTE", 8}};
        std::vector<Rewire::WireSpec> outputs{{"WORD", 32}};
        auto mappings = identity_mapping("BYTE", 0, 8, "WORD", 0);
        expect(runRows<Rewire>({
            {{{"BYTE", bits(0xF2)}}, {{"WORD", bits(0x000000F2)}}}
        }, inputs, outputs, mappings, Rewire::UnmappedBitValue::LOW), "RewireTest zero extend");
    }
    {
        std::vector<Rewire::WireSpec> inputs{{"BYTE", 8}};
        std::vector<Rewire::WireSpec> outputs{{"WORD", 16}};
        auto mappings = sign_extend_mapping("BYTE", 8, "WORD", 16);
        expect(runRows<Rewire>({
            {{{"BYTE", bits(0x80)}}, {{"WORD", bits(0xFF80)}}},
            {{{"BYTE", bits(0x7F)}}, {{"WORD", bits(0x007F)}}}
        }, inputs, outputs, mappings), "RewireTest sign extend");
    }
    {
        std::vector<Rewire::WireSpec> inputs{{"NIBBLE", 4}};
        std::vector<Rewire::WireSpec> outputs{{"BYTE", 8}};
        auto mappings = identity_mapping("NIBBLE", 0, 4, "BYTE", 0);
        expect(runRows<Rewire>({
            {{{"NIBBLE", bits(0x0A)}}, {{"BYTE", bits(0xFA)}}}
        }, inputs, outputs, mappings, Rewire::UnmappedBitValue::HIGH), "RewireTest unmapped high");
    }
    {
        Simulator local_sim;
        auto local_root = Component::create<Rewire>(
            "ROOT",
            std::vector<Rewire::WireSpec>{{"NIBBLE", 4}},
            std::vector<Rewire::WireSpec>{{"BYTE", 8}},
            identity_mapping("NIBBLE", 0, 4, "BYTE", 0));
        auto io = std::dynamic_pointer_cast<IOComponent>(local_root);
        ComponentBuilder local_builder(local_root);

        auto input_pin = io->getInputPinDynamic("NIBBLE");
        auto input_wire = local_builder.addNewWireDynamic("IN_NIBBLE", input_pin->getWidth(), nullptr, {input_pin});
        local_sim.scheduleEvent(makeTestWireUpdate(0, input_wire, bits(0x0A)));
        local_sim.runAndRecord(20);

        auto output_pin = io->getOutputPinDynamic("BYTE");
        expect(output_pin->getValueAsUInt64() == 0x0A
                   && output_pin->getBit(4) == LogicValue::UNKNOWN
                   && output_pin->getBit(5) == LogicValue::UNKNOWN
                   && output_pin->getBit(6) == LogicValue::UNKNOWN
                   && output_pin->getBit(7) == LogicValue::UNKNOWN,
               "RewireTest unmapped unknown");
    }
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 4, "B", 0}});
            (void)invalid;
        },
        "RewireTest invalid source bit");
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 4}});
            (void)invalid;
        },
        "RewireTest invalid destination bit");
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 5}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 0}});
            (void)invalid;
        },
        "RewireTest invalid width");
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"MISSING", 0, "B", 0}});
            (void)invalid;
        },
        "RewireTest invalid source name");
    expectInvalidArgument(
        [] {
            (void)unpack_mapping("BUS", 4, {"B0", "B1"});
        },
        "RewireTest invalid unpack helper");
    expectInvalidArgument(
        [] {
            (void)sign_extend_mapping("BYTE", 8, "NIBBLE", 4);
        },
        "RewireTest invalid sign extend helper");
}

std::string BitAdapterTest::getTestName() const {
    return "BitAdapterTest";
}

void BitAdapterTest::verifyResults() {
    expect(runRows<BitSplitter<8>>({
        {{{"IN", bits(0xA5)}},
         {{"OUT_0", bit(true)}, {"OUT_1", bit(false)}, {"OUT_2", bit(true)}, {"OUT_3", bit(false)},
          {"OUT_4", bit(false)}, {"OUT_5", bit(true)}, {"OUT_6", bit(false)}, {"OUT_7", bit(true)}}}
    }), "BitAdapterTest splitter8");

    expect(runRows<BitJoiner<8>>({
        {{{"IN_0", bit(true)}, {"IN_1", bit(false)}, {"IN_2", bit(true)}, {"IN_3", bit(false)},
          {"IN_4", bit(false)}, {"IN_5", bit(true)}, {"IN_6", bit(false)}, {"IN_7", bit(true)}},
         {{"OUT", bits(0xA5)}}}
    }), "BitAdapterTest joiner8");

    expect(runRows<BitSplitter<16>>({
        {{{"IN", bits(0xA55A)}},
         {{"OUT_0", bit(false)}, {"OUT_1", bit(true)}, {"OUT_2", bit(false)}, {"OUT_3", bit(true)},
          {"OUT_8", bit(true)}, {"OUT_14", bit(false)}, {"OUT_15", bit(true)}}}
    }), "BitAdapterTest splitter16 selected bits");

    expect(runRows<BitJoiner<32>>({
        {{{"IN_0", bit(true)}, {"IN_8", bit(true)}, {"IN_16", bit(true)}, {"IN_31", bit(true)}},
         {{"OUT", bits(0x80010101)}}}
    }), "BitAdapterTest joiner32 selected bits");

    Simulator local_sim;
    auto local_root = Component::create<BitJoiner<4>>("ROOT");
    auto io = std::dynamic_pointer_cast<IOComponent>(local_root);
    ComponentBuilder local_builder(local_root);

    auto input_pin = io->getInputPinDynamic("IN_0");
    auto input_wire = local_builder.addNewWireDynamic("IN_0_WIRE", input_pin->getWidth(), nullptr, {input_pin});
    local_sim.scheduleEvent(makeTestWireUpdate(0, input_wire, bit(true)));
    local_sim.runAndRecord(20);

    auto output_pin = io->getOutputPinDynamic("OUT");
    expect(output_pin->getBit(0) == LogicValue::HIGH
               && output_pin->getBit(1) == LogicValue::UNKNOWN
               && output_pin->getBit(2) == LogicValue::UNKNOWN
               && output_pin->getBit(3) == LogicValue::UNKNOWN,
           "BitAdapterTest preserves unknown");
}

std::string ConstantValueTest::getTestName() const {
    return "ConstantValueTest";
}

void ConstantValueTest::verifyResults() {
    expect(runRows<ConstantValue<1, 1>>({
        {{{"TRIGGER", bit(false)}}, {{"OUT", bit(true)}}},
        {{{"TRIGGER", bit(true)}}, {{"OUT", bit(true)}}},
    }, uint64_t{1}), "ConstantValueTest ConstantValue<1,1>");

    expect(runRows<ConstantValue<1, 8>>({
        {{{"TRIGGER", bits(0x00)}}, {{"OUT", bit(false)}}},
        {{{"TRIGGER", bits(0xFF)}}, {{"OUT", bit(false)}}},
    }, uint64_t{0}), "ConstantValueTest ConstantValue<1,8>");

    expect(runRows<ConstantValue<8, 8>>({
        {{{"TRIGGER", bits(0x00)}}, {{"OUT", bits(0xA5)}}},
        {{{"TRIGGER", bits(0xFF)}}, {{"OUT", bits(0xA5)}}},
    }, uint64_t{0xA5}), "ConstantValueTest ConstantValue<8,8>");
}
