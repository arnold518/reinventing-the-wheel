#include "tests/TestRegistry.hpp"

#include "tests/ArithmeticLogicTests.hpp"
#include "tests/CorePrimitiveTests.hpp"
#include "tests/FullCircuitTest.hpp"
#include "tests/UtilityComponentTests.hpp"
#include <algorithm>

namespace {
template<typename TestT>
TestRegistryEntry entry(const std::string& name) {
    return {name, [] { return std::make_unique<TestT>(); }};
}
}

const std::vector<TestRegistryEntry>& getTestRegistry() {
    static const std::vector<TestRegistryEntry> registry{
        entry<FullCircuitTest>("FullCircuitTest"),
        entry<WireTemplateTest>("WireTemplateTest"),
        entry<NOTGateTest>("NOTGateTest"),
        entry<ANDGateTest>("ANDGateTest"),
        entry<ORGateTest>("ORGateTest"),
        entry<XORGateTest>("XORGateTest"),
        entry<NANDGateTest>("NANDGateTest"),
        entry<NORGateTest>("NORGateTest"),
        entry<DFlipFlopTest>("DFlipFlopTest"),
        entry<ClockGeneratorTest>("ClockGeneratorTest"),
        entry<RewireUnpackTest>("RewireUnpackTest"),
        entry<RewirePackTest>("RewirePackTest"),
        entry<RewireSliceTest>("RewireSliceTest"),
        entry<RewireZeroExtendTest>("RewireZeroExtendTest"),
        entry<RewireSignExtendTest>("RewireSignExtendTest"),
        entry<RewireUnmappedHighTest>("RewireUnmappedHighTest"),
        entry<RewireUnmappedUnknownTest>("RewireUnmappedUnknownTest"),
        entry<RewireValidationTest>("RewireValidationTest"),
        entry<BitSplitter8Test>("BitSplitter8Test"),
        entry<BitJoiner8Test>("BitJoiner8Test"),
        entry<BitSplitter16Test>("BitSplitter16Test"),
        entry<BitJoiner32Test>("BitJoiner32Test"),
        entry<BitJoiner8UnknownTest>("BitJoiner8UnknownTest"),
        entry<ConstantValue1HighTest>("ConstantValue1HighTest"),
        entry<ConstantValue1Low8TriggerTest>("ConstantValue1Low8TriggerTest"),
        entry<ConstantValue8Test>("ConstantValue8Test"),
        entry<HalfAdderTest>("HalfAdderTest"),
        entry<FullAdderTest>("FullAdderTest"),
        entry<AND8Test>("AND8Test"),
        entry<OR8Test>("OR8Test"),
        entry<XOR8Test>("XOR8Test"),
        entry<NOT8Test>("NOT8Test"),
        entry<NAND8Test>("NAND8Test"),
        entry<NOR8Test>("NOR8Test"),
        entry<Mux2to1Test>("Mux2to1Test"),
        entry<Mux4to1Test>("Mux4to1Test"),
        entry<Mux8to1Test>("Mux8to1Test"),
        entry<Mux16to1Test>("Mux16to1Test"),
        entry<Mux2to1_8bitTest>("Mux2to1_8bitTest"),
        entry<Mux4to1_8bitTest>("Mux4to1_8bitTest"),
        entry<Mux8to1_8bitTest>("Mux8to1_8bitTest"),
        entry<Mux16to1_8bitTest>("Mux16to1_8bitTest"),
        entry<Adder8Test>("Adder8Test"),
        entry<TwosComplement8Test>("TwosComplement8Test"),
        entry<Subtractor8Test>("Subtractor8Test"),
        entry<SubtractorWithBorrow8Test>("SubtractorWithBorrow8Test"),
        entry<Incrementer8Test>("Incrementer8Test"),
        entry<Decrementer8Test>("Decrementer8Test"),
        entry<EqualityChecker8Test>("EqualityChecker8Test"),
        entry<Comparator8Test>("Comparator8Test"),
        entry<SignedComparator8Test>("SignedComparator8Test"),
        entry<ShiftLeftLogical8Test>("ShiftLeftLogical8Test"),
        entry<ShiftRightLogical8Test>("ShiftRightLogical8Test"),
        entry<ShiftRightArithmetic8Test>("ShiftRightArithmetic8Test"),
        entry<ZeroDetect8Test>("ZeroDetect8Test"),
        entry<ALU8Test>("ALU8Test"),
    };
    return registry;
}

std::unique_ptr<SimulationTest> createTestByName(const std::string& name) {
    const auto& registry = getTestRegistry();
    auto it = std::find_if(registry.begin(), registry.end(),
        [&](const TestRegistryEntry& entry) {
            return entry.name == name;
        });
    if (it == registry.end()) {
        return nullptr;
    }
    return it->create();
}

std::vector<std::string> getRegisteredTestNames() {
    std::vector<std::string> names;
    for (const auto& entry : getTestRegistry()) {
        names.push_back(entry.name);
    }
    return names;
}
