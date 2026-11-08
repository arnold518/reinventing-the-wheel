#pragma once

#include "basic/LogicValue.hpp"
#include "basic/PackedLogic.hpp"
#include "simulator/SimulationTime.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// The byte format of a saved machine (circuit::state::saveMachine): fixed-width
// little-endian numbers, length-prefixed lists. Section tags let a reader name
// the place where a save stops matching the machine it is restored into.
namespace circuit::state {

class Writer {
public:
    void u8(uint8_t value) { out_.push_back(value); }
    void boolean(bool value) { u8(value ? 1 : 0); }
    void u32(uint32_t value) { for (int i = 0; i < 4; ++i) u8(uint8_t(value >> (8 * i))); }
    void u64(uint64_t value) { for (int i = 0; i < 8; ++i) u8(uint8_t(value >> (8 * i))); }
    void i64(int64_t value) { u64(static_cast<uint64_t>(value)); }
    void time(SimulationTime value) { u64(static_cast<uint64_t>(value)); }
    void size(size_t value) { u64(static_cast<uint64_t>(value)); }
    void tag(uint32_t value) { u32(value); }
    void text(const std::string& value) { size(value.size()); out_.insert(out_.end(), value.begin(), value.end()); }
    void bytes(const std::vector<uint8_t>& value) { size(value.size()); out_.insert(out_.end(), value.begin(), value.end()); }
    void packed(const PackedLogic& value) { u64(value.value); u64(value.unknown); }
    void logic(LogicValue value) { u8(static_cast<uint8_t>(value)); }
    void logic(const std::vector<LogicValue>& values) { size(values.size()); for (auto v : values) logic(v); }
    void flags(const std::vector<bool>& values) { size(values.size()); for (bool v : values) boolean(v); }
    void times(const std::vector<SimulationTime>& values) { size(values.size()); for (auto v : values) time(v); }
    void signedTimes(const std::vector<long long>& values) { size(values.size()); for (auto v : values) i64(v); }
    void packedList(const std::vector<PackedLogic>& values) { size(values.size()); for (const auto& v : values) packed(v); }
    void optionalTime(const std::optional<SimulationTime>& value) { boolean(value.has_value()); if (value) time(*value); }

    const std::vector<uint8_t>& data() const noexcept { return out_; }
    std::vector<uint8_t> take() { return std::move(out_); }

private:
    std::vector<uint8_t> out_;
};

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& in) : in_(in) {}

    uint8_t u8() { need(1); return in_[at_++]; }
    bool boolean() {
        const auto v = u8();
        if (v > 1) fail("a flag that is neither 0 nor 1");
        return v == 1;
    }
    uint32_t u32() { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(u8()) << (8 * i); return v; }
    uint64_t u64() { uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= uint64_t(u8()) << (8 * i); return v; }
    int64_t i64() { return static_cast<int64_t>(u64()); }
    SimulationTime time() { return static_cast<SimulationTime>(u64()); }
    // A count of items that each take at least one byte.
    size_t size() {
        const auto v = u64();
        if (v > in_.size() - at_ + 1) fail("a list longer than the save");
        return static_cast<size_t>(v);
    }
    void tag(uint32_t expected, const char* what) {
        if (u32() != expected) fail(std::string("the ") + what + " section is missing");
    }
    std::string text() { const auto n = size(); need(n); std::string v(in_.begin() + at_, in_.begin() + at_ + n); at_ += n; return v; }
    std::vector<uint8_t> bytes() { const auto n = size(); need(n); std::vector<uint8_t> v(in_.begin() + at_, in_.begin() + at_ + n); at_ += n; return v; }
    PackedLogic packed() { PackedLogic v; v.value = u64(); v.unknown = u64(); return v; }
    LogicValue logic() {
        const auto v = u8();
        if (v > 3) fail("an invalid logic value");
        return static_cast<LogicValue>(v);
    }
    std::vector<LogicValue> logicList() { std::vector<LogicValue> v(size()); for (auto& x : v) x = logic(); return v; }
    std::vector<bool> flags() { std::vector<bool> v(size()); for (size_t i = 0; i < v.size(); ++i) v[i] = boolean(); return v; }
    std::vector<SimulationTime> times() { std::vector<SimulationTime> v(size()); for (auto& x : v) x = time(); return v; }
    std::vector<long long> signedTimes() { std::vector<long long> v(size()); for (auto& x : v) x = i64(); return v; }
    std::vector<PackedLogic> packedList() { std::vector<PackedLogic> v(size()); for (auto& x : v) x = packed(); return v; }
    std::optional<SimulationTime> optionalTime() { if (!boolean()) return std::nullopt; return time(); }

    bool done() const noexcept { return at_ == in_.size(); }
    size_t offset() const noexcept { return at_; }
    [[noreturn]] void fail(const std::string& what) const {
        throw std::runtime_error("Saved machine state does not fit: " + what + " (byte " + std::to_string(at_) + ")");
    }
    // A restored list must have the length the machine built.
    template <class T> void expectSize(const std::vector<T>& value, size_t expected, const char* what) const {
        if (value.size() != expected) fail(std::string(what) + " has " + std::to_string(value.size()) + " entries, the machine " + std::to_string(expected));
    }

private:
    void need(size_t n) const { if (in_.size() - at_ < n) fail("the save ends early"); }
    const std::vector<uint8_t>& in_;
    size_t at_ = 0;
};

} // namespace circuit::state
