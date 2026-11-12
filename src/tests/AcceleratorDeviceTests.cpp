// Chapter 6 device checks: MatrixUnit<N> programmed through its registers with
// a host memory answering its DMA port, against an independent integer model.
#include "tests/AcceleratorTests.hpp"
#include "tests/AcceleratorWorkloads.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/TestHelpers.hpp"
#include "tests/ComponentTestModel.hpp"
#include "modules/accelerator/MatrixUnit.hpp"
#include "modules/accelerator/MatrixParts.hpp"
#include "components/BasicComponent.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <random>

namespace circuit::accelerator::test {
namespace {

constexpr uint32_t COMMAND = 0x0, ARG0 = 0x4, STATUS = 0x14, DONE = 0x18, CLEAR = 0x1c;

// The device with a memory on its DMA port: a request is accepted when no
// answer is waiting and answered on the next edge (status 2 outside memory).
class DeviceHarness {
public:
    Rig rig;
    std::vector<uint8_t> memory;
    uint64_t cycles = 0, requests = 0;
    // For the recorded scenario: every input change, and checkpoints whose
    // expected outputs the scenario states (a label and the outputs).
    struct Entry { std::map<std::string, uint64_t> inputs; std::string label; std::map<std::string, uint64_t> expected; };
    std::vector<Entry>* log = nullptr;
    // Label for the answer of the next register access ("" for none).
    std::string answer_label;
    // The k-th byte write on the DMA port, by its expected address and value.
    std::function<std::map<std::string, uint64_t>(size_t k)> store_expected;
    size_t stores = 0;
    DeviceHarness(size_t n, bool compact, ParameterMap parameters, size_t bytes)
        : rig(circuit::families::matrixUnit(n), compact, std::move(parameters)), memory(bytes, 0) {}
    void drive(const std::map<std::string, uint64_t>& values) {
        if (log) log->push_back({values, {}, {}});
        rig.drive(values);
    }
    void mark(std::string label, std::map<std::string, uint64_t> expected) {
        if (log) log->push_back({{}, std::move(label), std::move(expected)});
    }
    void clockEdge() {
        if (log) for (uint64_t v : {0, 1, 0}) log->push_back({{{"CLK", v}}, {}, {}});
        rig.cycle();
    }
    void cycle() {
        drive({{"DMA_REQ_READY", !pending_}, {"DMA_RSP_VALID", pending_}, {"DMA_RSP_READ_DATA", data_}, {"DMA_RSP_STATUS", status_}});
        const bool request = !pending_ && rig.out("DMA_REQ_VALID");
        const bool answered = pending_ && rig.out("DMA_RSP_READY");
        uint32_t address = 0, write = 0, mask = 0, op = 0;
        if (request) {
            op = uint32_t(rig.out("DMA_REQ_OP"));
            address = uint32_t(rig.out("DMA_REQ_ADDR"));
            if (op == 1) { write = uint32_t(rig.out("DMA_REQ_WRITE_DATA")); mask = uint32_t(rig.out("DMA_REQ_WRITE_MASK")); }
            if (op == 1 && store_expected) { mark("store-byte-" + std::to_string(stores), store_expected(stores)); ++stores; }
        }
        clockEdge();
        ++cycles;
        if (answered) pending_ = false;
        if (request) {
            ++requests;
            pending_ = true;
            status_ = 0; data_ = 0;
            if (address + 4 > memory.size() || address % 4) status_ = 2;
            else if (op == 1) { for (int k = 0; k < 4; ++k) if (mask >> k & 1) memory[address + k] = uint8_t(write >> (8 * k)); }
            else for (int k = 0; k < 4; ++k) data_ |= uint32_t(memory[address + k]) << (8 * k);
        }
    }
    // One register access through the bus port; returns (status, data).
    std::pair<uint32_t, uint32_t> access(bool write, uint32_t offset, uint32_t value = 0) {
        drive({{"REQ_VALID", 1}, {"REQ_OP", write}, {"REQ_ADDR", offset}, {"REQ_WRITE_DATA", value}, {"REQ_WRITE_MASK", 15}, {"RSP_READY", 1}});
        bool accepted = false;
        // A write waits at the port while the command queue is full: allow a long command.
        for (int i = 0; i < 200000; ++i) {
            const bool take = !accepted && rig.out("REQ_READY");
            const bool answer = rig.out("RSP_VALID");
            std::pair<uint32_t, uint32_t> result{0, 0};
            if (answer) result = {uint32_t(rig.out("RSP_STATUS")), uint32_t(rig.out("RSP_READ_DATA"))};
            if (answer && (accepted || take) && !answer_label.empty()) {
                // A write's answer carries status only; a read's also its data.
                std::map<std::string, uint64_t> expected{{"RSP_VALID", 1}, {"RSP_STATUS", 0}};
                if (!write) expected["RSP_READ_DATA"] = answer_value;
                mark(answer_label, expected);
                answer_label.clear();
            }
            cycle();
            if (take) { accepted = true; drive({{"REQ_VALID", 0}}); }
            if (answer && (accepted || take)) { drive({{"RSP_READY", 0}}); return result; }
        }
        throw std::runtime_error("MatrixUnit register access timed out");
    }
    void command(MatrixCommand op, std::array<uint32_t, 4> args) {
        static const char* names[] = {"", "load", "store", "shift-weights", "matmul", "activate"};
        for (int k = 0; k < 4; ++k) require(access(true, ARG0 + 4 * k, args[k]).first == 0, "ARG write refused");
        if (log) answer_label = "queue-" + std::to_string(queued + 1) + "-" + names[uint32_t(op) <= 5 ? uint32_t(op) : 0];
        require(access(true, COMMAND, uint32_t(op)).first == 0, "COMMAND write refused");
        ++queued;
    }
    // The value the next labelled read must return.
    uint32_t answer_value = 0;
    // Waits until every queued command has finished.
    // Waits until every queued command has finished; a stall of 20,000 cycles
    // without a finished command fails with the unit's visible state.
    void finish() {
        uint32_t done = access(false, DONE).second;
        uint64_t last_progress = cycles;
        while (done < queued) {
            for (int i = 0; i < 16; ++i) cycle();
            const uint32_t now_done = access(false, DONE).second;
            if (now_done != done) { done = now_done; last_progress = cycles; }
            if (cycles - last_progress > 20000)
                throw std::runtime_error("MatrixUnit stalled: " + std::to_string(done) + " of " + std::to_string(queued) +
                    " commands done, STATUS " + std::to_string(access(false, STATUS).second) + ", DMA requests " +
                    std::to_string(requests) + ", DMA_REQ_VALID " + std::to_string(rig.out("DMA_REQ_VALID")) +
                    ", answer pending " + std::to_string(pending_));
        }
    }
    uint32_t queued = 0;
private:
    bool pending_ = false;
    uint32_t data_ = 0, status_ = 0;
};

// Places a workload's operands in memory and runs its command list.
void runProgram(DeviceHarness& d, const AcceleratorProgram& p) {
    for (const auto& [address, bytes] : p.memory)
        std::copy(bytes.begin(), bytes.end(), d.memory.begin() + address);
    for (const auto& c : p.commands) d.command(MatrixCommand(c.op), c.args);
    d.finish();
}
std::vector<uint8_t> readBack(const DeviceHarness& d, uint32_t address, size_t bytes) {
    return {d.memory.begin() + address, d.memory.begin() + address + bytes};
}

void checkLayer(size_t n, bool compact, ParameterMap parameters, const LayerShape& shape, uint64_t seed,
                uint64_t* cycles_out = nullptr) {
    const auto start = std::chrono::steady_clock::now();
    auto layer = randomLayer(shape, seed);
    const auto program = layerProgram(layer, n, 0x1000);
    DeviceHarness d(n, compact, parameters, 0x10000);
    runProgram(d, program);
    require((d.access(false, STATUS).second & 2) == 0, "MatrixUnit reported an error on a valid program");
    const auto got = readBack(d, program.output_address, program.output_bytes);
    const auto expected = referenceLayer(layer);
    require(got == expected, "MatrixUnit<" + std::to_string(n) + "> layer " + shape.name + (compact ? " (compact)" : " (gates)") +
            " differs from the reference");
    if (cycles_out) *cycles_out = d.cycles;
    std::cout << "MatrixUnit<" << n << "> " << (compact ? "compact" : "gate level") << " " << shape.name << ": "
              << program.commands.size() << " commands, " << d.cycles << " cycles, " << d.requests << " DMA requests, "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s\n";
}

void checkConvolution(size_t n, bool compact, ParameterMap parameters, const ConvShape& shape, uint64_t seed,
                      uint64_t* cycles_out = nullptr) {
    const auto start = std::chrono::steady_clock::now();
    auto conv = randomConvolution(shape, seed);
    const auto program = convolutionProgram(conv, n, 0x1000);
    DeviceHarness d(n, compact, parameters, 0x10000);
    runProgram(d, program);
    require((d.access(false, STATUS).second & 2) == 0, "MatrixUnit reported an error on a valid convolution");
    const auto got = readBack(d, program.output_address, program.output_bytes);
    require(got == referenceConvolution(conv), "MatrixUnit<" + std::to_string(n) + "> convolution " + shape.name +
            (compact ? " (compact)" : " (gates)") + " differs from the reference");
    if (cycles_out) *cycles_out = d.cycles;
    std::cout << "MatrixUnit<" << n << "> " << (compact ? "compact" : "gate level") << " convolution " << shape.name << ": "
              << program.commands.size() << " commands, " << d.cycles << " cycles, "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s\n";
}

void checkErrors(size_t n, bool compact, ParameterMap parameters) {
    DeviceHarness d(n, compact, parameters, 0x1000);
    require(d.access(false, 0x20).second == n, "SIZE register");
    // An unknown command finishes at once with the error flag.
    require(d.access(true, COMMAND, 9).first == 0, "Unknown command refused at the port");
    d.queued = 1;
    d.finish();
    require((d.access(false, STATUS).second & 2) == 2, "Unknown command did not set ERROR");
    require(d.access(true, CLEAR, 1).first == 0 && (d.access(false, STATUS).second & 2) == 0, "CLEAR did not clear ERROR");
    // A LOAD outside memory: the DMA answer's error ends the command.
    d.command(MatrixCommand::Load, {0x8000, 1u << 16, (1u << 24) | (4u << 16), 0});
    d.finish();
    require((d.access(false, STATUS).second & 2) == 2, "A DMA error did not set ERROR");
    d.access(true, CLEAR, 1);
    // Zero counts finish without moving data.
    d.command(MatrixCommand::Load, {0, 0, (1u << 24) | (4u << 16), 0});
    d.command(MatrixCommand::MatMul, {0, 0, 0, 0});
    d.command(MatrixCommand::Activate, {0, 0, 0, 0});
    d.finish();
    require((d.access(false, STATUS).second & 3) == 0, "Empty commands left the unit busy or failed");
    // A value too wide for COMMAND is refused by the register front end.
    require(d.access(true, COMMAND, 16).first == 6, "A 5-bit command was accepted");
    std::cout << "MatrixUnit<" << n << "> " << (compact ? "compact" : "gate level") << ": errors, CLEAR and empty commands\n";
}

void matrixUnit() {
    const ParameterMap small{{"act_rows", "64"}, {"acc_rows", "16"}};
    const LayerShape dense{"dense-5x8-to-3", 5, 8, 3, 1, 7, true}, wide{"dense-3x12-to-6", 3, 12, 6, 2, 3, false};
    const ConvShape conv{"6x5x2-to-3", 6, 5, 2, 3, 1, 4, true};
    // Compact first (fast), then the same programs on the gates: identical results and cycles.
    uint64_t compact_dense = 0, compact_conv = 0, gates = 0;
    checkLayer(4, true, small, dense, 11, &compact_dense);
    checkLayer(4, true, small, wide, 12);
    checkConvolution(4, true, small, conv, 5, &compact_conv);
    checkErrors(4, true, small);
    (void)compact_conv;
    // The gates are slow (several seconds per cycle with four requantizers): one
    // small layer on 8-row buffers, against the compact unit's cycles.
    const ParameterMap tiny{{"act_rows", "8"}, {"acc_rows", "4"}};
    uint64_t tiny_compact = 0;
    const LayerShape small_layer{"dense-2x4-to-3", 2, 4, 3, 1, 5, true};
    checkLayer(4, true, tiny, small_layer, 3, &tiny_compact);
    checkLayer(4, false, tiny, small_layer, 3, &gates);
    require(gates == tiny_compact, "Gate-level and compact MatrixUnit<4> took different cycles");
    // Larger arrays, compact.
    const ParameterMap big{{"act_rows", "512"}, {"acc_rows", "64"}};
    for (size_t n : {8u, 16u}) {
        checkLayer(n, true, big, {"dense-9x40-to-" + std::to_string(n - 1), 9, 40, n - 1, 3, 9, true}, 21 + n);
        checkConvolution(n, true, big, {"8x7x3-to-" + std::to_string(n / 2), 8, 7, 3, n / 2, 2, 6, true}, 7 + n);
        checkErrors(n, true, big);
    }
}

// The recorded device scenario: a 2 × 4 → 3 layer on MatrixUnit<4>. The input
// sequence is the harness's. Checkpoints: each command accepted, each byte
// STORE writes (address, lane and value from the integer reference layer),
// then DONE = the number of commands and STATUS = idle without error.
circuit::test::ComponentTestSpec unitScenario() {
    const ParameterMap small{{"act_rows", "8"}, {"acc_rows", "4"}};
    std::vector<DeviceHarness::Entry> log;
    // Shift 10 keeps the sums (up to about ±75,000) mostly inside a byte.
    auto layer = randomLayer({"scenario", 2, 4, 3, 1, 10, false}, 3);
    const auto program = layerProgram(layer, 4, 0x100);
    const auto expected_bytes = referenceLayer(layer);
    DeviceHarness d(4, true, small, 0x400);
    d.log = &log;
    d.store_expected = [&](size_t k) -> std::map<std::string, uint64_t> {
        require(k < expected_bytes.size(), "More STORE writes than output bytes");
        const uint32_t address = program.output_address + uint32_t(k);
        return {{"DMA_REQ_VALID", 1}, {"DMA_REQ_OP", 1}, {"DMA_REQ_ADDR", address & ~3u},
                {"DMA_REQ_WRITE_MASK", 1u << (address & 3)}, {"DMA_REQ_WRITE_DATA", 0x01010101u * expected_bytes[k]}};
    };
    runProgram(d, program);
    require(d.stores == expected_bytes.size(), "STORE wrote a different number of bytes");
    d.answer_label = "done-" + std::to_string(d.queued);
    d.answer_value = d.queued;
    require(d.access(false, DONE).second == d.queued, "DONE differs from the commands queued");
    d.answer_label = "status-idle";
    d.answer_value = 0;
    require(d.access(false, STATUS).second == 0, "The unit is not idle without error");
    require(readBack(d, program.output_address, program.output_bytes) == expected_bytes, "Scenario run differs from the reference");
    circuit::test::ComponentTestSpec spec{"MatrixUnitScenarioTest", "device.matrix-unit.4", "MATRIX_UNIT_4_ROOT", small,
                                          {{"contract", small, {}, {}}}};
    std::map<std::string, size_t> widths;
    circuit::test::NamedValues state;
    for (const auto& pin : builtinComponentCatalog().contract("device.matrix-unit.4").pinsFor(small)) {
        widths[pin.name] = pin.width;
        if (pin.direction == PinType::INPUT) state[pin.name] = circuit::test::logicBits(pin.width, pin.name == "RST");
    }
    auto bits = [&](const std::map<std::string, uint64_t>& values) {
        circuit::test::NamedValues out;
        for (const auto& [name, value] : values) out[name] = circuit::test::logicBits(widths.at(name), value);
        return out;
    };
    auto& actions = spec.scenarios[0].actions;
    actions.push_back({"reset", circuit::test::CheckpointKind::Settled, state, {}, true, {}});
    for (auto v : {0, 1, 0}) { state["CLK"] = circuit::test::logicBits(1, v); actions.push_back({"", circuit::test::CheckpointKind::Settled, state, {}, false, {}}); }
    state["RST"] = circuit::test::logicBits(1, 0);
    actions.push_back({"ready", circuit::test::CheckpointKind::Settled, state, bits({{"DMA_REQ_VALID", 0}, {"RSP_VALID", 0}}), true, {}});
    for (const auto& entry : log) {
        for (const auto& [name, value] : entry.inputs) state[name] = circuit::test::logicBits(widths.at(name), value);
        const bool emit = !entry.label.empty();
        actions.push_back({entry.label, circuit::test::CheckpointKind::Settled, state, emit ? bits(entry.expected) : circuit::test::NamedValues{}, emit, {}});
    }
    return spec;
}

class CheckCase : public StandaloneVerificationTest {
    std::string name_;
    std::function<void()> check_;
public:
    CheckCase(std::string n, std::function<void()> f) : name_(std::move(n)), check_(std::move(f)) {}
    std::string getTestName() const override { return name_; }
    void verifyResults() override {
        BasicComponent::resetSetupViolations();
        check_();
        require(BasicComponent::setupViolations() == 0, "Device check observed setup violations");
    }
};
} // namespace

void appendDeviceTests(std::vector<TestRegistryEntry>& entries) {
    entries.push_back({"MatrixUnitTest", "MatrixUnitTest", RegisteredTestKind::Integration, {}, {}, {"simulation", "accelerator"},
        [] { return std::make_unique<CheckCase>("MatrixUnitTest", matrixUnit); }, false});
    entries.push_back({"MatrixUnitScenarioTest", "MatrixUnitTest", RegisteredTestKind::Sequential, {"device.matrix-unit.4"},
        "contract", {"simulation", "accelerator-scenario"},
        [] { return std::make_unique<circuit::test::ComponentScenarioTest>(unitScenario(), "contract", compactProfile()); }, true});
    appendMachineTests(entries);
}

} // namespace circuit::accelerator::test
