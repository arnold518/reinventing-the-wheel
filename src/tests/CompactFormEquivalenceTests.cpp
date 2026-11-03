#include "tests/CompactFormEquivalenceTests.hpp"
#include "tests/SettledSweep.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"
#include <algorithm>
#include "components/selection/GateListCompact.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/ComponentBuilder.tpp"
#include "simulator/SimulationTest.hpp"
#include "modules/basic/WordMux.hpp"
#include "modules/composite/WordRecipes.hpp"
#include "modules/rv32i/RV32IALU.hpp"
#include <array>
#include <iostream>
#include <random>

namespace {
using namespace circuit::test;
constexpr std::array<LogicValue, 4> states{LogicValue::LOW, LogicValue::HIGH, LogicValue::UNKNOWN, LogicValue::HIGH_Z};

// Binary vectors over every input, then dense four-state ones (each bit L, H,
// X or Z), then binary vectors with one unknown bit; both forms must agree.
size_t sweep(const circuit::ComponentFamily& family, uint64_t seed) {
    FidelitySweep both(family);
    if (!both.hasBehavioral()) throw std::runtime_error(std::string(family.typeName()) + " has no compact form");
    std::mt19937_64 rng(seed);
    std::vector<std::pair<std::string, size_t>> pins;
    for (const auto& [name, pin] : both.gates().root()->getAllInputPins()) pins.emplace_back(name, pin->getWidth());
    auto binary = [&] {
        std::map<std::string, uint64_t> values;
        for (const auto& [name, width] : pins) values[name] = rng() & (width >= 64 ? ~uint64_t{0} : (uint64_t{1} << width) - 1);
        return both.gates().binary(values);
    };
    for (size_t i = 0; i < 96; ++i) both.evaluate(binary());
    for (size_t i = 0; i < 48; ++i) {
        auto in = binary();
        for (auto& [name, bits] : in) for (auto& bit : bits) bit = states[rng() % 4];
        both.evaluate(in);
    }
    for (size_t i = 0; i < 48; ++i) {
        auto in = binary();
        auto& [name, bits] = *std::next(in.begin(), long(rng() % in.size()));
        bits[rng() % bits.size()] = states[2 + rng() % 2];
        both.evaluate(in);
    }
    return both.vectors();
}
} // namespace

void CompactFormEquivalenceTest::verifyResults() {
    using circuit::word::Unit;
    size_t families = 0, vectors = 0;
    for (size_t width : {16u, 32u})
        for (auto unit : {Unit::ALU, Unit::AddSub, Unit::Arithmetic, Unit::Logic, Unit::Shift, Unit::Compare,
                          Unit::Flags, Unit::Result}) {
            vectors += sweep(circuit::word::family(unit, width), 0xC0A1 + width * 16 + static_cast<unsigned>(unit));
            ++families;
        }
    vectors += sweep(circuit::word::family(Unit::Zero, 16), 0xC0A2);
    ++families;
    for (size_t width : {1u, 4u, 8u, 16u})
        for (size_t inputs : {2u, 4u, 8u, 16u, 32u}) {
            vectors += sweep(circuit::word::mux(inputs, width), 0xC0A3 + width * 64 + inputs);
            ++families;
        }
    for (const auto* family : {&circuit::families::rv32iALUControl(), &circuit::families::rv32iALUAddSub(),
                               &circuit::families::rv32iALULogic(), &circuit::families::rv32iALUShifter(),
                               &circuit::families::rv32iALUCompare(), &circuit::families::rv32iALUResultSelect()}) {
        vectors += sweep(*family, 0xC0A4 + families);
        ++families;
    }
    std::cout << families << " families: " << vectors << " vectors (binary, dense four-state, one unknown bit); "
              << "gate-level and compact forms agree\n";
}

namespace {
// One root of a contract under a profile, driven by input wires.
class ContractRig {
public:
    ContractRig(const std::string& contract, circuit::BuildProfile profile, const circuit::ParameterMap& parameters = {})
        : root_(std::dynamic_pointer_cast<IOComponent>(
              circuit::builtinComponentCatalog().createRoot({contract, "DUT", parameters}, std::move(profile)).root)) {
        if (!root_) throw std::runtime_error(contract + ": no root");
        ComponentBuilder builder(root_);
        sim_.setHistoryRecordingEnabled(false);
        for (const auto& [name, pin] : root_->getAllInputPins())
            inputs_[name] = builder.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root_->getAllOutputPins())
            builder.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
        SimulationTest::scheduleInitialEventsForTree(root_, sim_, 0);
    }
    NamedValues evaluate(const NamedValues& in) {
        for (const auto& [name, bits] : in) sim_.scheduleEvent(makeTestWireUpdate(time_ + 1, inputs_.at(name), TestValue(bits)));
        sim_.advanceAndRecord(time_ += 200000);
        NamedValues out;
        for (const auto& [name, pin] : root_->getAllOutputPins()) out[name] = pin->getValueAsVector();
        return out;
    }
    const std::shared_ptr<IOComponent>& root() const { return root_; }
private:
    std::shared_ptr<IOComponent> root_;
    Simulator sim_;
    std::map<std::string, std::shared_ptr<WireBase>> inputs_;
    SimulationTime time_ = 0;
};
} // namespace

void GateListCompactEquivalenceTest::verifyResults() {
    size_t contracts = 0, vectors = 0;
    // Default parameters, and every parameter set a recorded scenario of the
    // contract is built with.
    std::vector<std::pair<std::string, circuit::ParameterMap>> cases;
    for (const auto& contract : circuit::gateListCompactContracts()) cases.push_back({contract, {}});
    for (const auto& entry : getTestRegistry()) {
        if (entry.contract_ids.size() != 1 || !circuit::gateListCompactContracts().contains(entry.contract_ids.front())) continue;
        const auto test = entry.create();
        if (const auto* scenario = dynamic_cast<const circuit::test::ComponentScenarioTest*>(test.get()))
            if (!scenario->getSpec().parameters.empty()
                && std::find(cases.begin(), cases.end(), std::pair(scenario->getSpec().contract_id, scenario->getSpec().parameters)) == cases.end())
                cases.push_back({scenario->getSpec().contract_id, scenario->getSpec().parameters});
    }
    for (const auto& [contract, parameters] : cases) {
        ContractRig gates(contract, circuit::strictAllStructural(), parameters);
        ContractRig compact(contract, circuit::withExactFidelity(circuit::canonicalDefaultProfile(), "DUT", circuit::Fidelity::Behavioral), parameters);
        if (!std::dynamic_pointer_cast<circuit::GateListCompact>(compact.root()))
            throw std::runtime_error(contract + ": the compact root is not its gate-list form");
        std::mt19937_64 rng(std::hash<std::string>{}(contract));
        std::vector<std::pair<std::string, size_t>> pins;
        for (const auto& [name, pin] : gates.root()->getAllInputPins()) pins.emplace_back(name, pin->getWidth());
        auto binary = [&] {
            NamedValues in;
            for (const auto& [name, width] : pins) {
                LogicVector bits(width);
                for (auto& bit : bits) bit = rng() & 1 ? LogicValue::HIGH : LogicValue::LOW;
                in[name] = bits;
            }
            return in;
        };
        auto compare = [&](const NamedValues& in) {
            if (gates.evaluate(in) != compact.evaluate(in))
                throw std::runtime_error(contract + ": gate-level and gate-list compact outputs differ for " + describeInputs(in));
            ++vectors;
        };
        for (size_t i = 0; i < 64; ++i) compare(binary());
        for (size_t i = 0; i < 24; ++i) {
            auto in = binary();
            for (auto& [name, bits] : in) for (auto& bit : bits) bit = states[rng() % 4];
            compare(in);
        }
        for (size_t i = 0; i < 24 && !pins.empty(); ++i) {
            auto in = binary();
            auto& [name, bits] = *std::next(in.begin(), long(rng() % in.size()));
            bits[rng() % bits.size()] = states[2 + rng() % 2];
            compare(in);
        }
        ++contracts;
    }
    std::cout << contracts << " gate-list compact forms (contracts and recorded parameter sets): " << vectors
              << " vectors (binary, dense four-state, one unknown bit) agree with their gates\n";
}
