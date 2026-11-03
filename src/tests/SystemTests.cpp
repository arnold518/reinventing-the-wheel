#include "tests/SystemTests.hpp"
#include "tests/TestHelpers.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/PortRule.hpp"
#include "system/Bus.hpp"
#include "system/ComputerSystem.hpp"
#include "system/SystemDescription.hpp"
#include "system/GuestHeader.hpp"
#include "system/BusPortChecker.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/SimulationTest.hpp"
#include <functional>
#include <set>
#include <tuple>
#include "RV32IMScenarioProfile.hpp"
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using namespace circuit::test;
using circuit::system::SystemDescription;
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }

// A small description: a fetch master and three data masters share a memory
// with two ports (three masters on its data port meet in an arbiter tree), a
// device is reached by the CPU's data master only, and a window passes whole
// addresses to a second bus.
const char* testSystem = R"({"schema":1,"name":"bus-test",
  "parts":[{"name":"CPU","family":"external"},{"name":"DMA","family":"external"},{"name":"GPU","family":"external"},{"name":"RAM","family":"external"},
           {"name":"DEV","family":"external"},{"name":"BRIDGE","family":"external"}],
  "buses":[{"name":"BUS","width":32,
    "masters":[{"name":"CPU_I","port":"CPU.IMEM_","access":"fetch"},{"name":"CPU_D","port":"CPU.DMEM_"},{"name":"DMA","port":"DMA"},{"name":"GPU","port":"GPU"}],
    "regions":[{"name":"RAM","base":"0x00000000","bytes":"0x1000","execute":true,"targets":{"CPU_I":"RAM.I_","CPU_D":"RAM.D_","DMA":"RAM.D_","GPU":"RAM.D_"}},
               {"name":"DEV","base":"0x10000000","bytes":"0x100","targets":{"CPU_D":"DEV"}},
               {"name":"IO","base":"0x80000000","bytes":"0x10000000","local":false,"target":"BRIDGE"}]}]})";

void rejects(const std::string& text, const std::string& why) {
    std::string failure;
    try { (void)SystemDescription::parse(text); } catch (const std::exception& e) { failure = e.what(); }
    require(failure.find(why) != std::string::npos, "Description not rejected for '" + why + "': " + failure);
}

void descriptionChecks() {
    const auto games = SystemDescription::parse(circuit::system::builtinSystemJson("games-machine"));
    require(games.parts.size() == 6 && games.buses.size() == 1 && games.buses[0].regions.size() == 6, "games-machine shape");
    const auto again = SystemDescription::fromJson(games.toJson());
    require(again.toJson().dump() == games.toJson().dump(), "Description round trip differs");
    require(games.buses[0].targetPorts().size() == 7, "games-machine target ports");
    const auto test = SystemDescription::parse(testSystem);
    const auto& bus = test.buses[0];
    require(bus.regions[1].targetFor(bus.masters[0]).empty() && bus.regions[1].targetFor(bus.masters[2]).empty(), "Per-master region visibility");
    require(bus.regions[2].targetFor(bus.masters[0]).empty(), "A fetch master must not reach a non-executable region");
    std::string text = testSystem;
    const auto replace = [&](const std::string& from, const std::string& to) { auto t = text; t.replace(t.find(from), from.size(), to); return t; };
    rejects(replace("\"0x10000000\",\"bytes\":\"0x100\"", "\"0x00000800\",\"bytes\":\"0x100\""), "overlap");
    rejects(replace("\"0x10000000\",\"bytes\":\"0x100\"", "\"0x10000010\",\"bytes\":\"0x100\""), "aligned");
    rejects(replace("\"target\":\"BRIDGE\"", "\"target\":\"NOWHERE\""), "unknown part");
    rejects(replace("\"execute\":true,", ""), "reaches no region");
    std::cout << "Descriptions: games-machine round trip, per-master visibility, four rejected mistakes\n";
}

// One root under a profile, driven settle by settle.
class Rig {
public:
    Rig(const circuit::ComponentBuildRequest& request, circuit::BuildProfile profile)
        : root_(std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(request, std::move(profile)).root)) {
        ComponentBuilder builder(root_);
        sim_.setHistoryRecordingEnabled(false);
        for (const auto& [name, pin] : root_->getAllInputPins()) inputs_[name] = builder.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root_->getAllOutputPins()) builder.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
        SimulationTest::scheduleInitialEventsForTree(root_, sim_, 0);
    }
    NamedValues evaluate(const NamedValues& in) {
        for (const auto& [name, bits] : in) sim_.scheduleEvent(makeTestWireUpdate(time_ + 1, inputs_.at(name), TestValue(bits)));
        sim_.advanceAndRecord(time_ += 20000);
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

uint64_t value(const LogicVector& bits) {
    uint64_t v = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        require(bits[i] == LogicValue::LOW || bits[i] == LogicValue::HIGH, "Unknown bus output");
        if (bits[i] == LogicValue::HIGH) v |= uint64_t{1} << i;
    }
    return v;
}

void busChecks() {
    const auto description = SystemDescription::parse(testSystem).buses[0];
    const circuit::ComponentBuildRequest request{circuit::system::busContractId(32), "BUS", {{"config", circuit::system::busConfigJson(description)}}};
    Rig gates(request, circuit::maximallyStructural());
    // Only the routers change form, so any difference is theirs (the
    // arbiter's compact model is defined only for protocol-legal traffic).
    auto compactProfile = circuit::withProfileOverrides(circuit::maximallyStructural(),
        {circuit::preferFidelity(circuit::Fidelity::Behavioral, circuit::ProfileSelector::contract("bus.router.32"), "compiled routers")},
        "bus-compiled-routers");
    Rig compact(request, compactProfile);
    // The whole bus compiled to one gate list, requested at its exact path.
    Rig whole(request, circuit::withExactFidelity(circuit::maximallyStructural(), "BUS", circuit::Fidelity::Behavioral, "whole-bus"));
    require(whole.root()->getSelectedFidelity() == "behavioral" && whole.root()->getChildren().size() <= 2,
            "The whole-bus compiled form was not selected");
    {
        size_t arbiters = 0;
        for (const auto& child : gates.root()->getChildren())
            if (child->getName().starts_with("RAM_D_ARBITER")) ++arbiters;
        require(arbiters == 2, "Three masters on one port need a tree of two arbiters");
    }
    NamedValues in;
    for (const auto& [name, pin] : gates.root()->getAllInputPins()) in[name] = logicBits(pin->getWidth(), 0);
    const auto set = [&](const std::string& name, uint64_t v) { in.at(name) = logicBits(in.at(name).size(), v); };
    size_t steps = 0;
    const auto step = [&](bool clock) {
        set("CLK", clock);
        const auto a = gates.evaluate(in), b = compact.evaluate(in), c = whole.evaluate(in);
        ++steps;
        if (a != c)
            for (const auto& [name, bits] : a)
                if (bits != c.at(name))
                    throw std::runtime_error("Gate-level and whole-bus compiled forms differ at step " + std::to_string(steps) + ", " + name);
        if (a != b)
            for (const auto& [name, bits] : a)
                if (bits != b.at(name))
                    throw std::runtime_error("Gate-level and compiled bus routers differ at step " + std::to_string(steps) + ", " + name +
                                             ": " + formatLogicVector(bits) + " vs " + formatLogicVector(b.at(name)));
        return a;
    };
    const auto cycle = [&] { step(true); return step(false); };
    set("RST", 1); cycle(); cycle(); set("RST", 0); cycle();
    // Directed: the data master reads DEV offset 4, then the bridge window
    // with the whole address, then an unmapped address. A target answers once
    // it sees the request; the master takes the response.
    const auto read = [&](uint32_t address, const std::string& target) {
        set("CPU_D_RSP_READY", 1); set("CPU_D_REQ_OP", 0); set("CPU_D_REQ_ADDR", address); set("CPU_D_REQ_WRITE_MASK", 0);
        set("CPU_D_REQ_VALID", 1);
        if (!target.empty()) set(target + "REQ_READY", 1);
        uint64_t seen_address = ~uint64_t{0};
        for (int i = 0; i < 12; ++i) {
            const auto out = cycle();
            if (value(out.at("CPU_D_REQ_READY"))) set("CPU_D_REQ_VALID", 0);
            if (!target.empty() && value(out.at(target + "REQ_VALID")) && seen_address == ~uint64_t{0}) {
                seen_address = value(out.at(target + "REQ_ADDR"));
                set(target + "REQ_READY", 0);
                set(target + "RSP_VALID", 1); set(target + "RSP_STATUS", 0); set(target + "RSP_READ_DATA", 0x1234);
                continue;
            }
            if (value(out.at("CPU_D_RSP_VALID"))) {
                const auto status = value(out.at("CPU_D_RSP_STATUS"));
                if (!target.empty()) set(target + "RSP_VALID", 0);
                set("CPU_D_REQ_VALID", 0);
                cycle(); cycle();
                return target.empty() ? status : seen_address;
            }
        }
        throw std::runtime_error("No response for address " + std::to_string(address));
    };
    require(read(0x10000004, "DEV_") == 4, "Device sees its local offset");
    require(read(0x80000010, "BRIDGE_") == 0x80000010, "Bridge sees the whole address");
    require(read(0x30000000, "") != 0, "Unmapped address answered with an error");
    // Random traffic: both forms must agree at every settle.
    std::mt19937_64 rng(0xB05);
    for (int i = 0; i < 400; ++i) {
        for (auto& [name, bits] : in)
            if (name != "CLK" && name != "RST") bits = logicBits(bits.size(), rng());
        // Mostly legal addresses, so routes fire.
        for (auto m : {"CPU_I_", "CPU_D_", "DMA_", "GPU_"}) {
            static const uint32_t bases[] = {0x0, 0x10000000, 0x80000000, 0x30000000};
            set(std::string(m) + "REQ_ADDR", bases[rng() % 4] | (rng() & 0xfc));
            set(std::string(m) + "REQ_OP", rng() % 2);
        }
        cycle();
    }
    std::cout << "Bus: local offset, whole-address window, error answer, a two-arbiter tree for three masters; 400 random cycles agree between the gate-level bus, compiled routers and the whole bus compiled\n";
}

// A device on its register port: one request at a time, answered by the
// device itself. Returns {status, read data}.
std::pair<uint64_t, uint64_t> access(Rig& r, NamedValues& in, bool write, uint32_t address, uint32_t data = 0, uint32_t mask = 15) {
    const auto set = [&](const std::string& n, uint64_t v) { in.at(n) = logicBits(in.at(n).size(), v); };
    const auto edge = [&] { set("CLK", 1); r.evaluate(in); set("CLK", 0); return r.evaluate(in); };
    set("REQ_VALID", 1); set("REQ_OP", write); set("REQ_ADDR", address); set("REQ_WRITE_DATA", data); set("REQ_WRITE_MASK", mask); set("RSP_READY", 1);
    for (int i = 0; i < 12; ++i) {
        auto out = edge();
        if (value(out.at("REQ_READY")) == 0) set("REQ_VALID", 0);
        if (value(out.at("RSP_VALID"))) {
            const auto result = std::make_pair(value(out.at("RSP_STATUS")), value(out.at("RSP_READ_DATA")));
            set("REQ_VALID", 0); edge();
            return result;
        }
    }
    throw std::runtime_error("Device did not answer");
}

void ledChecks() {
    for (const bool compact : {false, true}) {
        auto profile = compact ? circuit::BuildProfileBuilder("leds-compact").addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral, circuit::ProfileSelector::any()))
                                     .unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build()
                               : circuit::maximallyStructural();
        Rig r({"device.leds", "LEDS", {}}, profile);
        NamedValues in;
        for (const auto& [name, pin] : r.root()->getAllInputPins()) in[name] = logicBits(pin->getWidth(), 0);
        in.at("RST") = logicBits(1, 1); r.evaluate(in); in.at("RST") = logicBits(1, 0); r.evaluate(in);
        const auto leds = [&] { return value(r.evaluate(in).at("LEDS")); };
        require(leds() == 0, "LEDs start dark");
        require(access(r, in, true, 0, 0xa5c3).first == 0 && leds() == 0xa5c3, "LEDS write shows on the pins");
        require(access(r, in, false, 0) == std::make_pair<uint64_t, uint64_t>(0, 0xa5c3), "LEDS reads back");
        require(access(r, in, true, 0, 0x10000).first == 6, "A value wider than 16 bits is refused");
        require(access(r, in, false, 0).second == 0xa5c3, "A refused write changes nothing");
        require(access(r, in, true, 4, 2).first == 0 && access(r, in, false, 4).second == 2, "BLINK reads back");
        // With BLINK = 2 the pins follow counter bit 2: on for 4 cycles, off for 4.
        unsigned on = 0, off = 0;
        for (int i = 0; i < 16; ++i) {
            in.at("CLK") = logicBits(1, 1); r.evaluate(in); in.at("CLK") = logicBits(1, 0);
            const auto shown = leds();
            require(shown == 0xa5c3 || shown == 0, "Blinking shows all or nothing");
            (shown ? on : off)++;
        }
        require(on == 8 && off == 8, "Blink duty is half");
        require(access(r, in, false, 8).first == 2, "Unmapped offset answers Range");
        require(access(r, in, false, 2).first == 1, "Misaligned offset answers Alignment");
        require(access(r, in, true, 4, 0).first == 0 && leds() == 0xa5c3, "BLINK 0 is steady");
    }
    std::cout << "LED panel: writes, read-back, refused payload, blink duty, Range/Alignment; both forms\n";
}

void headerChecks() {
    const auto d = SystemDescription::parse(circuit::system::builtinSystemJson("games-machine-two-bus"));
    require(circuit::system::cpuAddressOf(d, "CONSOLE") == 0x10000000 && circuit::system::cpuAddressOf(d, "DISPLAY.FB_") == 0x20000000,
            "Bridged devices keep their CPU addresses");
    const auto one = circuit::system::guestHeader(SystemDescription::parse(circuit::system::builtinSystemJson("games-machine")), "x");
    const auto two = circuit::system::guestHeader(d, "x");
    const auto body = [](const std::string& h) { return h.substr(h.find("/* Address map")); };
    auto strip = [&](std::string h) { h = body(h); return h; };
    require(strip(one).find("#define CONSOLE_TX 0x10000000u") != std::string::npos, "Console TX address");
    require(strip(one).find("#define BUTTONS_STATUS_OVERFLOW (1u << 1)") != std::string::npos, "Bit fields");
    require(two.find("#define CONSOLE_TX 0x10000000u") != std::string::npos && two.find("PERIPHERALS_BASE") == std::string::npos,
            "Two-bus header: same device addresses, no bridge window");
    std::cout << "Guest headers: one- and two-bus machines give the same device addresses\n";
}

// Recorded scenarios for the site: settled checkpoints of one sequence.
using Values = std::map<std::string, uint64_t>;


class Sequence {
    std::map<std::string, size_t> widths_;
    NamedValues state_;
public:
    ComponentTestSpec spec;
    Sequence(const std::string& name, const std::string& contract, const std::string& instance, circuit::ParameterMap parameters = {})
        : spec{name, contract, instance, parameters, {{"contract", {}, {}, {}}}} {
        for (const auto& p : circuit::builtinComponentCatalog().contract(contract).pinsFor(parameters)) {
            widths_[p.name] = p.width;
            if (p.direction == PinType::INPUT) state_[p.name] = logicBits(p.width, 0);
        }
        Values reset;
        for (const auto& [n, v] : state_) if (n == "RST" || n.ends_with("_RST")) reset[n] = 1;
        const auto [held, idle] = circuit::test::portRuleExpectations(circuit::builtinComponentCatalog().contract(contract).pinsFor(parameters));
        step("reset", reset, held);
        cycle("reset-edge", {}, {}, false);
        for (auto& [n, v] : reset) v = 0;
        step("ready", reset, idle);
        edges = 0;
    }
    unsigned edges = 0;   // rising clock edges since reset was released
    void step(std::string id, const Values& changes, const Values& expected, bool emit = true) {
        for (const auto& [n, v] : changes) state_.at(n) = logicBits(widths_.at(n), v);
        NamedValues outputs;
        for (const auto& [n, v] : expected) outputs[n] = logicBits(widths_.at(n), v);
        spec.scenarios[0].actions.push_back({std::move(id), CheckpointKind::Settled, state_, outputs, emit, {}});
    }
    void cycle(const std::string& id, Values changes, const Values& expected, bool emit = true) {
        for (const auto& [n, v] : state_) if (n == "CLK" || n.ends_with("_CLK")) changes[n] = 0;
        step("", changes, {}, false);
        Values high, low;
        for (const auto& [n, v] : state_) if (n == "CLK" || n.ends_with("_CLK")) { high[n] = 1; low[n] = 0; }
        step(id, high, expected, emit);
        step("", low, {}, false);
        ++edges;
    }
    // A self-answering target (device, memory): request, wait, check the held
    // response, take it.
    void access(const std::string& id, bool write, uint32_t address, uint32_t data, uint32_t status, uint32_t read = 0,
                const std::string& prefix = "") {
        cycle("", {{prefix + "REQ_VALID", 1}, {prefix + "REQ_OP", write ? 1u : 0u}, {prefix + "REQ_ADDR", address},
                   {prefix + "REQ_WRITE_DATA", data}, {prefix + "REQ_WRITE_MASK", 15}, {prefix + "RSP_READY", 0}}, {}, false);
        for (int i = 0; i < 12; ++i) cycle("", {{prefix + "REQ_VALID", 0}}, {}, false);
        Values expected{{prefix + "RSP_VALID", 1}, {prefix + "RSP_STATUS", status}};
        if (!write && status == 0) expected[prefix + "RSP_READ_DATA"] = read;
        step(id, {}, expected);
        cycle("", {{prefix + "RSP_READY", 1}}, {}, false);
        cycle("", {{prefix + "RSP_READY", 0}}, {}, false);
    }
};

ComponentTestSpec routerSpec() {
    Sequence s("BusRouterScenarioTest", "bus.router.32", "BUS_ROUTER_ROOT",
               {{"access", "data"}, {"routes", "MEMORY_@0x00000000+0x1000,DEVICE_@0x10000000+0x100"}});
    s.step("idle", {{"RSP_READY", 1}}, {{"REQ_READY", 1}, {"RSP_VALID", 0}, {"DEVICE_REQ_VALID", 0}, {"MEMORY_REQ_VALID", 0}});
    s.cycle("read-accepted", {{"REQ_VALID", 1}, {"REQ_OP", 0}, {"REQ_ADDR", 0x10000004}},
            {{"REQ_READY", 0}, {"DEVICE_REQ_VALID", 1}, {"DEVICE_REQ_OP", 0}, {"DEVICE_REQ_ADDR", 4}, {"MEMORY_REQ_VALID", 0}, {"RSP_VALID", 0}});
    s.cycle("target-not-ready", {{"REQ_VALID", 0}}, {{"DEVICE_REQ_VALID", 1}, {"RSP_VALID", 0}});
    s.step("target-ready", {{"DEVICE_REQ_READY", 1}}, {{"DEVICE_REQ_VALID", 1}});
    s.cycle("issued", {}, {{"DEVICE_REQ_VALID", 0}, {"DEVICE_RSP_READY", 1}, {"RSP_VALID", 0}});
    s.step("target-answers", {{"DEVICE_REQ_READY", 0}, {"DEVICE_RSP_VALID", 1}, {"DEVICE_RSP_READ_DATA", 0x1234}},
           {{"RSP_VALID", 1}, {"RSP_READ_DATA", 0x1234}, {"RSP_STATUS", 0}});
    s.cycle("response-taken", {}, {{"RSP_VALID", 0}, {"REQ_READY", 1}});
    s.step("target-idle", {{"DEVICE_RSP_VALID", 0}}, {{"DEVICE_RSP_READY", 0}});
    s.cycle("unmapped", {{"REQ_VALID", 1}, {"REQ_ADDR", 0x30000000}},
            {{"RSP_VALID", 1}, {"RSP_STATUS", 2}, {"DEVICE_REQ_VALID", 0}, {"MEMORY_REQ_VALID", 0}});
    s.cycle("error-taken", {{"REQ_VALID", 0}}, {{"RSP_VALID", 0}, {"REQ_READY", 1}});
    s.cycle("memory-write", {{"REQ_VALID", 1}, {"REQ_OP", 1}, {"REQ_ADDR", 0x10}, {"REQ_WRITE_DATA", 0xcafe}, {"REQ_WRITE_MASK", 12}, {"MEMORY_REQ_READY", 1}},
            {{"MEMORY_REQ_VALID", 1}, {"MEMORY_REQ_OP", 1}, {"MEMORY_REQ_ADDR", 0x10}, {"MEMORY_REQ_WRITE_DATA", 0xcafe},
             {"MEMORY_REQ_WRITE_MASK", 12}, {"DEVICE_REQ_VALID", 0}});
    s.cycle("memory-issued", {{"REQ_VALID", 0}}, {{"MEMORY_REQ_VALID", 0}, {"MEMORY_RSP_READY", 1}});
    s.step("memory-answers", {{"MEMORY_REQ_READY", 0}, {"MEMORY_RSP_VALID", 1}}, {{"RSP_VALID", 1}, {"RSP_STATUS", 0}});
    s.cycle("write-done", {}, {{"RSP_VALID", 0}, {"REQ_READY", 1}});
    return std::move(s.spec);
}

ComponentTestSpec bridgeSpec() {
    Sequence s("BusBridgeScenarioTest", "bus.bridge.32", "BUS_BRIDGE_ROOT", {{"registered", "1"}});
    s.step("idle", {}, {{"S_REQ_READY", 1}, {"M_REQ_VALID", 0}, {"S_RSP_VALID", 0}});
    s.cycle("request-taken", {{"S_REQ_VALID", 1}, {"S_REQ_OP", 0}, {"S_REQ_ADDR", 0x10002004}},
            {{"S_REQ_READY", 0}, {"M_REQ_VALID", 1}, {"M_REQ_OP", 0}, {"M_REQ_ADDR", 0x10002004}});
    s.cycle("request-offered", {{"S_REQ_VALID", 0}, {"M_REQ_READY", 1}}, {{"M_REQ_VALID", 0}, {"M_RSP_READY", 1}});
    s.step("response-arrives", {{"M_REQ_READY", 0}, {"M_RSP_VALID", 1}, {"M_RSP_READ_DATA", 0x102}}, {{"S_RSP_VALID", 0}});
    s.cycle("response-held", {}, {{"S_RSP_VALID", 1}, {"S_RSP_READ_DATA", 0x102}, {"S_RSP_STATUS", 0}, {"M_RSP_READY", 0}});
    s.cycle("response-taken", {{"M_RSP_VALID", 0}, {"S_RSP_READY", 1}}, {{"S_RSP_VALID", 0}, {"S_REQ_READY", 1}});
    // A write crosses with its data and mask; an error status comes back unchanged.
    s.cycle("write-taken", {{"S_RSP_READY", 0}, {"S_REQ_VALID", 1}, {"S_REQ_OP", 1}, {"S_REQ_ADDR", 0x10002008},
                            {"S_REQ_WRITE_DATA", 0xdeadbeef}, {"S_REQ_WRITE_MASK", 3}},
            {{"S_REQ_READY", 0}, {"M_REQ_VALID", 1}, {"M_REQ_OP", 1}, {"M_REQ_ADDR", 0x10002008},
             {"M_REQ_WRITE_DATA", 0xdeadbeef}, {"M_REQ_WRITE_MASK", 3}});
    s.cycle("write-offered", {{"S_REQ_VALID", 0}, {"M_REQ_READY", 1}}, {{"M_REQ_VALID", 0}, {"M_RSP_READY", 1}});
    s.step("write-refused", {{"M_REQ_READY", 0}, {"M_RSP_VALID", 1}, {"M_RSP_STATUS", 2}}, {{"S_RSP_VALID", 0}});
    s.cycle("refusal-held", {}, {{"S_RSP_VALID", 1}, {"S_RSP_STATUS", 2}, {"M_RSP_READY", 0}});
    s.cycle("refusal-taken", {{"M_RSP_VALID", 0}, {"S_RSP_READY", 1}}, {{"S_RSP_VALID", 0}, {"S_REQ_READY", 1}});
    return std::move(s.spec);
}

ComponentTestSpec memorySpec() {
    Sequence s("MemorySubsystemScenarioTest", "memory.subsystem.32", "MEMORY_SUBSYSTEM_ROOT",
               {{"technology", "ff"}, {"words", "1024"}, {"caches", "none"}});
    s.access("data-write", true, 0x40, 0x12345678, 0, 0, "D_");
    s.access("data-read", false, 0x40, 0, 0, 0x12345678, "D_");
    s.access("fetch-same-word", false, 0x40, 0, 0, 0x12345678, "I_");
    s.access("second-word", true, 0x44, 0x9abcdef0, 0, 0, "D_");
    // Both ports ask in the same cycle. After reset the fetch port has
    // priority, so the data port keeps its request up until the memory is
    // free again (round robin then prefers it next time).
    s.step("both-ports-ask", {{"I_REQ_VALID", 1}, {"I_REQ_OP", 0}, {"I_REQ_ADDR", 0x40}, {"I_RSP_READY", 0},
                              {"D_REQ_VALID", 1}, {"D_REQ_OP", 0}, {"D_REQ_ADDR", 0x44}, {"D_RSP_READY", 0}},
           {{"I_REQ_READY", 1}, {"D_REQ_READY", 0}});
    s.cycle("fetch-granted", {}, {{"I_REQ_READY", 0}, {"D_REQ_READY", 0}});
    s.cycle("data-granted", {{"I_REQ_VALID", 0}}, {{"D_REQ_READY", 1}});
    s.cycle("data-accepted", {}, {{"D_REQ_READY", 0}});
    for (int i = 0; i < 16; ++i) s.cycle("", {{"D_REQ_VALID", 0}}, {}, false);
    s.step("both-answered", {}, {{"I_RSP_VALID", 1}, {"I_RSP_READ_DATA", 0x12345678}, {"I_RSP_STATUS", 0},
                                 {"D_RSP_VALID", 1}, {"D_RSP_READ_DATA", 0x9abcdef0}, {"D_RSP_STATUS", 0}});
    s.cycle("both-taken", {{"I_RSP_READY", 1}, {"D_RSP_READY", 1}}, {{"I_RSP_VALID", 0}, {"D_RSP_VALID", 0}});
    s.step("", {{"I_RSP_READY", 0}, {"D_RSP_READY", 0}}, {}, false);
    s.access("outside-memory", false, 0x1000, 0, 2, 0, "D_");
    return std::move(s.spec);
}

ComponentTestSpec busSpec() {
    // Two masters share MEMORY (an arbiter takes turns); only CPU sees DEVICE.
    // Both targets are always ready and always answering, with fixed data.
    const char* config = R"({"name":"BUS","width":32,
      "masters":[{"name":"CPU","port":"CORE.DMEM_","access":"data"},{"name":"DMA","port":"DMA","access":"data"}],
      "regions":[{"name":"MEMORY","base":"0x00000000","bytes":"0x1000","target":"MEMORY"},
                 {"name":"DEVICE","base":"0x10000000","bytes":"0x100","targets":{"CPU":"DEVICE"}}]})";
    Sequence s("BusInterconnectScenarioTest", "bus.interconnect.32", "BUS_ROOT",
               {{"config", circuit::system::busConfigJson(circuit::system::busFromConfigJson(config))}});
    s.step("targets-ready", {{"MEMORY_REQ_READY", 1}, {"MEMORY_RSP_VALID", 1}, {"MEMORY_RSP_READ_DATA", 0x1111},
                             {"DEVICE_REQ_READY", 1}, {"DEVICE_RSP_VALID", 1}, {"DEVICE_RSP_READ_DATA", 0x2222}},
           {{"CPU_REQ_READY", 1}, {"DMA_REQ_READY", 1}, {"CPU_RSP_VALID", 0}, {"DMA_RSP_VALID", 0}});
    s.cycle("both-request-memory", {{"CPU_REQ_VALID", 1}, {"CPU_REQ_ADDR", 0x40}, {"DMA_REQ_VALID", 1}, {"DMA_REQ_ADDR", 0x80}},
            {{"CPU_REQ_READY", 0}, {"DMA_REQ_READY", 0}});
    for (int i = 0; i < 10; ++i) s.cycle("", {{"CPU_REQ_VALID", 0}, {"DMA_REQ_VALID", 0}}, {}, false);
    s.step("both-answered", {}, {{"CPU_RSP_VALID", 1}, {"CPU_RSP_READ_DATA", 0x1111}, {"CPU_RSP_STATUS", 0},
                                 {"DMA_RSP_VALID", 1}, {"DMA_RSP_READ_DATA", 0x1111}, {"DMA_RSP_STATUS", 0}});
    s.cycle("both-taken", {{"CPU_RSP_READY", 1}, {"DMA_RSP_READY", 1}}, {});
    s.step("idle-again", {{"CPU_RSP_READY", 0}, {"DMA_RSP_READY", 0}}, {{"CPU_RSP_VALID", 0}, {"DMA_RSP_VALID", 0},
                                                                         {"CPU_REQ_READY", 1}, {"DMA_REQ_READY", 1}});
    s.cycle("cpu-reads-device", {{"CPU_REQ_VALID", 1}, {"CPU_REQ_ADDR", 0x10000008}}, {});
    for (int i = 0; i < 6; ++i) s.cycle("", {{"CPU_REQ_VALID", 0}}, {}, false);
    s.step("device-answered", {}, {{"CPU_RSP_VALID", 1}, {"CPU_RSP_READ_DATA", 0x2222}, {"CPU_RSP_STATUS", 0}});
    s.cycle("cpu-taken", {{"CPU_RSP_READY", 1}}, {});
    s.cycle("dma-tries-device", {{"CPU_RSP_READY", 0}, {"DMA_REQ_VALID", 1}, {"DMA_REQ_ADDR", 0x10000000}},
            {{"DMA_RSP_VALID", 1}, {"DMA_RSP_STATUS", 2}});
    return std::move(s.spec);
}

ComponentTestSpec checkerSpec() {
    Sequence s("BusPortCheckerScenarioTest", "bus.port-checker.32", "PORT_CHECKER_ROOT");
    s.step("idle", {}, {{"VIOLATION", 0}});
    s.cycle("request-waits", {{"REQ_VALID", 1}, {"REQ_ADDR", 4}}, {{"VIOLATION", 0}});
    s.cycle("request-taken", {{"REQ_READY", 1}}, {{"VIOLATION", 0}});
    s.cycle("answer-taken", {{"REQ_VALID", 0}, {"REQ_READY", 0}, {"RSP_VALID", 1}, {"RSP_READY", 1}, {"RSP_READ_DATA", 9}},
            {{"VIOLATION", 0}, {"EXTRA_RESPONSE", 0}});
    s.cycle("new-request-waits", {{"RSP_VALID", 0}, {"RSP_READY", 0}, {"REQ_VALID", 1}, {"REQ_ADDR", 8}}, {{"VIOLATION", 0}});
    s.cycle("address-changed", {{"REQ_ADDR", 12}}, {{"REQUEST_CHANGED", 1}, {"VIOLATION", 1}});
    s.cycle("stays-flagged", {{"REQ_VALID", 0}}, {{"VIOLATION", 1}});
    s.step("reset-clears", {{"RST", 1}}, {{"VIOLATION", 0}, {"REQUEST_CHANGED", 0}});
    return std::move(s.spec);
}

ComponentTestSpec ledSpec() {
    Sequence s("LedPanelScenarioTest", "device.leds", "LED_PANEL_ROOT");
    s.step("dark", {}, {{"LEDS", 0}});
    s.access("light-some", true, 0, 0x0f0f, 0);
    s.step("lit", {}, {{"LEDS", 0x0f0f}});
    s.access("read-back", false, 0, 0, 0, 0x0f0f);
    s.access("too-wide", true, 0, 0x10000, 6);
    s.access("blink-off", false, 4, 0, 0, 0);
    s.access("blink-set", true, 4, 2, 0);
    s.access("blink-read-back", false, 4, 0, 0, 2);
    // BLINK = 2: the lit LEDs show while bit 2 of the free-running cycle
    // counter is 0, so 4 cycles on and 4 off. Writing BLINK does not restart
    // the counter; it counts every edge since reset.
    for (int i = 0; i < 8; ++i) {
        const unsigned counter = s.edges + 1;
        const bool dark = (counter >> 2) & 1;
        s.cycle("blink-" + std::to_string(i + 1) + (dark ? "-off" : "-on"), {}, {{"LEDS", dark ? 0u : 0x0f0fu}});
    }
    s.access("steady-again", true, 4, 0, 0);
    s.step("steady", {}, {{"LEDS", 0x0f0f}});
    s.access("no-register", false, 8, 0, 2);
    return std::move(s.spec);
}

void checkerChecks() {
    // Each case starts from reset, drives the link through `steps` (each
    // step = the values held before one rising edge) and expects one flag.
    struct Case { std::string name; std::vector<std::map<std::string, uint64_t>> steps; std::string flag; };
    const std::map<std::string, uint64_t> idle{};
    const std::vector<Case> cases{
        {"good transfer", {{{"REQ_VALID", 1}, {"REQ_ADDR", 4}}, {{"REQ_VALID", 1}, {"REQ_ADDR", 4}, {"REQ_READY", 1}},
                           {{"RSP_VALID", 1}, {"RSP_READ_DATA", 9}}, {{"RSP_VALID", 1}, {"RSP_READ_DATA", 9}, {"RSP_READY", 1}}, idle}, ""},
        {"withdrawn request", {{{"REQ_VALID", 1}, {"REQ_ADDR", 4}}, idle, idle}, "REQUEST_CHANGED"},
        {"changed address", {{{"REQ_VALID", 1}, {"REQ_ADDR", 4}}, {{"REQ_VALID", 1}, {"REQ_ADDR", 8}}, idle}, "REQUEST_CHANGED"},
        {"answer without request", {{{"RSP_VALID", 1}, {"RSP_READY", 1}}, idle}, "EXTRA_RESPONSE"},
        {"changed answer", {{{"REQ_VALID", 1}, {"REQ_READY", 1}}, {{"RSP_VALID", 1}, {"RSP_READ_DATA", 1}},
                            {{"RSP_VALID", 1}, {"RSP_READ_DATA", 2}, {"RSP_READY", 1}}, idle}, "RESPONSE_CHANGED"}};
    for (const auto& c : cases) {
        Rig r({circuit::system::portCheckerContractId(32), "CHECK", {}}, circuit::maximallyStructural());
        NamedValues in;
        for (const auto& [name, pin] : r.root()->getAllInputPins()) in[name] = logicBits(pin->getWidth(), 0);
        in.at("RST") = logicBits(1, 1); r.evaluate(in); in.at("CLK") = logicBits(1, 1); r.evaluate(in);
        in.at("CLK") = logicBits(1, 0); in.at("RST") = logicBits(1, 0); r.evaluate(in);
        NamedValues out;
        for (const auto& step : c.steps) {
            for (auto& [name, bits] : in) if (name != "CLK" && name != "RST") bits = logicBits(bits.size(), step.contains(name) ? step.at(name) : 0);
            r.evaluate(in);
            in.at("CLK") = logicBits(1, 1); out = r.evaluate(in);
            in.at("CLK") = logicBits(1, 0); out = r.evaluate(in);
        }
        for (const auto* flag : {"REQUEST_CHANGED", "RESPONSE_CHANGED", "EXTRA_RESPONSE"})
            require(value(out.at(flag)) == (c.flag == flag), "Port checker, " + c.name + ": " + flag);
        require(value(out.at("VIOLATION")) == !c.flag.empty(), "Port checker, " + c.name + ": VIOLATION");
    }
    std::cout << "Port checker: a clean transfer stays clean; withdrawn or changed requests, a changed answer and an answer without a request are each flagged\n";
}

class Check : public StandaloneVerificationTest {
    std::string name_;
    std::function<void()> check_;
public:
    Check(std::string name, std::function<void()> check) : name_(std::move(name)), check_(std::move(check)) {}
    std::string getTestName() const override { return name_; }
    void verifyResults() override { check_(); }
};
} // namespace

void appendSystemTests(std::vector<TestRegistryEntry>& entries) {
    const auto add = [&](const std::string& name, std::vector<std::string> contracts, std::function<void()> check) {
        entries.push_back({name, name, RegisteredTestKind::Integration, std::move(contracts), {}, {"simulation", "system", "device"},
                           [name, check] { return std::make_unique<Check>(name, check); }, false});
    };
    add("SystemDescriptionTest", {}, descriptionChecks);
    add("BusInterconnectTest", {"bus.interconnect.32"}, busChecks);
    add("LedPanelTest", {"device.leds"}, ledChecks);
    add("BusPortCheckerTest", {"bus.port-checker.32"}, checkerChecks);
    add("GuestHeaderContentTest", {}, headerChecks);
    const auto profile = circuit::test::rv32imInspectionProfile();
    for (const auto& [name, owner, contract, make] : std::vector<std::tuple<std::string, std::string, std::string, ComponentTestSpec (*)()>>{
             {"BusRouterScenarioTest", "BusRouterScenarioTest", "bus.router.32", routerSpec},
             {"BusBridgeScenarioTest", "BusBridgeScenarioTest", "bus.bridge.32", bridgeSpec},
             {"MemorySubsystemScenarioTest", "MemorySubsystemScenarioTest", "memory.subsystem.32", memorySpec},
             {"LedPanelScenarioTest", "LedPanelTest", "device.leds", ledSpec},
             {"BusInterconnectScenarioTest", "BusInterconnectTest", "bus.interconnect.32", busSpec},
             {"BusPortCheckerScenarioTest", "BusPortCheckerTest", "bus.port-checker.32", checkerSpec}})
        entries.push_back({name, owner, RegisteredTestKind::Sequential, {contract}, "contract", {"simulation", "system-scenario"},
                           [make, profile] { return std::make_unique<circuit::test::ComponentScenarioTest>(make(), "contract", profile); }, true});
}
