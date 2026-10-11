// Compares the proven delay of gate-level parts with what the simulator
// actually shows, input by input.
//
// For each part it prints, per output: the proven worst case, and the
// distribution of the real settling time over random input changes. Then the
// same per input pin (only that pin changes), and for the listed control pins
// per control value. Output is Markdown on stdout.
//
//   delay_estimate_experiment [--rounds=N] [--budget=N] [--control=PART:PIN ...] [PART ...]
//   delay_estimate_experiment --compare [--four-state] [--rounds=N] [--watch=N] [PART ...]
//   (all parts by default; --four-state also drives X and Z)
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <iomanip>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

using namespace circuit::analysis;
namespace {
using Bits = std::vector<LogicValue>;
using Vector = std::map<std::string, Bits>;

struct Bench {
    std::shared_ptr<IOComponent> dut;
    std::shared_ptr<IOComponent> root = Component::create<IOComponent>("BENCH");
    std::map<std::string, std::shared_ptr<WireBase>> inputs, outputs;
    Simulator sim;
    size_t now = 0;

    explicit Bench(std::shared_ptr<IOComponent> part) : dut(std::move(part)) {
        root->addChild(dut);
        ComponentBuilder builder(root);
        for (const auto& [name, pin] : dut->getAllInputPins())
            inputs[name] = builder.addNewWireDynamic("i_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : dut->getAllOutputPins())
            outputs[name] = builder.addNewWireDynamic("o_" + name, pin->getWidth(), pin, {});
        sim.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root, sim, 0);
    }
    // Settle on `before`, switch to `after`, and report per output the last
    // tick at which it changed (absent: it did not change).
    std::map<std::string, size_t> change(const Vector& before, const Vector& after, size_t watch) {
        now += 1;
        for (const auto& [name, bits] : before) sim.scheduleEvent(makeWireUpdateEvent(now, inputs.at(name), bits));
        now += watch + 1;
        sim.advanceAndRecord(now - 1);
        std::map<std::string, Bits> seen;
        for (const auto& [name, wire] : outputs) seen[name] = wire->getValueVector();
        for (const auto& [name, bits] : after) sim.scheduleEvent(makeWireUpdateEvent(now, inputs.at(name), bits));
        std::map<std::string, size_t> last;
        for (size_t tick = 0; tick <= watch; ++tick) {
            sim.advanceAndRecord(now + tick);
            for (const auto& [name, wire] : outputs) {
                auto current = wire->getValueVector();
                if (current != seen[name]) { last[name] = tick; seen[name] = std::move(current); }
            }
        }
        if (sim.hasPendingEvents()) throw std::runtime_error(dut->getName() + ": did not go quiet");
        now += watch;
        return last;
    }
};

Vector randomVector(const IOComponent& part, std::mt19937_64& random, const std::map<std::string, uint64_t>& forced = {}) {
    Vector vector;
    for (const auto& [name, pin] : part.getAllInputPins()) {
        Bits bits(pin->getWidth());
        const auto held = forced.find(name);
        for (size_t bit = 0; bit < bits.size(); ++bit)
            bits[bit] = LogicValue(held != forced.end() ? (held->second >> bit) & 1 : random() & 1);
        vector[name] = std::move(bits);
    }
    return vector;
}

// Real settling times of one output over many input changes.
struct Seen {
    std::vector<size_t> ticks; // only the changes
    size_t trials = 0;
    void add(bool changed, size_t tick) { ++trials; if (changed) ticks.push_back(tick); }
    size_t maximum() const { return ticks.empty() ? 0 : *std::max_element(ticks.begin(), ticks.end()); }
    double mean() const {
        double sum = 0;
        for (const auto t : ticks) sum += static_cast<double>(t);
        return ticks.empty() ? 0 : sum / static_cast<double>(ticks.size());
    }
    size_t percentile(double p) {
        if (ticks.empty()) return 0;
        std::sort(ticks.begin(), ticks.end());
        return ticks[std::min(ticks.size() - 1, static_cast<size_t>(p * static_cast<double>(ticks.size())))];
    }
};

void row(const std::string& label, const std::string& output, const SettlingEndpoint& proven, Seen& seen) {
    std::cout << "| " << label << " | `" << output << "` | ";
    if (!proven.changes) std::cout << "never | ";
    else std::cout << proven.bound << (proven.tight ? "" : "*") << " | ";
    if (seen.ticks.empty()) { std::cout << "- | - | - | - | - | " << 0 << "% |\n"; return; }
    const double mean = seen.mean();
    std::cout << seen.maximum() << " | " << seen.percentile(0.95) << " | " << std::fixed << std::setprecision(1) << mean
              << " | " << seen.percentile(0.5) << " | ";
    if (proven.changes && proven.bound) std::cout << std::setprecision(0) << 100.0 * mean / static_cast<double>(proven.bound) << "% | ";
    else std::cout << "- | ";
    std::cout << std::setprecision(0) << 100.0 * static_cast<double>(seen.ticks.size()) / static_cast<double>(seen.trials) << "% |\n";
    if (proven.changes && seen.maximum() > proven.bound)
        throw std::runtime_error(label + "." + output + ": the simulator exceeded the proven bound");
    if (!proven.changes && !seen.ticks.empty())
        throw std::runtime_error(label + "." + output + ": changed although proven constant");
}

const char* HEADER =
    " | output | proven | seen max | seen 95% | seen mean | seen median | mean / proven | changes |\n"
    "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n";

void experiment(const std::string& contract, size_t rounds, size_t budget, const std::set<std::string>& controls) {
    const auto& catalog = circuit::builtinComponentCatalog();
    const auto build = [&] {
        return std::dynamic_pointer_cast<IOComponent>(
            catalog.createRoot({contract, "DUT", {}}, circuit::maximallyStructural()).root);
    };
    Bench bench(build());
    const auto& part = *bench.dut;
    SettlingOptions options;
    options.binary_sources = true;
    options.search_budget = budget;
    const auto whole = CircuitTimingAnalyzer::settling(bench.dut, options);
    if (!whole.valid) {
        std::cout << "\n## " << contract << "\n\nNo bound: " << (whole.diagnostics.empty() ? "?" : whole.diagnostics.front()) << "\n";
        return;
    }
    for (const auto& e : whole.endpoints) if (e.name == "(captures)") {
        std::cout << "\n## " << contract << "\n\nClocked part: not driven by this experiment.\n";
        return;
    }
    std::mt19937_64 random(0x7e57);
    const size_t watch = whole.structural + 4;
    const auto endpoint = [](const SettlingReport& report, const std::string& name) -> const SettlingEndpoint& {
        for (const auto& e : report.endpoints) if (e.name == name) return e;
        throw std::runtime_error("No endpoint " + name);
    };
    const auto measure = [&](const SettlingReport& proven, const std::string& label,
                             const std::function<std::pair<Vector, Vector>()>& next) {
        std::map<std::string, Seen> seen;
        for (size_t round = 0; round < rounds; ++round) {
            const auto [before, after] = next();
            const auto changed = bench.change(before, after, watch);
            for (const auto& [name, pin] : part.getAllOutputPins()) {
                const auto it = changed.find(name);
                seen[name].add(it != changed.end(), it == changed.end() ? 0 : it->second);
            }
        }
        for (auto& [name, s] : seen) row(label, name, endpoint(proven, name), s);
    };

    std::cout << "\n## " << contract << "\n\n### Any input changes\n\n| case" << HEADER;
    measure(whole, "all inputs", [&] { return std::pair{randomVector(part, random), randomVector(part, random)}; });

    if (part.getAllInputPins().size() > 1 && part.getAllInputPins().size() <= 8) {
        std::cout << "\n### One input pin changes, the others hold\n\n| changing pin" << HEADER;
        for (const auto& [changing, pin] : part.getAllInputPins()) {
            auto one = options;
            for (const auto& [name, other] : part.getAllInputPins()) if (name != changing) one.held_inputs.push_back(name);
            const auto proven = CircuitTimingAnalyzer::settling(bench.dut, one);
            if (!proven.valid) throw std::runtime_error(contract + ": pin analysis rejected");
            measure(proven, "`" + changing + "`", [&] {
                auto before = randomVector(part, random);
                auto after = before;
                after[changing] = randomVector(part, random)[changing];
                return std::pair{before, after};
            });
        }
    }
    for (const auto& control : controls) {
        const auto pin = part.getInputPinDynamic(control);
        if (!pin || pin->getWidth() > 6) continue;
        std::cout << "\n### By the value `" << control << "` ends at\n\n| `" << control << "`" << HEADER;
        for (uint64_t value = 0; value < (uint64_t{1} << pin->getWidth()); ++value) {
            auto fixed = options;
            fixed.final_inputs[control] = value;
            const auto proven = CircuitTimingAnalyzer::settling(bench.dut, fixed);
            if (!proven.valid) throw std::runtime_error(contract + ": control analysis rejected");
            measure(proven, std::to_string(value), [&] {
                return std::pair{randomVector(part, random), randomVector(part, random, {{control, value}})};
            });
        }
    }
}
// Side by side: the compact and the gate-level form of one part get the same
// single-pin input changes, each from a settled start. Reports how often the
// compact form's last output change came before the gates', and by how much.
// The rule is that it never does. Clock and reset are ordinary pins here, so
// clocked parts are exercised too.
void compare(const std::string& contract, size_t steps, size_t watch, bool four_state) {
    const auto& catalog = circuit::builtinComponentCatalog();
    std::shared_ptr<IOComponent> gate_form, compact_form;
    try {
        gate_form = std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({contract, "DUT", {}}, circuit::maximallyStructural()).root);
        compact_form = std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({contract, "DUT", {}}, circuit::strictAllBehavioral()).root);
    } catch (const std::exception&) { return; }
    if (!gate_form || !compact_form || gate_form->getAllInputPins().empty() || gate_form->getAllOutputPins().empty()) return;
    if (std::dynamic_pointer_cast<BasicComponent>(gate_form)) return; // one primitive in both forms
    const auto leaf = std::dynamic_pointer_cast<BasicComponent>(compact_form);
    Bench gates(gate_form), compact(compact_form);
    std::mt19937_64 random(0xc0de);
    Vector now;
    for (const auto& [name, pin] : gate_form->getAllInputPins()) now[name] = Bits(pin->getWidth(), LogicValue::LOW);
    const auto both = [&](const Vector& before, const Vector& after) {
        return std::pair{gates.change(before, after, watch), compact.change(before, after, watch)};
    };
    size_t unsettled = 0;
    const auto step = [&](const Vector& next) {
        std::pair<std::map<std::string, size_t>, std::map<std::string, size_t>> result;
        try { result = both(now, next); } catch (const std::exception&) { ++unsettled; }
        now = next;
        return result;
    };
    // Reset, with one clock pulse, then release.
    const bool has_reset = now.contains("RST"), has_clock = now.contains("CLK");
    auto next = now;
    if (has_reset) { next["RST"][0] = LogicValue::HIGH; step(next); }
    if (has_clock) { next["CLK"][0] = LogicValue::HIGH; step(next); next["CLK"][0] = LogicValue::LOW; step(next); }
    if (has_reset) { next["RST"][0] = LogicValue::LOW; step(next); }

    std::vector<std::string> pins;
    for (const auto& [name, bits] : now) if (name != "RST") pins.push_back(name);
    size_t changes = 0, early = 0, worst_early = 0, worst_late = 0, differing = 0;
    std::string worst_output;
    for (size_t i = 0; i < steps && unsettled < 5; ++i) {
        next = now;
        // The clock ticks half of the time, so clocked parts see many edges.
        const auto& pin = has_clock && i % 2 ? std::string("CLK") : pins[random() % pins.size()];
        // With --four-state a data pin may also go to X or Z. The clock stays 0/1.
        const unsigned values = four_state && pin != "CLK" ? 4 : 2;
        for (auto& bit : next[pin]) {
            if (values == 2 && next[pin].size() == 1) bit = bit == LogicValue::LOW ? LogicValue::HIGH : LogicValue::LOW;
            else bit = LogicValue(random() % values);
        }
        const auto [real, modeled] = step(next);
        for (const auto& [name, wire] : gates.outputs)
            differing += wire->getValueVector() != compact.outputs.at(name)->getValueVector();
        for (const auto& [name, tick] : real) {
            const auto shown = modeled.find(name);
            if (shown == modeled.end()) continue; // the gates glitched and came back
            ++changes;
            if (shown->second < tick) {
                ++early;
                if (tick - shown->second > worst_early) { worst_early = tick - shown->second; worst_output = name; }
            } else worst_late = std::max(worst_late, shown->second - tick);
        }
    }
    std::cout << "| `" << contract << "` | " << (leaf && leaf->getDelays() ? "table" : leaf ? "single delay" : "several parts") << " | "
              << changes << " | " << early << " | " << worst_early << (worst_early ? " (`" + worst_output + "`)" : "") << " | "
              << worst_late << " | " << differing << " | " << (unsettled ? std::to_string(unsettled) + " not quiet" : "") << " |\n" << std::flush;
}
} // namespace

int main(int argc, char** argv) {
    try {
        size_t rounds = 400, budget = 20000, watch = 200;
        bool comparing = false, four_state = false;
        std::vector<std::string> parts;
        std::map<std::string, std::set<std::string>> controls;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument.rfind("--rounds=", 0) == 0) rounds = std::stoul(argument.substr(9));
            else if (argument.rfind("--budget=", 0) == 0) budget = std::stoul(argument.substr(9));
            else if (argument.rfind("--watch=", 0) == 0) watch = std::stoul(argument.substr(8));
            else if (argument == "--compare") comparing = true;
            else if (argument == "--four-state") four_state = true;
            else if (argument.rfind("--control=", 0) == 0) {
                const auto spec = argument.substr(10);
                const auto colon = spec.rfind(':');
                if (colon == std::string::npos) throw std::invalid_argument("--control=PART:PIN");
                controls[spec.substr(0, colon)].insert(spec.substr(colon + 1));
            } else parts.push_back(argument);
        }
        if (comparing) {
            if (parts.empty()) for (const auto& contract : circuit::builtinComponentCatalog().contracts()) parts.push_back(contract.id);
            std::cout << "# Compact against gate-level form, side by side\n\n" << rounds << " single-pin input changes per part, "
                      << "each from a settled start; outputs watched for " << watch << " ticks.\n\n"
                      << "| part | compact timing | output changes compared | compact earlier | worst: earlier by | worst: later by | value differences | note |\n"
                      << "| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |\n";
            for (const auto& part : parts) compare(part, rounds, watch, four_state);
            return 0;
        }
        if (parts.empty()) throw std::invalid_argument("Name at least one catalog part");
        std::cout << "# Proven delay against real gate-level settling time\n\n"
                  << rounds << " random binary input changes per row, each from a settled start. Ticks.\n"
                  << "`*`: the search did not prove that the bound is reached.\n";
        for (const auto& part : parts) experiment(part, rounds, budget, controls[part]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "delay_estimate_experiment failed: " << error.what() << '\n';
        return 1;
    }
}
