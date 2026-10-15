#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "modules/storage/Register.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
void defineMemoryTransactionPortPins(IOComponent*, size_t width);
// The controller's behavioral form; the class is private to its source file.
std::shared_ptr<Component> createBehavioralMemoryController(
    const std::string& name, size_t width, circuit::memory::Technology, circuit::memory::EndpointConfig,
    const std::shared_ptr<circuit::BuildContext>& context);
void defineMemoryResponseBufferPins(IOComponent*, size_t width);

// One-entry fall-through response storage. Empty buffers do not add a cycle.
class MemoryResponseBuffer : public IOComponent {
public:
    MemoryResponseBuffer(std::string, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    std::string type_;
};

// This component contains registers and control, never memory contents.
class MemoryController : public IOComponent {
public:
    MemoryController(std::string, size_t width, circuit::memory::Technology,
                     circuit::memory::EndpointConfig = {});
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    circuit::memory::Technology technology_;
    circuit::memory::EndpointConfig config_;
    std::string type_;
};

// Structural frontend for delayed controllers. START captures live request fields
// in the native sequencer; FINISH releases its completion. Responses fall through
// when possible and are retained in FFs under backpressure.
class MemoryTransactionPort : public IOComponent {
public:
    MemoryTransactionPort(std::string, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    std::string type_;
};

// Convenient assembly: controller plus separately visible native memory.
class MemoryEndpoint : public IOComponent, public MemoryStateView {
public:
    // require_behavioral_device: keep the controller visible while using
    // scalable native storage behind it.
    MemoryEndpoint(std::string, size_t width, circuit::memory::Technology,
                   circuit::memory::EndpointConfig = {}, bool require_behavioral_device = false);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
    const MemoryStorage& memoryStorage() const override;
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t>&) override;
    void initializeMemoryBytes(uint32_t, const std::vector<uint8_t>&) override;
    void clearMemoryImage() override;
    void setMemoryHistoryRecordingEnabled(bool) override;
private:
    std::shared_ptr<MemoryStateView> storage_;
    const std::shared_ptr<MemoryStateView>& storageView() const;
    size_t width_;
    circuit::memory::Technology technology_;
    circuit::memory::EndpointConfig config_;
    bool require_behavioral_device_;
    std::string type_;
};
