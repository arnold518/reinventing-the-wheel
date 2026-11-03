#include "components/capabilities/DeviceView.hpp"
#pragma once
#include "components/IOComponent.hpp"
#include "modules/device/DeviceConfiguration.hpp"
#include "components/capabilities/ComponentLayoutView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/capabilities/MemoryStateView.hpp"
namespace circuit { class ComponentCatalog; }
namespace circuit::families {
extern const ComponentFamily ConsoleDevice, TimerDevice, ButtonDevice, FramebufferDisplay, LedPanelDevice;
const ComponentFamily& clockedCounter(size_t width);
const ComponentFamily& synchronousFifo(size_t width);
}
class ClockedCounter : public IOComponent {
    size_t width_;uint32_t divisor_;std::string type_;
public: ClockedCounter(std::string,size_t width=4,uint32_t divisor=1);
    const char* getTypeName() const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
class SynchronousFifo : public IOComponent, public DeviceView, public ComponentLayoutView {
    size_t width_,depth_;std::string type_;
public: SynchronousFifo(std::string,size_t width=8,size_t depth=4);
    std::string componentLayoutVariant() const override { return depth_==4 ? "" : "depth="+std::to_string(depth_); }
    const char* getTypeName() const override{return type_.c_str();}
    std::string deviceViewKind() const override {return "queue";}
    void buildInternals(ComponentBuilder&) override;
};
class ConsoleDevice : public IOComponent, public DeviceView {
    size_t fifo_depth_;
public: explicit ConsoleDevice(std::string,size_t fifo_depth=4);
    static constexpr const char* TypeName="ConsoleDevice";
    const char* getTypeName() const override{return TypeName;}
    std::string deviceViewKind() const override {return "console";}
    void buildInternals(ComponentBuilder&) override;
};
class TimerDevice : public IOComponent, public DeviceView {
    uint32_t divisor_,frequency_;
public: explicit TimerDevice(std::string,uint32_t divisor=1,uint32_t frequency_hz=0);
    static constexpr const char* TypeName="TimerDevice";
    const char* getTypeName() const override{return TypeName;}
    std::string deviceViewKind() const override {return "timer";}
    void buildInternals(ComponentBuilder&) override;
};
class ButtonDevice : public IOComponent, public DeviceView {
    size_t fifo_depth_;
public: explicit ButtonDevice(std::string,size_t fifo_depth=4);
    static constexpr const char* TypeName="ButtonDevice";
    const char* getTypeName() const override{return TypeName;}
    std::string deviceViewKind() const override {return "keyboard";}
    void buildInternals(ComponentBuilder&) override;
};
// The control bank is not the pixel-memory protocol endpoint. PIXELS owns
// MemoryStateView so the existing inspector follows FB accesses correctly.
class FramebufferDisplay : public IOComponent, public DeviceView, public ComponentLayoutView {
    size_t width_,height_,storage_bytes_;
    std::shared_ptr<MemoryStateView> storage_;
public: explicit FramebufferDisplay(std::string,size_t width=4,size_t height=4,size_t storage_bytes=0);
    static constexpr const char* TypeName="FramebufferDisplay";
    const char* getTypeName() const override{return TypeName;}
    std::string deviceViewKind() const override {return "display";}
    std::optional<DeviceSurfaceShape> deviceSurfaceShape() const override {return DeviceSurfaceShape{width_,height_,width_,4};}
    void buildInternals(ComponentBuilder&) override;
    std::string componentLayoutVariant() const override;
    circuit::device::FramebufferConfig configuration() const { return {width_,height_,storage_bytes_}; }
    const MemoryStorage& memoryStorage() const;
    void loadMemoryBytes(uint32_t,const std::vector<uint8_t>&);
    void initializeMemoryBytes(uint32_t,const std::vector<uint8_t>&);
    void clearMemoryImage();
    void setMemoryHistoryRecordingEnabled(bool);
};
// Sixteen LEDs: a register table (LEDS, BLINK) and a free-running counter.
// Added to a system only through its description.
class LedPanel : public IOComponent, public DeviceView {
public: explicit LedPanel(std::string);
    static constexpr const char* TypeName="LedPanel";
    const char* getTypeName() const override{return TypeName;}
    std::string deviceViewKind() const override {return "leds";}
    void buildInternals(ComponentBuilder&) override;
};
namespace circuit::device { void registerComponents(ComponentCatalog&); }
