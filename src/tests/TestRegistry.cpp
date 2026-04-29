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
        entry<GateTest>("GateTest"),
        entry<DFlipFlopTest>("DFlipFlopTest"),
        entry<ClockGeneratorTest>("ClockGeneratorTest"),
        entry<RewireTest>("RewireTest"),
        entry<BitAdapterTest>("BitAdapterTest"),
        entry<ConstantValueTest>("ConstantValueTest"),
        entry<HalfAdderTest>("HalfAdderTest"),
        entry<FullAdderTest>("FullAdderTest"),
        entry<Logic8Test>("Logic8Test"),
        entry<MuxTest>("MuxTest"),
        entry<Adder8Test>("Adder8Test"),
        entry<Arithmetic8Test>("Arithmetic8Test"),
        entry<Comparator8Test>("Comparator8Test"),
        entry<Shifter8Test>("Shifter8Test"),
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
