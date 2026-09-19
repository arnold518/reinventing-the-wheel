#pragma once
#include "modules/memory/MemoryStorage.hpp"
#include <stdexcept>

// Observation/setup capability, independent of a particular memory technology.
// Historical reads never perform simulated accesses or mutate device state.
class MemoryStateView {
public:
    virtual ~MemoryStateView() = default;
    virtual const MemoryStorage& memoryStorage() const = 0;
    virtual void loadMemoryBytes(uint32_t address, const std::vector<uint8_t>& data) = 0;
    // Optional host image provisioning. Devices without a setup hook must be
    // initialized through their pins (or their construction-time ROM image).
    virtual void initializeMemoryBytes(uint32_t, const std::vector<uint8_t>&) {
        throw std::logic_error("This memory has no pre-simulation image loader");
    }
    virtual void clearMemoryImage() {
        throw std::logic_error("This memory has no pre-simulation image clear hook");
    }
    virtual void setMemoryHistoryRecordingEnabled(bool) {
        throw std::logic_error("This memory cannot change its history policy");
    }
};
