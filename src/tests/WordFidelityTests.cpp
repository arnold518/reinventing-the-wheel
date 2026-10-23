#include "tests/ALUUnitTests.hpp"
#include "modules/composite/WordComponent.hpp"
#include "modules/composite/ALUControlDecoder.hpp"
#include "components/ComponentBuilder.tpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using namespace circuit;
using namespace circuit::test;
constexpr std::array<LogicValue, 4> states{
    LogicValue::LOW, LogicValue::HIGH, LogicValue::UNKNOWN, LogicValue::HIGH_Z};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
size_t countComponents(const std::shared_ptr<Component>& component) {
    size_t count = 1;
    for (const auto& child : component->getChildren()) count += countComponents(child);
    return count;
}
class Probe {
public:
    Probe(const ComponentFamily& family, BuildProfile profile)
        : root(std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(
              family.request("DUT"), std::move(profile)).root)), builder(root) {
        sim.setHistoryRecordingEnabled(false);
        for (const auto& [name, pin] : root->getAllInputPins())
            inputs[pin->getName()] = builder.addNewWireDynamic("IN_" + pin->getName(), pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root->getAllOutputPins())
            builder.addNewWireDynamic("OUT_" + pin->getName(), pin->getWidth(), pin, {});
        SimulationTest::scheduleInitialEventsForTree(root, sim, 0);
    }
    NamedValues evaluate(const NamedValues& values) {
        require(values.size() == inputs.size(), "Every test input must be driven");
        const auto start = sim.getCurrentTime() + 1;
        for (const auto& [name, bits] : values)
            sim.scheduleEvent(makeTestWireUpdate(start, inputs.at(name), TestValue(bits)));
        const auto settled = sim.drainUntilIdle(start + 10000, 1000000);
        require(settled.idle(), "Behavioral equivalence circuit did not settle");
        NamedValues result;
        for (const auto& [name, pin] : root->getAllOutputPins()) result[pin->getName()] = pin->getValueAsVector();
        return result;
    }
    NamedValues randomInputs(std::mt19937& rng, unsigned mode) const {
        NamedValues result;
        for (const auto& [name, pin] : root->getAllInputPins()) {
            auto& bits = result[pin->getName()];
            for (size_t bit = 0; bit < pin->getWidth(); ++bit)
                bits.push_back(states[rng() % (mode == 2 ? 4 : 2)]);
            if (mode == 1 && rng() % 2) bits[rng() % bits.size()] = states[2 + rng() % 2];
        }
        return result;
    }
    std::shared_ptr<IOComponent> root;
    Simulator sim;
private:
    ComponentBuilder builder;
    std::map<std::string, std::shared_ptr<WireBase>> inputs;
};
void compare(const NamedValues& expected, const NamedValues& actual,
             const NamedValues& inputs, const std::string& label) {
    if (expected == actual) return;
    std::string detail = label;
    for (const auto& [name, values] : inputs) detail += " " + name + "=" + formatLogicVector(values);
    for (const auto& [name, values] : expected)
        if (actual.at(name) != values) detail += " output " + name + ": " + formatLogicVector(values)
            + " vs " + formatLogicVector(actual.at(name));
    throw std::runtime_error(detail);
}
LogicVector fourStateBits(size_t width, unsigned index) {
    LogicVector result(width);
    for (auto& bit : result) { bit = states[index & 3]; index >>= 2; }
    return result;
}
} // namespace

void ALUFidelityContractTest::verifyResults() {
    using circuit::word::Unit;
    auto f8 = [](Unit u) { return &circuit::word::family(u, 8); };
    // Every Chapter 2 compact form: all units at 8 bits, both adders at 16 and 32.
    const std::array<const ComponentFamily*, 16> chapter_families{
        f8(Unit::RippleAdder), f8(Unit::PrefixAdder), f8(Unit::AddSub), f8(Unit::Arithmetic), f8(Unit::Logic),
        f8(Unit::Shift), f8(Unit::Zero), f8(Unit::Compare), f8(Unit::Flags), &circuit::families::aluControlDecoder(),
        f8(Unit::Result), f8(Unit::ALU),
        &circuit::word::family(Unit::RippleAdder, 16), &circuit::word::family(Unit::PrefixAdder, 16),
        &circuit::word::family(Unit::RippleAdder, 32), &circuit::word::family(Unit::PrefixAdder, 32)};
    std::mt19937 rng(0xA108B);
    for (const auto* family : chapter_families) {
        Probe structural(*family, canonicalDefaultProfile());
        Probe behavioral(*family, withExactFidelity(canonicalDefaultProfile(), "DUT", Fidelity::Behavioral));
        require(countComponents(behavioral.root) == 1, "Behavioral family must be a single component");
        require(countComponents(structural.root) > 1, "Structural default must retain its internals");
        require(behavioral.root->getInstanceMetadata()->fidelity == Fidelity::Behavioral, "Wrong behavioral metadata");
        require(builtinComponentCatalog().contract(std::string(family->id())).version==1,
                "Generic family must declare its own version");
        size_t rows = 0;
        auto check = [&](const NamedValues& inputs) {
            const auto expected = structural.evaluate(inputs);
            compare(expected, behavioral.evaluate(inputs), inputs, std::string(family->typeName()) + " row " + std::to_string(rows));
            ++rows;
        };
        // Binary, sparse X/Z, and dense four-state vectors. Do not use the
        // behavioral value functions as the expected-output oracle.
        for (size_t i = 0; i < 1024; ++i) check(structural.randomInputs(rng, i % 3));
        for (unsigned fill : {0u, 1u}) {
            auto base = structural.randomInputs(rng, 0);
            for (auto& [name, bits] : base) std::fill(bits.begin(), bits.end(), states[fill]);
            for (const auto& [name, bits] : base) for (size_t bit = 0; bit < bits.size(); ++bit)
                for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
                    auto inputs = base;
                    inputs.at(name)[bit] = unknown;
                    check(inputs);
                }
        }
        // Every four-state local/global opcode with independently driven data.
        auto base = structural.randomInputs(rng, 0);
        if (base.contains("OP")) {
            const size_t width = base.at("OP").size();
            for (unsigned i = 0; i < (1u << (2 * width)); ++i) {
                auto inputs = structural.randomInputs(rng, 0);
                inputs.at("OP") = fourStateBits(width, i);
                check(inputs);
            }
        }
        // Exhaustive four-state zero detector and independent selector enables.
        if (family == &circuit::word::family(Unit::Zero,8)) {
            for (unsigned i = 0; i < 65536; ++i) check({{"A", fourStateBits(8, i)}});
        }
        if (family == &circuit::word::family(Unit::Result,8)) {
            for (unsigned i = 0; i < 4096; ++i) {
                auto inputs = structural.randomInputs(rng, i % 3);
                auto index = i;
                for (auto& [name, bits] : inputs) if (bits.size() == 1) {
                    bits[0] = states[index & 3]; index >>= 2;
                }
                check(inputs);
            }
        }
        std::cout << family->typeName() << ": " << rows << " settled equivalence rows\n";
    }

    Probe structural(circuit::word::family(Unit::ALU,8), canonicalDefaultProfile());
    std::vector<NamedValues> inputs, expected;
    for (size_t i = 0; i < 256; ++i) {
        inputs.push_back(structural.randomInputs(rng, i % 3));
        // Ensure every binary opcode appears with both carry values as well.
        if (i < 64) {
            inputs.back() = structural.randomInputs(rng, 0);
            inputs.back()["OP"] = logicBits(5, i / 2);
            inputs.back()["C_IN"] = logicBits(1, i & 1);
        }
        expected.push_back(structural.evaluate(inputs.back()));
    }
    const std::array<const char*, 11> paths{"DUT", "DUT.CONTROL", "DUT.ARITHMETIC", "DUT.ARITHMETIC.ADD_SUB",
        "DUT.ARITHMETIC.ADD_SUB.ADD", "DUT.LOGIC", "DUT.SHIFTER", "DUT.COMPARE_SELECT",
        "DUT.COMPARE_SELECT.EQUAL", "DUT.RESULT_SELECT", "DUT.FLAGS"};
    auto checkMixed = [&](const std::string& label, BuildProfile profile) {
        Probe mixed(circuit::word::family(Unit::ALU,8), std::move(profile));
        require(countComponents(mixed.root) < countComponents(structural.root), label + ": selection did not collapse internals");
        for (size_t i = 0; i < inputs.size(); ++i) compare(expected[i], mixed.evaluate(inputs[i]), inputs[i], label);
    };
    for (auto path : paths)
        checkMixed(path, withExactFidelity(canonicalDefaultProfile(), path, Fidelity::Behavioral));
    checkMixed("DUT.FLAGS.ZERO_DETECT", withExactFidelity(canonicalDefaultProfile(), "DUT.FLAGS.ZERO_DETECT", Fidelity::Behavioral));
    std::vector<ProfileRule> overrides;
    for (const auto* family : chapter_families) overrides.push_back(preferFidelity(Fidelity::Behavioral, ProfileSelector::contract(*family)));
    overrides.push_back(preferFidelity(Fidelity::Structural, ProfileSelector::exactPath("DUT")));
    auto hybrid = withProfileOverrides(canonicalDefaultProfile(), overrides, "chapter2-hybrid");
    Probe hybrid_probe(circuit::word::family(Unit::ALU,8), hybrid);
    require(countComponents(hybrid_probe.root) == 8, "Hybrid ALU<8> must contain root plus seven behavioral units");
    checkMixed("all seven units", hybrid);
    std::cout << "Mixed fidelity: 13 profiles x 256 settled rows\n";

    // Comparable native no-history workload; excludes construction and JSON.
    // Report measurements, never impose a noisy wall-clock speed threshold.
    std::vector<NamedValues> workload;
    for (size_t i = 0; i < 1024; ++i) workload.push_back(structural.randomInputs(rng, 0));
    for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        Probe probe(circuit::word::family(Unit::ALU,8), withExactFidelity(canonicalDefaultProfile(), "DUT", fidelity));
        probe.evaluate(workload.front());
        const auto events_before = probe.sim.getPerformanceCounters().processed_events;
        const auto start = std::chrono::steady_clock::now();
        for (const auto& row : workload) probe.evaluate(row);
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "ALU<8> " << toString(fidelity) << ": 1024 binary rows, " << countComponents(probe.root)
            << " components, " << probe.sim.getPerformanceCounters().processed_events - events_before
            << " events, " << seconds << " seconds\n";
    }
}
