#include "runtime/AcceleratorGuest.hpp"
#include "system/ComputerSystem.hpp"
#include "system/SystemDescription.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include <cstdio>
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/FidelityPresets.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include "simulator/SimulationTest.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include <chrono>
#include <map>
#include <stdexcept>
#include "AcceleratorPrograms.generated.hpp"

namespace circuit::runtime {
namespace {
using Clock = std::chrono::steady_clock;
void demand(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
std::vector<LogicValue> bits(size_t width, uint64_t value) {
    std::vector<LogicValue> r(width, LogicValue::LOW);
    for (size_t i = 0; i < width && i < 64; ++i) if ((value >> i) & 1) r[i] = LogicValue::HIGH;
    return r;
}
void replace(std::string& text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    demand(at != std::string::npos, "Accelerator machine description lacks: " + from);
    text.replace(at, from.size(), to);
}
uint64_t value(const std::shared_ptr<PinBase>& pin) {
    for (auto bit : pin->getValueAsVector()) if (bit != LogicValue::LOW && bit != LogicValue::HIGH) return ~uint64_t{0};
    return pin->getValueAsUInt64();
}
// The handshake and address pins of the parts near the top of the machine:
// enough to see which request or answer a stalled machine waits on.
void dumpHandshakes(const std::shared_ptr<Component>& c, const std::string& path, int depth, std::string& out) {
    static const char* keys[] = {"REQ_", "RSP_", "BUSY", "READY", "VALID", "ADDR", "PC", "STATE", "DONE", "START"};
    if (auto io = std::dynamic_pointer_cast<IOComponent>(c)) {
        for (const auto* pins : {&io->getAllInputPins(), &io->getAllOutputPins()})
            for (const auto& [n, p] : *pins) {
                bool wanted = false;
                for (const char* k : keys) wanted = wanted || n.find(k) != std::string::npos;
                if (!wanted || p->getWidth() > 32 || n.find("DATA") != std::string::npos) continue;
                const uint64_t v = value(p);
                char text[32];
                if (v == ~uint64_t{0}) snprintf(text, sizeof text, "X");
                else snprintf(text, sizeof text, "0x%llx", static_cast<unsigned long long>(v));
                out += path + "." + n + " = " + text + "\n";
            }
    }
    if (depth == 0) return;
    for (const auto& child : c->getChildren()) dumpHandshakes(child, path + "/" + child->getName(), depth - 1, out);
}
}

uint32_t AcceleratorRunResult::word(size_t index) const {
    demand(report.size() >= 4 * index + 4, "Report shorter than requested");
    uint32_t w = 0;
    for (int k = 0; k < 4; ++k) w |= uint32_t(report[4 * index + k]) << (8 * k);
    return w;
}

std::string acceleratorMachineDescription(const AcceleratorRunOptions& o) {
    std::string text = system::builtinSystemJson(o.matrix ? "accelerator-machine" : "accelerator-machine-cpu");
    if (o.matrix) {
        demand(o.matrix == 4 || o.matrix == 8 || o.matrix == 16, "Matrix unit size must be 4, 8 or 16");
        replace(text, "\"device.matrix-unit.16\"", "\"device.matrix-unit." + std::to_string(o.matrix) + "\"");
    }
    demand(o.memory == "ff" || o.memory == "sram" || o.memory == "dram", "Memory must be ff, sram or dram");
    std::string memory = "\"technology\": \"" + o.memory + "\", \"words\": \"65536\", \"caches\": \"none\"";
    if (o.memory != "ff") memory += ", \"clock_period\": \"" + std::to_string(o.period) + "\"";
    if (o.pipelined_bus) memory += ", \"pipelined\": \"1\"";
    demand(!o.dram_retention || o.memory == "dram", "A retention is for dram only");
    if (o.dram_retention) memory += ", \"retention_ticks\": \"" + std::to_string(o.dram_retention) + "\"";
    replace(text, "\"technology\": \"sram\", \"words\": \"65536\", \"caches\": \"none\"", memory);
    std::string core;
    if (o.array_multiplier) core += "\"multiplier\": \"array\"";
    if (o.pipelined_bus) core += std::string(core.empty() ? "" : ", ") + "\"overlapped_fetch\": \"1\"";
    if (!core.empty())
        replace(text, "{\"name\": \"CORE\", \"family\": \"rv32im.core.educational-five-stage\"}",
                "{\"name\": \"CORE\", \"family\": \"rv32im.core.educational-five-stage\", \"parameters\": {" + core + "}}");
    if (o.pipelined_bus) replace(text, "{\"name\": \"BUS\", \"width\": 32,", "{\"name\": \"BUS\", \"width\": 32, \"pipelined\": true,");
    return text;
}

std::shared_ptr<system::ComputerSystem> buildAcceleratorMachine(const AcceleratorRunOptions& o, const std::string& root) {
    const auto profile = o.profile ? *o.profile : o.fidelity.empty()
        ? BuildProfileBuilder("accelerator-machine-compact").addRule(preferFidelity(Fidelity::Behavioral, ProfileSelector::any()))
              .unavailablePolicy(UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build()
        : fidelityPreset(o.fidelity, builtinComponentCatalog());
    auto built = builtinComponentCatalog().createRoot({"system.computer", root, {{"description", acceleratorMachineDescription(o)}}}, profile);
    auto machine = std::dynamic_pointer_cast<system::ComputerSystem>(built.root);
    demand(machine != nullptr, "system.computer did not build a computer system");
    return machine;
}

AcceleratorRunResult runAcceleratorGuest(const std::vector<uint8_t>& elf, const AcceleratorRunOptions& o) {
    const auto start = Clock::now();
    auto machine = buildAcceleratorMachine(o);
    machine->setMemoryHistoryRecordingEnabled(false);
    Simulator sim;
    sim.setHistoryRecordingEnabled(false);
    auto result = runAcceleratorGuestOn(machine, sim, elf, o);
    result.setup_seconds += std::chrono::duration<double>(Clock::now() - start).count() - result.run_seconds - result.setup_seconds;
    return result;
}

AcceleratorRunResult runAcceleratorGuestOn(const std::shared_ptr<system::ComputerSystem>& machine, Simulator& sim,
                                           const std::vector<uint8_t>& elf_bytes, const AcceleratorRunOptions& o) {
    const auto setup_start = Clock::now();
    auto elf = rv32i::RV32IElfImage::fromBytes(elf_bytes);
    elf.requireFitsMemory(0x40000);
    demand(elf.entryPoint() == 0, "Guest entry must be zero");
    machine->loadElf(elf);
    ComponentBuilder b(machine);
    std::map<std::string, std::shared_ptr<WireBase>> inputs;
    for (const auto& [n, p] : machine->getAllInputPins()) inputs[n] = b.addNewWireDynamic("HOST_" + n, p->getWidth(), nullptr, {p});
    for (const auto& [n, p] : machine->getAllOutputPins()) b.addNewWireDynamic("HOST_" + n, p->getWidth(), p, {});
    auto drive = [&](SimulationTime t, const std::string& n, uint64_t v) {
        sim.scheduleEvent(makeWireUpdateEvent(t, inputs.at(n), bits(inputs.at(n)->getWidth(), v)));
    };
    auto out = [&](const std::string& n) { return value(machine->getOutputPinDynamic(n)); };
    auto advance = [&](SimulationTime target) {
        for (;;) {
            const auto r = sim.drainUntilIdle(target, o.quantum_events);
            if (r.status != DrainStatus::EventLimitReached) break;
        }
    };
    auto note = [&](SimulationTime t, const std::string& label, const std::string& detail) { if (o.on_event) o.on_event(t, label, detail); };
    BasicComponent::resetSetupViolations();
    SimulationTest::scheduleInitialEventsForTree(machine, sim, o.start_time);
    for (const auto& [n, p] : inputs) drive(o.start_time, n, n == "RST" || n == "ENABLE" || n == "TX_READY");
    drive(o.reset_release, "RST", 0);
    for (SimulationTime e = o.start_time + o.period / 2; e < o.reset_release; e += o.period) {
        drive(e, "CLK", 1);
        drive(e + o.period / 2, "CLK", 0);
    }
    AcceleratorRunResult result;
    result.setup_seconds = std::chrono::duration<double>(Clock::now() - setup_start).count();
    if (o.progress) o.progress(0, 0);
    const auto run_start = Clock::now();
    bool released = false;
    std::string line;
    uint64_t last_commit = 0;
    for (uint64_t cycle = 0;; ++cycle) {
        demand(cycle < o.max_cycles, "Accelerator guest did not halt within " + std::to_string(o.max_cycles) + " cycles");
        if (o.stall_cycles && cycle - last_commit > o.stall_cycles) {
            std::string pins;
            dumpHandshakes(machine, machine->getName(), 3, pins);
            throw std::runtime_error("Accelerator guest stalled: no instruction completed in cycles " + std::to_string(last_commit) +
                                     "-" + std::to_string(cycle) + " (" + std::to_string(result.instructions) +
                                     " instructions; console \"" + result.console + line + "\"). Pins before the edge:\n" + pins);
        }
        const SimulationTime edge = o.origin + o.period * cycle;
        drive(edge, "CLK", 1);
        drive(edge + o.period / 2, "CLK", 0);
        advance(edge - 1);
        if (!released) { released = true; note(edge - 1, "reset released", "RST is low; the core fetches its first instruction from 0x00000000."); }
        // Sampled before the edge: what the edge takes.
        const auto commit = out("COMMIT_VALID");
        if (commit == 1) { ++result.instructions; last_commit = cycle; }
        else if (commit != 0 && result.instructions) {
            // An unknown commit: the core has taken an X, for example code or
            // data read from a DRAM row kept past its retention.
            std::string pins;
            dumpHandshakes(machine, machine->getName(), 3, pins);
            throw std::runtime_error("Accelerator guest became unknown (X) at cycle " + std::to_string(cycle) + " after " +
                                     std::to_string(result.instructions) + " instructions. Pins before the edge:\n" + pins);
        }
        if (out("TX_VALID") == 1) {
            const char c = char(out("TX_DATA"));
            result.console += c;
            if (c == '\n') { note(edge, "console: " + line, "The program printed \"" + line + "\"."); line.clear(); }
            else line += c;
        }
        if (out("HALTED") == 1 || out("TRAPPED") == 1) {
            result.halted = out("HALTED") == 1;
            result.trapped = out("TRAPPED") == 1;
            result.trap_cause = uint32_t(out("TRAP_CAUSE"));
            result.cycles = cycle;
            result.end_time = edge - 1;
            note(edge - 1, result.trapped ? "trapped" : "halted",
                 "EBREAK after " + std::to_string(cycle) + " cycles and " + std::to_string(result.instructions) + " instructions.");
            break;
        }
        if (o.progress && cycle && cycle % 4096 == 0) o.progress(cycle, result.instructions);
    }
    result.run_seconds = std::chrono::duration<double>(Clock::now() - run_start).count();
    result.setup_violations = BasicComponent::setupViolations();
    result.events = sim.getPerformanceCounters().processed_events;
    result.report = machine->ramStorage().readBytes(0x3f000, 0x1000);
    return result;
}

std::vector<uint8_t> acceleratorFixture(const std::string& name) {
    for (const auto& [program, hex] : generated::acceleratorPrograms()) {
        if (program != name) continue;
        const auto nibble = [](char c) { return static_cast<uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10); };
        std::vector<uint8_t> bytes(hex.size() / 2);
        for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<uint8_t>(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
        return bytes;
    }
    demand(false, "No accelerator program " + name + " (tests/fixtures/accelerator/" + name + ".elf)");
    return {};
}

} // namespace circuit::runtime
