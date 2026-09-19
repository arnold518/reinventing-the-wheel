#pragma once

#include "components/BasicComponent.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace circuit::families {
extern const ComponentFamily Memory64Kx32;
}

class Memory64Kx32 : public BasicComponent, public MemoryStateView {
public:
    using MemoryWordState = std::pair<uint32_t, std::vector<LogicValue>>;

    Memory64Kx32(std::string name);
    static constexpr const char* TypeName = "Memory64Kx32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

    static constexpr size_t capacityBytes() { return ByteCount; }
    static constexpr size_t capacityWords() { return WordCount; }

    bool canAccess(uint32_t address, size_t count) const;

    void clearContents();
    void loadBytes(uint32_t base_address, const std::vector<uint8_t>& data);
    void loadWords(uint32_t base_address, const std::vector<uint32_t>& words);
    std::vector<uint8_t> readBytes(uint32_t base_address, size_t count) const;
    uint8_t readByte(uint32_t address) const;
    uint32_t readWord(uint32_t address) const;
    uint8_t readU8(uint32_t address) const;
    uint16_t readU16(uint32_t address) const;
    uint32_t readU32(uint32_t address) const;
    void setHistoryRecordingEnabled(bool enabled);
    bool isHistoryRecordingEnabled() const noexcept;
    std::vector<MemoryWordState> getTouchedWordsAtTime(size_t target_time, size_t max_words) const;
    size_t getTouchedWordCountAtTime(size_t target_time) const;
    std::vector<MemoryWordState> getOccupiedWordsAtTime(size_t target_time, size_t max_words) const;
    size_t getOccupiedWordCountAtTime(size_t target_time) const;
    std::vector<MemoryWordState> getWordsAtTime(size_t target_time, uint32_t base_address, size_t word_count) const;
    std::map<uint32_t, uint8_t> getByteWritesInTimeRange(
        size_t start_time_exclusive,
        size_t end_time_inclusive) const;
    void writeU8(uint32_t address, uint8_t value);
    void writeU16(uint32_t address, uint16_t value);
    void writeU32(uint32_t address, uint32_t value);
    void writeU8AtTime(size_t time, uint32_t address, uint8_t value);
    void writeU16AtTime(size_t time, uint32_t address, uint16_t value);
    void writeU32AtTime(size_t time, uint32_t address, uint32_t value);

    const MemoryStorage& memoryStorage() const override { return storage_; }
    void loadMemoryBytes(uint32_t address, const std::vector<uint8_t>& data) override {
        loadBytes(address, data);
    }

private:
    static constexpr size_t WordCount = 64 * 1024;
    static constexpr size_t BytesPerWord = 4;
    static constexpr size_t ByteCount = WordCount * BytesPerWord;

    MemoryStorage storage_;
    LogicValue previous_clk;
};
