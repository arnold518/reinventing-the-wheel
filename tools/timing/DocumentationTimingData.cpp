// The timing numbers the documentation shows, per timing profile, in one
// generated file (tools/timing/documentation-timing.generated.json): the
// leaf delays, the measured flip-flop and SRAM-cell contracts, the stored
// certified core periods, and the access latency of each memory technology
// behind the transaction port. tools/mkdocs_timing.py renders them.
//
//   documentation_timing_data --write FILE     measure every profile, write JSON
//   documentation_timing_data --check FILE     measure again; must equal FILE
//   documentation_timing_data --compare-gates  `unit` only: the gate-level
//                                              endpoints take the same cycles
//
// Each memory is built through the catalog as the RV32I experiment builds it
// (the compact endpoint, which runs on its proven delay tables), clocked at
// the advanced core's certified period of the profile, at the experiment's
// memory size (65,536 words; delays grown from the proven 64- and 128-word
// forms, as for the experiment). SRAM and ROM wait
// states come from `staticAccessCycles`, DRAM rows and timing from the
// profile's reference chip; refresh is off so that one access is measured
// alone. Latency is counted in clock cycles from the edge that accepts the
// request to the edge that takes the response (0: the response is taken at
// the accepting edge). Operations longer than the simulation budget (NOR
// sector erase in the realistic profiles) are reported as their configured
// device interval, marked as not simulated.

#include "analysis/ExperimentClock.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/TestHelpers.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using circuit::memory::EndpointConfig;
using circuit::memory::Technology;

namespace {

constexpr size_t MAX_SIMULATED_CYCLES = 200000;

struct Measurement {
    std::string memory, operation, note;
    size_t cycles = 0;
    bool simulated = true;
    size_t interval_ticks = 0; // the configured device interval when not simulated
};

// One memory endpoint with its transaction port on the root's pins.
class Rig {
public:
    Rig(Technology technology, const EndpointConfig& config, bool gates, size_t period) : half_(period / 2) {
        const auto& catalog = circuit::builtinComponentCatalog();
        auto profile = gates ? circuit::maximallyStructural() : circuit::strictAllBehavioral();
        auto manifest = std::make_shared<circuit::BuildManifest>(profile.name(), profile.fingerprint());
        auto context = circuit::BuildContext::rootScope(catalog, std::make_shared<const circuit::BuildProfile>(profile), manifest)
                           ->child("PROBE", std::nullopt);
        root_ = Component::createWithContext<IOComponent>(context, "PROBE", [](IOComponent* self) {
            circuit::memory::protocolPins(self, 32);
        });
        ComponentBuilder b(root_, context);
        rv32i::MemoryConfiguration memory;
        memory.technology = technology;
        memory.expanded_endpoint = gates;
        memory.device = config;
        auto endpoint = rv32i::buildMemory(b, *root_, "M", memory);
        {
            circuit::detail::StructuralWiring w(b);
            for (const auto& [name, pin] : root_->getAllInputPins()) w.connect(pin, endpoint->getInputPinDynamic(name));
            for (const auto& [name, pin] : root_->getAllOutputPins()) w.connect(endpoint->getOutputPinDynamic(name), pin);
            w.finish();
        }
        for (const auto& [name, pin] : root_->getAllInputPins())
            in_[name] = b.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root_->getAllOutputPins())
            b.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
        sim_.setHistoryRecordingEnabled(false);
        for (const auto& [name, wire] : in_)
            sim_.scheduleEvent(makeTestWireUpdate(0, wire, TestValue(uint64_t(name == "RST"))));
        SimulationTest::scheduleInitialEventsForTree(root_, sim_, 0);
        sim_.advanceAndRecord(now_);
        for (int i = 0; i < 4; ++i) cycle();
        set({{"RST", 0}});
        for (int i = 0; i < 4; ++i) cycle();
    }

    // Cycles from the accepting edge to the edge that takes the response.
    size_t request(unsigned op, uint32_t address, uint32_t data = 0, unsigned mask = 0) {
        set({{"REQ_VALID", 1}, {"REQ_OP", op}, {"REQ_ADDR", address}, {"REQ_WRITE_DATA", data},
             {"REQ_WRITE_MASK", mask}, {"RSP_READY", 1}});
        for (size_t waited = 0;; ++waited) {
            if (waited > 1000) throw std::runtime_error("request never accepted");
            low();
            const bool accepted = high("REQ_READY"), immediate = accepted && high("RSP_VALID");
            if (immediate) checkStatus();
            rise();
            if (accepted) {
                set({{"REQ_VALID", 0}});
                if (immediate) return 0;
                break;
            }
        }
        for (size_t cycles = 1; cycles <= MAX_SIMULATED_CYCLES; ++cycles) {
            low();
            const bool response = high("RSP_VALID");
            if (response) checkStatus();
            rise();
            if (response) return cycles;
        }
        throw std::runtime_error("no response within the simulation budget");
    }

private:
    void set(const std::map<std::string, uint64_t>& values) {
        for (const auto& [name, value] : values)
            sim_.scheduleEvent(makeTestWireUpdate(now_ + 1, in_.at(name), TestValue(value)));
        now_ += 2;
        sim_.advanceAndRecord(now_);
    }
    void low() {
        sim_.scheduleEvent(makeTestWireUpdate(now_ + 1, in_.at("CLK"), TestValue(uint64_t(0))));
        now_ += half_;
        sim_.advanceAndRecord(now_);
    }
    void rise() {
        sim_.scheduleEvent(makeTestWireUpdate(now_ + 1, in_.at("CLK"), TestValue(uint64_t(1))));
        now_ += half_;
        sim_.advanceAndRecord(now_);
    }
    void cycle() { low(); rise(); }
    bool high(const char* pin) {
        return root_->getOutputPinDynamic(pin)->getValueAsVector() == std::vector<LogicValue>{LogicValue::HIGH};
    }
    void checkStatus() {
        if (root_->getOutputPinDynamic("RSP_STATUS")->getValueAsUInt64() != 0)
            throw std::runtime_error("response with an error status");
    }

    size_t half_, now_ = 10;
    std::shared_ptr<IOComponent> root_;
    Simulator sim_;
    std::map<std::string, std::shared_ptr<WireBase>> in_;
};

// The experiment's memory (256 KiB) for the documented table; 64 words, which
// gate-level endpoints can build, for the comparison with the gates.
EndpointConfig baseConfig(bool full_size) {
    EndpointConfig c;
    c.depth_words = full_size ? 65536 : 64;
    c.words_per_row = 2;
    c.block_words = 2;
    c.record_history = false;
    c.refresh_interval_cycles = 0;
    c.refresh_interval_ticks = 0;
    return c;
}

// Every measured access of one profile at one clock period.
std::vector<Measurement> measure(size_t period, bool gates, bool reference_rows = true) {
    const auto& reference = circuit::timing::activeTimingProfile().memory;
    std::vector<Measurement> rows;
    const auto add = [&](std::string memory, std::string operation, size_t cycles, std::string note = {}) {
        rows.push_back({std::move(memory), std::move(operation), std::move(note), cycles});
    };
    const auto statics = [&](Technology t, EndpointConfig c) {
        c.access_cycles = circuit::memory::staticAccessCycles(32, t, c, period);
        return c;
    };
    {
        Rig ff(Technology::FF, baseConfig(reference_rows), gates, period);
        add("ff", "write", ff.request(1, 0x08, 0x12345678, 15));
        add("ff", "read", ff.request(0, 0x08));
    }
    for (const bool clocked : {false, true}) {
        auto c = baseConfig(reference_rows);
        c.clocked = clocked;
        c = statics(Technology::SRAM, c);
        Rig sram(Technology::SRAM, c, gates, period);
        const std::string name = clocked ? "sram-clocked" : "sram";
        const std::string note = "access_cycles " + std::to_string(c.access_cycles);
        add(name, "write", sram.request(1, 0x08, 0x12345678, 15), note);
        add(name, "read", sram.request(0, 0x08), note);
    }
    {
        auto c = baseConfig(reference_rows);
        c.image_hex = std::string(c.depth_words * 8, '5');
        c = statics(Technology::ROM, c);
        Rig rom(Technology::ROM, c, gates, period);
        add("rom", "read", rom.request(0, 0x08), "access_cycles " + std::to_string(c.access_cycles));
    }
    {
        // Rows of the profile's reference chip; gate-level endpoints are
        // limited to 4,096 cells, so their row is the `unit` stand-in.
        auto c = baseConfig(reference_rows);
        c.words_per_row = gates || !reference_rows ? 2 : reference.dram_row_words;
        Rig dram(Technology::DRAM, c, gates, period);
        const uint32_t other_row = static_cast<uint32_t>(c.words_per_row * 4);
        const std::string note = std::to_string(c.words_per_row) + "-word rows";
        add("dram", "read, row closed", dram.request(0, 0x00), note);
        add("dram", "read, row open (hit)", dram.request(0, 0x04), note);
        add("dram", "read, other row open (conflict)", dram.request(0, other_row), note);
        add("dram", "write, row open (hit)", dram.request(1, other_row + 4, 0x12345678, 15), note);
    }
    {
        auto c = baseConfig(reference_rows);
        Rig nor(Technology::NORFlash, c, gates, period);
        add("nor", "read", nor.request(0, 0x08));
        const size_t program_cycles = (c.program_ticks + period - 1) / period;
        if (program_cycles <= (gates ? 1000 : MAX_SIMULATED_CYCLES / 2)) add("nor", "program one word", nor.request(2, 0x10, 0x0F0F0F0F, 15));
        else rows.push_back({"nor", "program one word", "not simulated", 0, false, c.program_ticks});
        const size_t erase_cycles = (c.erase_ticks + period - 1) / period;
        if (erase_cycles <= (gates ? 1000 : MAX_SIMULATED_CYCLES / 2)) add("nor", "erase block", nor.request(3, 0x10));
        else rows.push_back({"nor", "erase block", "not simulated", 0, false, c.erase_ticks});
    }
    return rows;
}

std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (char ch : text) out += (ch == '"' || ch == '\\') ? std::string("\\") + ch : std::string(1, ch);
    return out + "\"";
}

std::string json() {
    std::ostringstream out;
    out << "{\n  \"schema\": 1,\n  \"latency_core\": \"advanced\",\n  \"profiles\": [\n";
    bool first_profile = true;
    for (const char* name : {"unit", "embedded", "desktop"}) {
        circuit::timing::setActiveTimingProfile(name);
        const auto& profile = circuit::timing::activeTimingProfile();
        const auto period = circuit::analysis::certifiedCorePeriod(name, "advanced");
        if (!period) throw std::runtime_error(std::string("no certified period for ") + name);
        const auto rows = measure(*period, false);
        const auto& ff = profile.flip_flop;
        const auto& cell = profile.sram_cell;
        out << (first_profile ? "" : ",\n") << "    {\"name\": " << quoted(name)
            << ", \"description\": " << quoted(profile.description)
            << ", \"tick_picoseconds\": " << profile.tick_picoseconds << ",\n"
            << "     \"gates\": {\"NOT\": " << profile.not_gate << ", \"NAND\": " << profile.nand_gate
            << ", \"NOR\": " << profile.nor_gate << ", \"AND\": " << profile.and_gate << ", \"OR\": " << profile.or_gate
            << ", \"XOR\": " << profile.xor_gate << ", \"splitter\": " << profile.splitter
            << ", \"memory primitive\": " << profile.memory_primitive << "},\n"
            << "     \"flip_flop\": {\"setup\": " << ff.setup << ", \"hold\": " << ff.hold
            << ", \"clock_to_output_minimum\": " << ff.clock_to_output_minimum
            << ", \"clock_to_output_maximum\": " << ff.clock_to_output_maximum
            << ", \"minimum_high\": " << ff.minimum_high << ", \"minimum_low\": " << ff.minimum_low << "},\n"
            << "     \"sram_cell\": {\"earliest\": " << cell.earliest << ", \"latest\": " << cell.latest
            << ", \"minimum_spacing\": " << cell.minimum_spacing << "},\n"
            << "     \"memory_intervals\": {";
        const auto& m = profile.memory;
        const std::vector<std::pair<const char*, size_t>> intervals = {
            {"sram_settle", m.sram_settle}, {"sram_precharge", m.sram_precharge},
            {"sram_bitline_base", m.sram_bitline_base}, {"sram_bitline_per_row", m.sram_bitline_per_row},
            {"sram_wire_per_side", m.sram_wire_per_side}, {"rom_settle", m.rom_settle},
            {"rom_column_base", m.rom_column_base}, {"rom_column_per_row", m.rom_column_per_row},
            {"dram_wordline", m.dram_wordline}, {"dram_sense_base", m.dram_sense_base},
            {"dram_sense_per_row", m.dram_sense_per_row}, {"dram_restore", m.dram_restore},
            {"dram_read", m.dram_read}, {"dram_write_on", m.dram_write_on}, {"dram_write_off", m.dram_write_off},
            {"dram_write_recovery", m.dram_write_recovery}, {"dram_close", m.dram_close},
            {"dram_precharge", m.dram_precharge}, {"dram_equalize", m.dram_equalize},
            {"dram_refresh_restore", m.dram_refresh_restore}, {"dram_refresh_extra", m.dram_refresh_extra},
            {"dram_refresh_interval", m.dram_refresh_interval}, {"dram_device_retention", m.dram_device_retention},
            {"nor_read", m.nor_read}, {"nor_read_per_row", m.nor_read_per_row}, {"nor_pulse_end", m.nor_pulse_end},
            {"nor_recovery", m.nor_recovery}, {"nor_program", m.nor_program}, {"nor_erase", m.nor_erase}};
        for (size_t i = 0; i < intervals.size(); ++i)
            out << (i ? ", " : "") << quoted(intervals[i].first) << ": " << intervals[i].second;
        out << "},\n     \"dram_row_words\": " << m.dram_row_words << ",\n"
            << "     \"certified_periods\": {";
        bool first_core = true;
        for (const char* core : {"single", "baseline", "advanced"}) {
            out << (first_core ? "" : ", ") << quoted(core) << ": " << *circuit::analysis::certifiedCorePeriod(name, core);
            first_core = false;
        }
        out << "},\n     \"period_ticks\": " << *period << ", \"accesses\": [\n";
        first_profile = false;
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            out << "      {\"memory\": " << quoted(r.memory) << ", \"operation\": " << quoted(r.operation)
                << ", \"simulated\": " << (r.simulated ? "true" : "false");
            if (r.simulated) out << ", \"cycles\": " << r.cycles;
            else out << ", \"interval_ticks\": " << r.interval_ticks;
            if (!r.note.empty()) out << ", \"note\": " << quoted(r.note);
            out << "}" << (i + 1 < rows.size() ? ",\n" : "\n");
        }
        out << "    ]}";
    }
    out << "\n  ]\n}\n";
    circuit::timing::setActiveTimingProfile("unit");
    return out.str();
}

// The compact endpoints are cycle-exact with the gates: the same cycles in
// every profile, on the small DRAM row both forms can build. NOR program and
// erase are compared in `unit` only (their realistic intervals are millions
// of gate-level cycles).
int compareGates() {
    int failures = 0;
    for (const char* name : {"unit", "embedded", "desktop"}) {
        circuit::timing::setActiveTimingProfile(name);
        const auto period = *circuit::analysis::certifiedCorePeriod(name, "advanced");
        const auto compact = measure(period, false, false), gates = measure(period, true, false);
        for (size_t i = 0; i < compact.size(); ++i) {
            if (!compact[i].simulated || compact[i].cycles > 1000) continue;
            const bool same = compact[i].cycles == gates[i].cycles && gates[i].simulated;
            std::cout << (same ? "same   " : "DIFFER ") << name << " " << compact[i].memory << " / " << compact[i].operation
                      << ": compact " << compact[i].cycles << ", gates " << gates[i].cycles << "\n";
            failures += !same;
        }
    }
    circuit::timing::setActiveTimingProfile("unit");
    return failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "";
        if (mode == "--compare-gates" && argc == 2) return compareGates();
        if ((mode == "--write" || mode == "--check") && argc == 3) {
            const auto text = json();
            if (mode == "--write") {
                std::ofstream(argv[2]) << text;
                std::cout << "wrote " << argv[2] << "\n";
                return 0;
            }
            std::ifstream in(argv[2]);
            std::stringstream stored;
            stored << in.rdbuf();
            if (stored.str() == text) {
                std::cout << "documentation timing data matches " << argv[2] << "\n";
                return 0;
            }
            std::cerr << "documentation timing data differs from " << argv[2] << "; measured:\n" << text;
            return 1;
        }
        std::cerr << "usage: documentation_timing_data --write FILE | --check FILE | --compare-gates\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "documentation_timing_data: " << error.what() << "\n";
        return 1;
    }
}
