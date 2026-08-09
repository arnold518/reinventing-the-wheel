#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IProgram.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class ClockGenerator;
class Memory64Kx32;
class Simulator;
template<size_t WIDTH> class Wire;

/**
 * Fixed testbench container shared by every visual RV32I program scenario.
 *
 * The root deliberately has exactly four children at one level: CLOCK, CORE,
 * INSTRUCTION_MEMORY, and DATA_MEMORY.  The root is not a fidelity choice;
 * the recursive BuildProfile starts at CORE and the two memories.
 */
class RV32IProgramRoot : public IOComponent {
public:
    RV32IProgramRoot(
        std::string name,
        const circuit::ComponentFamily& core_family,
        size_t clock_half_period);

    static constexpr const char* TypeName = "RV32IProgramRoot";
    static constexpr const char* RootName = "RV32I_PROGRAM_ROOT";
    const char* getTypeName() const override { return TypeName; }

    void buildInternals(ComponentBuilder& builder) override;

    void initializeFixedInputs(
        Simulator& simulator,
        size_t time = 0) const;
    void startClock(Simulator& simulator, size_t first_rising_time) const;

    void clearInstructionMemory();
    void clearDataMemory();
    void loadProgram(
        const rv32i::RV32IProgram& program,
        uint32_t base_address = 0);
    void loadInstructionBytes(
        uint32_t base_address,
        const std::vector<uint8_t>& bytes);
    void loadDataBytes(
        uint32_t base_address,
        const std::vector<uint8_t>& bytes);
    std::vector<uint8_t> readDataBytes(
        uint32_t base_address,
        size_t count) const;
    void setMemoryHistoryRecordingEnabled(bool enabled);
    std::map<uint32_t, uint8_t> dataMemoryWritesInTimeRange(
        size_t start_time,
        size_t end_time) const;
    rv32i::RV32IArchitecturalState snapshotArchitecturalState() const;

    std::shared_ptr<IOComponent> core() const { return core_; }
    std::shared_ptr<RV32IStateView> stateView() const {
        return state_view_;
    }
    std::shared_ptr<Memory64Kx32> instructionMemory() const {
        return instruction_memory_;
    }
    std::shared_ptr<Memory64Kx32> dataMemory() const {
        return data_memory_;
    }
    std::shared_ptr<ClockGenerator> clock() const { return clock_; }

private:
    circuit::ComponentFamily core_family_;
    size_t clock_half_period_;
    std::shared_ptr<ClockGenerator> clock_{};
    std::shared_ptr<IOComponent> core_{};
    std::shared_ptr<RV32IStateView> state_view_{};
    std::shared_ptr<Memory64Kx32> instruction_memory_{};
    std::shared_ptr<Memory64Kx32> data_memory_{};

    std::shared_ptr<Wire<1>> imem_write_enable_{};
    std::shared_ptr<Wire<1>> imem_sign_extend_{};
    std::shared_ptr<Wire<1>> imem_reset_{};
    std::shared_ptr<Wire<1>> dmem_reset_{};
    std::shared_ptr<Wire<2>> imem_size_{};
    std::shared_ptr<Wire<32>> imem_write_data_{};
};
