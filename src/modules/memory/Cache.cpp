#include "modules/memory/Cache.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/storage/Register.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "MemoryHandshakeWiring.hpp"
#include <bit>
#include <array>
#include <limits>
#include <vector>

namespace circuit::memory {
using circuit::memory_detail::StructuralWiring;
using PinPtr = StructuralWiring::PinPtr;

void CacheConfig::validate(size_t width) const {
    if (width != 8 && width != 16 && width != 32)
        throw std::invalid_argument("Cache requires an 8, 16 or 32-bit endpoint word");
    const size_t bytes = block_bytes ? block_bytes : width / 8;
    const size_t words_per_line = bytes / (width / 8);
    if (!std::has_single_bit(sets) || sets > (scalable_storage ? 64 : 4) ||
        (ways != 1 && ways != 2) ||
        !std::has_single_bit(bytes) || bytes < width / 8 ||
        words_per_line > 4 || (!scalable_storage && sets * words_per_line > 4096 / width))
        throw std::invalid_argument("Cache supports power-of-two sets up to 4 expanded or 64 scalable, one or two ways and 1/2/4 words per line");
    if (!std::has_single_bit(backing_depth_words) || backing_depth_words < sets * words_per_line ||
        backing_depth_words > std::numeric_limits<uint32_t>::max() / (width / 8))
        throw std::invalid_argument("Cache backing depth must be a sufficient power of two");
    for (auto technology : {tag_technology, data_technology})
        if (technology != Technology::FF && technology != Technology::SRAM)
            throw std::invalid_argument("Cache tag/data banks require writable FF or SRAM endpoints");
}

CacheConfig cacheConfig(const ParameterMap& p) {
    memory_device::keys(p, {"sets", "ways", "block_bytes", "backing_depth_words", "mode", "storage_form",
                            "tag_technology", "data_technology"});
    CacheConfig c;
    c.sets = memory_device::parameter(p, "sets", c.sets);
    c.ways = memory_device::parameter(p, "ways", c.ways);
    c.block_bytes = memory_device::parameter(p, "block_bytes", c.block_bytes);
    c.backing_depth_words = memory_device::parameter(p, "backing_depth_words", c.backing_depth_words);
    if (auto it = p.find("mode"); it != p.end()) {
        if (it->second == "read_only") c.read_write = false;
        else if (it->second == "read_write") c.read_write = true;
        else throw std::invalid_argument("Cache mode must be read_only or read_write");
    }
    if (auto it = p.find("storage_form"); it != p.end()) {
        if (it->second == "expanded") c.scalable_storage = false;
        else if (it->second == "scalable") c.scalable_storage = true;
        else throw std::invalid_argument("Cache storage_form must be expanded or scalable");
    }
    if (auto it = p.find("tag_technology"); it != p.end()) c.tag_technology = technologyFromName(it->second);
    if (auto it = p.find("data_technology"); it != p.end()) c.data_technology = technologyFromName(it->second);
    return c;
}

ParameterMap cacheParameters(const CacheConfig& c) {
    return {{"sets", std::to_string(c.sets)}, {"ways", std::to_string(c.ways)},
            {"block_bytes", std::to_string(c.block_bytes)},
            {"backing_depth_words", std::to_string(c.backing_depth_words)},
            {"mode", c.read_write ? "read_write" : "read_only"},
            {"storage_form", c.scalable_storage ? "scalable" : "expanded"},
            {"tag_technology", technologyName(c.tag_technology)},
            {"data_technology", technologyName(c.data_technology)}};
}

void cachePins(IOComponent* self, size_t width) {
    protocolPins(self, width);
    protocolPins(self, width, "MEM_", true);
    self->addPin("INV_VALID", PinType::INPUT);
    self->addPin("INV_READY", PinType::OUTPUT);
}

namespace {

// X/Z detection is simulator metadata, shared with memory request validation.
class KnownCacheRequest : public memory_device::Primitive {
public:
    KnownCacheRequest(std::string name, size_t width)
        : Primitive(std::move(name), "CacheRequestKnownBits", [width](IOComponent* self) {
              self->addPin<3>("OP", PinType::INPUT);
              self->addPin<32>("ADDR", PinType::INPUT);
              self->addPinDynamic("MASK", PinType::INPUT, width / 8);
              self->addPin("FIELDS_KNOWN", PinType::OUTPUT);
              self->addPin("MASK_KNOWN", PinType::OUTPUT);
          }) {}
    bool respondsOnlyToInputs() const override { return true; }
    void evaluate(size_t time, Simulator& simulator) override {
        publish(simulator, time, "FIELDS_KNOWN",
                memory_device::known(read("OP")) && memory_device::known(read("ADDR"))
                    ? memory_device::H : memory_device::L);
        publish(simulator, time, "MASK_KNOWN",
                memory_device::known(read("MASK")) ? memory_device::H : memory_device::L);
    }
};

class CacheRequestCheck : public IOComponent {
public:
    CacheRequestCheck(std::string name, size_t width, size_t depth, bool writable)
        : IOComponent(std::move(name), [width](IOComponent* self) {
              self->addPin<3>("OP", PinType::INPUT);
              self->addPin<32>("ADDR", PinType::INPUT);
              self->addPinDynamic("MASK", PinType::INPUT, width / 8);
              self->addPin("READ", PinType::OUTPUT);
              self->addPin("WRITE", PinType::OUTPUT);
              self->addPin("ACCEPT", PinType::OUTPUT);
              self->addPin<3>("STATUS", PinType::OUTPUT);
          }), width_(width), depth_(depth), writable_(writable),
          type_("CacheRequestCheck<" + std::to_string(width) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto op = builder.addNewComponent<BitSplitter<3>>("OP_BITS");
        auto addr = builder.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        auto known = builder.addNewComponent<KnownCacheRequest>("KNOWN_FIELDS", width_);
        w.connect(getInputPin<3>("OP"), {op->getInputPin<3>("IN"), known->getInputPin<3>("OP")});
        w.connect(getInputPin<32>("ADDR"), {addr->getInputPin<32>("IN"), known->getInputPin<32>("ADDR")});
        w.connect(getInputPinDynamic("MASK"), known->getInputPinDynamic("MASK"));
        auto o0 = op->getOutputPin("OUT_0");
        auto o1 = op->getOutputPin("OUT_1");
        auto o2 = op->getOutputPin("OUT_2");
        auto reserved = w.either("RESERVED_OPCODE", o1, o2);
        auto read_opcode = w.reduce("READ_OPCODE", {w.invert("NOT_WRITE", o0),
            w.invert("NOT_RESERVED", reserved)}, true);
        auto write_opcode = w.both("WRITE_OPCODE", o0, w.invert("WRITE_NOT_RESERVED", reserved));
        std::vector<PinPtr> low;
        for (size_t i = 0; i < std::countr_zero(width_ / 8); ++i)
            low.push_back(addr->getOutputPin("OUT_" + std::to_string(i)));
        auto unaligned = w.reduce("UNALIGNED", low, false);
        std::vector<PinPtr> high;
        const auto capacity = depth_ * (width_ / 8);
        for (size_t i = std::countr_zero(capacity); i < 32; ++i)
            high.push_back(addr->getOutputPin("OUT_" + std::to_string(i)));
        auto range = w.reduce("OUT_OF_RANGE", high, false);
        auto valid = w.reduce("VALID_ADDRESS", {known->getOutputPin("FIELDS_KNOWN"),
            w.invert("ALIGNED", unaligned), w.invert("IN_RANGE", range)}, true);
        auto read = w.both("VALID_READ", valid, read_opcode);
        auto write = w.reduce("VALID_WRITE", {valid, write_opcode,
            known->getOutputPin("MASK_KNOWN"), w.constant("WRITABLE", writable_)}, true);
        w.connect(read, getOutputPin("READ"));
        w.connect(write, getOutputPin("WRITE"));
        w.connect(w.either("ACCEPT", read, write), getOutputPin("ACCEPT"));
        auto constant = [&](const std::string& name, unsigned value) -> PinPtr {
            return builder.addNewComponent<ConstantValue<3>>(name, value)->getOutputPin<3>("OUT");
        };
        auto permission = writable_ ? w.constant("WRITE_ALLOWED", false) : write_opcode;
        auto status = memory_detail::payloadMux(builder, w, "PERMISSION_STATUS",
            constant("OK", 0), constant("UNSUPPORTED", 4), permission);
        status = memory_detail::payloadMux(builder, w, "RANGE_STATUS", status,
            constant("RANGE", 2), range);
        status = memory_detail::payloadMux(builder, w, "ALIGN_STATUS", status,
            constant("ALIGNMENT", 1), unaligned);
        auto unknown_mask = w.both("UNKNOWN_WRITE_MASK", write_opcode,
            w.invert("MASK_NOT_KNOWN", known->getOutputPin("MASK_KNOWN")));
        status = memory_detail::payloadMux(builder, w, "MASK_STATUS", status,
            constant("BAD_MASK", 6), unknown_mask);
        status = memory_detail::payloadMux(builder, w, "OP_STATUS", status,
            constant("RESERVED", 4), reserved);
        status = memory_detail::payloadMux(builder, w, "KNOWN_STATUS",
            constant("UNKNOWN", 6), status, known->getOutputPin("FIELDS_KNOWN"));
        w.connect(status, getOutputPin<3>("STATUS"));
        w.finish();
    }
private:
    size_t width_, depth_;
    bool writable_;
    std::string type_;
};

class CacheTagMatch32 : public IOComponent {
public:
    explicit CacheTagMatch32(std::string name)
        : IOComponent(std::move(name), [](IOComponent* self) {
              self->addPin<32>("EXPECTED", PinType::INPUT);
              self->addPin<32>("STORED", PinType::INPUT);
              self->addPin("VALID", PinType::INPUT);
              self->addPin("HIT", PinType::OUTPUT);
          }) {}
    const char* getTypeName() const override { return "CacheTagMatch32"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto expected = builder.addNewComponent<BitSplitter<32>>("EXPECTED_BITS");
        auto stored = builder.addNewComponent<BitSplitter<32>>("STORED_BITS");
        w.connect(getInputPin<32>("EXPECTED"), expected->getInputPin<32>("IN"));
        w.connect(getInputPin<32>("STORED"), stored->getInputPin<32>("IN"));
        std::vector<PinPtr> unequal;
        for (size_t i = 0; i < 32; ++i)
            unequal.push_back(w.binary<XORGate>("DIFFER_" + std::to_string(i),
                expected->getOutputPin("OUT_" + std::to_string(i)),
                stored->getOutputPin("OUT_" + std::to_string(i))));
        auto equal = w.invert("EQUAL", w.reduce("ANY_DIFFERENCE", unequal, false));
        w.connect(w.both("HIT", equal, getInputPin("VALID")), getOutputPin("HIT"));
        w.finish();
    }
};

class CacheLineState : public IOComponent {
public:
    CacheLineState(std::string name, size_t sets, size_t address_offset)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "INVALIDATE", "CLEAR", "MARK"})
                  self->addPin(pin, PinType::INPUT);
              self->addPin<32>("ADDRESS", PinType::INPUT);
              self->addPin("VALID", PinType::OUTPUT);
          }), sets_(sets), offset_(address_offset),
          type_("CacheLineState<" + std::to_string(sets) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto addr = builder.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(getInputPin<32>("ADDRESS"), addr->getInputPin<32>("IN"));
        auto clear_all = w.either("RESET_OR_INVALIDATE", getInputPin("RST"), getInputPin("INVALIDATE"));
        auto low = w.constant("LOW", false);
        std::vector<PinPtr> selected_valid;
        for (size_t set = 0; set < sets_; ++set) {
            std::vector<PinPtr> bits;
            for (size_t i = 0; i < std::countr_zero(sets_); ++i) {
                auto bit = addr->getOutputPin("OUT_" + std::to_string(offset_ + i));
                bits.push_back((set & (size_t{1} << i)) ? bit :
                    w.invert("NOT_SET_" + std::to_string(set) + "_" + std::to_string(i), bit));
            }
            auto selected = bits.empty() ? w.invert("SELECT_ZERO", low)
                                         : w.reduce("SELECT_" + std::to_string(set), bits, true);
            auto cell = builder.add(circuit::families::ClockedStateBit, "VALID_" + std::to_string(set));
            w.connect(getInputPin("CLK"), cell->getInputPin("CLK"));
            w.connect(clear_all, cell->getInputPin("RST"));
            auto clear = w.both("CLEAR_" + std::to_string(set), getInputPin("CLEAR"), selected);
            auto mark = w.both("MARK_" + std::to_string(set), getInputPin("MARK"), selected);
            auto keep = w.both("KEEP_" + std::to_string(set), cell->getOutputPin("Q"),
                               w.invert("NOT_CLEAR_" + std::to_string(set), clear));
            w.connect(w.either("NEXT_" + std::to_string(set), keep, mark), cell->getInputPin("D"));
            selected_valid.push_back(w.both("SELECTED_VALID_" + std::to_string(set),
                                              selected, cell->getOutputPin("Q")));
        }
        w.connect(w.reduce("VALID", selected_valid, false), getOutputPin("VALID"));
        w.finish();
    }
private:
    size_t sets_, offset_;
    std::string type_;
};

class CacheReplacementState : public IOComponent {
public:
    CacheReplacementState(std::string name, size_t sets, size_t address_offset)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "INVALIDATE", "TOUCH", "TOUCH_WAY"})
                  self->addPin(pin, PinType::INPUT);
              self->addPin<32>("ADDRESS", PinType::INPUT);
              self->addPin("MRU", PinType::OUTPUT);
          }), sets_(sets), offset_(address_offset) {}
    const char* getTypeName() const override { return "CacheReplacementState"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto addr = builder.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(getInputPin<32>("ADDRESS"), addr->getInputPin<32>("IN"));
        auto reset = w.either("RESET", getInputPin("RST"), getInputPin("INVALIDATE"));
        auto low = w.constant("LOW", false);
        std::vector<PinPtr> selected_mru;
        for (size_t set = 0; set < sets_; ++set) {
            std::vector<PinPtr> bits;
            for (size_t i = 0; i < std::countr_zero(sets_); ++i) {
                auto bit = addr->getOutputPin("OUT_" + std::to_string(offset_ + i));
                bits.push_back((set & (size_t{1} << i)) ? bit :
                    w.invert("NOT_SET_" + std::to_string(set) + "_" + std::to_string(i), bit));
            }
            auto selected = bits.empty() ? w.invert("SELECT_ZERO", low)
                                         : w.reduce("SELECT_" + std::to_string(set), bits, true);
            auto cell = builder.add(circuit::families::ClockedStateBit, "MRU_" + std::to_string(set));
            w.connect(getInputPin("CLK"), cell->getInputPin("CLK"));
            w.connect(reset, cell->getInputPin("RST"));
            auto update = w.both("UPDATE_" + std::to_string(set), getInputPin("TOUCH"), selected);
            auto next = w.either("NEXT_" + std::to_string(set),
                w.both("KEEP_" + std::to_string(set), cell->getOutputPin("Q"),
                       w.invert("NOT_UPDATE_" + std::to_string(set), update)),
                w.both("WRITE_" + std::to_string(set), update, getInputPin("TOUCH_WAY")));
            w.connect(next, cell->getInputPin("D"));
            selected_mru.push_back(w.both("SELECTED_" + std::to_string(set), selected, cell->getOutputPin("Q")));
        }
        w.connect(w.reduce("MRU", selected_mru, false), getOutputPin("MRU"));
        w.finish();
    }
private:
    size_t sets_, offset_;
};

template<size_t Width>
class CacheAddressPath : public IOComponent {
public:
    CacheAddressPath(std::string name, CacheConfig config)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "READ_MISS", "FILL_DATA_DONE",
                               "FILL_DATA_SUCCESS", "MEMORY_FAILURE"})
                  self->addPin(pin, PinType::INPUT);
              self->addPin<32>("REQUEST_ADDRESS", PinType::INPUT);
              for (auto pin : {"TAG_VALUE", "TAG_LOCAL_ADDRESS", "DATA_LOCAL_ADDRESS",
                               "FILL_BACKING_ADDRESS", "FILL_DATA_ADDRESS"})
                  self->addPin<32>(pin, PinType::OUTPUT);
              self->addPin("LAST_FILL_WORD", PinType::OUTPUT);
          }), config_(config), type_("CacheAddressPath<" + std::to_string(Width) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto low = w.constant("LOW", false);
        const size_t offset = std::countr_zero(config_.block_bytes);
        const size_t set_bits = std::countr_zero(config_.sets);
        const size_t word_shift = std::countr_zero(Width / 8);
        const size_t words_per_line = config_.block_bytes / (Width / 8);
        auto bits = builder.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(getInputPin<32>("REQUEST_ADDRESS"), bits->getInputPin<32>("IN"));
        auto joined = [&](const std::string& name, auto bit_source) -> PinPtr {
            auto join = builder.addNewComponent<BitJoiner<32>>(name);
            for (size_t bit = 0; bit < 32; ++bit)
                w.connect(bit_source(bit), join->getInputPin("IN_" + std::to_string(bit)));
            return join->getOutputPin<32>("OUT");
        };
        auto tag_value = joined("TAG_VALUE_BITS", [&](size_t bit) -> PinPtr {
            return bit + offset + set_bits < 32
                ? bits->getOutputPin("OUT_" + std::to_string(bit + offset + set_bits)) : low;
        });
        auto data_local = joined("DATA_LOCAL_BITS", [&](size_t bit) -> PinPtr {
            return bit >= word_shift && bit < offset + set_bits
                ? bits->getOutputPin("OUT_" + std::to_string(bit)) : low;
        });
        auto tag_local = joined("TAG_LOCAL_BITS", [&](size_t bit) -> PinPtr {
            return bit >= 2 && bit < 2 + set_bits
                ? bits->getOutputPin("OUT_" + std::to_string(offset + bit - 2)) : low;
        });
        w.connect(tag_value, getOutputPin<32>("TAG_VALUE"));
        w.connect(data_local, getOutputPin<32>("DATA_LOCAL_ADDRESS"));
        w.connect(tag_local, getOutputPin<32>("TAG_LOCAL_ADDRESS"));

        std::vector<PinPtr> fill_q;
        for (size_t i = 0; i < words_per_line; ++i) {
            auto state = builder.add(circuit::families::ClockedStateBit, "FILL_WORD_" + std::to_string(i));
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), state->getInputPin(pin));
            fill_q.push_back(state->getOutputPin("Q"));
            auto keep = w.reduce("KEEP_FILL_WORD_" + std::to_string(i),
                {w.invert("NO_DATA_COMPLETION_" + std::to_string(i), getInputPin("FILL_DATA_DONE")),
                 w.invert("NO_MEMORY_FAILURE_" + std::to_string(i), getInputPin("MEMORY_FAILURE")),
                 fill_q[i]}, true);
            auto advance = i == 0 ? PinPtr(getInputPin("READ_MISS"))
                : w.both("ADVANCE_FILL_WORD_" + std::to_string(i),
                         getInputPin("FILL_DATA_SUCCESS"), fill_q[i - 1]);
            w.connect(w.either("NEXT_FILL_WORD_" + std::to_string(i), keep, advance),
                      state->getInputPin("D"));
        }
        w.connect(fill_q.back(), getOutputPin("LAST_FILL_WORD"));
        std::vector<PinPtr> fill_bits;
        for (size_t bit = 0; bit < std::countr_zero(words_per_line); ++bit) {
            std::vector<PinPtr> selected;
            for (size_t i = 0; i < words_per_line; ++i)
                if ((i & (size_t{1} << bit)) != 0) selected.push_back(fill_q[i]);
            fill_bits.push_back(w.reduce("FILL_INDEX_BIT_" + std::to_string(bit), std::move(selected), false));
        }
        auto lower = joined("FILL_BACKING_BITS", [&](size_t bit) -> PinPtr {
            if (bit < word_shift) return low;
            if (bit < offset) return fill_bits.at(bit - word_shift);
            return bits->getOutputPin("OUT_" + std::to_string(bit));
        });
        auto fill_data = joined("FILL_DATA_BITS", [&](size_t bit) -> PinPtr {
            if (bit < word_shift || bit >= offset + set_bits) return low;
            if (bit < offset) return fill_bits.at(bit - word_shift);
            return bits->getOutputPin("OUT_" + std::to_string(bit));
        });
        w.connect(lower, getOutputPin<32>("FILL_BACKING_ADDRESS"));
        w.connect(fill_data, getOutputPin<32>("FILL_DATA_ADDRESS"));
        w.finish();
    }
private:
    CacheConfig config_;
    std::string type_;
};

// The controller's sequence, one state bit each: LOOKUP (the cycle after a
// request is taken: every way's tag and data arrive and are compared),
// MEMORY_SEND and MEMORY_WAIT (a backing-memory request: a fill word for a
// read miss, or the write of a write-through) and DONE (the response is
// shown until the front end finishes it).
class CacheLookupPhases : public IOComponent {
public:
    explicit CacheLookupPhases(std::string name)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "START", "REQUEST_OK", "FINISH", "IS_WRITE", "HIT",
                               "MEM_REQ_READY", "MEM_RSP_VALID", "MEM_OK", "LAST_FILL_WORD"})
                  self->addPin(pin, PinType::INPUT);
              for (auto pin : {"LOOKUP", "MEMORY_SEND", "MEMORY_WAIT", "DONE", "WORKING",
                               "INVALID_REQUEST", "READ_HIT", "READ_MISS", "WRITE_LOOKUP",
                               "FILL_DONE", "FILL_SUCCESS", "FILL_FAILURE", "FILL_COMPLETE",
                               "WRITE_DONE", "WRITE_OK"})
                  self->addPin(pin, PinType::OUTPUT);
          }) {}
    const char* getTypeName() const override { return "CacheLookupPhases"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto state = [&](const char* name) {
            auto bit = builder.add(circuit::families::ClockedStateBit, name);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bit->getInputPin(pin));
            return bit;
        };
        auto lookup = state("LOOKUP"), send = state("MEMORY_SEND"), wait = state("MEMORY_WAIT"), done = state("DONE");
        const auto L = lookup->getOutputPin("Q"), MS = send->getOutputPin("Q"), MW = wait->getOutputPin("Q"),
                   D = done->getOutputPin("Q");
        auto start = getInputPin("START");
        auto ok = getInputPin("REQUEST_OK");
        auto is_write = getInputPin("IS_WRITE");
        auto is_read = w.invert("IS_READ", is_write);
        auto hit = getInputPin("HIT");
        auto valid_start = w.both("VALID_START", start, ok);
        auto invalid = w.both("INVALID_REQUEST", start, w.invert("REQUEST_REJECTED", ok));
        auto read_hit = w.reduce("READ_HIT", {L, is_read, hit}, true);
        auto read_miss = w.reduce("READ_MISS", {L, is_read, w.invert("NOT_HIT", hit)}, true);
        auto write_lookup = w.both("WRITE_LOOKUP", L, is_write);
        // The backing-memory request: accepted while MEMORY_SEND, answered
        // then or later.
        auto sent = w.both("MEMORY_SENT", MS, getInputPin("MEM_REQ_READY"));
        auto rsp = getInputPin("MEM_RSP_VALID");
        auto mem_done = w.both("MEMORY_DONE", w.either("MEMORY_ACTIVE", sent, MW), rsp);
        auto mem_ok = getInputPin("MEM_OK");
        auto fill_done = w.both("FILL_DONE", mem_done, is_read);
        auto fill_success = w.both("FILL_SUCCESS", fill_done, mem_ok);
        auto fill_failure = w.both("FILL_FAILURE", fill_done, w.invert("FILL_NOT_OK", mem_ok));
        auto last = getInputPin("LAST_FILL_WORD");
        auto fill_complete = w.both("FILL_COMPLETE", fill_success, last);
        auto more = w.both("MORE_FILL_WORDS", fill_success, w.invert("NOT_LAST_FILL_WORD", last));
        auto write_done = w.both("WRITE_DONE", mem_done, is_write);
        auto write_ok = w.both("WRITE_OK", write_done, mem_ok);
        w.connect(valid_start, lookup->getInputPin("D"));
        w.connect(w.reduce("NEXT_MEMORY_SEND", {read_miss, write_lookup, more,
            w.both("STAY_MEMORY_SEND", MS, w.invert("NOT_SENT", getInputPin("MEM_REQ_READY")))}, false),
            send->getInputPin("D"));
        auto no_response = w.invert("NO_MEMORY_RESPONSE", rsp);
        w.connect(w.both("NEXT_MEMORY_WAIT", w.either("WAITING", sent, MW), no_response), wait->getInputPin("D"));
        w.connect(w.reduce("NEXT_DONE", {invalid, read_hit, fill_failure, fill_complete, write_done,
            w.both("STAY_DONE", D, w.invert("NOT_FINISHED", getInputPin("FINISH")))}, false), done->getInputPin("D"));
        for (const auto& [name, signal] : std::vector<std::pair<const char*, PinPtr>>{
                 {"LOOKUP", L}, {"MEMORY_SEND", MS}, {"MEMORY_WAIT", MW}, {"DONE", D},
                 {"WORKING", w.reduce("WORKING_NOW", {L, MS, MW}, false)},
                 {"INVALID_REQUEST", invalid}, {"READ_HIT", read_hit}, {"READ_MISS", read_miss},
                 {"WRITE_LOOKUP", write_lookup}, {"FILL_DONE", fill_done}, {"FILL_SUCCESS", fill_success},
                 {"FILL_FAILURE", fill_failure}, {"FILL_COMPLETE", fill_complete},
                 {"WRITE_DONE", write_done}, {"WRITE_OK", write_ok}})
            w.connect(signal, getOutputPin(name));
        w.finish();
    }
};

// Bank pins of one way's tag or data bank, as the controller drives them:
// READ (read at the next edge, every way at once), WRITE (write at the next
// edge), WORD_ADDR, WRITE_DATA, WRITE_MASK; READ_DATA comes back.
void bankPins(IOComponent* self, const std::string& prefix, size_t width) {
    self->addPin(prefix + "READ", PinType::OUTPUT);
    self->addPin(prefix + "WRITE", PinType::OUTPUT);
    self->addPinDynamic(prefix + "WORD_ADDR", PinType::OUTPUT, 32);
    self->addPinDynamic(prefix + "WRITE_DATA", PinType::OUTPUT, width);
    self->addPinDynamic(prefix + "WRITE_MASK", PinType::OUTPUT, width / 8);
    self->addPinDynamic(prefix + "READ_DATA", PinType::INPUT, width);
}

// The cache controller: a request is looked up in every way at once (one
// cycle), a read miss fills the victim line word by word from the backing
// memory, a write goes through to the backing memory. The banks are native
// devices driven directly (Cache<Width>::buildInternals).
template<size_t Width>
class CacheBankController : public IOComponent {
public:
    CacheBankController(std::string name, CacheConfig config)
        : IOComponent(std::move(name), [config](IOComponent* self) {
              cachePins(self, Width);
              for (size_t way = 0; way < config.ways; ++way) {
                  bankPins(self, "TAG" + std::to_string(way) + "_", 32);
                  bankPins(self, "DATA" + std::to_string(way) + "_", Width);
              }
          }), config_(config), type_("CacheBankController<" + std::to_string(Width) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    CacheConfig config_;
    std::string type_;
};

template<size_t Width>
void CacheBankController<Width>::buildInternals(ComponentBuilder& builder) {
    StructuralWiring w(builder);
    auto low = w.constant("LOW", false);
    auto zero_data = builder.addNewComponent<ConstantValue<Width>>("ZERO_DATA", 0)->template getOutputPin<Width>("OUT");
    auto zero3 = builder.addNewComponent<ConstantValue<3>>("ZERO_STATUS", 0)->getOutputPin<3>("OUT");
    auto read_op = builder.addNewComponent<ConstantValue<3>>("READ_OP", 0)->getOutputPin<3>("OUT");
    auto write_op = builder.addNewComponent<ConstantValue<3>>("WRITE_OP", 1)->getOutputPin<3>("OUT");
    auto full_tag_mask = builder.addNewComponent<ConstantValue<4>>("FULL_TAG_MASK", 15)->getOutputPin<4>("OUT");
    auto full_data_mask = builder.addNewComponent<ConstantValue<Width / 8>>("FULL_DATA_MASK", (1u << (Width / 8)) - 1)
                              ->template getOutputPin<Width / 8>("OUT");
    auto zero_mask = builder.addNewComponent<ConstantValue<Width / 8>>("ZERO_MASK", 0)
                         ->template getOutputPin<Width / 8>("OUT");

    auto front = builder.addNewComponent<MemoryTransactionPort>("FRONTEND", Width);
    for (auto pin : {"CLK", "RST", "REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA",
                     "REQ_WRITE_MASK", "RSP_READY"})
        w.connect(getInputPinDynamic(pin), front->getInputPinDynamic(pin));
    for (auto pin : {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"})
        w.connect(front->getOutputPinDynamic(pin), getOutputPinDynamic(pin));
    auto check = builder.addNewComponent<CacheRequestCheck>("REQUEST_CHECK", Width,
        config_.backing_depth_words, config_.read_write);
    w.connect(front->getOutputPin<3>("OP"), check->getInputPin<3>("OP"));
    w.connect(front->getOutputPin<32>("ADDR"), check->getInputPin<32>("ADDR"));
    w.connect(front->getOutputPinDynamic("MASK"), check->getInputPinDynamic("MASK"));
    auto start = front->getOutputPin("START");

    auto addr_reg = builder.add(circuit::families::registerWord(32), "REQUEST_ADDRESS");
    auto result_reg = builder.add(circuit::families::registerWord(Width), "READ_RESULT");
    auto status_reg = builder.add(circuit::families::registerWord(3), "RESULT_STATUS");
    auto write_data_reg = builder.add(circuit::families::registerWord(Width), "WRITE_DATA");
    auto write_mask_reg = builder.add(circuit::families::registerWord(Width / 8), "WRITE_MASK");
    for (const auto& reg : {addr_reg, result_reg, status_reg, write_data_reg, write_mask_reg})
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), reg->getInputPin(pin));
    for (auto reg : {addr_reg, write_data_reg, write_mask_reg}) w.connect(start, reg->getInputPin("WE"));
    w.connect(front->getOutputPin<32>("ADDR"), addr_reg->getInputPin<32>("D"));
    w.connect(front->getOutputPinDynamic("DATA"), write_data_reg->getInputPinDynamic("D"));
    w.connect(front->getOutputPinDynamic("MASK"), write_mask_reg->getInputPinDynamic("D"));
    auto state_bit = [&](const char* name) {
        auto bit = builder.add(circuit::families::ClockedStateBit, name);
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bit->getInputPin(pin));
        return bit;
    };
    // A clocked state bit has no enable: hold the operation while busy.
    auto write_bit = state_bit("IS_WRITE");
    w.connect(w.either("NEXT_WRITE", w.both("ACCEPT_WRITE", start, check->getOutputPin("WRITE")),
        w.both("HOLD_WRITE", write_bit->getOutputPin("Q"), w.invert("NOT_NEW_REQUEST", start))), write_bit->getInputPin("D"));
    auto is_write = write_bit->getOutputPin("Q");
    auto is_read = w.invert("IS_READ", is_write);
    auto child_clock = memory_detail::bridge(builder, w, "CHILD_CLOCK", getInputPin("CLK"));
    auto child_reset = memory_detail::bridge(builder, w, "CHILD_RESET", getInputPin("RST"));
    w.connect(child_clock, getOutputPin("MEM_CLK"));
    w.connect(child_reset, getOutputPin("MEM_RST"));

    const size_t offset = std::countr_zero(config_.block_bytes);
    const size_t set_bits = std::countr_zero(config_.sets);
    const size_t word_shift = std::countr_zero(Width / 8);
    auto address_path = builder.addNewComponent<CacheAddressPath<Width>>("ADDRESS_PATH", config_);
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), address_path->getInputPin(pin));
    w.connect(addr_reg->getOutputPin<32>("Q"), address_path->template getInputPin<32>("REQUEST_ADDRESS"));
    auto tag_value = address_path->template getOutputPin<32>("TAG_VALUE");
    // A 32-bit field `count` bits wide from bit `from` of `source`, moved to bit 0.
    auto field = [&](const std::string& name, PinPtr source, size_t from, size_t count) -> PinPtr {
        auto split = builder.addNewComponent<BitSplitter<32>>(name + "_BITS");
        w.connect(source, split->getInputPin<32>("IN"));
        auto join = builder.addNewComponent<BitJoiner<32>>(name);
        for (size_t bit = 0; bit < 32; ++bit)
            w.connect(bit < count ? PinPtr(split->getOutputPin("OUT_" + std::to_string(from + bit))) : low,
                      join->getInputPin("IN_" + std::to_string(bit)));
        return join->getOutputPin<32>("OUT");
    };
    // Bank word addresses: the set (tag bank) and the set and word in the
    // line (data bank), from the front end's address in the accepting cycle
    // (a clocked bank registers it there) and from the held address after it
    // (a flip-flop bank reads it then).
    auto read_address = memory_detail::payloadMux(builder, w, "READ_ADDRESS",
        addr_reg->getOutputPin<32>("Q"), front->getOutputPin<32>("ADDR"), start);
    auto tag_read_index = field("TAG_READ_INDEX", read_address, offset, set_bits);
    auto data_read_index = field("DATA_READ_INDEX", read_address, word_shift, offset + set_bits - word_shift);
    auto tag_index = field("TAG_INDEX", addr_reg->getOutputPin<32>("Q"), offset, set_bits);
    auto data_index = field("DATA_INDEX", addr_reg->getOutputPin<32>("Q"), word_shift, offset + set_bits - word_shift);
    auto fill_index = field("FILL_INDEX", address_path->template getOutputPin<32>("FILL_DATA_ADDRESS"),
                            word_shift, offset + set_bits - word_shift);

    std::vector<std::shared_ptr<CacheLineState>> lines;
    std::vector<PinPtr> matches;
    for (size_t way = 0; way < config_.ways; ++way) {
        auto line = builder.addNewComponent<CacheLineState>("LINE_STATE_" + std::to_string(way), config_.sets, offset);
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), line->getInputPin(pin));
        w.connect(addr_reg->getOutputPin<32>("Q"), line->getInputPin<32>("ADDRESS"));
        lines.push_back(line);
        auto match = builder.addNewComponent<CacheTagMatch32>("TAG_MATCH_" + std::to_string(way));
        w.connect(tag_value, match->getInputPin<32>("EXPECTED"));
        w.connect(getInputPinDynamic("TAG" + std::to_string(way) + "_READ_DATA"), match->getInputPin<32>("STORED"));
        w.connect(line->getOutputPin("VALID"), match->getInputPin("VALID"));
        matches.push_back(match->getOutputPin("HIT"));
    }
    auto hit = config_.ways == 1 ? matches[0] : w.either("HIT", matches[0], matches[1]);
    auto hit_one = config_.ways == 1 ? low : matches[1];
    std::shared_ptr<CacheReplacementState> replacement;
    PinPtr victim_one = low;
    if (config_.ways == 2) {
        replacement = builder.addNewComponent<CacheReplacementState>("REPLACEMENT", config_.sets, offset);
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), replacement->getInputPin(pin));
        w.connect(addr_reg->getOutputPin<32>("Q"), replacement->getInputPin<32>("ADDRESS"));
        victim_one = w.both("VICTIM_WAY_ONE", lines[0]->getOutputPin("VALID"),
            w.either("WAY_ONE_INVALID_OR_LRU",
                w.invert("WAY_ONE_INVALID", lines[1]->getOutputPin("VALID")),
                w.invert("WAY_ONE_LRU", replacement->getOutputPin("MRU"))));
    }

    auto status_ok = [&](const std::string& name, PinPtr status) -> PinPtr {
        auto split = builder.addNewComponent<BitSplitter<3>>(name + "_BITS");
        w.connect(status, split->getInputPin<3>("IN"));
        return w.invert(name + "_OK", w.reduce(name + "_ERROR", {split->getOutputPin("OUT_0"),
            split->getOutputPin("OUT_1"), split->getOutputPin("OUT_2")}, false));
    };
    auto phases = builder.addNewComponent<CacheLookupPhases>("PHASES");
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), phases->getInputPin(pin));
    w.connect(start, phases->getInputPin("START"));
    w.connect(check->getOutputPin("ACCEPT"), phases->getInputPin("REQUEST_OK"));
    w.connect(front->getOutputPin("FINISH"), phases->getInputPin("FINISH"));
    w.connect(is_write, phases->getInputPin("IS_WRITE"));
    w.connect(hit, phases->getInputPin("HIT"));
    w.connect(getInputPin("MEM_REQ_READY"), phases->getInputPin("MEM_REQ_READY"));
    w.connect(getInputPin("MEM_RSP_VALID"), phases->getInputPin("MEM_RSP_VALID"));
    w.connect(status_ok("MEMORY_STATUS", getInputPin<3>("MEM_RSP_STATUS")), phases->getInputPin("MEM_OK"));
    w.connect(address_path->getOutputPin("LAST_FILL_WORD"), phases->getInputPin("LAST_FILL_WORD"));
    auto signal = [&](const char* name) -> PinPtr { return phases->getOutputPin(name); };
    auto read_hit = signal("READ_HIT"), read_miss = signal("READ_MISS"), write_lookup = signal("WRITE_LOOKUP");
    auto fill_success = signal("FILL_SUCCESS"), fill_failure = signal("FILL_FAILURE"), fill_complete = signal("FILL_COMPLETE");
    auto write_done = signal("WRITE_DONE"), write_ok = signal("WRITE_OK"), invalid = signal("INVALID_REQUEST");

    // The way a fill or a write-through targets: the victim of a read miss,
    // the hit way of a write; and whether that write hit.
    std::shared_ptr<IOComponent> current_way;
    PinPtr way_one = low;
    if (config_.ways == 2) {
        current_way = state_bit("CURRENT_WAY");
        way_one = current_way->getOutputPin("Q");
        auto choose = w.either("CHOOSE_WAY", read_miss, write_lookup);
        auto chosen_one = w.either("CHOSEN_WAY_ONE", w.both("VICTIM_IS_ONE", read_miss, victim_one),
                                   w.both("WRITE_HIT_ONE", write_lookup, hit_one));
        w.connect(w.either("NEXT_CURRENT_WAY", chosen_one, w.both("HOLD_CURRENT_WAY", way_one, w.invert("NOT_CHOOSING", choose))),
                  current_way->getInputPin("D"));
    }
    auto write_hit_bit = state_bit("WRITE_HIT");
    w.connect(w.either("NEXT_WRITE_HIT", w.both("WRITE_HIT_NOW", write_lookup, hit),
        w.both("HOLD_WRITE_HIT", write_hit_bit->getOutputPin("Q"), w.invert("NOT_LOOKING_UP", signal("LOOKUP")))),
        write_hit_bit->getInputPin("D"));
    auto update_written = w.both("UPDATE_WRITTEN_LINE", write_ok, write_hit_bit->getOutputPin("Q"));

    // Banks: every way read when a request is taken; the current way's data
    // bank written with a fill word or a write hit, its tag bank with the
    // last fill word.
    auto valid_start = w.both("VALID_START", start, check->getOutputPin("ACCEPT"));
    auto data_write_word = memory_detail::payloadMux(builder, w, "DATA_WRITE_WORD",
        getInputPinDynamic("MEM_RSP_READ_DATA"), write_data_reg->getOutputPinDynamic("Q"), is_write);
    auto data_write_mask = memory_detail::payloadMux(builder, w, "DATA_WRITE_MASK_SOURCE",
        full_data_mask, write_mask_reg->getOutputPinDynamic("Q"), is_write);
    auto data_write_index = memory_detail::payloadMux(builder, w, "DATA_WRITE_INDEX", fill_index, data_index, is_write);
    auto data_writing = w.either("DATA_WRITING", fill_success, update_written);
    for (size_t way = 0; way < config_.ways; ++way) {
        const auto n = std::to_string(way);
        auto this_way = way == 0 ? w.invert("CURRENT_WAY_ZERO", way_one) : way_one;
        auto data_write = w.both("DATA_WRITE_" + n, data_writing, this_way);
        auto tag_write = w.both("TAG_WRITE_" + n, fill_complete, this_way);
        for (auto [bank, write] : std::vector<std::pair<std::string, PinPtr>>{{"TAG", tag_write}, {"DATA", data_write}}) {
            const auto p = bank + n + "_";
            w.connect(valid_start, getOutputPin(p + "READ"));
            w.connect(write, getOutputPin(p + "WRITE"));
        }
        w.connect(memory_detail::payloadMux(builder, w, "TAG_ADDRESS_" + n, tag_read_index, tag_index, tag_write),
                  getOutputPin<32>("TAG" + n + "_WORD_ADDR"));
        w.connect(tag_value, getOutputPin<32>("TAG" + n + "_WRITE_DATA"));
        w.connect(full_tag_mask, getOutputPin<4>("TAG" + n + "_WRITE_MASK"));
        w.connect(memory_detail::payloadMux(builder, w, "DATA_ADDRESS_" + n, data_read_index, data_write_index, data_write),
                  getOutputPin<32>("DATA" + n + "_WORD_ADDR"));
        w.connect(data_write_word, getOutputPinDynamic("DATA" + n + "_WRITE_DATA"));
        w.connect(data_write_mask, getOutputPinDynamic("DATA" + n + "_WRITE_MASK"));
        // Line state: evicted at the miss decision, marked with the last fill word.
        auto victim = way == 0 ? w.invert("VICTIM_ZERO", victim_one) : victim_one;
        w.connect(w.both("EVICT_LINE_" + n, read_miss, victim), lines[way]->getInputPin("CLEAR"));
        w.connect(tag_write, lines[way]->getInputPin("MARK"));
    }

    // Idle: no lookup and no backing-memory request; a shown response
    // still lets the front end take the next request as it is finished.
    auto working = signal("WORKING");
    auto idle = w.both("IDLE", w.invert("NOT_WORKING", working), w.invert("NOT_DONE", signal("DONE")));
    w.connect(w.both("AVAILABLE", w.invert("NO_WORK", working), w.invert("NO_INVALIDATION", getInputPin("INV_VALID"))),
              front->getInputPin("BACK_READY"));
    w.connect(idle, getOutputPin("INV_READY"));
    auto invalidate = w.both("ACCEPT_INVALIDATION", getInputPin("INV_VALID"), idle);
    for (const auto& line : lines) w.connect(invalidate, line->getInputPin("INVALIDATE"));
    if (replacement) {
        w.connect(invalidate, replacement->getInputPin("INVALIDATE"));
        w.connect(w.reduce("TOUCH_REPLACEMENT", {read_hit, fill_complete, update_written}, false), replacement->getInputPin("TOUCH"));
        w.connect(memory_detail::payloadMux(builder, w, "TOUCHED_WAY", way_one, hit_one, read_hit), replacement->getInputPin("TOUCH_WAY"));
    }
    w.connect(signal("DONE"), front->getInputPin("DONE"));

    // Result: the hit way's word, or the requested word as the fill passes it.
    auto hit_word = config_.ways == 1 ? PinPtr(getInputPinDynamic("DATA0_READ_DATA"))
        : memory_detail::payloadMux(builder, w, "HIT_WORD", getInputPinDynamic("DATA0_READ_DATA"),
                                    getInputPinDynamic("DATA1_READ_DATA"), hit_one);
    PinPtr requested = w.constant("ONE_WORD_LINE", true);
    if (offset > word_shift) {
        auto fill_bits = builder.addNewComponent<BitSplitter<32>>("FILL_WORD_BITS");
        auto want_bits = builder.addNewComponent<BitSplitter<32>>("WANTED_WORD_BITS");
        w.connect(address_path->template getOutputPin<32>("FILL_BACKING_ADDRESS"), fill_bits->getInputPin<32>("IN"));
        w.connect(addr_reg->getOutputPin<32>("Q"), want_bits->getInputPin<32>("IN"));
        std::vector<PinPtr> same;
        for (size_t bit = word_shift; bit < offset; ++bit)
            same.push_back(w.invert("SAME_WORD_" + std::to_string(bit), w.binary<XORGate>("WORD_DIFFERS_" + std::to_string(bit),
                fill_bits->getOutputPin("OUT_" + std::to_string(bit)), want_bits->getOutputPin("OUT_" + std::to_string(bit)))));
        requested = w.reduce("REQUESTED_WORD", same, true);
    }
    w.connect(w.either("CAPTURE_READ_RESULT", read_hit, w.both("CAPTURE_FILL_WORD", fill_success, requested)),
              result_reg->getInputPin("WE"));
    w.connect(memory_detail::payloadMux(builder, w, "READ_DATA_SOURCE", getInputPinDynamic("MEM_RSP_READ_DATA"), hit_word, read_hit),
              result_reg->getInputPinDynamic("D"));
    w.connect(w.reduce("CAPTURE_STATUS", {invalid, read_hit, fill_failure, fill_complete, write_done}, false),
              status_reg->getInputPin("WE"));
    auto status = memory_detail::payloadMux(builder, w, "STATUS_FROM_MEMORY", zero3, getInputPin<3>("MEM_RSP_STATUS"),
                                            w.either("MEMORY_STATUS_SELECTED", fill_failure, write_done));
    status = memory_detail::payloadMux(builder, w, "STATUS_FROM_REQUEST", status, check->getOutputPin<3>("STATUS"), invalid);
    w.connect(status, status_reg->getInputPin<3>("D"));
    w.connect(status_reg->getOutputPin<3>("Q"), front->getInputPin<3>("STATUS"));
    auto return_read_data = w.both("RETURN_READ_DATA", status_ok("COMPLETED_STATUS", status_reg->getOutputPin<3>("Q")), is_read);
    w.connect(memory_detail::payloadMux(builder, w, "ZERO_FAILED_RESULT", zero_data, result_reg->getOutputPinDynamic("Q"), return_read_data),
              front->getInputPinDynamic("RESULT"));

    // Backing memory: fill words for a read miss, the write of a write-through.
    w.connect(read_miss, address_path->getInputPin("READ_MISS"));
    w.connect(signal("FILL_DONE"), address_path->getInputPin("FILL_DATA_DONE"));
    w.connect(fill_success, address_path->getInputPin("FILL_DATA_SUCCESS"));
    w.connect(fill_failure, address_path->getInputPin("MEMORY_FAILURE"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_ADDRESS", address_path->template getOutputPin<32>("FILL_BACKING_ADDRESS"),
        addr_reg->getOutputPin<32>("Q"), is_write), getOutputPin<32>("MEM_REQ_ADDR"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_OP", read_op, write_op, is_write), getOutputPin<3>("MEM_REQ_OP"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_WRITE_DATA", zero_data, write_data_reg->getOutputPinDynamic("Q"), is_write),
              getOutputPinDynamic("MEM_REQ_WRITE_DATA"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_WRITE_MASK", zero_mask, write_mask_reg->getOutputPinDynamic("Q"), is_write),
              getOutputPinDynamic("MEM_REQ_WRITE_MASK"));
    w.connect(signal("MEMORY_SEND"), getOutputPin("MEM_REQ_VALID"));
    w.connect(w.either("MEMORY_RESPONSE_READY", signal("MEMORY_SEND"), signal("MEMORY_WAIT")), getOutputPin("MEM_RSP_READY"));
    (void)is_read;
    w.finish();
}

} // namespace

template<size_t Width>
Cache<Width>::Cache(std::string name, CacheConfig config)
    : IOComponent(std::move(name), [](IOComponent* self) { cachePins(self, Width); }),
      config_(config), type_("Cache<" + std::to_string(Width) + ">") {
    if (!config_.block_bytes) config_.block_bytes = Width / 8;
    config_.validate(Width);
}

// The controller and, per way, a tag bank and a data bank: native devices
// driven directly. An `sram` bank is the clocked on-chip macro (CE_N for a
// read or a write, WE_N for a write, the output always enabled); an `ff` bank
// is the flip-flop device (WE for a write, read always enabled).
template<size_t Width>
void Cache<Width>::buildInternals(ComponentBuilder& builder) {
    StructuralWiring w(builder);
    auto controller = builder.addNewComponent<CacheBankController<Width>>("CONTROLLER", config_);
    for (const auto& [name, pin] : getAllInputPins()) w.connect(pin, controller->getInputPinDynamic(name));
    for (const auto& [name, pin] : getAllOutputPins()) w.connect(controller->getOutputPinDynamic(name), pin);
    auto low = w.constant("LOW", false);
    auto high = w.constant("HIGH", true);
    const size_t words_per_line = config_.block_bytes / (Width / 8);
    for (size_t way = 0; way < config_.ways; ++way) {
        for (const bool tag : {true, false}) {
            const auto n = std::to_string(way);
            const auto prefix = std::string(tag ? "TAG" : "DATA") + n + "_";
            const size_t width = tag ? 32 : Width;
            const size_t depth = tag ? config_.sets : config_.sets * words_per_line;
            const auto technology = tag ? config_.tag_technology : config_.data_technology;
            ParameterMap parameters{{"depth_words", std::to_string(depth)}};
            if (technology == Technology::SRAM) {
                parameters["words_per_row"] = std::to_string(std::min<size_t>(2, depth));
                parameters["clocked"] = "1";
            }
            auto bank = memory_device::child(builder, *this, memoryContractId(technology, "device", width),
                                             std::string(tag ? "TAG_STORE_" : "DATA_STORE_") + n, parameters);
            auto read = controller->getOutputPin(prefix + "READ"), write = controller->getOutputPin(prefix + "WRITE");
            if (technology == Technology::SRAM) {
                w.connect(getInputPin("CLK"), bank->getInputPin("CLK"));
                w.connect(w.invert(prefix + "SELECT", w.either(prefix + "ACCESS", read, write)), bank->getInputPin("CE_N"));
                w.connect(w.invert(prefix + "WRITE_STROBE", write), bank->getInputPin("WE_N"));
                w.connect(low, bank->getInputPin("OE_N"));
            } else {
                for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bank->getInputPin(pin));
                w.connect(write, bank->getInputPin("WE"));
                w.connect(high, bank->getInputPin("READ_EN"));
            }
            w.connect(controller->getOutputPinDynamic(prefix + "WORD_ADDR"), bank->getInputPinDynamic("WORD_ADDR"));
            w.connect(controller->getOutputPinDynamic(prefix + "WRITE_DATA"), bank->getInputPinDynamic("WRITE_DATA"));
            w.connect(controller->getOutputPinDynamic(prefix + "WRITE_MASK"), bank->getInputPinDynamic("WRITE_MASK"));
            w.connect(bank->getOutputPinDynamic("READ_DATA"), controller->getInputPinDynamic(prefix + "READ_DATA"));
        }
    }
    w.finish();
}

template class Cache<8>;
template class Cache<16>;
template class Cache<32>;

void registerCaches(ComponentCatalog& catalog) {
    for (size_t width : {8, 16, 32}) {
        for (bool scalable : {false, true}) {
            const auto id = std::string(scalable ? "memory.cache.scalable." : "memory.cache.") +
                std::to_string(width);
            auto schema = [width, scalable](const ParameterMap& parameters) {
                auto config = cacheConfig(parameters);
                if (!parameters.contains("storage_form")) config.scalable_storage = scalable;
                if (config.scalable_storage != scalable)
                    throw std::invalid_argument("Cache storage form does not match contract");
                config.validate(width);
                return memory_device::describe([width](IOComponent* self) { cachePins(self, width); });
            };
            CacheConfig defaults;
            defaults.scalable_storage = scalable;
            catalog.registerContract({id, 1, "Cache<" + std::to_string(width) + ">",
                schema(cacheParameters(defaults)), {}, "four-state", "transaction-completion-v2", schema});
            ImplementationDescriptor implementation;
            implementation.id = id + ".structural";
            implementation.contract_id = id;
            implementation.fidelity = Fidelity::Structural;
            implementation.concrete_type_name = "Cache<" + std::to_string(width) + ">";
            implementation.supports = [schema](const ParameterMap& parameters) {
                try { schema(parameters); return true; }
                catch (const std::exception&) { return false; }
            };
            implementation.factory = [width, scalable](const std::string& name, const ParameterMap& parameters,
                                             const std::shared_ptr<BuildContext>& context) -> std::shared_ptr<Component> {
                auto config = cacheConfig(parameters);
                if (!parameters.contains("storage_form")) config.scalable_storage = scalable;
                if (config.scalable_storage != scalable)
                    throw std::invalid_argument("Cache storage form does not match contract");
                if (width == 8) return Component::createWithContext<Cache<8>>(context, name, config);
                if (width == 16) return Component::createWithContext<Cache<16>>(context, name, config);
                return Component::createWithContext<Cache<32>>(context, name, config);
            };
            implementation.evidence = {VerificationStatus::Characterized,
                {"structural controller, tag comparator and valid state; replaceable endpoint banks"},
                {(!scalable && width == 8) ? "CacheComponentScenarioTest" : "CacheReadTest"},
                {}, (!scalable && width == 8) ? std::vector<std::string>{"CacheReadTest"}
                                           : std::vector<std::string>{},
                "four-state", "transaction-completion-v2"};
            catalog.registerImplementation(std::move(implementation));
        }
    }
}

} // namespace circuit::memory
