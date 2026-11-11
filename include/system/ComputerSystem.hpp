#pragma once
// A whole computer built from a SystemDescription: its parts, its buses and
// the outside pins. Contract `system.computer`, one parameter: `machine` (a
// built-in description from systems/*.json) or `description` (JSON text).
// Every part input named CLK or RST, or ending in _CLK or _RST, follows the
// system clock and reset.
#include "components/IOComponent.hpp"
#include "components/capabilities/ComponentLayoutView.hpp"
#include "components/capabilities/FrameStateView.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/capabilities/RV32IPipelineStateView.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include "system/SystemDescription.hpp"
#include <map>

namespace circuit { class ComponentCatalog; }

namespace circuit::system {

class ComputerSystem : public IOComponent, public ComponentLayoutView, public RV32IPipelineStateView,
                       public RV32IRetirementView, public FrameStateView {
public:
    ComputerSystem(std::string name, SystemDescription);
    static constexpr const char* TypeName = "ComputerSystem";
    const char* getTypeName() const override { return TypeName; }
    std::string componentLayoutVariant() const override { return description_.name; }
    void buildInternals(ComponentBuilder&) override;

    const SystemDescription& description() const { return description_; }
    // A part by its name in the description (CORE, MEMORY, CONSOLE, ...).
    std::shared_ptr<IOComponent> part(const std::string& name) const;
    std::shared_ptr<IOComponent> bus(const std::string& name) const;
    std::shared_ptr<IOComponent> core() const { return core_; }
    // Main memory: the first memory subsystem part.
    const MemoryStorage& ramStorage() const;
    const MemoryStorage& videoStorage() const;
    bool hasDisplay() const { return static_cast<bool>(video_); }
    // Loads a program at address 0 of main memory (the executable region
    // that reaches it), clears video memory.
    void loadElf(const rv32i::RV32IElfImage&);
    void setMemoryHistoryRecordingEnabled(bool);

    rv32i::RV32IArchitecturalState snapshotPipelineState(uint32_t) const override;
    RV32IRetirementObservation pendingRetirement() const override;
    FrameSnapshot captureFrame(SimulationTime) const override;
private:
    SystemDescription description_;
    std::map<std::string, std::shared_ptr<IOComponent>> parts_, buses_;
    std::shared_ptr<IOComponent> core_, display_;
    std::shared_ptr<MemoryStateView> ram_, video_;
    uint64_t ram_limit_ = 0;
    size_t frame_width_ = 0, frame_height_ = 0;
};

// Pins of a system: CLK, RST and the described inputs and outputs, with the
// widths of the part pins they connect to.
IOComponent::PinInitFunction systemPins(const SystemDescription&);
SystemDescription systemFromParameters(const ParameterMap&);
void registerComputerSystems(ComponentCatalog&);

// Options of the built-in games machines, applied to a description: memory
// technology and caches, timer, display geometry, queue depths. A default
// option leaves the description unchanged.
// The main memory's technology: the on-chip SRAM macro by default, a
// zero-wait flip-flop memory, or DRAM.
enum class MainMemory { SRAM, FF, DRAM };
const char* mainMemoryName(MainMemory);
MainMemory mainMemoryFromName(const std::string&);   // "sram", "ff" or "dram"
struct GameMachineOptions {
    bool caches = false;
    MainMemory memory = MainMemory::SRAM;
    // The clock an SRAM or DRAM main memory is set up for (ticks; 0: none).
    uint64_t clock_period = 0;
    uint32_t timer_divisor = 1, timer_frequency_hz = 0;
    size_t width = 64, height = 48, video_bytes = 0, console_depth = 4, button_depth = 4;
    size_t videoCapacityBytes() const;
    void validate() const;
};
GameMachineOptions gameMachineOptions(const ParameterMap&);
SystemDescription withGameOptions(SystemDescription, const GameMachineOptions&);

} // namespace circuit::system
