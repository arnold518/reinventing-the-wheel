#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "modules/utility/Constant.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/RV32IExternalValidationTests.hpp"

#include <charconv>
#include <chrono>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr const char* FiveStageRootName =
    "RV32I_FIVE_STAGE_BENCHMARK_ROOT";
constexpr size_t FiveStageCycleTime = 100;

struct BenchmarkResult {
    std::string core;
    uint32_t tohost = 0;
    uint32_t pc = 0;
    uint64_t instructions = 0;
    size_t cycles = 0;
    size_t load_use_stalls = 0;
    size_t memory_stalls = 0;
    size_t data_port_stalls = 0;
    size_t flushes = 0;
    uint64_t occupied_pipeline_slots = 0;
    SimulatorPerformanceCounters simulator{};
    double host_seconds = 0.0;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void drive(
    Simulator& simulator,
    size_t time,
    const std::shared_ptr<Wire<>>& wire,
    bool value) {
    simulator.scheduleEvent(
        std::make_shared<WireUpdateEvent<>>(
            time,
            wire,
            value ? LogicValue::HIGH : LogicValue::LOW));
}

uint32_t readWord(
    const Memory64Kx32& memory,
    uint32_t address) {
    const auto bytes = memory.readBytes(address, 4);
    return static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8)
        | (static_cast<uint32_t>(bytes[2]) << 16)
        | (static_cast<uint32_t>(bytes[3]) << 24);
}

BenchmarkResult runSingleCycle(
    const std::filesystem::path& elf_path,
    uint32_t tohost_address,
    size_t maximum_cycles) {
    const auto started = std::chrono::steady_clock::now();
    const auto run = rv32i::test::runExternalFixture(
        elf_path,
        circuit::Fidelity::Behavioral,
        tohost_address,
        maximum_cycles,
        false);
    const auto stopped = std::chrono::steady_clock::now();

    BenchmarkResult result;
    result.core = "single-cycle";
    result.tohost = run.tohost;
    result.pc = run.pc;
    result.instructions = run.instruction_count;
    result.cycles = run.hardware_cycles;
    result.simulator = run.simulator_counters;
    result.host_seconds =
        std::chrono::duration<double>(stopped - started).count();
    return result;
}

BenchmarkResult runFiveStage(
    const std::filesystem::path& elf_path,
    uint32_t tohost_address,
    size_t maximum_cycles) {
    const auto started = std::chrono::steady_clock::now();
    const auto image = rv32i::RV32IElfImage::fromFile(elf_path);
    image.requireFitsMemory(Memory64Kx32::capacityBytes());
    require(
        image.entryPoint() == 0,
        "Five-stage benchmark reset vector requires ELF entry zero");

    auto sim = std::make_shared<Simulator>();
    sim->setHistoryRecordingEnabled(false);
    const auto profile = std::make_shared<const circuit::BuildProfile>(
        circuit::strictAllBehavioral());
    auto manifest = std::make_shared<circuit::BuildManifest>(
        profile->name(), profile->fingerprint());
    auto scope = circuit::BuildContext::rootScope(
        circuit::builtinComponentCatalog(),
        profile,
        manifest);
    auto root_context = scope->child(
        FiveStageRootName,
        std::nullopt);
    auto root = Component::createWithContext<Component>(
        root_context,
        FiveStageRootName);
    ComponentBuilder builder(root, root_context);

    auto core = builder.add(
        circuit::families::RV32IFiveStageCore,
        "CORE");
    auto state_view = std::dynamic_pointer_cast<RV32IStateView>(core);
    auto instruction_memory =
        std::dynamic_pointer_cast<Memory64Kx32>(
            builder.add(
                circuit::families::Memory64Kx32,
                "INSTRUCTION_MEMORY"));
    auto data_memory =
        std::dynamic_pointer_cast<Memory64Kx32>(
            builder.add(
                circuit::families::Memory64Kx32,
                "DATA_MEMORY"));
    require(core != nullptr, "Five-stage benchmark core was not built");
    require(
        state_view != nullptr,
        "Five-stage benchmark core lacks architectural state");
    require(
        instruction_memory != nullptr && data_memory != nullptr,
        "Five-stage benchmark memories were not built");

    builder.addNewComponent<ConstantValue<1>>(
        "CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<2>>(
        "CONST_WORD_SIZE", 2);
    builder.addNewComponent<ConstantValue<32>>(
        "CONST_ZERO32", 0);

    auto clk_wire = builder.addNewWire(
        "CLK_IN",
        nullptr,
        {core->getInputPin("CLK"),
         instruction_memory->getInputPin("CLK"),
         data_memory->getInputPin("CLK")});
    auto rst_wire = builder.addNewWire(
        "RST_IN", nullptr, {core->getInputPin("RST")});
    auto enable_wire = builder.addNewWire(
        "ENABLE_IN", nullptr, {core->getInputPin("ENABLE")});

    builder.addNewWire<32>(
        "IMEM_ADDR",
        core->getOutputPin<32>("IMEM_ADDR"),
        {instruction_memory->getInputPin<32>("ADDR")});
    builder.addNewWire(
        "IMEM_READ_EN",
        core->getOutputPin("IMEM_READ_EN"),
        {instruction_memory->getInputPin("READ_EN")});
    builder.addNewWire<32>(
        "IMEM_READ_DATA",
        instruction_memory->getOutputPin<32>("READ_DATA"),
        {core->getInputPin<32>("IMEM_READ_DATA")});
    builder.addNewWire(
        "IMEM_READY",
        instruction_memory->getOutputPin("READY"),
        {core->getInputPin("IMEM_READY")});
    builder.addNewWire(
        "IMEM_FAULT",
        instruction_memory->getOutputPin("FAULT"),
        {core->getInputPin("IMEM_FAULT")});

    builder.addNewWire<32>(
        "DMEM_ADDR",
        core->getOutputPin<32>("DMEM_ADDR"),
        {data_memory->getInputPin<32>("ADDR")});
    builder.addNewWire<32>(
        "DMEM_WRITE_DATA",
        core->getOutputPin<32>("DMEM_WRITE_DATA"),
        {data_memory->getInputPin<32>("WRITE_DATA")});
    builder.addNewWire(
        "DMEM_READ_EN",
        core->getOutputPin("DMEM_READ_EN"),
        {data_memory->getInputPin("READ_EN")});
    builder.addNewWire(
        "DMEM_WRITE_EN",
        core->getOutputPin("DMEM_WRITE_EN"),
        {data_memory->getInputPin("WRITE_EN")});
    builder.addNewWire<2>(
        "DMEM_SIZE",
        core->getOutputPin<2>("DMEM_SIZE"),
        {data_memory->getInputPin<2>("SIZE")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND",
        core->getOutputPin("DMEM_SIGN_EXTEND"),
        {data_memory->getInputPin("SIGN_EXTEND")});
    builder.addNewWire<32>(
        "DMEM_READ_DATA",
        data_memory->getOutputPin<32>("READ_DATA"),
        {core->getInputPin<32>("DMEM_READ_DATA")});
    builder.addNewWire(
        "DMEM_READY",
        data_memory->getOutputPin("READY"),
        {core->getInputPin("DMEM_READY")});
    builder.addNewWire(
        "DMEM_FAULT",
        data_memory->getOutputPin("FAULT"),
        {core->getInputPin("DMEM_FAULT")});

    auto constant_low =
        builder.getComponent<ConstantValue<1>>("CONST_LOW");
    auto constant_word =
        builder.getComponent<ConstantValue<2>>(
            "CONST_WORD_SIZE");
    auto constant_zero =
        builder.getComponent<ConstantValue<32>>(
            "CONST_ZERO32");
    builder.addNewWire(
        "CONST_LOW_fanout",
        constant_low->getOutputPin("OUT"),
        {instruction_memory->getInputPin("WRITE_EN"),
         instruction_memory->getInputPin("SIGN_EXTEND"),
         instruction_memory->getInputPin("RST"),
         data_memory->getInputPin("RST")});
    builder.addNewWire<2>(
        "CONST_WORD_SIZE_to_imem",
        constant_word->getOutputPin<2>("OUT"),
        {instruction_memory->getInputPin<2>("SIZE")});
    builder.addNewWire<32>(
        "CONST_ZERO32_to_imem",
        constant_zero->getOutputPin<32>("OUT"),
        {instruction_memory->getInputPin<32>("WRITE_DATA")});

    instruction_memory->clearContents();
    data_memory->clearContents();
    instruction_memory->setHistoryRecordingEnabled(false);
    data_memory->setHistoryRecordingEnabled(false);
    for (const auto& segment : image.segments()) {
        instruction_memory->loadBytes(
            segment.address, segment.bytes);
        data_memory->loadBytes(
            segment.address, segment.bytes);
    }

    SimulationTest::scheduleInitialEventsForTree(root, *sim, 0);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);
    drive(*sim, 0, enable_wire, true);
    sim->advanceAndRecord(20);
    drive(*sim, 20, rst_wire, false);
    sim->advanceAndRecord(40);
    sim->resetPerformanceCounters();

    const auto load_use_pin = core->getOutputPin("LOAD_USE_STALL");
    const auto memory_stall_pin = core->getOutputPin("MEMORY_STALL");
    const auto data_port_stall_pin =
        core->getOutputPin("DATA_PORT_STALL");
    const auto flush_pin = core->getOutputPin("PIPELINE_FLUSH");
    const std::array<std::shared_ptr<Pin<>>, 4> occupancy_pins{
        core->getOutputPin("IF_ID_VALID"),
        core->getOutputPin("ID_EX_VALID"),
        core->getOutputPin("EX_MEM_VALID"),
        core->getOutputPin("MEM_WB_VALID"),
    };
    const auto retired_write_pin =
        core->getOutputPin("RETIRED_MEM_WRITE");
    const auto retired_address_pin =
        core->getOutputPin<32>("RETIRED_MEM_ADDR");
    const auto halted_pin = core->getOutputPin("HALTED");
    const auto trapped_pin = core->getOutputPin("TRAPPED");
    const auto retired_count_pin =
        core->getOutputPin<32>("RETIRED_COUNT");
    const auto high = [](const std::shared_ptr<Pin<>>& pin) {
        return pin->getValue() == LogicValue::HIGH;
    };

    BenchmarkResult result;
    result.core = "five-stage";
    for (size_t cycle = 0; cycle < maximum_cycles; ++cycle) {
        result.load_use_stalls += high(load_use_pin);
        result.memory_stalls += high(memory_stall_pin);
        result.data_port_stalls += high(data_port_stall_pin);
        result.flushes += high(flush_pin);
        for (const auto& pin : occupancy_pins) {
            result.occupied_pipeline_slots += high(pin);
        }

        const size_t cycle_start = 40 + cycle * FiveStageCycleTime;
        drive(*sim, cycle_start + 20, clk_wire, true);
        drive(*sim, cycle_start + 40, clk_wire, false);
        sim->advanceAndRecord(
            cycle_start + FiveStageCycleTime);
        result.cycles = cycle + 1;
        if (high(retired_write_pin)
            && retired_address_pin->getValueAsUInt64()
                == tohost_address) {
            result.tohost = readWord(
                *data_memory, tohost_address);
        }
        if (result.tohost == 1 || result.tohost == 3) {
            break;
        }
        require(
            !high(halted_pin),
            "Five-stage benchmark halted before writing tohost");
        require(
            !high(trapped_pin),
            "Five-stage benchmark trapped before writing tohost");
    }

    const auto final_state = state_view
        ->snapshotArchitecturalState()
        .toKnownState();
    result.pc = final_state.pc;
    result.instructions = retired_count_pin->getValueAsUInt64();

    require(
        result.tohost != 3,
        "Five-stage benchmark self-check reported failure");
    require(
        result.tohost == 1,
        "Five-stage benchmark timed out before writing tohost");
    result.simulator = sim->getPerformanceCounters();
    const auto stopped = std::chrono::steady_clock::now();
    result.host_seconds =
        std::chrono::duration<double>(stopped - started).count();
    return result;
}

uint64_t parseUnsigned(
    const std::string& text,
    const std::string& label) {
    const bool hexadecimal =
        text.size() > 2
        && text[0] == '0'
        && (text[1] == 'x' || text[1] == 'X');
    const char* begin = text.data() + (hexadecimal ? 2 : 0);
    const char* end = text.data() + text.size();
    uint64_t value = 0;
    const auto parsed = std::from_chars(
        begin,
        end,
        value,
        hexadecimal ? 16 : 10);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw std::invalid_argument(
            "Invalid " + label + ": " + text);
    }
    return value;
}

void printResult(const BenchmarkResult& result) {
    const double cpi = result.instructions == 0
        ? 0.0
        : static_cast<double>(result.cycles)
            / static_cast<double>(result.instructions);
    const double occupancy = result.cycles == 0
        ? 0.0
        : static_cast<double>(result.occupied_pipeline_slots)
            / static_cast<double>(result.cycles * 4ULL);
    std::cout << std::fixed << std::setprecision(6)
              << "RESULT core=" << result.core
              << " verdict=" << result.tohost
              << " instructions=" << result.instructions
              << " cycles=" << result.cycles
              << " cpi=" << cpi
              << " load_use_stalls=" << result.load_use_stalls
              << " memory_stalls=" << result.memory_stalls
              << " data_port_stalls=" << result.data_port_stalls
              << " flushes=" << result.flushes
              << " occupancy=" << occupancy
              << " processed_events="
              << result.simulator.processed_events
              << " maximum_queue_depth="
              << result.simulator.maximum_event_queue_depth
              << " host_seconds=" << result.host_seconds
              << '\n';
}

void usage(const char* executable) {
    std::cerr
        << "Usage: " << executable
        << " <rv32i-elf> [tohost-address] [maximum-cycles] [core]\n"
        << "Defaults: tohost-address=0x3ffc0, maximum-cycles=10000000,"
        << " core=both\n"
        << "Core: single-cycle, five-stage, or both\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 5) {
        usage(argv[0]);
        return 2;
    }
    try {
        const std::filesystem::path elf_path(argv[1]);
        const auto tohost = argc >= 3
            ? parseUnsigned(argv[2], "tohost address")
            : uint64_t{0x0003ffc0U};
        const auto maximum_cycles = argc >= 4
            ? parseUnsigned(argv[3], "maximum cycle count")
            : uint64_t{10000000U};
        require(tohost <= UINT32_MAX, "tohost address exceeds RV32");
        require(
            maximum_cycles > 0 && maximum_cycles <= SIZE_MAX,
            "maximum cycle count exceeds host width");
        const std::string core = argc >= 5 ? argv[4] : "both";

        if (core == "single-cycle" || core == "both") {
            printResult(runSingleCycle(
                elf_path,
                static_cast<uint32_t>(tohost),
                static_cast<size_t>(maximum_cycles)));
        }
        if (core == "five-stage" || core == "both") {
            printResult(runFiveStage(
                elf_path,
                static_cast<uint32_t>(tohost),
                static_cast<size_t>(maximum_cycles)));
        }
        if (core != "single-cycle"
            && core != "five-stage"
            && core != "both") {
            throw std::invalid_argument("Invalid core: " + core);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
