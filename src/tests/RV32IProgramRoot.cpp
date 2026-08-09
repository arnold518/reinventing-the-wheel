#include "tests/RV32IProgramRoot.hpp"

#include "basic/Wire.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <stdexcept>
#include <utility>

namespace {
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

RV32IProgramRoot::RV32IProgramRoot(
    std::string name,
    const circuit::ComponentFamily& core_family,
    size_t clock_half_period)
    : IOComponent(std::move(name), initializePins),
      core_family_(core_family),
      clock_half_period_(clock_half_period) {
    if (clock_half_period_ == 0) {
        throw std::invalid_argument(
            "RV32I program clock half-period must be positive");
    }
}

void RV32IProgramRoot::buildInternals(ComponentBuilder& builder) {
    clock_ = builder.addNewComponent<ClockGenerator>(
        "CLOCK", clock_half_period_);
    core_ = builder.add(core_family_, "CORE");
    instruction_memory_ = std::dynamic_pointer_cast<Memory64Kx32>(
        builder.add(
            circuit::families::Memory64Kx32,
            "INSTRUCTION_MEMORY"));
    data_memory_ = std::dynamic_pointer_cast<Memory64Kx32>(
        builder.add(
            circuit::families::Memory64Kx32,
            "DATA_MEMORY"));
    state_view_ = std::dynamic_pointer_cast<RV32IStateView>(core_);

    requireBuilt(clock_ != nullptr, "clock");
    requireBuilt(core_ != nullptr, "core");
    requireBuilt(state_view_ != nullptr, "architectural state view");
    requireBuilt(
        instruction_memory_ != nullptr && data_memory_ != nullptr,
        "memories");

    builder.addNewWire(
        "CLK",
        clock_->getOutputPin("CLK_OUT"),
        {core_->getInputPin("CLK"),
         instruction_memory_->getInputPin("CLK"),
         data_memory_->getInputPin("CLK")});
    builder.addNewWire(
        "RST",
        getInputPin("RST"),
        {core_->getInputPin("RST")});
    builder.addNewWire(
        "ENABLE",
        getInputPin("ENABLE"),
        {core_->getInputPin("ENABLE")});

    builder.addNewWire<32>(
        "PC",
        core_->getOutputPin<32>("PC"),
        {getOutputPin<32>("PC")});
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

    builder.addNewWire<32>(
        "IMEM_ADDR",
        core_->getOutputPin<32>("IMEM_ADDR"),
        {instruction_memory_->getInputPin<32>("ADDR")});
    builder.addNewWire(
        "IMEM_READ_EN",
        core_->getOutputPin("IMEM_READ_EN"),
        {instruction_memory_->getInputPin("READ_EN")});
    builder.addNewWire<32>(
        "IMEM_READ_DATA",
        instruction_memory_->getOutputPin<32>("READ_DATA"),
        {core_->getInputPin<32>("IMEM_READ_DATA")});
    builder.addNewWire(
        "IMEM_READY",
        instruction_memory_->getOutputPin("READY"),
        {core_->getInputPin("IMEM_READY")});
    builder.addNewWire(
        "IMEM_FAULT",
        instruction_memory_->getOutputPin("FAULT"),
        {core_->getInputPin("IMEM_FAULT")});

    builder.addNewWire<32>(
        "DMEM_ADDR",
        core_->getOutputPin<32>("DMEM_ADDR"),
        {data_memory_->getInputPin<32>("ADDR")});
    builder.addNewWire<32>(
        "DMEM_WRITE_DATA",
        core_->getOutputPin<32>("DMEM_WRITE_DATA"),
        {data_memory_->getInputPin<32>("WRITE_DATA")});
    builder.addNewWire(
        "DMEM_READ_EN",
        core_->getOutputPin("DMEM_READ_EN"),
        {data_memory_->getInputPin("READ_EN")});
    builder.addNewWire(
        "DMEM_WRITE_EN",
        core_->getOutputPin("DMEM_WRITE_EN"),
        {data_memory_->getInputPin("WRITE_EN")});
    builder.addNewWire<2>(
        "DMEM_SIZE",
        core_->getOutputPin<2>("DMEM_SIZE"),
        {data_memory_->getInputPin<2>("SIZE")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND",
        core_->getOutputPin("DMEM_SIGN_EXTEND"),
        {data_memory_->getInputPin("SIGN_EXTEND")});
    builder.addNewWire<32>(
        "DMEM_READ_DATA",
        data_memory_->getOutputPin<32>("READ_DATA"),
        {core_->getInputPin<32>("DMEM_READ_DATA")});
    builder.addNewWire(
        "DMEM_READY",
        data_memory_->getOutputPin("READY"),
        {core_->getInputPin("DMEM_READY")});
    builder.addNewWire(
        "DMEM_FAULT",
        data_memory_->getOutputPin("FAULT"),
        {core_->getInputPin("DMEM_FAULT")});

    // These are fixed testbench stimuli, not additional visual components.
    imem_write_enable_ = builder.addNewWire(
        "IMEM_WRITE_ENABLE_FIXED",
        nullptr,
        {instruction_memory_->getInputPin("WRITE_EN")});
    imem_sign_extend_ = builder.addNewWire(
        "IMEM_SIGN_EXTEND_FIXED",
        nullptr,
        {instruction_memory_->getInputPin("SIGN_EXTEND")});
    imem_reset_ = builder.addNewWire(
        "IMEM_RESET_FIXED",
        nullptr,
        {instruction_memory_->getInputPin("RST")});
    dmem_reset_ = builder.addNewWire(
        "DMEM_RESET_FIXED",
        nullptr,
        {data_memory_->getInputPin("RST")});
    imem_size_ = builder.addNewWire<2>(
        "IMEM_SIZE_FIXED",
        nullptr,
        {instruction_memory_->getInputPin<2>("SIZE")});
    imem_write_data_ = builder.addNewWire<32>(
        "IMEM_WRITE_DATA_FIXED",
        nullptr,
        {instruction_memory_->getInputPin<32>("WRITE_DATA")});
}

void RV32IProgramRoot::initializeFixedInputs(
    Simulator& simulator,
    size_t time) const {
    requireBuilt(
        imem_write_enable_ && imem_sign_extend_
            && imem_reset_ && dmem_reset_
            && imem_size_ && imem_write_data_,
        "fixed memory inputs");
    drive(simulator, time, imem_write_enable_, 0);
    drive(simulator, time, imem_sign_extend_, 0);
    drive(simulator, time, imem_reset_, 0);
    drive(simulator, time, dmem_reset_, 0);
    drive(simulator, time, imem_size_, 2);
    drive(simulator, time, imem_write_data_, 0);
}

void RV32IProgramRoot::startClock(
    Simulator& simulator,
    size_t first_rising_time) const {
    requireBuilt(clock_ != nullptr, "clock start");
    clock_->startClock(simulator, first_rising_time);
}

void RV32IProgramRoot::clearInstructionMemory() {
    requireBuilt(instruction_memory_ != nullptr, "clear instruction memory");
    instruction_memory_->clearContents();
}

void RV32IProgramRoot::clearDataMemory() {
    requireBuilt(data_memory_ != nullptr, "clear data memory");
    data_memory_->clearContents();
}

void RV32IProgramRoot::loadProgram(
    const rv32i::RV32IProgram& program,
    uint32_t base_address) {
    requireBuilt(instruction_memory_ != nullptr, "load program");
    program.loadInto(*instruction_memory_, base_address);
}

void RV32IProgramRoot::loadInstructionBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& bytes) {
    requireBuilt(instruction_memory_ != nullptr, "load instruction bytes");
    instruction_memory_->loadBytes(base_address, bytes);
}

void RV32IProgramRoot::loadDataBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& bytes) {
    requireBuilt(data_memory_ != nullptr, "load data bytes");
    data_memory_->loadBytes(base_address, bytes);
}

std::vector<uint8_t> RV32IProgramRoot::readDataBytes(
    uint32_t base_address,
    size_t count) const {
    requireBuilt(data_memory_ != nullptr, "read data bytes");
    return data_memory_->readBytes(base_address, count);
}

void RV32IProgramRoot::setMemoryHistoryRecordingEnabled(bool enabled) {
    requireBuilt(
        instruction_memory_ != nullptr && data_memory_ != nullptr,
        "memory history");
    instruction_memory_->setHistoryRecordingEnabled(enabled);
    data_memory_->setHistoryRecordingEnabled(enabled);
}

std::map<uint32_t, uint8_t>
RV32IProgramRoot::dataMemoryWritesInTimeRange(
    size_t start_time,
    size_t end_time) const {
    requireBuilt(data_memory_ != nullptr, "data-memory history");
    return data_memory_->getByteWritesInTimeRange(
        start_time, end_time);
}

rv32i::RV32IArchitecturalState
RV32IProgramRoot::snapshotArchitecturalState() const {
    requireBuilt(state_view_ != nullptr, "architectural state");
    return state_view_->snapshotArchitecturalState();
}
