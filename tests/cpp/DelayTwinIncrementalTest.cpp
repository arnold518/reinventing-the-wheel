// The production delay twin re-evaluates only the gates whose inputs changed
// (ordered dirty masks). This compares it, step by step, with the previous
// whole-list implementation kept in DelayTwinReference.inc: the same answer,
// output values and arrival times for every call, in every timing profile.
#include "DelayTwinReference.inc"
#include "components/IOComponent.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/composite/IterativeArithmetic.hpp"
#include "modules/device/DeviceComponents.hpp"
#include "modules/storage/Register.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using L = LogicValue;
struct Input {
    long long now;
    std::vector<std::vector<L>> values;
    std::vector<std::vector<long long>> changed;
};
struct Instance {
    std::shared_ptr<const circuit::DelayTwin> twin;
    std::unique_ptr<circuit::DelayTwin::State> state;
    std::vector<long long> times;
    std::vector<std::vector<uint8_t>> result;
    bool answered = false;
    explicit Instance(std::shared_ptr<const circuit::DelayTwin> t) : twin(std::move(t)), state(twin->start()) {}
    void step(const Input& input) { answered = twin->settle(state.get(), input.now, input.values, input.changed, times, result); }
};

// A clocked timeline: CLK toggles, resets pulse now and then, every other input
// changes sparsely or densely (sometimes to X), and some steps change only an
// arrival time.
std::vector<Input> timeline(const circuit::DelayTwin& twin, size_t count, bool dense, uint64_t seed) {
    std::vector<Input> all;
    Input current;
    for (const auto& [name, width] : twin.inputs) { current.values.emplace_back(width, L::LOW); current.changed.emplace_back(width, -1); }
    uint64_t random = seed;
    auto next = [&] { random = random * 6364136223846793005ULL + 1442695040888963407ULL; return random >> 33; };
    for (size_t step = 0; step < count; ++step) {
        current.now = 1000000 + static_cast<long long>(step) * 10000;
        for (size_t pin = 0; pin < twin.inputs.size(); ++pin) {
            const auto& name = twin.inputs[pin].first;
            auto& bits = current.values[pin];
            for (size_t bit = 0; bit < bits.size(); ++bit) {
                auto value = bits[bit];
                if (name == "CLK") value = step % 4 == 1 ? L::HIGH : L::LOW;
                else if (name == "RST" || name.ends_with("_RST")) value = step % 512 < 4 ? L::HIGH : L::LOW;
                else if (step % 4 == 0 && (dense || next() % bits.size() == 0))
                    value = L(next() % (step % 64 == 0 ? 4 : 2));
                const bool arrival_only = step % 37 == 0 && bit == step % bits.size() && name != "CLK";
                if (value != bits[bit] || arrival_only) current.changed[pin][bit] = current.now;
                bits[bit] = value;
            }
        }
        all.push_back(current);
    }
    return all;
}

size_t compare(const std::string& label, const circuit::ComponentFamily& family) {
    auto root = circuit::builtinComponentCatalog().createRoot(family.request("KERNEL"), circuit::maximallyStructural()).root;
    std::string why;
    const auto reference = circuit::analysis::reference::buildDelayTwin(root, &why);
    if (!reference) throw std::runtime_error(label + ": reference twin unavailable: " + why);
    const auto incremental = circuit::analysis::buildDelayTwin(root, &why);
    if (!incremental) throw std::runtime_error(label + ": production twin unavailable: " + why);
    if (reference->inputs != incremental->inputs || reference->outputs != incremental->outputs)
        throw std::runtime_error(label + ": twin interfaces differ");
    size_t steps = 0;
    for (bool dense : {false, true}) {
        const auto cases = timeline(*reference, 3000, dense, dense ? 0x5eed : 0xfeed);
        // Two owners per implementation, interleaved, so owner state is kept apart.
        Instance a(reference), b(incremental), c(reference), d(incremental);
        for (size_t i = 0; i < cases.size(); ++i) {
            a.step(cases[i]); b.step(cases[i]);
            if (i % 2 == 0) { c.step(cases[i]); d.step(cases[i]); }
            if (a.answered != b.answered || a.times != b.times || a.result != b.result ||
                c.answered != d.answered || c.times != d.times || c.result != d.result)
                throw std::runtime_error(label + ": incremental twin differs at step " + std::to_string(i) +
                    (dense ? " (dense)" : " (sparse)") + " in profile " + circuit::timing::activeTimingProfile().name);
            ++steps;
        }
    }
    return steps;
}
} // namespace

int main() {
    try {
        size_t steps = 0;
        for (const auto& profile : circuit::timing::timingProfileNames()) {
            circuit::timing::setActiveTimingProfile(profile);
            steps += compare("Register<8>", circuit::families::registerWord(8));
            steps += compare("Register<32>", circuit::families::registerWord(32));
            steps += compare("ClockedCounter<8>", circuit::families::clockedCounter(8));
            steps += compare("MulDivUnit<4>", circuit::families::mulDivUnit(4));
        }
        std::cout << "PASS incremental delay twin equals the whole-list reference on " << steps
                  << " clocked steps (registers, counter, 4-bit M unit; sparse and dense; every timing profile)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
