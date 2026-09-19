#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include <memory>

namespace circuit::memory {
struct FFModel { static constexpr auto technology = Technology::FF; };
struct SRAMModel { static constexpr auto technology = Technology::SRAM; };
struct DRAMModel { static constexpr auto technology = Technology::DRAM; };
struct ROMModel { static constexpr auto technology = Technology::ROM; };
struct NORFlashModel { static constexpr auto technology = Technology::NORFlash; };
}

// Complete behavioral endpoint: shared protocol, technology-specific timing and
// storage. Unlike LegacyMemoryContainer this is not the old array-pin contract.
class BehavioralMemoryContainer : public circuit::memory_device::Primitive, public MemoryStateView {
public:
    BehavioralMemoryContainer(std::string, size_t width, circuit::memory::Technology,
                              circuit::memory::EndpointConfig = {});
    ~BehavioralMemoryContainer();
    void evaluate(size_t, Simulator&) override;
    const MemoryStorage& memoryStorage() const override;
    void loadMemoryBytes(uint32_t, const std::vector<uint8_t>&) override;
    // Image provisioning only, before the first simulator evaluation. This is
    // deliberately separate from runtime writes and from electrical reset.
    void initializeBytes(uint32_t, const std::vector<uint8_t>&);
    void initializeMemoryBytes(uint32_t a, const std::vector<uint8_t>& b) override { initializeBytes(a, b); }
    void clearMemoryImage() override { clearContents(); }
    void setMemoryHistoryRecordingEnabled(bool enabled) override { setHistoryRecordingEnabled(enabled); }
    void clearContents();
    void loadBytes(uint32_t a, const std::vector<uint8_t>& v) { initializeBytes(a, v); }
    void loadWords(uint32_t, const std::vector<uint32_t>&);
    void setHistoryRecordingEnabled(bool);
    std::vector<uint8_t> readBytes(uint32_t a, size_t n) const { return memoryStorage().readBytes(a, n); }
    bool canAccess(uint32_t a, size_t n) const { return memoryStorage().canAccess(a, n); }
    std::map<uint32_t, uint8_t> getByteWritesInTimeRange(size_t a, size_t b) const {
        return memoryStorage().getByteWritesInTimeRange(a, b);
    }
    auto getTouchedWordsAtTime(size_t t, size_t n) const { return memoryStorage().getTouchedWordsAtTime(t, n); }
    size_t getTouchedWordCountAtTime(size_t t) const { return memoryStorage().getTouchedWordCountAtTime(t); }
private:
    struct State;
    std::unique_ptr<State> state_;
};
template<size_t W, class Model>
class MemoryContainer : public BehavioralMemoryContainer {
public:
    MemoryContainer(std::string name, circuit::memory::EndpointConfig config = {})
        : BehavioralMemoryContainer(std::move(name), W, Model::technology, std::move(config)) {}
};
