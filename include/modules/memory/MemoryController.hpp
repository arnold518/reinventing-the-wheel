#pragma once
#include "modules/memory/MemoryProtocol.hpp"
void defineMemoryTransactionPortPins(IOComponent*, size_t width);

// Shared FF-based payload register, also used for CPU request metadata.
class MemoryPayloadRegister : public IOComponent {
public:
    MemoryPayloadRegister(std::string, size_t width);
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

// A structural protocol frontend shared by all controllers. BACK_READY/DONE connect
// to the native sequencer; request and response payloads are real FF registers.
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
class MemoryEndpoint : public IOComponent {
public:
    MemoryEndpoint(std::string, size_t width, circuit::memory::Technology,
                   circuit::memory::EndpointConfig = {});
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    circuit::memory::Technology technology_;
    circuit::memory::EndpointConfig config_;
    std::string type_;
};
