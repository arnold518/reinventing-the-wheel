#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/Register.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
void defineMemoryTransactionPortPins(IOComponent*, size_t width);
void defineMemoryResponseBufferPins(IOComponent*, size_t width);

namespace circuit::families {
extern const ComponentFamily ClockedStateBit;
}

class ClockedStateBit : public IOComponent {
public:
    explicit ClockedStateBit(std::string name);
    const char* getTypeName() const override { return "ClockedStateBit"; }
    void buildInternals(ComponentBuilder&) override;
};

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
    // scalable_device: put one scalable MemoryContainer behind the controller
    // instead of the detailed cell-level device.
    MemoryEndpoint(std::string, size_t width, circuit::memory::Technology,
                   circuit::memory::EndpointConfig = {}, bool scalable_device = false);
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
    bool scalable_device_;
    std::string type_;
};
