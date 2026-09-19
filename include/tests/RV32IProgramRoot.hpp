#pragma once

#include "components/IOComponent.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IProgram.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class ClockGenerator;
class Simulator;
template<size_t WIDTH> class Wire;

/**
 * Fixed testbench container shared by every visual RV32I program scenario.
 *
 * CORE connects through two explicit CPU adapters to universal memory endpoints.
 * The root is a fixed testbench, not a fidelity choice.
 */
class RV32IProgramRoot : public IOComponent {
public:
    RV32IProgramRoot(
        std::string name,
        const circuit::ComponentFamily& core_family,
        size_t clock_half_period,
        rv32i::MemoryConfiguration instructions = {},
        rv32i::MemoryConfiguration data = {});

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
    std::shared_ptr<IOComponent> instructionMemory() const {
        return instruction_memory_;
    }
    std::shared_ptr<IOComponent> dataMemory() const {
        return data_memory_;
    }
    std::shared_ptr<ClockGenerator> clock() const { return clock_; }

private:
    circuit::ComponentFamily core_family_;
    size_t clock_half_period_;
    std::shared_ptr<ClockGenerator> clock_{};
    std::shared_ptr<IOComponent> core_{};
    std::shared_ptr<RV32IStateView> state_view_{};
    std::shared_ptr<IOComponent> instruction_memory_{};
    std::shared_ptr<IOComponent> data_memory_{};

    rv32i::MemoryConfiguration instruction_config_, data_config_;
};
