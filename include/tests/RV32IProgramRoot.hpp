#pragma once

#include "components/IOComponent.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/memory/Cache.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IProgram.hpp"
#include "rv32i/RV32IPipelineObserver.hpp"
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class ClockGenerator;

// The program fixtures' times (clock half-period, reset release, cycle step,
// sampling points) are written for the `unit` profile. Under another
// profile they are all multiplied by this factor: the smallest whole number
// that makes the fixture's cycle at least the clock the analysis certifies
// for `core` ("single", "baseline", "advanced") in that profile. 1 in `unit`.
size_t rv32iFixtureTimeScale(std::string_view core, size_t unit_cycle_ticks);
class Simulator;
template<size_t WIDTH> class Wire;

// How the advanced core's instruction and data ports reach memory (§4.7):
// A, two separate memories; B, one shared memory through the two-port
// arbiter; C-ID, an instruction and a data cache above the arbiter; D-0, a
// shared cache below it; D-ID, both.
enum class RV32IMemoryArrangement { Separate, Shared, SharedL1, SharedL2, SharedL1L2 };
const char* rv32iMemoryArrangementName(RV32IMemoryArrangement);
// The registered scenario of a shared arrangement other than C-ID
// (RV32ISharedMemoryProgramTest), for example "RV32IMemoryArrangementD0Test".
std::string rv32iMemoryArrangementTestName(RV32IMemoryArrangement);
// The caches an arrangement's scenario uses over `memory` (small, so the
// visualizer can show them): two 4-set L1s, a 4-set two-way L2.
struct RV32IArrangementCaches {
    std::optional<circuit::memory::CacheConfig> instruction, data, shared;
    bool unified = false;
};
RV32IArrangementCaches rv32iArrangementCaches(RV32IMemoryArrangement, const rv32i::MemoryConfiguration& memory);

/**
 * Fixed testbench container shared by every visual RV32I program scenario.
 *
 * Single-cycle and baseline CORE connect directly to FF devices; advanced
 * pipeline CORE uses transaction endpoints, separately or through the
 * two-port arbiter to one shared memory, with or without caches. No external CPU adapter.
 * Pipeline completed PC is explicitly host-observed, not a core register.
 * The root is a fixed testbench, not a fidelity choice.
 */
class RV32IProgramRoot : public IOComponent {
public:
    RV32IProgramRoot(std::string name, const circuit::ComponentFamily& core_family,
        size_t clock_half_period, circuit::ParameterMap core_parameters = {});
    RV32IProgramRoot(
        std::string name,
        const circuit::ComponentFamily& core_family,
        size_t clock_half_period,
        rv32i::MemoryConfiguration instructions,
        rv32i::MemoryConfiguration data = {}, circuit::ParameterMap core_parameters = {},
        std::optional<circuit::memory::CacheConfig> instruction_cache = std::nullopt,
        std::optional<circuit::memory::CacheConfig> data_cache = std::nullopt,
        bool unified_memory = false,
        std::optional<circuit::memory::CacheConfig> shared_cache = std::nullopt);

    static constexpr const char* TypeName = "RV32IProgramRoot";
    static constexpr const char* RootName = "RV32I_PROGRAM_ROOT";
    const char* getTypeName() const override { return TypeName; }

    void buildInternals(ComponentBuilder& builder) override;

    void initializeFixedInputs(
        Simulator& simulator,
        size_t time = 0) const;
    void startClock(Simulator& simulator, size_t first_rising_time) const;
    // Host hook: sample settled pre-edge outputs once, before advancing across
    // the rising edge. observed PC is a testbench output, never a core input.
    void observeRetirementBeforeEdge(Simulator& simulator, size_t edge_time);

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
    circuit::ParameterMap core_parameters_;
    size_t clock_half_period_;
    std::shared_ptr<ClockGenerator> clock_{};
    std::shared_ptr<IOComponent> core_{};
    std::shared_ptr<RV32IStateView> state_view_{};
    std::shared_ptr<RV32IPipelineObserver> pipeline_observer_{};
    std::shared_ptr<Wire<32>> observed_pc_wire_{};
    std::shared_ptr<IOComponent> instruction_memory_{};
    std::shared_ptr<IOComponent> data_memory_{};
    std::optional<circuit::memory::CacheConfig> instruction_cache_config_, data_cache_config_, shared_cache_config_;
    bool unified_memory_ = false;
    // Bytes placed in the one shared memory: instruction and data images must
    // not put different bytes at one address.
    std::map<uint32_t, uint8_t> unified_image_;
    void placeUnified(uint32_t base_address, const std::vector<uint8_t>& bytes);

    rv32i::MemoryConfiguration instruction_config_, data_config_;
};
