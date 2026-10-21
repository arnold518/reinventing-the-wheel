#pragma once
#include "components/IOComponent.hpp"
#include "modules/device/DeviceConfiguration.hpp"
#include "components/capabilities/FrameStateView.hpp"
#include "components/capabilities/RV32IPipelineStateView.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include <map>

namespace circuit { class ComponentCatalog; }
namespace circuit::families { extern const ComponentFamily RV32IMDeviceSystem; }
namespace rv32im {
struct DeviceSystemConfig {
    bool caches = false, dram = false;
    uint32_t timer_divisor = 1, timer_frequency_hz = 0;
    size_t width = 64, height = 48;
    size_t video_bytes = 0, console_depth = 4, button_depth = 4;
    size_t videoCapacityBytes() const;
    void validate() const;
};
void registerDeviceSystem(circuit::ComponentCatalog&);
}
// One shared 256 KiB RAM. Guarded fetch and RAM-only optional WT caches.
// All MMIO and video traffic takes a separate, uncached route.
class RV32IMDeviceSystem : public IOComponent, public RV32IPipelineStateView,
                          public RV32IRetirementView, public FrameStateView {
public:
    explicit RV32IMDeviceSystem(std::string, rv32im::DeviceSystemConfig = {});
    static constexpr const char* TypeName = "RV32IMDeviceSystem";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
    void loadElf(const rv32i::RV32IElfImage&);
    void setMemoryHistoryRecordingEnabled(bool);
    const MemoryStorage& videoStorage() const { return video_view_->memoryStorage(); }
    const MemoryStorage& ramStorage() const { return ram_view_->memoryStorage(); }
    std::shared_ptr<IOComponent> core() const { return core_; }
    std::shared_ptr<IOComponent> router() const { return router_; }
    std::shared_ptr<IOComponent> peripheral(const std::string&) const;
    rv32i::RV32IArchitecturalState snapshotPipelineState(uint32_t) const override;
    RV32IRetirementObservation pendingRetirement() const override;
    FrameSnapshot captureFrame(SimulationTime) const override;
private:
    rv32im::DeviceSystemConfig config_;
    std::shared_ptr<IOComponent> core_, router_, ram_;
    std::shared_ptr<MemoryStateView> ram_view_, video_view_;
    std::map<std::string, std::shared_ptr<IOComponent>> peripherals_;
};
