#include "tests/RV32IProgramRoot.hpp"

#include "basic/Wire.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/ClockGenerator.hpp"
#include "analysis/ExperimentClock.hpp"
#include "components/TimingProfile.hpp"
#include <stdexcept>
#include <tuple>
#include <string>
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "modules/memory/TwoPortMemoryArbiter.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {
rv32i::MemoryConfiguration defaultMemory(const circuit::ComponentFamily& family) {
    return (family.id() == "rv32i.core.educational-single-cycle" || family.id() == "rv32i.core.baseline-pipeline")
        ? rv32i::zeroWaitMemoryConfiguration() : rv32i::MemoryConfiguration{};
}
void initializePins(IOComponent* self) {
    self->addPin("RST", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin<32>("PC", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
}

template<size_t Width>
void drive(
    Simulator& simulator,
    size_t time,
    const std::shared_ptr<Wire<Width>>& wire,
    uint64_t value) {
    simulator.scheduleEvent(
        std::make_shared<WireUpdateEvent<Width>>(
            time, wire, value));
}

void requireBuilt(bool condition, const char* operation) {
    if (!condition) {
        throw std::logic_error(
            std::string("RV32IProgramRoot is not built: ") + operation);
    }
}
} // namespace

RV32IProgramRoot::RV32IProgramRoot(std::string name,
    const circuit::ComponentFamily& core_family, size_t clock_half_period, circuit::ParameterMap core_parameters)
    : RV32IProgramRoot(std::move(name), core_family, clock_half_period,
                      defaultMemory(core_family), defaultMemory(core_family), std::move(core_parameters)) {}

RV32IProgramRoot::RV32IProgramRoot(
    std::string name,
    const circuit::ComponentFamily& core_family,
    size_t clock_half_period,
    rv32i::MemoryConfiguration instructions,
    rv32i::MemoryConfiguration data, circuit::ParameterMap core_parameters,
    std::optional<circuit::memory::CacheConfig> instruction_cache,
    std::optional<circuit::memory::CacheConfig> data_cache, bool unified_memory,
    std::optional<circuit::memory::CacheConfig> shared_cache)
    : IOComponent(std::move(name), initializePins),
      core_family_(core_family),
      core_parameters_(std::move(core_parameters)),
      clock_half_period_(clock_half_period),
      instruction_config_(std::move(instructions)), data_config_(std::move(data)),
      instruction_cache_config_(std::move(instruction_cache)),
      data_cache_config_(std::move(data_cache)), shared_cache_config_(std::move(shared_cache)),
      unified_memory_(unified_memory) {
    if (clock_half_period_ == 0) {
        throw std::invalid_argument(
            "RV32I program clock half-period must be positive");
    }
    if (shared_cache_config_ && (!unified_memory_ || !shared_cache_config_->read_write))
        throw std::invalid_argument("A shared cache sits below the arbiter of a unified memory and accepts writes");
    if ((instruction_cache_config_ || data_cache_config_) &&
        core_family_.id() != circuit::families::RV32IFiveStageCore.id())
        throw std::invalid_argument("Caches currently require the advanced five-stage core");
    if (instruction_cache_config_ && instruction_cache_config_->read_write)
        throw std::invalid_argument("Instruction cache must be read only");
    if (data_cache_config_ && !data_cache_config_->read_write)
        throw std::invalid_argument("Data cache must accept writes");
    if (unified_memory_ &&
        (core_family_.id() != circuit::families::RV32IFiveStageCore.id() ||
         instruction_config_.technology != data_config_.technology ||
         instruction_config_.expanded_endpoint != data_config_.expanded_endpoint ||
         circuit::memory::endpointParameters(instruction_config_.device) !=
             circuit::memory::endpointParameters(data_config_.device)))
        throw std::invalid_argument("Unified memory requires the advanced core and one matching backing configuration");
}

void RV32IProgramRoot::buildInternals(ComponentBuilder& builder) {
    clock_ = builder.addNewComponent<ClockGenerator>(
        "CLOCK", clock_half_period_);
    core_ = builder.add(core_family_, "CORE", core_parameters_);
    const bool fixed_cycle=core_family_.id()=="rv32i.core.educational-single-cycle" || core_family_.id()=="rv32i.core.baseline-pipeline";
    if(fixed_cycle&&(instruction_config_.technology!=circuit::memory::Technology::FF || data_config_.technology!=circuit::memory::Technology::FF))
        throw std::invalid_argument("Fixed-cycle cores require zero-wait FF endpoints");
    if (unified_memory_) {
        data_memory_ = rv32i::buildMemory(builder, *this, "SHARED_MEMORY", data_config_);
        instruction_memory_ = data_memory_;
    } else {
        instruction_memory_ = rv32i::buildMemory(builder, *this, "INSTRUCTION_MEMORY", instruction_config_);
        data_memory_ = rv32i::buildMemory(builder, *this, "DATA_MEMORY", data_config_);
    }
    auto instruction_port = instruction_memory_;
    auto data_port = data_memory_;
    circuit::detail::StructuralWiring cache_wiring(builder);
    auto make_cache = [&](const std::string& name, const circuit::memory::CacheConfig& config) {
        auto cache = builder.addNewComponent<circuit::memory::Cache<32>>(name, config);
        cache_wiring.connect(cache_wiring.constant(name + "_NO_INVALIDATION", false),
                             cache->getInputPin("INV_VALID"));
        return std::static_pointer_cast<IOComponent>(cache);
    };
    auto connect_backing = [&](const std::shared_ptr<IOComponent>& requester,
                               const std::shared_ptr<IOComponent>& backing,
                               const std::string& requester_prefix) {
        for (const auto& [pin_name, pin] : backing->getAllInputPins())
            cache_wiring.connect(requester->getOutputPinDynamic(requester_prefix + pin_name), pin);
        for (const auto& [pin_name, pin] : backing->getAllOutputPins())
            cache_wiring.connect(pin, requester->getInputPinDynamic(requester_prefix + pin_name));
    };
    std::string instruction_prefix, data_prefix;
    if (instruction_cache_config_)
        instruction_port = make_cache("INSTRUCTION_CACHE", *instruction_cache_config_);
    if (data_cache_config_)
        data_port = make_cache("DATA_CACHE", *data_cache_config_);
    if (unified_memory_) {
        // Each side reaches the arbiter through its cache, or directly; below
        // the arbiter the shared cache, if any, then the one memory.
        auto arbiter = builder.addNewComponent<circuit::memory::TwoPortMemoryArbiter<32>>("MEMORY_ARBITER");
        const auto link = [&](const std::shared_ptr<IOComponent>& upper, const std::string& up,
                              const std::shared_ptr<IOComponent>& lower, const std::string& down) {
            for (auto name : {"CLK", "RST", "REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"})
                cache_wiring.connect(upper->getOutputPinDynamic(up + name), lower->getInputPinDynamic(down + name));
            for (auto name : {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"})
                cache_wiring.connect(lower->getOutputPinDynamic(down + name), upper->getInputPinDynamic(up + name));
        };
        for (auto [port, prefix, cached] : {std::tuple{&instruction_port, &instruction_prefix, bool(instruction_cache_config_)},
                                            std::tuple{&data_port, &data_prefix, bool(data_cache_config_)}}) {
            const std::string side = port == &instruction_port ? "I_" : "D_";
            if (cached) link(*port, "MEM_", arbiter, side);
            else { *port = arbiter; *prefix = side; }
        }
        std::shared_ptr<IOComponent> above_memory = arbiter;
        if (shared_cache_config_) {
            auto shared = make_cache("SHARED_CACHE", *shared_cache_config_);
            link(arbiter, "MEM_", shared, "");
            above_memory = shared;
        }
        connect_backing(above_memory, data_memory_, "MEM_");
    } else {
        if (instruction_cache_config_) connect_backing(instruction_port, instruction_memory_, "MEM_");
        if (data_cache_config_) connect_backing(data_port, data_memory_, "MEM_");
    }
    cache_wiring.finish();
    state_view_ = std::dynamic_pointer_cast<RV32IStateView>(core_);
    if (auto pipeline = std::dynamic_pointer_cast<RV32IPipelineStateView>(core_)) {
        pipeline_observer_ = std::make_shared<RV32IPipelineObserver>(pipeline);
        state_view_ = pipeline_observer_;
    }

    requireBuilt(clock_ != nullptr, "clock");
    requireBuilt(core_ != nullptr, "core");
    requireBuilt(state_view_ != nullptr, "architectural state view");
    requireBuilt(
        instruction_memory_ != nullptr && data_memory_ != nullptr,
        "memories");

    rv32i::wireMemorySystem(builder, core_, instruction_port, data_port,
        clock_->getOutputPin("CLK_OUT"), getInputPin("RST"), instruction_prefix, data_prefix);
    builder.addNewWire(
        "ENABLE",
        getInputPin("ENABLE"),
        {core_->getInputPin("ENABLE")});

    if (pipeline_observer_) {
        observed_pc_wire_ = builder.addNewWire<32>("OBSERVED_PC", nullptr, {getOutputPin<32>("PC")});
    } else {
        builder.addNewWire<32>("PC", core_->getOutputPin<32>("PC"), {getOutputPin<32>("PC")});
    }
    builder.addNewWire(
        "HALTED",
        core_->getOutputPin("HALTED"),
        {getOutputPin("HALTED")});
    builder.addNewWire(
        "TRAPPED",
        core_->getOutputPin("TRAPPED"),
        {getOutputPin("TRAPPED")});
    builder.addNewWire<4>(
        "TRAP_CAUSE",
        core_->getOutputPin<4>("TRAP_CAUSE"),
        {getOutputPin<4>("TRAP_CAUSE")});

}

void RV32IProgramRoot::initializeFixedInputs(Simulator& simulator, size_t time) const {
    if (pipeline_observer_) {
        pipeline_observer_->reset();
        drive(simulator, time, observed_pc_wire_, 0);
    }
}

void RV32IProgramRoot::observeRetirementBeforeEdge(Simulator& simulator, size_t edge_time) {
    if (!pipeline_observer_) return;
    if (getInputPin("RST")->getValue() == LogicValue::HIGH) {
        pipeline_observer_->reset();
    } else if (core_->getOutputPin("COMMIT_VALID")->getValue() == LogicValue::HIGH) {
        const auto view = std::dynamic_pointer_cast<RV32IRetirementView>(core_);
        if (!view) throw std::logic_error("Missing pipeline retirement view");
        pipeline_observer_->retire(view->pendingRetirement());
    }
    drive(simulator, edge_time, observed_pc_wire_, pipeline_observer_->pc());
}

void RV32IProgramRoot::startClock(
    Simulator& simulator,
    size_t first_rising_time) const {
    requireBuilt(clock_ != nullptr, "clock start");
    clock_->startClock(simulator, first_rising_time);
}

void RV32IProgramRoot::clearInstructionMemory() {
    requireBuilt(instruction_memory_ != nullptr, "clear instruction memory");
    unified_image_.clear();
    rv32i::memoryView(instruction_memory_)->clearMemoryImage();
}

void RV32IProgramRoot::clearDataMemory() {
    requireBuilt(data_memory_ != nullptr, "clear data memory");
    unified_image_.clear();
    rv32i::memoryView(data_memory_)->clearMemoryImage();
}

void RV32IProgramRoot::loadProgram(
    const rv32i::RV32IProgram& program,
    uint32_t base_address) {
    requireBuilt(instruction_memory_ != nullptr, "load program");
    placeUnified(base_address, program.bytes());
    rv32i::memoryView(instruction_memory_)->initializeMemoryBytes(base_address, program.bytes());
}

void RV32IProgramRoot::placeUnified(uint32_t base_address, const std::vector<uint8_t>& bytes) {
    if (!unified_memory_) return;
    for (size_t i = 0; i < bytes.size(); ++i) {
        const uint32_t address = base_address + uint32_t(i);
        const auto [placed, inserted] = unified_image_.emplace(address, bytes[i]);
        if (!inserted && placed->second != bytes[i])
            throw std::invalid_argument("Instruction and data images overlap at byte address "
                + std::to_string(address) + "; a unified memory needs them apart");
    }
}

void RV32IProgramRoot::loadInstructionBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& bytes) {
    requireBuilt(instruction_memory_ != nullptr, "load instruction bytes");
    placeUnified(base_address, bytes);
    rv32i::memoryView(instruction_memory_)->initializeMemoryBytes(base_address, bytes);
}

void RV32IProgramRoot::loadDataBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& bytes) {
    requireBuilt(data_memory_ != nullptr, "load data bytes");
    placeUnified(base_address, bytes);
    rv32i::memoryView(data_memory_)->initializeMemoryBytes(base_address, bytes);
}

std::vector<uint8_t> RV32IProgramRoot::readDataBytes(
    uint32_t base_address,
    size_t count) const {
    requireBuilt(data_memory_ != nullptr, "read data bytes");
    return rv32i::memoryView(data_memory_)->memoryStorage().readBytes(base_address, count);
}

void RV32IProgramRoot::setMemoryHistoryRecordingEnabled(bool enabled) {
    requireBuilt(
        instruction_memory_ != nullptr && data_memory_ != nullptr,
        "memory history");
    rv32i::memoryView(instruction_memory_)->setMemoryHistoryRecordingEnabled(enabled);
    rv32i::memoryView(data_memory_)->setMemoryHistoryRecordingEnabled(enabled);
}

std::map<uint32_t, uint8_t>
RV32IProgramRoot::dataMemoryWritesInTimeRange(
    size_t start_time,
    size_t end_time) const {
    requireBuilt(data_memory_ != nullptr, "data-memory history");
    return rv32i::memoryView(data_memory_)->memoryStorage().getByteWritesInTimeRange(
        start_time, end_time);
}

rv32i::RV32IArchitecturalState
RV32IProgramRoot::snapshotArchitecturalState() const {
    requireBuilt(state_view_ != nullptr, "architectural state");
    return state_view_->snapshotArchitecturalState();
}

size_t rv32iFixtureTimeScale(std::string_view core, size_t unit_cycle_ticks) {
    const auto& profile = circuit::timing::activeTimingProfile();
    if (profile.name == "unit") return 1;
    const auto period = circuit::analysis::certifiedCorePeriod(profile.name, core);
    if (!period || unit_cycle_ticks == 0)
        throw std::runtime_error("No certified clock for the " + std::string(core)
            + " core under timing profile " + profile.name);
    return std::max<size_t>(1, (*period + unit_cycle_ticks - 1) / unit_cycle_ticks);
}

const char* rv32iMemoryArrangementName(RV32IMemoryArrangement arrangement) {
    switch (arrangement) {
    case RV32IMemoryArrangement::Shared: return "B";
    case RV32IMemoryArrangement::SharedL1: return "C-ID";
    case RV32IMemoryArrangement::SharedL2: return "D-0";
    case RV32IMemoryArrangement::SharedL1L2: return "D-ID";
    default: return "A";
    }
}

RV32IArrangementCaches rv32iArrangementCaches(RV32IMemoryArrangement arrangement, const rv32i::MemoryConfiguration& memory) {
    RV32IArrangementCaches caches;
    caches.unified = arrangement != RV32IMemoryArrangement::Separate;
    const bool l1 = arrangement == RV32IMemoryArrangement::SharedL1 || arrangement == RV32IMemoryArrangement::SharedL1L2;
    const bool l2 = arrangement == RV32IMemoryArrangement::SharedL2 || arrangement == RV32IMemoryArrangement::SharedL1L2;
    if (l1) {
        circuit::memory::CacheConfig instruction, data;
        instruction.sets = data.sets = 4;
        instruction.backing_depth_words = data.backing_depth_words = memory.device.depth_words;
        data.read_write = true;
        caches.instruction = instruction;
        caches.data = data;
    }
    if (l2) {
        circuit::memory::CacheConfig shared;
        shared.sets = 4;
        shared.ways = 2;
        shared.backing_depth_words = memory.device.depth_words;
        shared.read_write = true;
        caches.shared = shared;
    }
    return caches;
}

std::string rv32iMemoryArrangementTestName(RV32IMemoryArrangement arrangement) {
    std::string letters;
    for (const char c : std::string(rv32iMemoryArrangementName(arrangement))) if (c != '-') letters += c;
    return "RV32IMemoryArrangement" + letters + "Test";
}
