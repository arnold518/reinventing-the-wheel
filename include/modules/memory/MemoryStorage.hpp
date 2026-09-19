#pragma once

#include "basic/LogicValue.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

// Host-side storage only: no pins, timing, CPU formatting, or device behavior.
// Each byte uses a value and known-bit mask; Z is stored as X.
class MemoryStorage {
public:
    struct Limits {
        bool paged = false;
        size_t max_resident_bytes = size_t(-1);
        size_t max_history_bytes = size_t(-1);
    };
    using WordState = std::pair<uint32_t, std::vector<LogicValue>>;
    struct PackedByte {
        uint8_t value = 0;
        uint8_t known = 0;
        bool operator==(const PackedByte&) const = default;
    };

    MemoryStorage(size_t depth_words, size_t word_bits,
                  LogicValue initial = LogicValue::UNKNOWN, bool history = true);
    MemoryStorage(size_t depth_words, size_t word_bits, LogicValue initial, bool history, Limits);
    size_t capacityBytes() const noexcept { return capacity_; }
    size_t capacityWords() const noexcept { return capacity_ / word_bytes_; }
    size_t wordBits() const noexcept { return word_bytes_ * 8; }
    bool canAccess(uint32_t address, size_t count) const noexcept;
    std::vector<LogicValue> readLogic(uint32_t address, size_t count) const;
    std::vector<uint8_t> readBytes(uint32_t address, size_t count) const;
    void writeLogic(size_t time, uint32_t address, const std::vector<LogicValue>& bits,
                    bool bus_write = false);
    void loadBytes(uint32_t address, const std::vector<uint8_t>& data);
    void fill(size_t time, LogicValue value);
    void clearContents();
    void setHistoryRecordingEnabled(bool enabled);
    bool isHistoryRecordingEnabled() const noexcept { return history_enabled_; }
    std::vector<WordState> getWordsAtTime(size_t time, uint32_t address, size_t count) const;
    std::vector<WordState> getTouchedWordsAtTime(size_t time, size_t limit) const;
    size_t getTouchedWordCountAtTime(size_t time) const;
    std::map<uint32_t, uint8_t> getByteWritesInTimeRange(size_t start, size_t end) const;
    size_t allocatedDataBytes() const noexcept;
    size_t historicalByteCount() const noexcept { return history_.size(); }

private:
    struct Entry { size_t time; size_t order; PackedByte byte; };
    struct BusWrite { size_t time; uint32_t address; PackedByte byte; };
    static PackedByte uniform(LogicValue value);
    static void appendBits(std::vector<LogicValue>& out, PackedByte byte);
    static uint8_t binary(PackedByte byte);
    void requireRange(uint32_t address, size_t count) const;
    void requireHistory() const;
    void record(size_t time, uint32_t address);
    Entry resetAt(size_t time) const;
    const Entry* byteEntryAt(size_t time, uint32_t address) const;
    PackedByte byteAt(size_t time, uint32_t address) const;
    bool touchedAt(size_t time, uint32_t word) const;
    size_t word_bytes_;
    size_t capacity_ = 0;
    Limits limits_;
    static constexpr size_t PageBytes = 4096;
    std::map<size_t, std::vector<PackedByte>> pages_;
    PackedByte live_fill_;
    PackedByte currentByte(uint32_t) const;
    void setByte(uint32_t, PackedByte);
    void reserveHistory(size_t bytes);
    size_t history_cost_ = 0;
    PackedByte initial_;
    bool history_enabled_;
    size_t order_ = 0;
    std::vector<PackedByte> bytes_;
    // Only allocated when recording is re-enabled on an existing live store.
    std::vector<PackedByte> epoch_baseline_;
    std::map<uint32_t, std::vector<Entry>> history_;
    std::vector<Entry> resets_;
    std::vector<BusWrite> bus_writes_;
    std::set<uint32_t> touched_words_;
};
