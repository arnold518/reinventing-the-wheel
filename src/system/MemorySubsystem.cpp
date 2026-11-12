#include "system/MemorySubsystem.hpp"
#include "system/SystemRegistration.hpp"
#include "system/Json.hpp"
#include "modules/memory/TwoPortMemoryArbiter.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "components/TimingProfile.hpp"
#include <algorithm>
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include <bit>
#include <stdexcept>

namespace circuit::system {
namespace {
constexpr const char* requests[] = {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"};
constexpr const char* responses[] = {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"};
}

void memorySubsystemPins(IOComponent* self, const MemorySubsystemConfig& config) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    memory::protocolPins(self, 32, "I_");
    memory::protocolPins(self, 32, "D_");
    if (config.invalidation_inputs) {
        self->addPin("INV_L1", PinType::INPUT);
        self->addPin("INV_L2", PinType::INPUT);
    }
}

MemorySubsystemConfig memorySubsystemFromParameters(const ParameterMap& p) {
    MemorySubsystemConfig c;
    for (const auto& [key, _] : p)
        if (key != "technology" && key != "words" && key != "caches" && key != "clock_period" && key != "pipelined" &&
            key != "retention_ticks")
            throw std::invalid_argument("Unknown memory subsystem parameter " + key);
    if (const auto it = p.find("technology"); it != p.end()) {
        if (it->second == "ff") c.memory.technology = memory::Technology::FF;
        else if (it->second == "sram") c.memory.technology = memory::Technology::SRAM;
        else if (it->second == "dram") c.memory.technology = memory::Technology::DRAM;
        else throw std::invalid_argument("Memory technology must be ff, sram or dram");
    }
    if (const auto it = p.find("pipelined"); it != p.end()) {
        if (it->second != "0" && it->second != "1") throw std::invalid_argument("Memory subsystem pipelined must be 0 or 1");
        c.arbiter_pipelined = it->second == "1";
    }
    size_t words = 65536;
    if (const auto it = p.find("words"); it != p.end()) words = parseUnsigned(it->second, "memory words");
    if (words < 16 || !std::has_single_bit(words) || words > (size_t{1} << 28))
        throw std::invalid_argument("Memory words must be a power of two from 16");
    c.memory.device.depth_words = words;
    // A real memory as a system would set it up (as in the memory-system
    // experiments of 4.8): SRAM is the on-chip clocked macro, its wait states
    // from the proven access time at the clock; DRAM has the profile's
    // datasheet row size and one REFRESH per tREFI at the clock.
    SimulationTime period = 0;
    if (const auto it = p.find("clock_period"); it != p.end()) period = parseUnsigned(it->second, "clock period");
    const auto& reference = timing::activeTimingProfile().memory;
    if (c.memory.technology == memory::Technology::SRAM) {
        c.memory.device.clocked = true;
        if (period) c.memory.device.access_cycles = memory::staticAccessCycles(32, memory::Technology::SRAM, c.memory.device, period);
    }
    if (c.memory.technology == memory::Technology::DRAM) {
        c.memory.device.words_per_row = std::min(words, reference.dram_row_words);
        // How long a closed row keeps its data (default: the profile's).
        if (const auto it = p.find("retention_ticks"); it != p.end())
            c.memory.device.retention_ticks = parseUnsigned(it->second, "DRAM retention");
        if (period && reference.dram_refresh_interval)
            c.memory.device.refresh_interval_cycles = static_cast<size_t>(std::max<SimulationTime>(24, reference.dram_refresh_interval / period));
    }
    if (p.contains("retention_ticks") && c.memory.technology != memory::Technology::DRAM)
        throw std::invalid_argument("Memory retention_ticks applies to dram only");
    if (const auto it = p.find("caches"); it != p.end()) {
        if (it->second != "none" && it->second != "default") throw std::invalid_argument("Memory caches must be none or default");
        if (it->second == "default") {
            // A small direct-mapped cache per port: 4 sets of 16-byte lines.
            memory::CacheConfig cc;
            cc.sets = 4; cc.block_bytes = 16; cc.backing_depth_words = words; cc.scalable_storage = true;
            c.instruction_cache = cc;
            cc.read_write = true;
            c.data_cache = cc;
        }
    }
    return c;
}

MemorySubsystem::MemorySubsystem(std::string name, MemorySubsystemConfig config)
    : IOComponent(std::move(name), [config](IOComponent* self) { memorySubsystemPins(self, config); }), config_(std::move(config)) {
    if (config_.instruction_cache && config_.instruction_cache->read_write)
        throw std::invalid_argument("The instruction cache must be read only");
    if (config_.data_cache && !config_.data_cache->read_write)
        throw std::invalid_argument("The data cache must accept writes");
    if (config_.shared_cache && !config_.shared_cache->read_write)
        throw std::invalid_argument("The shared cache sits below the arbiter and must accept writes");
}

std::string MemorySubsystem::componentLayoutVariant() const {
    std::string parts;
    if (config_.instruction_cache) parts += "i";
    if (config_.data_cache) parts += "d";
    if (config_.shared_cache) parts += "s";
    return "caches-" + (parts.empty() ? std::string("none") : parts);
}

void MemorySubsystem::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    const auto clocked = [&](const std::shared_ptr<IOComponent>& part) {
        w.connect(getInputPin("CLK"), part->getInputPin("CLK"));
        w.connect(getInputPin("RST"), part->getInputPin("RST"));
    };
    // Upper side's outputs `up` drive lower side's inputs `down`, and back.
    const auto join = [&](const std::shared_ptr<IOComponent>& upper, const std::string& up,
                          const std::shared_ptr<IOComponent>& lower, const std::string& down) {
        for (const auto* n : requests) w.connect(upper->getOutputPinDynamic(up + n), lower->getInputPinDynamic(down + n));
        for (const auto* n : responses) w.connect(lower->getOutputPinDynamic(down + n), upper->getInputPinDynamic(up + n));
    };
    // From this subsystem's own port (an input side) into a part.
    const auto enter = [&](const std::string& port, const std::shared_ptr<IOComponent>& part, const std::string& prefix) {
        for (const auto* n : requests) w.connect(getInputPinDynamic(port + n), part->getInputPinDynamic(prefix + n));
        for (const auto* n : responses) w.connect(part->getOutputPinDynamic(prefix + n), getOutputPinDynamic(port + n));
    };
    const auto cache = [&](const std::string& name, const memory::CacheConfig& settings, const char* invalidation) {
        std::shared_ptr<IOComponent> part = b.addNewComponent<memory::Cache<32>>(name, settings);
        clocked(part);
        if (config_.invalidation_inputs) w.connect(getInputPin(invalidation), part->getInputPin("INV_VALID"));
        else w.connect(w.constant(name + "_NO_INVALIDATION", false), part->getInputPin("INV_VALID"));
        return part;
    };
    arbiter_ = b.addNewComponent<memory::TwoPortMemoryArbiter<32>>("ARBITER", config_.arbiter_block_words, config_.arbiter_pipelined);
    for (const auto* side : {"I_", "D_"}) {
        w.connect(getInputPin("CLK"), arbiter_->getInputPin(std::string(side) + "CLK"));
        w.connect(getInputPin("RST"), arbiter_->getInputPin(std::string(side) + "RST"));
    }
    ram_ = rv32i::buildMemory(b, *this, "RAM", config_.memory);
    view_ = rv32i::memoryView(ram_);
    clocked(ram_);
    for (const bool data : {false, true}) {
        const std::string port = data ? "D_" : "I_";
        const auto& settings = data ? config_.data_cache : config_.instruction_cache;
        if (!settings) { enter(port, arbiter_, port); continue; }
        auto part = cache(data ? "DATA_CACHE" : "INSTRUCTION_CACHE", *settings, "INV_L1");
        (data ? data_cache_ : instruction_cache_) = part;
        enter(port, part, "");
        join(part, "MEM_", arbiter_, port);
    }
    std::shared_ptr<IOComponent> above_memory = arbiter_;
    if (config_.shared_cache) {
        shared_cache_ = cache("SHARED_CACHE", *config_.shared_cache, "INV_L2");
        join(arbiter_, "MEM_", shared_cache_, "");
        above_memory = shared_cache_;
    }
    join(above_memory, "MEM_", ram_, "");
    w.finish();
}

void registerMemorySubsystem(ComponentCatalog& catalog) {
    SystemPart<MemorySubsystemConfig> part;
    part.id = "memory.subsystem.32";
    part.type = MemorySubsystem::TypeName;
    part.test = "MemorySubsystemScenarioTest";
    part.parse = memorySubsystemFromParameters;
    part.pins = [](const MemorySubsystemConfig& c) -> IOComponent::PinInitFunction {
        return [c](IOComponent* self) { memorySubsystemPins(self, c); };
    };
    part.structural = [](const std::string& name, const MemorySubsystemConfig& c, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<MemorySubsystem>(context, name, c);
    };
    registerSystemPart(catalog, std::move(part));
}

} // namespace circuit::system
