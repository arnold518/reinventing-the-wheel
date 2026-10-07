#include "modules/memory/MemoryStorage.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

MemoryStorage::PackedByte MemoryStorage::uniform(LogicValue value) {
    if (value == LogicValue::LOW) return {0, 255};
    if (value == LogicValue::HIGH) return {255, 255};
    return {};
}

MemoryStorage::MemoryStorage(size_t depth, size_t bits, LogicValue initial, bool history)
    : MemoryStorage(depth, bits, initial, history, Limits{}) {}
MemoryStorage::MemoryStorage(size_t depth, size_t bits, LogicValue initial, bool history, Limits limits)
    : word_bytes_(bits / 8), limits_(limits), live_fill_(uniform(initial)),
      initial_(uniform(initial)), history_enabled_(history) {
    if (bits == 0 || bits % 8 || depth == 0)
        throw std::invalid_argument("Memory requires nonzero depth and a byte-multiple width");
    if (depth > std::numeric_limits<uint32_t>::max() / word_bytes_ ||
        depth > bytes_.max_size() / word_bytes_)
        throw std::invalid_argument("Memory capacity exceeds supported address/host range");
    capacity_ = depth * word_bytes_;
    if (!limits_.paged) {
        if (capacity_ > limits_.max_resident_bytes / sizeof(PackedByte))
            throw std::length_error("Memory resident budget exceeded");
        bytes_.assign(capacity_, initial_);
    }
}
size_t MemoryStorage::allocatedDataBytes() const noexcept {
    return bytes_.capacity() * sizeof(PackedByte) + pages_.size() * (PageBytes * sizeof(PackedByte) + 128);
}
bool MemoryStorage::isFullyBinary() const noexcept {
    if (!limits_.paged) {
        return std::all_of(bytes_.begin(), bytes_.end(), [](const auto& b) { return b.known == 255; });
    }
    if (live_fill_.known != 255 && pages_.size() != capacity_ / PageBytes + (capacity_ % PageBytes != 0))
        return false;
    for (const auto& [page, bytes] : pages_) {
        const auto count = std::min(PageBytes, capacity_ - page * PageBytes);
        for (size_t i = 0; i < count; ++i) if (bytes[i].known != 255) return false;
    }
    return true;
}
MemoryStorage::PackedByte MemoryStorage::currentByte(uint32_t at) const {
    if (!limits_.paged) return bytes_[at];
    auto page = pages_.find(at / PageBytes);
    return page == pages_.end() ? live_fill_ : page->second[at % PageBytes];
}
void MemoryStorage::setByte(uint32_t at, PackedByte value) {
    if (!limits_.paged) { bytes_[at] = value; return; }
    auto page = pages_.find(at / PageBytes);
    if (page == pages_.end()) {
        if (value == live_fill_) return;
        constexpr size_t cost = PageBytes * sizeof(PackedByte) + 128;
        if (cost > limits_.max_resident_bytes || allocatedDataBytes() > limits_.max_resident_bytes - cost)
            throw std::length_error("Memory resident budget exceeded");
        page = pages_.emplace(at / PageBytes, std::vector<PackedByte>(PageBytes, live_fill_)).first;
    }
    page->second[at % PageBytes] = value;
}
void MemoryStorage::reserveHistory(size_t bytes) {
    if (!history_enabled_) return;
    if (bytes > limits_.max_history_bytes || history_cost_ > limits_.max_history_bytes - bytes)
        throw std::length_error("Memory history budget exceeded; disable history or increase budget");
    history_cost_ += bytes;
}

bool MemoryStorage::canAccess(uint32_t address, size_t count) const noexcept {
    return address <= capacity_ && count <= capacity_ - address;
}
void MemoryStorage::requireRange(uint32_t address, size_t count) const {
    if (!canAccess(address, count)) throw std::out_of_range("Memory access outside capacity");
}
void MemoryStorage::appendBits(std::vector<LogicValue>& out, PackedByte byte) {
    for (size_t bit = 0; bit < 8; ++bit)
        out.push_back(!(byte.known & (1u << bit)) ? LogicValue::UNKNOWN :
                      (byte.value & (1u << bit)) ? LogicValue::HIGH : LogicValue::LOW);
}
uint8_t MemoryStorage::binary(PackedByte byte) {
    if (byte.known != 255) throw std::logic_error("Cannot read unknown memory byte as uint8_t");
    return byte.value;
}
std::vector<LogicValue> MemoryStorage::readLogic(uint32_t address, size_t count) const {
    requireRange(address, count);
    std::vector<LogicValue> result;
    result.reserve(count * 8);
    for (size_t i = 0; i < count; ++i) appendBits(result, currentByte(address + i));
    return result;
}
std::vector<uint8_t> MemoryStorage::readBytes(uint32_t address, size_t count) const {
    requireRange(address, count);
    std::vector<uint8_t> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) result.push_back(binary(currentByte(address + i)));
    return result;
}
void MemoryStorage::record(size_t time, uint32_t address) {
    if (!history_enabled_) return;
    reserveHistory(sizeof(Entry) * 2 + 160);
    auto& entries = history_[address];
    // Preserve legacy setup-time rewrites of a byte's history.
    if (!entries.empty() && time < entries.back().time) entries.clear();
    touched_words_.insert(address / word_bytes_);
    Entry entry{time, ++order_, currentByte(address)};
    if (!entries.empty() && entries.back().time == time) entries.back() = entry;
    else entries.push_back(entry);
}
void MemoryStorage::writeLogic(size_t time, uint32_t address,
                               const std::vector<LogicValue>& bits, bool bus_write) {
    if (bits.size() % 8) throw std::invalid_argument("Memory write must contain whole bytes");
    requireRange(address, bits.size() / 8);
    for (size_t i = 0; i < bits.size() / 8; ++i) {
        PackedByte value{};
        for (size_t b = 0; b < 8; ++b) {
            const auto bit = bits[i * 8 + b];
            if (bit == LogicValue::LOW || bit == LogicValue::HIGH) value.known |= 1u << b;
            if (bit == LogicValue::HIGH) value.value |= 1u << b;
        }
        const auto at = static_cast<uint32_t>(address + i);
        setByte(at, value);
        record(time, at);
        if (bus_write && history_enabled_) { reserveHistory(sizeof(BusWrite) * 2); bus_writes_.push_back({time, at, value}); }
    }
}
void MemoryStorage::loadBytes(uint32_t address, const std::vector<uint8_t>& data) {
    requireRange(address, data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        const auto at = static_cast<uint32_t>(address + i);
        setByte(at, {data[i], 255});
        record(0, at);
    }
}
void MemoryStorage::fill(size_t time, LogicValue value) {
    const auto byte = uniform(value);
    live_fill_ = byte;
    pages_.clear();
    std::fill(bytes_.begin(), bytes_.end(), byte);
    if (!history_enabled_) return;
    reserveHistory(sizeof(Entry) * 2);
    if (!resets_.empty() && time < resets_.back().time) resets_.clear();
    Entry entry{time, ++order_, byte};
    if (!resets_.empty() && resets_.back().time == time) resets_.back() = entry;
    else resets_.push_back(entry);
}
void MemoryStorage::clearContents() {
    history_.clear(); resets_.clear(); bus_writes_.clear(); touched_words_.clear(); order_ = 0; history_cost_ = 0;
    std::vector<PackedByte>{}.swap(epoch_baseline_);
    fill(0, LogicValue::LOW);
}
void MemoryStorage::setHistoryRecordingEnabled(bool enabled) {
    if (enabled == history_enabled_) return;
    // A sparse store cannot take an unbounded dense epoch copy implicitly.
    if (enabled && limits_.paged)
        throw std::logic_error("Enable paged memory history at construction, not mid-simulation");
    history_.clear(); resets_.clear(); bus_writes_.clear(); touched_words_.clear(); order_ = 0;
    history_enabled_ = enabled; history_cost_ = 0;
    // A new epoch starts at time zero with the retained contents, not the
    // constructor's initial fill. No snapshots are promised across epochs.
    if (enabled) epoch_baseline_ = bytes_;
    else std::vector<PackedByte>{}.swap(epoch_baseline_);
}
void MemoryStorage::requireHistory() const {
    if (!history_enabled_) throw std::logic_error("Memory time history is disabled");
}
MemoryStorage::Entry MemoryStorage::resetAt(size_t time) const {
    requireHistory();
    auto upper = std::upper_bound(resets_.begin(), resets_.end(), time,
        [](size_t t, const Entry& e) { return t < e.time; });
    return upper == resets_.begin() ? Entry{0, 0, initial_} : *--upper;
}
const MemoryStorage::Entry* MemoryStorage::byteEntryAt(size_t time, uint32_t address) const {
    const auto it = history_.find(address);
    if (it == history_.end()) return nullptr;
    auto upper = std::upper_bound(it->second.begin(), it->second.end(), time,
        [](size_t t, const Entry& e) { return t < e.time; });
    return upper == it->second.begin() ? nullptr : &*--upper;
}
MemoryStorage::PackedByte MemoryStorage::byteAt(size_t time, uint32_t address) const {
    const auto reset = resetAt(time);
    const auto entry = byteEntryAt(time, address);
    return entry && (entry->time > reset.time ||
           (entry->time == reset.time && entry->order > reset.order)) ? entry->byte :
           (reset.order == 0 && !epoch_baseline_.empty() ? epoch_baseline_[address] : reset.byte);
}
bool MemoryStorage::touchedAt(size_t time, uint32_t word) const {
    const auto reset = resetAt(time);
    for (size_t i = 0; i < word_bytes_; ++i) {
        const auto entry = byteEntryAt(time, static_cast<uint32_t>(word * word_bytes_ + i));
        if (entry && (entry->time > reset.time ||
            (entry->time == reset.time && entry->order > reset.order))) return true;
    }
    return false;
}
std::vector<MemoryStorage::WordState> MemoryStorage::getWordsAtTime(
    size_t time, uint32_t address, size_t count) const {
    requireHistory();
    std::vector<WordState> result;
    if (address >= capacity_) return result;
    address -= address % word_bytes_;
    count = std::min(count, (capacity_ - address) / word_bytes_);
    result.reserve(count);
    for (size_t w = 0; w < count; ++w) {
        const auto at = static_cast<uint32_t>(address + w * word_bytes_);
        std::vector<LogicValue> bits;
        bits.reserve(wordBits());
        for (size_t b = 0; b < word_bytes_; ++b) appendBits(bits, byteAt(time, at + b));
        result.emplace_back(at, std::move(bits));
    }
    return result;
}
std::vector<MemoryStorage::WordState> MemoryStorage::getTouchedWordsAtTime(size_t time, size_t limit) const {
    requireHistory();
    std::vector<WordState> result;
    for (auto word : touched_words_) {
        if (result.size() == limit) break;
        if (touchedAt(time, word)) {
            auto value = getWordsAtTime(time, static_cast<uint32_t>(word * word_bytes_), 1);
            result.push_back(std::move(value.front()));
        }
    }
    return result;
}
size_t MemoryStorage::getTouchedWordCountAtTime(size_t time) const {
    requireHistory();
    size_t count = 0;
    for (auto word : touched_words_) if (touchedAt(time, word)) ++count;
    return count;
}
std::map<uint32_t, uint8_t> MemoryStorage::getByteWritesInTimeRange(size_t start, size_t end) const {
    requireHistory();
    if (end < start) throw std::invalid_argument("Memory write-history range runs backward");
    std::map<uint32_t, uint8_t> result;
    for (const auto& write : bus_writes_)
        if (write.time > start && write.time <= end) result[write.address] = binary(write.byte);
    return result;
}
