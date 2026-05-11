#include "tests/UtilityComponentTests.hpp"

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "tests/TestHelpers.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

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

RewireUnpackTest::RewireUnpackTest()
    : ComponentRowsTest<Rewire,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::BitMap>>(
          "RewireUnpackTest",
          "REWIRE_UNPACK_ROOT",
          {
              {{{"BUS", bits(0xA5)}},
               {{"B0", bit(true)}, {"B1", bit(false)}, {"B2", bit(true)}, {"B3", bit(false)},
                {"B4", bit(false)}, {"B5", bit(true)}, {"B6", bit(false)}, {"B7", bit(true)}}},
          },
          std::vector<Rewire::WireSpec>{{"BUS", 8}},
          std::vector<Rewire::WireSpec>{{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"B4"}, {"B5"}, {"B6"}, {"B7"}},
          unpack_mapping("BUS", 8, {"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"})) {}

RewirePackTest::RewirePackTest()
    : ComponentRowsTest<Rewire,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::BitMap>>(
          "RewirePackTest",
          "REWIRE_PACK_ROOT",
          {
              {{{"B0", bit(true)}, {"B1", bit(false)}, {"B2", bit(true)}, {"B3", bit(false)},
                {"B4", bit(false)}, {"B5", bit(true)}, {"B6", bit(false)}, {"B7", bit(true)}},
               {{"BUS", bits(0xA5)}}},
          },
          std::vector<Rewire::WireSpec>{{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"B4"}, {"B5"}, {"B6"}, {"B7"}},
          std::vector<Rewire::WireSpec>{{"BUS", 8}},
          pack_mapping({"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"}, "BUS")) {}

RewireSliceTest::RewireSliceTest()
    : ComponentRowsTest<Rewire,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::BitMap>>(
          "RewireSliceTest",
          "REWIRE_SLICE_ROOT",
          {
              {{{"DATA", bits(0xABCD)}}, {{"BYTE", bits(0xBC)}}},
          },
          std::vector<Rewire::WireSpec>{{"DATA", 16}},
          std::vector<Rewire::WireSpec>{{"BYTE", 8}},
          slice_mapping("DATA", 4, 8, "BYTE")) {}

RewireZeroExtendTest::RewireZeroExtendTest()
    : ComponentRowsTest<Rewire,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::BitMap>,
                        Rewire::UnmappedBitValue>(
          "RewireZeroExtendTest",
          "REWIRE_ZERO_EXTEND_ROOT",
          {
              {{{"BYTE", bits(0xF2)}}, {{"WORD", bits(0x000000F2)}}},
          },
          std::vector<Rewire::WireSpec>{{"BYTE", 8}},
          std::vector<Rewire::WireSpec>{{"WORD", 32}},
          identity_mapping("BYTE", 0, 8, "WORD", 0),
          Rewire::UnmappedBitValue::LOW) {}

RewireSignExtendTest::RewireSignExtendTest()
    : ComponentRowsTest<Rewire,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::BitMap>>(
          "RewireSignExtendTest",
          "REWIRE_SIGN_EXTEND_ROOT",
          {
              {{{"BYTE", bits(0x80)}}, {{"WORD", bits(0xFF80)}}},
              {{{"BYTE", bits(0x7F)}}, {{"WORD", bits(0x007F)}}},
          },
          std::vector<Rewire::WireSpec>{{"BYTE", 8}},
          std::vector<Rewire::WireSpec>{{"WORD", 16}},
          sign_extend_mapping("BYTE", 8, "WORD", 16)) {}

RewireUnmappedHighTest::RewireUnmappedHighTest()
    : ComponentRowsTest<Rewire,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::WireSpec>,
                        std::vector<Rewire::BitMap>,
                        Rewire::UnmappedBitValue>(
          "RewireUnmappedHighTest",
          "REWIRE_UNMAPPED_HIGH_ROOT",
          {
              {{{"NIBBLE", bits(0x0A)}}, {{"BYTE", bits(0xFA)}}},
          },
          std::vector<Rewire::WireSpec>{{"NIBBLE", 4}},
          std::vector<Rewire::WireSpec>{{"BYTE", 8}},
          identity_mapping("NIBBLE", 0, 4, "BYTE", 0),
          Rewire::UnmappedBitValue::HIGH) {}

std::string RewireUnmappedUnknownTest::getTestName() const {
    return "RewireUnmappedUnknownTest";
}

void RewireUnmappedUnknownTest::setupCircuit() {
    root = Component::create<Rewire>(
        "REWIRE_UNMAPPED_UNKNOWN_ROOT",
        std::vector<Rewire::WireSpec>{{"NIBBLE", 4}},
        std::vector<Rewire::WireSpec>{{"BYTE", 8}},
        identity_mapping("NIBBLE", 0, 4, "BYTE", 0));
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void RewireUnmappedUnknownTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "RewireUnmappedUnknownTest requires IOComponent root");

    auto input_pin = io_root->getInputPinDynamic("NIBBLE");
    auto input_wire = builder->addNewWireDynamic("INPUT_NIBBLE", input_pin->getWidth(), nullptr, {input_pin});
    auto output_pin = io_root->getOutputPinDynamic("BYTE");
    builder->addNewWireDynamic("OUTPUT_BYTE", output_pin->getWidth(), output_pin, {});
    sim->scheduleEvent(makeTestWireUpdate(0, input_wire, bits(0x0A)));
}

void RewireUnmappedUnknownTest::verifyResults() {
    sim->setCircuitStateAtTime(getRunDuration());
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    auto output_pin = io_root->getOutputPinDynamic("BYTE");
    expect(output_pin->getValueAsUInt64() == 0x0A
               && output_pin->getBit(4) == LogicValue::UNKNOWN
               && output_pin->getBit(5) == LogicValue::UNKNOWN
               && output_pin->getBit(6) == LogicValue::UNKNOWN
               && output_pin->getBit(7) == LogicValue::UNKNOWN,
           "RewireUnmappedUnknownTest");
}

size_t RewireUnmappedUnknownTest::getRunDuration() const {
    return 20;
}

std::string RewireValidationTest::getTestName() const {
    return "RewireValidationTest";
}

void RewireValidationTest::verifyResults() {
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 4, "B", 0}});
            (void)invalid;
        },
        "RewireValidationTest invalid source bit");
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 4}});
            (void)invalid;
        },
        "RewireValidationTest invalid destination bit");
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 5}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"A", 0, "B", 0}});
            (void)invalid;
        },
        "RewireValidationTest invalid width");
    expectInvalidArgument(
        [] {
            auto invalid = Component::create<Rewire>(
                "ROOT",
                std::vector<Rewire::WireSpec>{{"A", 4}},
                std::vector<Rewire::WireSpec>{{"B", 4}},
                std::vector<Rewire::BitMap>{{"MISSING", 0, "B", 0}});
            (void)invalid;
        },
        "RewireValidationTest invalid source name");
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

BitSplitter8Test::BitSplitter8Test()
    : ComponentRowsTest<BitSplitter<8>>("BitSplitter8Test", "BIT_SPLITTER8_ROOT", {
          {{{"IN", bits(0xA5)}},
           {{"OUT_0", bit(true)}, {"OUT_1", bit(false)}, {"OUT_2", bit(true)}, {"OUT_3", bit(false)},
            {"OUT_4", bit(false)}, {"OUT_5", bit(true)}, {"OUT_6", bit(false)}, {"OUT_7", bit(true)}}},
      }) {}

BitJoiner8Test::BitJoiner8Test()
    : ComponentRowsTest<BitJoiner<8>>("BitJoiner8Test", "BIT_JOINER8_ROOT", {
          {{{"IN_0", bit(true)}, {"IN_1", bit(false)}, {"IN_2", bit(true)}, {"IN_3", bit(false)},
            {"IN_4", bit(false)}, {"IN_5", bit(true)}, {"IN_6", bit(false)}, {"IN_7", bit(true)}},
           {{"OUT", bits(0xA5)}}},
      }) {}

BitSplitter16Test::BitSplitter16Test()
    : ComponentRowsTest<BitSplitter<16>>("BitSplitter16Test", "BIT_SPLITTER16_ROOT", {
          {{{"IN", bits(0xA55A)}},
           {{"OUT_0", bit(false)}, {"OUT_1", bit(true)}, {"OUT_2", bit(false)}, {"OUT_3", bit(true)},
            {"OUT_8", bit(true)}, {"OUT_14", bit(false)}, {"OUT_15", bit(true)}}},
      }) {}

BitJoiner32Test::BitJoiner32Test()
    : ComponentRowsTest<BitJoiner<32>>("BitJoiner32Test", "BIT_JOINER32_ROOT", {
          {{{"IN_0", bit(true)}, {"IN_8", bit(true)}, {"IN_16", bit(true)}, {"IN_31", bit(true)}},
           {{"OUT", bits(0x80010101)}}},
      }) {}

std::string BitJoiner8UnknownTest::getTestName() const {
    return "BitJoiner8UnknownTest";
}

void BitJoiner8UnknownTest::setupCircuit() {
    root = Component::create<BitJoiner<8>>("BIT_JOINER8_UNKNOWN_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void BitJoiner8UnknownTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "BitJoiner8UnknownTest requires IOComponent root");

    auto input_pin = io_root->getInputPinDynamic("IN_0");
    auto input_wire = builder->addNewWireDynamic("INPUT_IN_0", input_pin->getWidth(), nullptr, {input_pin});
    auto output_pin = io_root->getOutputPinDynamic("OUT");
    builder->addNewWireDynamic("OUTPUT_OUT", output_pin->getWidth(), output_pin, {});
    sim->scheduleEvent(makeTestWireUpdate(0, input_wire, bit(true)));
}

void BitJoiner8UnknownTest::verifyResults() {
    sim->setCircuitStateAtTime(getRunDuration());
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    auto output_pin = io_root->getOutputPinDynamic("OUT");
    expect(output_pin->getBit(0) == LogicValue::HIGH
               && output_pin->getBit(1) == LogicValue::UNKNOWN
               && output_pin->getBit(2) == LogicValue::UNKNOWN
               && output_pin->getBit(3) == LogicValue::UNKNOWN
               && output_pin->getBit(4) == LogicValue::UNKNOWN
               && output_pin->getBit(5) == LogicValue::UNKNOWN
               && output_pin->getBit(6) == LogicValue::UNKNOWN
               && output_pin->getBit(7) == LogicValue::UNKNOWN,
           "BitJoiner8UnknownTest");
}

size_t BitJoiner8UnknownTest::getRunDuration() const {
    return 20;
}

ConstantValue1HighTest::ConstantValue1HighTest()
    : ComponentRowsTest<ConstantValue<1, 1>, uint64_t>(
          "ConstantValue1HighTest",
          "CONSTANT_VALUE1_HIGH_ROOT",
          {
              {{{"TRIGGER", bit(false)}}, {{"OUT", bit(true)}}},
              {{{"TRIGGER", bit(true)}}, {{"OUT", bit(true)}}},
          },
          uint64_t{1}) {}

ConstantValue1Low8TriggerTest::ConstantValue1Low8TriggerTest()
    : ComponentRowsTest<ConstantValue<1, 8>, uint64_t>(
          "ConstantValue1Low8TriggerTest",
          "CONSTANT_VALUE1_LOW_8_TRIGGER_ROOT",
          {
              {{{"TRIGGER", bits(0x00)}}, {{"OUT", bit(false)}}},
              {{{"TRIGGER", bits(0xFF)}}, {{"OUT", bit(false)}}},
          },
          uint64_t{0}) {}

ConstantValue8Test::ConstantValue8Test()
    : ComponentRowsTest<ConstantValue<8, 8>, uint64_t>(
          "ConstantValue8Test",
          "CONSTANT_VALUE8_ROOT",
          {
              {{{"TRIGGER", bits(0x00)}}, {{"OUT", bits(0xA5)}}},
              {{{"TRIGGER", bits(0xFF)}}, {{"OUT", bits(0xA5)}}},
          },
          uint64_t{0xA5}) {}
