#pragma once
#include "components/IOComponent.hpp"
#include "modules/device/DeviceConfiguration.hpp"
#include "components/capabilities/ComponentLayoutView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/capabilities/MemoryStateView.hpp"
namespace circuit { class ComponentCatalog; }
namespace circuit::families {
extern const ComponentFamily DeviceRegisterPort32, BusErrorTarget32, AddressRouter32, InstructionAccessGuard32;
extern const ComponentFamily ConsoleDevice32, TimerDevice32, ButtonDevice32, IndexedPixelSelector32, FramebufferDisplay32;
const ComponentFamily& clockedCounter(size_t width);
const ComponentFamily& synchronousFifo(size_t width);
}
class DeviceRegisterPort32 : public IOComponent {
public: explicit DeviceRegisterPort32(std::string);
    static constexpr const char* TypeName="DeviceRegisterPort32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class BusErrorTarget32 : public IOComponent {
public: explicit BusErrorTarget32(std::string);
    static constexpr const char* TypeName="BusErrorTarget32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class AddressRouter32 : public IOComponent, public ComponentLayoutView {
    size_t video_bytes_;
public: explicit AddressRouter32(std::string,size_t video_bytes=4096);
    std::string componentLayoutVariant() const override { return video_bytes_==4096 ? "" : "video_bytes="+std::to_string(video_bytes_); }
    static constexpr const char* TypeName="AddressRouter32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class InstructionAccessGuard32 : public IOComponent {
public: explicit InstructionAccessGuard32(std::string);
    static constexpr const char* TypeName="InstructionAccessGuard32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class ClockedCounter : public IOComponent {
    size_t width_;uint32_t divisor_;std::string type_;
public: ClockedCounter(std::string,size_t width=4,uint32_t divisor=1);
    const char* getTypeName() const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
class SynchronousFifo : public IOComponent, public ComponentLayoutView {
    size_t width_,depth_;std::string type_;
public: SynchronousFifo(std::string,size_t width=8,size_t depth=4);
    std::string componentLayoutVariant() const override { return depth_==4 ? "" : "depth="+std::to_string(depth_); }
    const char* getTypeName() const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
class ConsoleDevice32 : public IOComponent {
    size_t fifo_depth_;
public: explicit ConsoleDevice32(std::string,size_t fifo_depth=4);
    static constexpr const char* TypeName="ConsoleDevice32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class TimerDevice32 : public IOComponent {
    uint32_t divisor_,frequency_;
public: explicit TimerDevice32(std::string,uint32_t divisor=1,uint32_t frequency_hz=0);
    static constexpr const char* TypeName="TimerDevice32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class ButtonDevice32 : public IOComponent {
    size_t fifo_depth_;
public: explicit ButtonDevice32(std::string,size_t fifo_depth=4);
    static constexpr const char* TypeName="ButtonDevice32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class IndexedPixelSelector32 : public IOComponent {
public: explicit IndexedPixelSelector32(std::string);
    static constexpr const char* TypeName="IndexedPixelSelector32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
// The control bank is not the pixel-memory protocol endpoint. PIXELS owns
// MemoryStateView so the existing inspector follows FB accesses correctly.
class FramebufferDisplay32 : public IOComponent, public ComponentLayoutView {
    size_t width_,height_,storage_bytes_;
    std::shared_ptr<MemoryStateView> storage_;
public: explicit FramebufferDisplay32(std::string,size_t width=4,size_t height=4,size_t storage_bytes=0);
    static constexpr const char* TypeName="FramebufferDisplay32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
    std::string componentLayoutVariant() const override;
    circuit::device::FramebufferConfig configuration() const { return {width_,height_,storage_bytes_}; }
    const MemoryStorage& memoryStorage() const;
    void loadMemoryBytes(uint32_t,const std::vector<uint8_t>&);
    void initializeMemoryBytes(uint32_t,const std::vector<uint8_t>&);
    void clearMemoryImage();
    void setMemoryHistoryRecordingEnabled(bool);
};
namespace circuit::device { void registerComponents(ComponentCatalog&); }
