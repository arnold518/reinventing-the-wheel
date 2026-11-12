// Chapter 6 machine checks: guest programs on the accelerator machine, their
// report pages against host references, and the recorded machine scenario.
#include "tests/AcceleratorTests.hpp"
#include "tests/AcceleratorWorkloads.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/TestHelpers.hpp"
#include "runtime/AcceleratorGuest.hpp"
#include "system/ComputerSystem.hpp"
#include "components/BasicComponent.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/FidelityPresets.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <deque>
#include <random>
#include <tuple>

#ifndef CIRCUITSIM_SOURCE_DIR
#define CIRCUITSIM_SOURCE_DIR "."
#endif

namespace circuit::accelerator::test {
namespace {
using circuit::runtime::AcceleratorRunOptions;
using circuit::runtime::AcceleratorRunResult;

constexpr uint32_t REPORT_MAGIC = 0x31434341u;   // "ACC1"

// The matrix-multiply workload's operands, as the guest's xorshift makes them.
std::vector<uint8_t> referenceMatmul() {
    uint32_t seed = 0x2545f491u;
    auto random8 = [&] { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return int8_t(seed >> 8); };
    const size_t shapes[3][3] = {{4, 32, 16}, {8, 64, 32}, {16, 64, 64}};
    std::vector<uint8_t> all;
    for (const auto& s : shapes) {
        Layer l{{"matmul", s[0], s[1], s[2], 1, 8, false}, std::vector<int8_t>(s[0] * s[1]), std::vector<int8_t>(s[1] * s[2]),
                std::vector<int32_t>(s[2])};
        for (auto& v : l.x) v = random8();
        for (auto& v : l.w) v = random8();
        for (auto& v : l.bias) v = int32_t(random8()) * 37;
        const auto y = referenceLayer(l);
        all.insert(all.end(), y.begin(), y.end());
    }
    return all;
}

// The expected int8 logits of the embedded images (tests/fixtures/accelerator/model.json).
std::vector<uint8_t> expectedLogits(const std::string& key, size_t images) {
    std::ifstream in(std::string(CIRCUITSIM_SOURCE_DIR) + "/tests/fixtures/accelerator/model.json");
    require(static_cast<bool>(in), "Cannot read tests/fixtures/accelerator/model.json");
    std::stringstream text;
    text << in.rdbuf();
    const auto json = text.str();
    auto at = json.find("\"" + key + "\"");
    require(at != std::string::npos, "model.json lacks " + key);
    at = json.find('[', at) + 1;
    std::vector<uint8_t> values;
    while (values.size() < images * 10) {
        while (at < json.size() && json[at] != '-' && !std::isdigit(static_cast<unsigned char>(json[at]))) ++at;
        size_t used = 0;
        values.push_back(uint8_t(int8_t(std::stoi(json.substr(at), &used))));
        at += used;
    }
    return values;
}

AcceleratorRunResult runChecked(const std::string& program, AcceleratorRunOptions o, const std::vector<uint8_t>& expected,
                                uint32_t workload) {
    const auto result = circuit::runtime::runAcceleratorGuest(circuit::runtime::acceleratorFixture(program), o);
    require(result.halted && !result.trapped, program + " did not halt cleanly (trap cause " + std::to_string(result.trap_cause) + ")");
    require(result.console == "done\n", program + " console: " + result.console);
    require(result.word(0) == REPORT_MAGIC && result.word(1) == workload && result.word(3) == 0,
            program + " report: status " + std::to_string(result.word(3)));
    const std::vector<uint8_t> got(result.report.begin() + 0x100, result.report.begin() + 0x100 + expected.size());
    require(got == expected, program + " outputs differ from the host reference");
    require(result.setup_violations == 0, program + " observed setup violations");
    std::cout << program << " (" << o.memory << (o.array_multiplier ? ", array multiplier" : "") << "): " << result.cycles
              << " cycles, " << result.instructions << " instructions, " << result.word(4) << " timed sections, "
              << result.run_seconds << " s\n";
    return result;
}

void machine() {
    AcceleratorRunOptions o;
    // A layer on the unit and on the CPU, compared in the guest.
    runChecked("demo-n4", [&] { auto p = o; p.matrix = 4; return p; }(), {}, 4);
    const auto matmul = referenceMatmul();
    {   // (N = 4 and 8 run in the § 6.6 campaign, which checks the same outputs.)
        auto p = o;
        p.matrix = 16;
        runChecked("matmul-n16", p, matmul, 1);
    }
    auto p = o;
    p.matrix = 16;
    runChecked("mlp-n16", p, expectedLogits("expected_mlp", 2), 2);
    runChecked("cnn-n16", p, expectedLogits("expected_cnn", 2), 3);
    // The CPU alone with the array multiplier, on the smallest workload's machine.
    auto cpu = o;
    cpu.matrix = 0;
    cpu.array_multiplier = true;
    runChecked("demo-cpu", cpu, {}, 4);
}

// Two requesters on the memory subsystem's fetch (I_) and data (D_) ports,
// each offering its next request as soon as its last one was accepted (so a
// request can wait while the previous answer is on its way). Every answer is
// checked against a host copy of memory, in order per port. Returns the
// cycles taken.
uint64_t traffic(const std::string& technology, bool pipelined, bool both_ports, unsigned requests, uint64_t seed,
                 bool hesitant = false) {
    ParameterMap p{{"technology", technology}, {"words", "1024"}, {"caches", "none"}};
    if (technology != "ff") p["clock_period"] = "1000";
    if (pipelined) p["pipelined"] = "1";
    Rig r("memory.subsystem.32", true, p);
    std::mt19937_64 rng(seed);
    std::vector<uint32_t> memory(1024, 0);
    struct Port {
        std::string prefix;
        unsigned left = 0;
        bool offering = false;
        uint32_t address = 0, data = 0;
        bool write = false;
        std::deque<std::pair<bool, uint32_t>> expected;   // (is read, value)
    };
    std::vector<Port> ports{{"I_"}, {"D_"}};
    // Memory starts unknown: first write every word of the tested range
    // through the data port (not counted), then the measured traffic.
    bool warmup = true;
    uint32_t fill = 0;
    ports[1].left = 256;
    uint64_t cycles = 0, measured_from = 0;
    while (warmup || std::any_of(ports.begin(), ports.end(), [](const Port& q) { return q.left || q.offering || !q.expected.empty(); })) {
        if (warmup && !ports[1].left && !ports[1].offering && ports[1].expected.empty()) {
            warmup = false;
            measured_from = cycles;
            ports[0].left = requests;
            ports[1].left = both_ports ? requests : 0;
            continue;
        }
        require(cycles < 20 * uint64_t(requests) + 100000, "Memory traffic did not finish (" + technology +
                (pipelined ? ", pipelined" : ", plain") + (hesitant ? ", hesitant requesters" : "") + ", cycle " +
                std::to_string(cycles) + ")");
        std::vector<bool> ready(2, true);
        std::map<std::string, uint64_t> in;
        for (size_t k = 0; k < 2; ++k) {
            auto& q = ports[k];
            if (hesitant && !warmup) ready[k] = rng() % 3 != 0;
            if (!q.offering && q.left) {
                q.offering = true;
                --q.left;
                if (warmup) { q.address = 4 * fill++; q.write = true; q.data = uint32_t(rng()); }
                else {
                    q.address = uint32_t(rng() % 256) * 4;
                    q.write = q.prefix == "D_" && rng() % 3 == 0;
                    q.data = uint32_t(rng());
                }
            }
            in[q.prefix + "REQ_VALID"] = q.offering;
            in[q.prefix + "REQ_OP"] = q.write;
            in[q.prefix + "REQ_ADDR"] = q.address;
            in[q.prefix + "REQ_WRITE_DATA"] = q.data;
            in[q.prefix + "REQ_WRITE_MASK"] = 15;
            in[q.prefix + "RSP_READY"] = ready[k];
        }
        r.drive(in);
        std::vector<bool> accepted(2), answered(2);
        std::vector<uint32_t> answer(2), status(2);
        for (size_t k = 0; k < 2; ++k) {
            auto& q = ports[k];
            accepted[k] = q.offering && r.out(q.prefix + "REQ_READY");
            answered[k] = ready[k] && r.out(q.prefix + "RSP_VALID") != 0;
            if (answered[k]) {
                status[k] = uint32_t(r.out(q.prefix + "RSP_STATUS"));
                // A write's answer carries no data (its lines may be unknown).
                // (A zero-wait memory answers the request it accepts in this same cycle.)
                const bool for_read = q.expected.empty() ? !q.write : q.expected.front().first;
                if (for_read) answer[k] = uint32_t(r.out(q.prefix + "RSP_READ_DATA"));
            }
        }
        r.cycle();
        ++cycles;
        for (size_t k = 0; k < 2; ++k) {
            auto& q = ports[k];
            if (accepted[k]) {
                q.offering = false;
                if (q.write) { memory[q.address / 4] = q.data; q.expected.push_back({false, 0}); }
                else q.expected.push_back({true, memory[q.address / 4]});
            }
            if (answered[k]) {
                require(!q.expected.empty(), q.prefix + " answer without a request");
                const auto [read, value] = q.expected.front();
                q.expected.pop_front();
                require(status[k] == 0, q.prefix + " answer status " + std::to_string(status[k]));
                if (read) require(answer[k] == value, q.prefix + " read the wrong word");
            }
        }
    }
    return cycles - measured_from;
}

void pipelinedBus() {
    for (const std::string technology : {"ff", "sram", "dram"}) {
        for (bool both : {false, true}) {
            const auto before = traffic(technology, false, both, 64, 7);
            const auto after = traffic(technology, true, both, 64, 7);
            std::cout << "memory subsystem (" << technology << (both ? ", both ports" : ", fetch port") << "): " << (both ? 128 : 64)
                      << " accesses, " << before << " cycles plain, " << after << " cycles pipelined\n";
            require(after <= before, "The pipelined arbiter was slower");
        }
        // Requesters that refuse answers in a random third of the cycles (a
        // core stalled on its other port), over many accesses.
        for (bool pipelined : {false, true}) {
            const auto cycles = traffic(technology, pipelined, true, 3000, 11, true);
            std::cout << "memory subsystem (" << technology << ", both ports, hesitant requesters): 6000 accesses, " << cycles
                      << " cycles " << (pipelined ? "pipelined" : "plain") << "\n";
        }
    }
    // Whole programs on the machine with the pipelined bus and fetch: the same
    // results, fewer cycles.
    for (const auto& [program, matrix, workload] : std::vector<std::tuple<std::string, size_t, uint32_t>>{
             {"demo-cpu", 0, 4}, {"demo-n4", 4, 4}}) {
        AcceleratorRunOptions o;
        o.matrix = matrix;
        const auto expected = workload == 1 ? referenceMatmul() : std::vector<uint8_t>{};
        const auto plain = runChecked(program, o, expected, workload);
        o.pipelined_bus = true;
        const auto fast = runChecked(program, o, expected, workload);
        std::cout << program << ": " << plain.cycles << " cycles plain bus, " << fast.cycles << " pipelined (" << fast.instructions
                  << " instructions)\n";
        require(fast.instructions == plain.instructions, program + ": the pipelined bus changed the instruction count");
        require(fast.cycles < plain.cycles, program + ": the pipelined bus was not faster");
    }
}

// The machine running the demo program, recorded for the visualizer.
class MachineScenario : public SimulationTest {
public:
    MachineScenario() : profile_(fidelityPreset("compact", builtinComponentCatalog())) {}
    std::string getTestName() const override { return "AcceleratorMachineScenarioTest"; }
    bool run() override {
        try {
            preparePrecomputedSimulation();
            std::cout << "[PASS] AcceleratorMachineScenarioTest: " << cycles_ << " cycles\n";
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[FAIL] AcceleratorMachineScenarioTest: " << e.what() << '\n';
            return false;
        }
    }
    void setupCircuit() override {
        if (root) return;
        options_.matrix = 4;
        options_.profile = profile_;
        root = circuit::runtime::buildAcceleratorMachine(options_, "ACCELERATOR_MACHINE_ROOT");
    }
    void preparePrecomputedSimulation() override {
        if (precomputed_) return;
        setupCircuit();
        auto machine = std::dynamic_pointer_cast<circuit::system::ComputerSystem>(root);
        auto o = options_;
        o.on_event = [this](SimulationTime t, const std::string& label, const std::string& detail) {
            checkpoints_.push_back({t, label, detail, checkpoints_.size()});
        };
        const auto result = circuit::runtime::runAcceleratorGuestOn(machine, *sim, circuit::runtime::acceleratorFixture("demo-n4"), o);
        require(result.halted && result.console == "done\n" && result.word(3) == 0, "Demo program failed on the machine");
        duration_ = result.end_time + 1;
        cycles_ = result.cycles;
        precomputed_ = true;
    }
    bool isSimulationPrecomputed() const override { return precomputed_; }
    SimulationTime getRunDuration() const override { return duration_; }
    std::vector<SimulationCheckpoint> getCheckpoints() const override { return checkpoints_; }
    bool supportsBuildProfile() const override { return true; }
    std::map<std::string, std::string> unavailableFidelityPresets() const override {
        auto result = fidelityPresetRestrictions("system.computer");
        result["gate"] = "Gate is unavailable for a computer system: its main memory exceeds the structural storage limit.";
        return result;
    }
    circuit::BuildProfile getBuildProfile() const override { return profile_; }
    void setBuildProfile(circuit::BuildProfile profile) override {
        profile_ = std::move(profile);
        root.reset();
        precomputed_ = false;
        checkpoints_.clear();
    }
protected:
    void buildCircuit() override {}
    void setInitialState() override {}
    void verifyResults() override {}
private:
    circuit::BuildProfile profile_;
    AcceleratorRunOptions options_;
    std::vector<SimulationCheckpoint> checkpoints_;
    SimulationTime duration_ = 0;
    uint64_t cycles_ = 0;
    bool precomputed_ = false;
};

class CheckCase : public StandaloneVerificationTest {
    std::string name_;
    std::function<void()> check_;
public:
    CheckCase(std::string n, std::function<void()> f) : name_(std::move(n)), check_(std::move(f)) {}
    std::string getTestName() const override { return name_; }
    void verifyResults() override { check_(); }
};
} // namespace

void appendMachineTests(std::vector<TestRegistryEntry>& entries) {
    entries.push_back({"PipelinedBusTest", "PipelinedBusTest", RegisteredTestKind::Integration, {}, {},
        {"simulation", "accelerator"}, [] { return std::make_unique<CheckCase>("PipelinedBusTest", pipelinedBus); }, false});
    entries.push_back({"AcceleratorMachineTest", "AcceleratorMachineTest", RegisteredTestKind::Integration, {}, {},
        {"simulation", "accelerator", "slow"}, [] { return std::make_unique<CheckCase>("AcceleratorMachineTest", machine); }, false});
    // A scenario of the machine's one logical test. It names no contract:
    // system.computer belongs to ComputerSystemTest, and a contract has one owner.
    entries.push_back({"AcceleratorMachineScenarioTest", "AcceleratorMachineTest", RegisteredTestKind::Program, {}, "accelerator-demo",
        {"simulation", "accelerator", "system-scenario"}, [] { return std::make_unique<MachineScenario>(); }, true});
}

} // namespace circuit::accelerator::test
