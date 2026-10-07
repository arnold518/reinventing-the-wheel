#include "tests/RV32IProgramRoot.hpp"

#include "basic/Wire.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <stdexcept>
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
    rv32i::MemoryConfiguration data, circuit::ParameterMap core_parameters)
    : IOComponent(std::move(name), initializePins),
      core_family_(core_family),
      core_parameters_(std::move(core_parameters)),
      clock_half_period_(clock_half_period),
      instruction_config_(std::move(instructions)), data_config_(std::move(data)) {
    if (clock_half_period_ == 0) {
        throw std::invalid_argument(
            "RV32I program clock half-period must be positive");
    }
}

void RV32IProgramRoot::buildInternals(ComponentBuilder& builder) {
    clock_ = builder.addNewComponent<ClockGenerator>(
        "CLOCK", clock_half_period_);
    core_ = builder.add(core_family_, "CORE", core_parameters_);
    const bool fixed_cycle=core_family_.id()=="rv32i.core.educational-single-cycle" || core_family_.id()=="rv32i.core.baseline-pipeline";
    if(fixed_cycle&&(instruction_config_.technology!=circuit::memory::Technology::FF || data_config_.technology!=circuit::memory::Technology::FF))
        throw std::invalid_argument("Fixed-cycle cores require zero-wait FF endpoints");
    instruction_memory_ = rv32i::buildMemory(builder, *this, "INSTRUCTION_MEMORY", instruction_config_);
    data_memory_ = rv32i::buildMemory(builder, *this, "DATA_MEMORY", data_config_);
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

    rv32i::wireMemorySystem(builder, core_, instruction_memory_, data_memory_,
        clock_->getOutputPin("CLK_OUT"), getInputPin("RST"));
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
    rv32i::memoryView(instruction_memory_)->clearMemoryImage();
}

void RV32IProgramRoot::clearDataMemory() {
    requireBuilt(data_memory_ != nullptr, "clear data memory");
    rv32i::memoryView(data_memory_)->clearMemoryImage();
}

void RV32IProgramRoot::loadProgram(
    const rv32i::RV32IProgram& program,
    uint32_t base_address) {
    requireBuilt(instruction_memory_ != nullptr, "load program");
    rv32i::memoryView(instruction_memory_)->initializeMemoryBytes(base_address, program.bytes());
}

void RV32IProgramRoot::loadInstructionBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& bytes) {
    requireBuilt(instruction_memory_ != nullptr, "load instruction bytes");
    rv32i::memoryView(instruction_memory_)->initializeMemoryBytes(base_address, bytes);
}

void RV32IProgramRoot::loadDataBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& bytes) {
    requireBuilt(data_memory_ != nullptr, "load data bytes");
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
