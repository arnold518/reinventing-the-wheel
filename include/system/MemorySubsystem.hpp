#pragma once
// The memory subsystem: one main memory with two request ports, I_ (for
// instruction fetch) and D_ (for data). An optional cache sits on each port,
// the two-port arbiter takes turns, an optional shared cache sits below the
// arbiter, then one memory endpoint. Contract `memory.subsystem.32`,
// parameters technology=ff|sram|dram, words (a power of two),
// caches=none|default. C++ builders (the § 4.7 program root and the § 4.8
// experiments) pass a full MemorySubsystemConfig instead.
#include "components/IOComponent.hpp"
#include "components/capabilities/ComponentLayoutView.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "modules/memory/Cache.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include <optional>
namespace circuit { class ComponentCatalog; }
namespace circuit::system {
struct MemorySubsystemConfig {
    rv32i::MemoryConfiguration memory;
    std::optional<memory::CacheConfig> instruction_cache, data_cache, shared_cache;
    // Words per burst the arbiter keeps one owner for (a cache line below it).
    size_t arbiter_block_words = 2;
    // Adds inputs INV_L1 (both port caches) and INV_L2 (the shared cache);
    // otherwise cache invalidation is tied off.
    bool invalidation_inputs = false;
    size_t bytes() const { return memory.device.depth_words * 4; }
};
class MemorySubsystem : public IOComponent, public ComponentLayoutView {
public:
    MemorySubsystem(std::string name, MemorySubsystemConfig);
    static constexpr const char* TypeName = "MemorySubsystem";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
    const MemorySubsystemConfig& config() const { return config_; }
    // Subsystems with different parts keep different floorplans.
    std::string componentLayoutVariant() const override;
    // The main memory's storage (for loading programs and inspection).
    std::shared_ptr<MemoryStateView> storage() const { return view_; }
    // Built parts, for observation; a cache is null when not configured.
    std::shared_ptr<IOComponent> memoryEndpoint() const { return ram_; }
    std::shared_ptr<IOComponent> arbiter() const { return arbiter_; }
    std::shared_ptr<IOComponent> instructionCache() const { return instruction_cache_; }
    std::shared_ptr<IOComponent> dataCache() const { return data_cache_; }
    std::shared_ptr<IOComponent> sharedCache() const { return shared_cache_; }
private:
    MemorySubsystemConfig config_;
    std::shared_ptr<MemoryStateView> view_;
    std::shared_ptr<IOComponent> ram_, arbiter_, instruction_cache_, data_cache_, shared_cache_;
};
MemorySubsystemConfig memorySubsystemFromParameters(const ParameterMap&);
void memorySubsystemPins(IOComponent*, const MemorySubsystemConfig&);
void registerMemorySubsystem(ComponentCatalog&);
} // namespace circuit::system
