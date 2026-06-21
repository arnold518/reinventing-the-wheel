#pragma once

#include "components/BasicComponent.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

class BehavioralMemory64Kx32 : public BasicComponent {
public:
    using MemoryWordState = std::pair<uint32_t, std::vector<LogicValue>>;

    BehavioralMemory64Kx32(std::string name);
    static constexpr const char* TypeName = "BehavioralMemory64Kx32";
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
    std::vector<MemoryWordState> getTouchedWordsAtTime(size_t target_time, size_t max_words) const;
    size_t getTouchedWordCountAtTime(size_t target_time) const;
    std::vector<MemoryWordState> getOccupiedWordsAtTime(size_t target_time, size_t max_words) const;
    size_t getOccupiedWordCountAtTime(size_t target_time) const;
    std::vector<MemoryWordState> getWordsAtTime(size_t target_time, uint32_t base_address, size_t word_count) const;
    void writeU8(uint32_t address, uint8_t value);
    void writeU16(uint32_t address, uint16_t value);
    void writeU32(uint32_t address, uint32_t value);
    void writeU8AtTime(size_t time, uint32_t address, uint8_t value);
    void writeU16AtTime(size_t time, uint32_t address, uint16_t value);
    void writeU32AtTime(size_t time, uint32_t address, uint32_t value);

private:
    static constexpr size_t WordCount = 64 * 1024;
    static constexpr size_t BytesPerWord = 4;
    static constexpr size_t ByteCount = WordCount * BytesPerWord;

    using ByteValue = std::array<LogicValue, 8>;
    struct ByteHistoryEntry {
        size_t time;
        size_t order;
        ByteValue value;
    };
    struct ResetHistoryEntry {
        size_t time;
        size_t order;
        ByteValue value;
    };

    void recordByteHistory(size_t time, size_t address);
    void recordRangeHistory(size_t time, size_t base_address, size_t count);
    void recordResetHistory(size_t time, const ByteValue& value);
    ResetHistoryEntry resetAtTime(size_t target_time) const;
    bool byteTouchedAtTime(size_t target_time, size_t address, const ResetHistoryEntry& reset) const;
    bool wordTouchedAtTime(size_t target_time, uint32_t word_index, const ResetHistoryEntry& reset) const;
    ByteValue byteAtTime(size_t target_time, size_t address) const;
    std::vector<LogicValue> wordAtTime(size_t target_time, uint32_t address) const;
    void markTrackedWord(size_t byte_address);

    std::vector<std::array<LogicValue, 8>> bytes;
    std::vector<std::vector<ByteHistoryEntry>> byte_history;
    std::vector<ResetHistoryEntry> reset_history;
    std::vector<uint32_t> tracked_word_indices;
    std::vector<uint8_t> tracked_words;
    size_t history_order;
    LogicValue previous_clk;
};
