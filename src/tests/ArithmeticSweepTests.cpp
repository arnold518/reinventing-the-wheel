#include "tests/ArithmeticSweepTests.hpp"
#include "tests/SettledSweep.hpp"
#include "modules/composite/WordComponent.hpp"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using namespace circuit::test;
using circuit::word::Unit;

struct AdderCase {
    uint64_t a, b;
    bool cin;
};

// Independent reference: plain integer addition.
NamedValues adderReference(size_t w, bool sign_carry, uint64_t a, uint64_t b, bool cin) {
    const uint64_t mask = (uint64_t{1} << w) - 1, low = mask >> 1;
    const uint64_t total = a + b + (cin ? 1 : 0);
    NamedValues result{{"OUT", logicBits(w, total & mask)}, {"C_OUT", logicBit(((total >> w) & 1) != 0)}};
    if (sign_carry)
        result["C_SIGN"] = logicBit(((((a & low) + (b & low) + (cin ? 1 : 0)) >> (w - 1)) & 1) != 0);
    return result;
}

std::vector<AdderCase> directedCases(size_t w) {
    const uint64_t mask = (uint64_t{1} << w) - 1, sign = uint64_t{1} << (w - 1);
    std::vector<AdderCase> cases;
    // (2^k - 1) + 1 and a carry-in through k ones, for every k, both operand orders.
    for (size_t k = 0; k <= w; ++k) {
        const uint64_t run = k == w ? mask : (uint64_t{1} << k) - 1;
        for (bool cin : {false, true}) {
            cases.push_back({run, 1, cin});
            cases.push_back({1, run, cin});
            cases.push_back({run, 0, cin});
            cases.push_back({0, run, cin});
        }
        cases.push_back({run, run, false});
    }
    // A carry generated at bit j that propagates through k - 1 further bits:
    // every span length at every start position.
    for (size_t j = 0; j < w; ++j)
        for (size_t k = 1; j + k <= w; ++k) {
            const uint64_t span = k == 64 ? ~uint64_t{0} : (uint64_t{1} << k) - 1;
            cases.push_back({(span << j) & mask, uint64_t{1} << j, false});
        }
    const uint64_t alternate = 0x5555555555555555ULL & mask;
    for (uint64_t a : {uint64_t{0}, alternate, alternate ^ mask, mask, sign, sign - 1})
        for (uint64_t b : {uint64_t{0}, uint64_t{1}, alternate, alternate ^ mask, mask, sign, sign - 1})
            for (bool cin : {false, true}) cases.push_back({a, b, cin});
    return cases;
}

void sweepAdder(Unit unit, size_t w) {
    const auto& family = circuit::word::family(unit, w);
    const std::string label(family.typeName());
    const bool sign_carry = unit == Unit::PrefixAdder;
    const uint64_t mask = (uint64_t{1} << w) - 1;
    FidelitySweep sweep(family);
    auto& gates = sweep.gates();
    auto drive = [&](uint64_t a, uint64_t b, bool cin) {
        return gates.binary({{"A", a}, {"B", b}, {"C_IN", cin ? 1u : 0u}});
    };

    size_t binary_rows = 0;
    auto checkBinary = [&](uint64_t a, uint64_t b, bool cin) {
        const auto inputs = drive(a, b, cin);
        const auto reference = adderReference(w, sign_carry, a, b, cin);
        sweep.evaluate(inputs, &reference);
        ++binary_rows;
    };
    for (const auto& c : directedCases(w)) checkBinary(c.a, c.b, c.cin);
    std::mt19937_64 rng(0xADD0000 + w * 2 + (sign_carry ? 1 : 0));
    for (size_t i = 0; i < 1024; ++i) {
        const uint64_t a = rng() & mask;
        const uint64_t b = rng() & mask;
        const bool cin = (rng() & 1) != 0;
        checkBinary(a, b, cin);
    }

    // One X or Z input bit on random bases. Outputs may be pessimistically
    // UNKNOWN but never a wrong known value; sum bits below an unknown A/B bit
    // (and C_SIGN for the top bit) cannot depend on it and must stay exact.
    size_t unknown_rows = 0;
    for (size_t base = 0; base < 2; ++base) {
        const uint64_t a = rng() & mask;
        const uint64_t b = rng() & mask;
        const bool cin = (rng() & 1) != 0;
        auto resolved = [&](const std::string& pin, size_t bit, bool value) {
            uint64_t ra = a, rb = b;
            bool rc = cin;
            const uint64_t flag = uint64_t{1} << bit;
            if (pin == "A") ra = value ? (ra | flag) : (ra & ~flag);
            else if (pin == "B") rb = value ? (rb | flag) : (rb & ~flag);
            else rc = value;
            return adderReference(w, sign_carry, ra, rb, rc);
        };
        for (const std::string pin : {"A", "B", "C_IN"}) {
            const size_t bits = pin == "C_IN" ? 1 : w;
            for (size_t bit = 0; bit < bits; ++bit)
                for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
                    auto inputs = drive(a, b, cin);
                    inputs.at(pin)[bit] = unknown;
                    const auto actual = sweep.evaluate(inputs);
                    const auto low = resolved(pin, bit, false);
                    requireSoundUnknown(low, resolved(pin, bit, true), actual, inputs, label + " unknown input");
                    if (pin != "C_IN") {
                        for (size_t j = 0; j < bit; ++j)
                            if (actual.at("OUT")[j] != low.at("OUT")[j])
                                throw std::runtime_error(label + ": OUT[" + std::to_string(j) +
                                    "] depends on a higher unknown bit:" + describeInputs(inputs));
                        if (sign_carry && bit == w - 1 && actual.at("C_SIGN") != low.at("C_SIGN"))
                            throw std::runtime_error(label + ": C_SIGN depends on the top bit:" + describeInputs(inputs));
                    }
                    ++unknown_rows;
                }
        }
    }
    std::cout << label << ": " << binary_rows << " binary vectors against a + b + C_IN, " << unknown_rows
              << " single-unknown vectors, " << (sweep.hasBehavioral() ? "gate-level and behavioral forms agree"
                                                                         : "gate-level form only")
              << '\n';
}
} // namespace

void AdderWidthSweepTest::verifyResults() {
    for (size_t w : {8u, 16u, 32u})
        for (auto unit : {Unit::RippleAdder, Unit::PrefixAdder}) sweepAdder(unit, w);
}
