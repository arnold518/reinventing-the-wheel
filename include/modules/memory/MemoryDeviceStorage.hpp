#pragma once
#include "components/capabilities/MemoryStateView.hpp"

// Host storage and provisioning shared by native behavioral devices. This is
// not a hardware interface: all runtime operations still use the device pins.
class MemoryDeviceStorage : public MemoryStateView {
public:
    MemoryDeviceStorage(size_t words, size_t width, LogicValue initial, bool history,
                        MemoryStorage::Limits limits)
        : storage_(words, width, initial, history, limits) {}
    const MemoryStorage& memoryStorage() const override { return storage_; }
    void setContainerDisplayType(std::string name) { display_type_ = std::move(name); }
    const char* memoryDisplayType(const char* fallback) const { return display_type_.empty() ? fallback : display_type_.c_str(); }
    void initializeBytes(uint32_t a, const std::vector<uint8_t>& bytes) { initializeMemoryBytes(a,bytes); }
    void loadBytes(uint32_t a, const std::vector<uint8_t>& bytes) { initializeMemoryBytes(a,bytes); }
    void clearContents() { clearMemoryImage(); }
    void setHistoryRecordingEnabled(bool enabled) { setMemoryHistoryRecordingEnabled(enabled); }
    void loadWords(uint32_t a, const std::vector<uint32_t>& words) {
        std::vector<uint8_t> bytes;
        for(auto word:words) for(unsigned i=0;i<4;++i) bytes.push_back(uint8_t(word>>(8*i)));
        initializeMemoryBytes(a,bytes);
    }
    std::vector<uint8_t> readBytes(uint32_t a,size_t n) const { return storage_.readBytes(a,n); }
    bool canAccess(uint32_t a,size_t n) const { return storage_.canAccess(a,n); }
    void initializeMemoryBytes(uint32_t address, const std::vector<uint8_t>& bytes) override {
        requireProvisioning();
        storage_.loadBytes(address, bytes);
    }
    void clearMemoryImage() override {
        requireProvisioning();
        storage_.clearContents();
    }
    void setMemoryHistoryRecordingEnabled(bool enabled) override {
        storage_.setHistoryRecordingEnabled(enabled);
    }
protected:
    void requireProvisioning() const {
        if (started_) throw std::logic_error("Memory image provisioning is only allowed before simulation starts");
    }
    MemoryStorage storage_;
    bool started_ = false;
    std::string display_type_;
};
