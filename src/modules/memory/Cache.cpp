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

enum CachePhase { TagSend, TagWait, Decide, DataSend, DataWait, MemorySend, MemoryWait,
                  FillDataSend, FillDataWait, FillTagSend, FillTagWait,
                  WriteMemorySend, WriteMemoryWait, WriteDataSend, WriteDataWait, Done, PhaseCount };
constexpr std::array<const char*, PhaseCount> cachePhaseNames = {
    "TAG_SEND", "TAG_WAIT", "DECIDE", "DATA_SEND", "DATA_WAIT", "MEMORY_SEND", "MEMORY_WAIT",
    "FILL_DATA_SEND", "FILL_DATA_WAIT", "FILL_TAG_SEND", "FILL_TAG_WAIT",
    "WRITE_MEMORY_SEND", "WRITE_MEMORY_WAIT", "WRITE_DATA_SEND", "WRITE_DATA_WAIT", "DONE"
};

class CachePhaseSequencer : public IOComponent {
public:
    CachePhaseSequencer(std::string name, size_t ways)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "START", "FINISH", "REQUEST_OK", "IS_WRITE",
                               "HIT", "WAY_ONE", "LAST_FILL_WORD"})
                  self->addPin(pin, PinType::INPUT);
              for (auto prefix : {"TAG_", "DATA_", "MEM_"}) {
                  self->addPin(std::string(prefix) + "REQ_READY", PinType::INPUT);
                  self->addPin(std::string(prefix) + "RSP_VALID", PinType::INPUT);
                  self->addPin<3>(std::string(prefix) + "RSP_STATUS", PinType::INPUT);
              }
              for (auto name : cachePhaseNames) self->addPin(name, PinType::OUTPUT);
              for (auto pin : {"IDLE", "INVALID_REQUEST", "READ_MISS", "RETRY_SECOND_WAY",
                               "TAG_DONE", "TAG_FAILURE", "DATA_DONE", "DATA_OK",
                               "MEMORY_DONE", "MEMORY_FAILURE", "FILL_DATA_DONE",
                               "FILL_DATA_SUCCESS", "FILL_DATA_FAILURE", "FILL_TAG_SUCCESS",
                               "FILL_TAG_FAILURE", "WRITE_MEMORY_FAILURE",
                               "WRITE_NO_ALLOCATE", "WRITE_DATA_DONE"})
                  self->addPin(pin, PinType::OUTPUT);
          }), ways_(ways) {}
    const char* getTypeName() const override { return "CachePhaseSequencer"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto low = w.constant("LOW", false);
        auto high = w.constant("HIGH", true);
        std::vector<std::shared_ptr<IOComponent>> state;
        std::vector<PinPtr> q;
        for (auto name : cachePhaseNames) {
            auto bit = builder.add(circuit::families::ClockedStateBit, name);
            state.push_back(bit);
            q.push_back(bit->getOutputPin("Q"));
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bit->getInputPin(pin));
        }
        auto gate = [&](const std::string& name, PinPtr a, PinPtr b) { return w.both(name, a, b); };
        auto neg = [&](const std::string& name, PinPtr value) { return w.invert(name, value); };
        auto any = [&](const std::string& name, std::vector<PinPtr> terms) {
            return w.reduce(name, std::move(terms), false);
        };
        auto status_ok = [&](const std::string& name, PinPtr status) -> PinPtr {
            auto split = builder.addNewComponent<BitSplitter<3>>(name + "_BITS");
            w.connect(status, split->getInputPin<3>("IN"));
            return neg(name + "_OK", any(name + "_ERROR", {split->getOutputPin("OUT_0"),
                split->getOutputPin("OUT_1"), split->getOutputPin("OUT_2")}));
        };
        auto tag_ok = status_ok("TAG_STATUS", getInputPin<3>("TAG_RSP_STATUS"));
        auto data_ok = status_ok("DATA_STATUS", getInputPin<3>("DATA_RSP_STATUS"));
        auto mem_ok = status_ok("MEM_STATUS", getInputPin<3>("MEM_RSP_STATUS"));
        auto request_ok = getInputPin("REQUEST_OK");
        auto is_write = getInputPin("IS_WRITE");
        auto is_read = neg("IS_READ", is_write);
        auto start = getInputPin("START");
        auto invalid = gate("INVALID_REQUEST", start, neg("REQUEST_REJECTED", request_ok));
        auto transfer = [&](const std::string& name, CachePhase send, const std::string& port) {
            return gate(name, q[send], getInputPin(port + "REQ_READY"));
        };
        auto completion = [&](const std::string& name, CachePhase send, CachePhase wait,
                              const std::string& port, PinPtr sent) {
            return gate(name, any(name + "_ACTIVE", {sent, q[wait]}), getInputPin(port + "RSP_VALID"));
        };
        auto tag_sent = transfer("TAG_SENT", TagSend, "TAG_");
        auto tag_done = completion("TAG_COMPLETE", TagSend, TagWait, "TAG_", tag_sent);
        auto tag_success = gate("TAG_SUCCESS", tag_done, tag_ok);
        auto tag_fail = gate("TAG_FAILURE", tag_done, neg("TAG_NOT_OK", tag_ok));
        auto data_sent = transfer("DATA_SENT", DataSend, "DATA_");
        auto data_done = completion("DATA_COMPLETE", DataSend, DataWait, "DATA_", data_sent);
        auto memory_sent = transfer("MEMORY_SENT", MemorySend, "MEM_");
        auto memory_done = completion("MEMORY_COMPLETE", MemorySend, MemoryWait, "MEM_", memory_sent);
        auto memory_success = gate("MEMORY_SUCCESS", memory_done, mem_ok);
        auto memory_fail = gate("MEMORY_FAILURE", memory_done, neg("MEMORY_NOT_OK", mem_ok));
        auto fill_data_sent = transfer("FILL_DATA_SENT", FillDataSend, "DATA_");
        auto fill_data_done = completion("FILL_DATA_COMPLETE", FillDataSend, FillDataWait, "DATA_", fill_data_sent);
        auto fill_data_success = gate("FILL_DATA_SUCCESS", fill_data_done, data_ok);
        auto fill_data_fail = gate("FILL_DATA_FAILURE", fill_data_done, neg("FILL_DATA_NOT_OK", data_ok));
        auto fill_tag_sent = transfer("FILL_TAG_SENT", FillTagSend, "TAG_");
        auto fill_tag_done = completion("FILL_TAG_COMPLETE", FillTagSend, FillTagWait, "TAG_", fill_tag_sent);
        auto fill_tag_success = gate("FILL_TAG_SUCCESS", fill_tag_done, tag_ok);
        auto fill_tag_fail = gate("FILL_TAG_FAILURE", fill_tag_done, neg("FILL_TAG_NOT_OK", tag_ok));
        auto write_memory_sent = transfer("WRITE_MEMORY_SENT", WriteMemorySend, "MEM_");
        auto write_memory_done = completion("WRITE_MEMORY_COMPLETE", WriteMemorySend, WriteMemoryWait,
                                            "MEM_", write_memory_sent);
        auto write_memory_success = gate("WRITE_MEMORY_SUCCESS", write_memory_done, mem_ok);
        auto write_memory_fail = gate("WRITE_MEMORY_FAILURE", write_memory_done, neg("WRITE_MEMORY_NOT_OK", mem_ok));
        auto write_data_sent = transfer("WRITE_DATA_SENT", WriteDataSend, "DATA_");
        auto write_data_done = completion("WRITE_DATA_COMPLETE", WriteDataSend, WriteDataWait,
                                         "DATA_", write_data_sent);
        auto hit = getInputPin("HIT");
        auto hit_decision = gate("HIT_DECISION", q[Decide], hit);
        auto read_hit = gate("READ_HIT", hit_decision, is_read);
        auto write_hit = gate("WRITE_HIT", hit_decision, is_write);
        auto missed_tag = gate("MISSED_TAG", q[Decide], neg("NOT_HIT", hit));
        auto way_select = getInputPin("WAY_ONE");
        auto retry_second_way = ways_ == 2
            ? gate("RETRY_SECOND_WAY", missed_tag, neg("WAY_ZERO", way_select)) : low;
        auto miss_decision = ways_ == 2
            ? gate("MISS_BOTH_WAYS", missed_tag, way_select) : missed_tag;
        auto read_miss = gate("READ_MISS", miss_decision, is_read);
        auto write_miss = gate("WRITE_MISS", miss_decision, is_write);
        auto enter = [&](CachePhase phase, std::vector<PinPtr> terms) {
            return any("ENTER_" + std::string(cachePhaseNames[phase]), std::move(terms));
        };
        auto drive = [&](CachePhase phase, PinPtr incoming, PinPtr leaving) {
            auto name = std::string(cachePhaseNames[phase]);
            auto stay = gate("STAY_" + name, q[phase], neg("NOT_LEAVING_" + name, leaving));
            w.connect(w.either("NEXT_" + name, incoming, stay), state[phase]->getInputPin("D"));
        };
        drive(TagSend, enter(TagSend, {gate("START_VALID", start, request_ok), retry_second_way}), tag_sent);
        drive(TagWait, gate("TAG_NEEDS_WAIT", tag_sent, neg("TAG_NO_RESPONSE", getInputPin("TAG_RSP_VALID"))), tag_done);
        drive(Decide, tag_success, high);
        drive(DataSend, enter(DataSend, {read_hit, fill_tag_success}), data_sent);
        drive(DataWait, gate("DATA_NEEDS_WAIT", data_sent, neg("DATA_NO_RESPONSE", getInputPin("DATA_RSP_VALID"))), data_done);
        auto last_fill_word = getInputPin("LAST_FILL_WORD");
        auto more_fill_words = gate("MORE_FILL_WORDS", fill_data_success,
                                    neg("NOT_LAST_FILL_WORD", last_fill_word));
        drive(MemorySend, enter(MemorySend, {read_miss, more_fill_words}), memory_sent);
        drive(MemoryWait, gate("MEMORY_NEEDS_WAIT", memory_sent, neg("MEMORY_NO_RESPONSE", getInputPin("MEM_RSP_VALID"))), memory_done);
        drive(FillDataSend, memory_success, fill_data_sent);
        drive(FillDataWait, gate("FILL_DATA_NEEDS_WAIT", fill_data_sent,
            neg("FILL_DATA_NO_RESPONSE", getInputPin("DATA_RSP_VALID"))), fill_data_done);
        drive(FillTagSend, gate("LAST_FILL_WORD_WRITTEN", fill_data_success, last_fill_word), fill_tag_sent);
        drive(FillTagWait, gate("FILL_TAG_NEEDS_WAIT", fill_tag_sent,
            neg("FILL_TAG_NO_RESPONSE", getInputPin("TAG_RSP_VALID"))), fill_tag_done);
        drive(WriteMemorySend, enter(WriteMemorySend, {write_hit, write_miss}), write_memory_sent);
        drive(WriteMemoryWait, gate("WRITE_MEMORY_NEEDS_WAIT", write_memory_sent,
            neg("WRITE_MEMORY_NO_RESPONSE", getInputPin("MEM_RSP_VALID"))), write_memory_done);
        auto write_update = gate("UPDATE_CACHED_WRITE", write_memory_success, hit);
        auto write_no_allocate = gate("COMPLETE_UNCACHED_WRITE", write_memory_success, neg("NOT_WRITE_HIT", hit));
        drive(WriteDataSend, write_update, write_data_sent);
        drive(WriteDataWait, gate("WRITE_DATA_NEEDS_WAIT", write_data_sent,
            neg("WRITE_DATA_NO_RESPONSE", getInputPin("DATA_RSP_VALID"))), write_data_done);
        drive(Done, enter(Done, {invalid, tag_fail, data_done, memory_fail, fill_data_fail,
            fill_tag_fail, write_memory_fail, write_no_allocate, write_data_done}), getInputPin("FINISH"));
        auto idle = neg("IDLE", any("BUSY", q));
        for (size_t phase = 0; phase < q.size(); ++phase)
            w.connect(q[phase], getOutputPin(cachePhaseNames[phase]));
        for (const auto& [name, signal] : std::vector<std::pair<const char*, PinPtr>>{
                 {"IDLE", idle}, {"INVALID_REQUEST", invalid}, {"READ_MISS", read_miss},
                 {"RETRY_SECOND_WAY", retry_second_way}, {"TAG_DONE", tag_done},
                 {"TAG_FAILURE", tag_fail}, {"DATA_DONE", data_done}, {"DATA_OK", data_ok},
                 {"MEMORY_DONE", memory_done}, {"MEMORY_FAILURE", memory_fail},
                 {"FILL_DATA_DONE", fill_data_done}, {"FILL_DATA_SUCCESS", fill_data_success},
                 {"FILL_DATA_FAILURE", fill_data_fail}, {"FILL_TAG_SUCCESS", fill_tag_success},
                 {"FILL_TAG_FAILURE", fill_tag_fail}, {"WRITE_MEMORY_FAILURE", write_memory_fail},
                 {"WRITE_NO_ALLOCATE", write_no_allocate},
                 {"WRITE_DATA_DONE", write_data_done}})
            w.connect(signal, getOutputPin(name));
        w.finish();
    }
private:
    size_t ways_;
};

template<size_t Width>
class CacheReadController : public IOComponent {
public:
    CacheReadController(std::string name, CacheConfig config)
        : IOComponent(std::move(name), [](IOComponent* self) {
              cachePins(self, Width);
              protocolPins(self, 32, "TAG_", true);
              protocolPins(self, Width, "DATA_", true);
              self->addPin("WAY_SELECT", PinType::OUTPUT);
          }), config_(config), type_("CacheReadController<" + std::to_string(Width) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
private:
    CacheConfig config_;
    std::string type_;
};

template<size_t Width>
void CacheReadController<Width>::buildInternals(ComponentBuilder& builder) {
    StructuralWiring w(builder);
    auto low = w.constant("LOW", false);
    auto zero8 = builder.addNewComponent<ConstantValue<Width>>("ZERO_DATA", 0)->template getOutputPin<Width>("OUT");
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

    auto addr_reg = builder.add(circuit::families::registerWord(32), "REQUEST_ADDRESS");
    auto tag_reg = builder.add(circuit::families::registerWord(32), "STORED_TAG");
    auto result_reg = builder.add(circuit::families::registerWord(Width), "READ_RESULT");
    auto status_reg = builder.add(circuit::families::registerWord(3), "RESULT_STATUS");
    auto write_data_reg = builder.add(circuit::families::registerWord(Width), "WRITE_DATA");
    auto write_mask_reg = builder.add(circuit::families::registerWord(Width / 8), "WRITE_MASK");
    auto write_bit = builder.add(circuit::families::ClockedStateBit, "IS_WRITE");
    for (const auto& reg : {addr_reg, tag_reg, result_reg, status_reg, write_data_reg, write_mask_reg})
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), reg->getInputPin(pin));
    w.connect(front->getOutputPin("START"), addr_reg->getInputPin("WE"));
    w.connect(front->getOutputPin<32>("ADDR"), addr_reg->getInputPin<32>("D"));
    for (auto reg : {write_data_reg, write_mask_reg}) w.connect(front->getOutputPin("START"), reg->getInputPin("WE"));
    w.connect(front->getOutputPinDynamic("DATA"), write_data_reg->getInputPinDynamic("D"));
    w.connect(front->getOutputPinDynamic("MASK"), write_mask_reg->getInputPinDynamic("D"));
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), write_bit->getInputPin(pin));
    // A clocked state bit has no enable: retain the operation while busy.
    auto write_next = w.either("NEXT_WRITE", w.both("ACCEPT_WRITE", front->getOutputPin("START"),
        check->getOutputPin("WRITE")), w.both("HOLD_WRITE", write_bit->getOutputPin("Q"),
        w.invert("NOT_NEW_REQUEST", front->getOutputPin("START"))));
    w.connect(write_next, write_bit->getInputPin("D"));
    std::shared_ptr<IOComponent> current_way;
    PinPtr way_select = low;
    if (config_.ways == 2) {
        current_way = builder.add(circuit::families::ClockedStateBit, "CURRENT_WAY");
        w.connect(getInputPin("CLK"), current_way->getInputPin("CLK"));
        w.connect(getInputPin("RST"), current_way->getInputPin("RST"));
        way_select = current_way->getOutputPin("Q");
    }
    w.connect(way_select, getOutputPin("WAY_SELECT"));
    auto child_clock = memory_detail::bridge(builder, w, "CHILD_CLOCK", getInputPin("CLK"));
    auto child_reset = memory_detail::bridge(builder, w, "CHILD_RESET", getInputPin("RST"));
    w.connect(child_clock, {getOutputPin("MEM_CLK"), getOutputPin("TAG_CLK"), getOutputPin("DATA_CLK")});
    w.connect(child_reset, {getOutputPin("MEM_RST"), getOutputPin("TAG_RST"), getOutputPin("DATA_RST")});

    const size_t offset = std::countr_zero(config_.block_bytes);
    auto address_path = builder.addNewComponent<CacheAddressPath<Width>>("ADDRESS_PATH", config_);
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), address_path->getInputPin(pin));
    w.connect(addr_reg->getOutputPin<32>("Q"), address_path->template getInputPin<32>("REQUEST_ADDRESS"));
    auto tag_value = address_path->template getOutputPin<32>("TAG_VALUE");
    auto data_local = address_path->template getOutputPin<32>("DATA_LOCAL_ADDRESS");
    auto tag_local = address_path->template getOutputPin<32>("TAG_LOCAL_ADDRESS");
    w.connect(tag_local, getOutputPin<32>("TAG_REQ_ADDR"));
    w.connect(tag_value, getOutputPin<32>("TAG_REQ_WRITE_DATA"));
    w.connect(full_tag_mask, getOutputPin<4>("TAG_REQ_WRITE_MASK"));

    std::vector<std::shared_ptr<CacheLineState>> lines;
    for (size_t way = 0; way < config_.ways; ++way) {
        auto line = builder.addNewComponent<CacheLineState>("LINE_STATE_" + std::to_string(way), config_.sets, offset);
        w.connect(getInputPin("CLK"), line->getInputPin("CLK"));
        w.connect(getInputPin("RST"), line->getInputPin("RST"));
        w.connect(addr_reg->getOutputPin<32>("Q"), line->getInputPin<32>("ADDRESS"));
        lines.push_back(line);
    }
    auto selected_valid = config_.ways == 1 ? PinPtr(lines[0]->getOutputPin("VALID"))
        : memory_detail::payloadMux(builder, w, "SELECTED_VALID",
            lines[0]->getOutputPin("VALID"), lines[1]->getOutputPin("VALID"), way_select);
    std::shared_ptr<CacheReplacementState> replacement;
    PinPtr victim_one = low;
    if (config_.ways == 2) {
        replacement = builder.addNewComponent<CacheReplacementState>("REPLACEMENT", config_.sets, offset);
        w.connect(getInputPin("CLK"), replacement->getInputPin("CLK"));
        w.connect(getInputPin("RST"), replacement->getInputPin("RST"));
        w.connect(addr_reg->getOutputPin<32>("Q"), replacement->getInputPin<32>("ADDRESS"));
        victim_one = w.both("VICTIM_WAY_ONE", lines[0]->getOutputPin("VALID"),
            w.either("WAY_ONE_INVALID_OR_LRU",
                w.invert("WAY_ONE_INVALID", lines[1]->getOutputPin("VALID")),
                w.invert("WAY_ONE_LRU", replacement->getOutputPin("MRU"))));
    }
    auto match = builder.addNewComponent<CacheTagMatch32>("TAG_MATCH");
    w.connect(tag_value, match->getInputPin<32>("EXPECTED"));
    w.connect(tag_reg->getOutputPin<32>("Q"), match->getInputPin<32>("STORED"));
    w.connect(selected_valid, match->getInputPin("VALID"));

    auto phases = builder.addNewComponent<CachePhaseSequencer>("PHASES", config_.ways);
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), phases->getInputPin(pin));
    w.connect(front->getOutputPin("START"), phases->getInputPin("START"));
    w.connect(front->getOutputPin("FINISH"), phases->getInputPin("FINISH"));
    w.connect(check->getOutputPin("ACCEPT"), phases->getInputPin("REQUEST_OK"));
    auto is_write = write_bit->getOutputPin("Q");
    w.connect(is_write, phases->getInputPin("IS_WRITE"));
    w.connect(match->getOutputPin("HIT"), phases->getInputPin("HIT"));
    w.connect(way_select, phases->getInputPin("WAY_ONE"));
    w.connect(address_path->getOutputPin("LAST_FILL_WORD"), phases->getInputPin("LAST_FILL_WORD"));
    for (auto prefix : {"TAG_", "DATA_", "MEM_"}) {
        for (auto pin : {"REQ_READY", "RSP_VALID", "RSP_STATUS"})
            w.connect(getInputPinDynamic(std::string(prefix) + pin),
                      phases->getInputPinDynamic(std::string(prefix) + pin));
    }
    auto signal = [&](const char* name) -> PinPtr { return phases->getOutputPin(name); };
    std::vector<PinPtr> q;
    for (auto name : cachePhaseNames) q.push_back(signal(name));
    auto gate = [&](const std::string& name, PinPtr a, PinPtr b) { return w.both(name, a, b); };
    auto neg = [&](const std::string& name, PinPtr value) { return w.invert(name, value); };
    auto any = [&](const std::string& name, std::vector<PinPtr> terms) {
        return w.reduce(name, std::move(terms), false);
    };
    auto status_ok = [&](const std::string& name, PinPtr status) -> PinPtr {
        auto split = builder.addNewComponent<BitSplitter<3>>(name + "_BITS");
        w.connect(status, split->getInputPin<3>("IN"));
        return neg(name + "_OK", any(name + "_ERROR", {split->getOutputPin("OUT_0"),
            split->getOutputPin("OUT_1"), split->getOutputPin("OUT_2")}));
    };
    auto start = front->getOutputPin("START");
    auto is_read = neg("IS_READ", is_write);
    auto invalid = signal("INVALID_REQUEST");
    auto read_miss = signal("READ_MISS");
    auto retry_second_way = signal("RETRY_SECOND_WAY");
    auto tag_done = signal("TAG_DONE");
    auto tag_fail = signal("TAG_FAILURE");
    auto data_done = signal("DATA_DONE");
    auto data_ok = signal("DATA_OK");
    auto memory_done = signal("MEMORY_DONE");
    auto memory_fail = signal("MEMORY_FAILURE");
    auto fill_data_done = signal("FILL_DATA_DONE");
    auto fill_data_success = signal("FILL_DATA_SUCCESS");
    auto fill_data_fail = signal("FILL_DATA_FAILURE");
    auto fill_tag_success = signal("FILL_TAG_SUCCESS");
    auto fill_tag_fail = signal("FILL_TAG_FAILURE");
    auto write_memory_fail = signal("WRITE_MEMORY_FAILURE");
    auto write_no_allocate = signal("WRITE_NO_ALLOCATE");
    auto write_data_done = signal("WRITE_DATA_DONE");
    auto idle = signal("IDLE");
    auto available = gate("AVAILABLE", idle, neg("NO_INVALIDATION", getInputPin("INV_VALID")));
    w.connect(available, front->getInputPin("BACK_READY"));
    w.connect(idle, getOutputPin("INV_READY"));
    w.connect(q[Done], front->getInputPin("DONE"));
    auto invalidate = gate("ACCEPT_INVALIDATION", getInputPin("INV_VALID"), idle);
    auto data_failed = gate("DATA_READ_FAILED", data_done, neg("DATA_READ_NOT_OK", data_ok));
    auto write_data_failed = gate("CACHED_WRITE_FAILED", write_data_done,
                                  neg("CACHED_WRITE_NOT_OK", data_ok));
    for (size_t way = 0; way < config_.ways; ++way) {
        w.connect(invalidate, lines[way]->getInputPin("INVALIDATE"));
        auto chosen_victim = way == 0 ? neg("VICTIM_ZERO", victim_one) : victim_one;
        auto chosen_current = way == 0 ? neg("CURRENT_ZERO", way_select) : way_select;
        auto clear = any("INVALIDATE_LINE_" + std::to_string(way),
            {gate("EVICT_LINE_" + std::to_string(way), read_miss, chosen_victim),
             gate("FAILED_READ_" + std::to_string(way), data_failed, chosen_current),
             gate("FAILED_CACHED_WRITE_" + std::to_string(way), write_data_failed, chosen_current)});
        w.connect(clear, lines[way]->getInputPin("CLEAR"));
        w.connect(gate("MARK_LINE_" + std::to_string(way), fill_tag_success, chosen_current),
                  lines[way]->getInputPin("MARK"));
    }
    if (current_way) {
        auto set_one = any("SET_CURRENT_ONE", {retry_second_way,
            gate("SELECT_VICTIM_ONE", read_miss, victim_one)});
        auto clear_way = any("CLEAR_CURRENT_WAY", {start,
            gate("SELECT_VICTIM_ZERO", read_miss, neg("NOT_VICTIM_ONE", victim_one))});
        auto hold_way = gate("HOLD_CURRENT_WAY", way_select, neg("NOT_CLEAR_WAY", clear_way));
        w.connect(w.either("NEXT_CURRENT_WAY", set_one, hold_way), current_way->getInputPin("D"));
        w.connect(invalidate, replacement->getInputPin("INVALIDATE"));
        w.connect(any("TOUCH_REPLACEMENT", {fill_tag_success,
            gate("SUCCESSFUL_DATA_READ", data_done, data_ok),
            gate("SUCCESSFUL_CACHED_WRITE", write_data_done, data_ok)}), replacement->getInputPin("TOUCH"));
        w.connect(way_select, replacement->getInputPin("TOUCH_WAY"));
    }
    w.connect(tag_done, tag_reg->getInputPin("WE"));
    w.connect(getInputPin<32>("TAG_RSP_READ_DATA"), tag_reg->getInputPin<32>("D"));
    w.connect(any("CAPTURE_READ_RESULT", {data_done, memory_done}), result_reg->getInputPin("WE"));
    auto read_data = memory_detail::payloadMux(builder, w, "READ_DATA_SOURCE",
        getInputPinDynamic("DATA_RSP_READ_DATA"), getInputPinDynamic("MEM_RSP_READ_DATA"), memory_done);
    w.connect(read_data, result_reg->getInputPinDynamic("D"));
    auto status_enable = any("CAPTURE_STATUS", {invalid, tag_fail, data_done, memory_fail,
        fill_data_fail, fill_tag_fail, write_memory_fail, write_no_allocate, write_data_done});
    w.connect(status_enable, status_reg->getInputPin("WE"));
    auto status = memory_detail::payloadMux(builder, w, "STATUS_FROM_DATA", zero3,
        getInputPin<3>("DATA_RSP_STATUS"), any("DATA_STATUS_SELECTED", {data_done, fill_data_fail}));
    status = memory_detail::payloadMux(builder, w, "STATUS_FROM_TAG", status,
        getInputPin<3>("TAG_RSP_STATUS"), any("TAG_STATUS_SELECTED", {tag_fail, fill_tag_fail}));
    status = memory_detail::payloadMux(builder, w, "STATUS_FROM_MEMORY", status,
        getInputPin<3>("MEM_RSP_STATUS"), any("MEMORY_STATUS_SELECTED",
            {memory_fail, write_memory_fail, write_no_allocate}));
    status = memory_detail::payloadMux(builder, w, "STATUS_FROM_REQUEST", status,
        check->getOutputPin<3>("STATUS"), invalid);
    w.connect(status, status_reg->getInputPin<3>("D"));
    w.connect(status_reg->getOutputPin<3>("Q"), front->getInputPin<3>("STATUS"));
    auto completed_ok = status_ok("COMPLETED_STATUS", status_reg->getOutputPin<3>("Q"));
    auto return_read_data = gate("RETURN_READ_DATA", completed_ok, is_read);
    w.connect(memory_detail::payloadMux(builder, w, "ZERO_FAILED_RESULT", zero8,
        result_reg->getOutputPinDynamic("Q"), return_read_data), front->getInputPinDynamic("RESULT"));

    w.connect(read_miss, address_path->getInputPin("READ_MISS"));
    w.connect(fill_data_done, address_path->getInputPin("FILL_DATA_DONE"));
    w.connect(fill_data_success, address_path->getInputPin("FILL_DATA_SUCCESS"));
    w.connect(memory_fail, address_path->getInputPin("MEMORY_FAILURE"));
    auto lower_address = address_path->template getOutputPin<32>("FILL_BACKING_ADDRESS");
    auto fill_data_address = address_path->template getOutputPin<32>("FILL_DATA_ADDRESS");
    auto lower_write = q[WriteMemorySend];
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_ADDRESS", lower_address,
        addr_reg->getOutputPin<32>("Q"), lower_write), getOutputPin<32>("MEM_REQ_ADDR"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_OP", read_op, write_op, lower_write),
              getOutputPin<3>("MEM_REQ_OP"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_WRITE_DATA", zero8,
        write_data_reg->getOutputPinDynamic("Q"), lower_write), getOutputPinDynamic("MEM_REQ_WRITE_DATA"));
    w.connect(memory_detail::payloadMux(builder, w, "LOWER_WRITE_MASK", zero_mask,
        write_mask_reg->getOutputPinDynamic("Q"), lower_write), getOutputPinDynamic("MEM_REQ_WRITE_MASK"));

    auto tag_read = any("TAG_READ", {q[TagSend]});
    auto tag_write = any("TAG_WRITE", {q[FillTagSend]});
    auto data_read = any("DATA_READ", {q[DataSend]});
    auto data_write = any("DATA_WRITE", {q[FillDataSend]});
    w.connect(memory_detail::payloadMux(builder, w, "DATA_ADDRESS", data_local,
        fill_data_address, data_write), getOutputPin<32>("DATA_REQ_ADDR"));
    w.connect(any("TAG_REQUEST", {tag_read, tag_write}), getOutputPin("TAG_REQ_VALID"));
    w.connect(any("TAG_RESPONSE_READY", {q[TagSend], q[TagWait], q[FillTagSend], q[FillTagWait]}),
              getOutputPin("TAG_RSP_READY"));
    w.connect(memory_detail::payloadMux(builder, w, "TAG_OP", read_op, write_op, tag_write),
              getOutputPin<3>("TAG_REQ_OP"));
    w.connect(any("DATA_REQUEST", {data_read, data_write, q[WriteDataSend]}), getOutputPin("DATA_REQ_VALID"));
    w.connect(any("DATA_RESPONSE_READY", {q[DataSend], q[DataWait], q[FillDataSend],
        q[FillDataWait], q[WriteDataSend], q[WriteDataWait]}),
              getOutputPin("DATA_RSP_READY"));
    auto any_data_write = any("ANY_DATA_WRITE", {data_write, q[WriteDataSend]});
    w.connect(memory_detail::payloadMux(builder, w, "DATA_OP", read_op, write_op, any_data_write),
              getOutputPin<3>("DATA_REQ_OP"));
    w.connect(memory_detail::payloadMux(builder, w, "DATA_WRITE_PAYLOAD",
        result_reg->getOutputPinDynamic("Q"), write_data_reg->getOutputPinDynamic("Q"), q[WriteDataSend]),
        getOutputPinDynamic("DATA_REQ_WRITE_DATA"));
    w.connect(memory_detail::payloadMux(builder, w, "DATA_WRITE_MASK", full_data_mask,
        write_mask_reg->getOutputPinDynamic("Q"), q[WriteDataSend]),
        getOutputPinDynamic("DATA_REQ_WRITE_MASK"));
    w.connect(any("MEMORY_REQUEST", {q[MemorySend], q[WriteMemorySend]}), getOutputPin("MEM_REQ_VALID"));
    w.connect(any("MEMORY_RESPONSE_READY", {q[MemorySend], q[MemoryWait],
        q[WriteMemorySend], q[WriteMemoryWait]}), getOutputPin("MEM_RSP_READY"));
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

template<size_t Width>
void Cache<Width>::buildInternals(ComponentBuilder& builder) {
    StructuralWiring w(builder);
    auto controller = builder.addNewComponent<CacheReadController<Width>>("CONTROLLER", config_);
    EndpointConfig tag_config, data_config;
    tag_config.depth_words = config_.sets;
    data_config.depth_words = config_.sets * (config_.block_bytes / (Width / 8));
    tag_config.words_per_row = data_config.words_per_row = 1;
    tag_config.block_words = data_config.block_words = 1;
    std::vector<std::shared_ptr<IOComponent>> tags, data;
    for (size_t way = 0; way < config_.ways; ++way) {
        tags.push_back(memory_device::child(builder, *this,
            memoryContractId(config_.tag_technology,
                config_.scalable_storage ? "scalable-endpoint" : "endpoint", 32),
            "TAG_STORE_" + std::to_string(way), endpointParameters(tag_config)));
        data.push_back(memory_device::child(builder, *this,
            memoryContractId(config_.data_technology,
                config_.scalable_storage ? "scalable-endpoint" : "endpoint", Width),
            "DATA_STORE_" + std::to_string(way), endpointParameters(data_config)));
    }
    for (const auto& [name, pin] : getAllInputPins())
        if (!name.starts_with("MEM_")) w.connect(pin, controller->getInputPinDynamic(name));
    for (const auto& [name, pin] : getAllOutputPins())
        w.connect(controller->getOutputPinDynamic(name), pin);
    auto route = [&](const std::string& prefix, const std::vector<std::shared_ptr<IOComponent>>& banks) {
        auto selected = controller->getOutputPin("WAY_SELECT");
        auto zero = w.invert(prefix + "SELECT_ZERO", selected);
        for (const auto& [name, pin] : controller->getAllOutputPins()) {
            if (!name.starts_with(prefix)) continue;
            const auto local = name.substr(prefix.size());
            for (size_t way = 0; way < banks.size(); ++way) {
                PinPtr drive = pin;
                if (banks.size() == 2 && (local == "REQ_VALID" || local == "RSP_READY"))
                    drive = w.both(prefix + local + "_WAY_" + std::to_string(way), pin,
                                   way == 0 ? zero : selected);
                w.connect(drive, banks[way]->getInputPinDynamic(local));
            }
        }
        for (const auto& [name, pin] : controller->getAllInputPins()) {
            if (!name.starts_with(prefix)) continue;
            const auto local = name.substr(prefix.size());
            auto source = banks[0]->getOutputPinDynamic(local);
            if (banks.size() == 2)
                source = memory_detail::payloadMux(builder, w, prefix + "RESPONSE_" + local,
                    source, banks[1]->getOutputPinDynamic(local), selected);
            w.connect(source, pin);
        }
    };
    route("TAG_", tags);
    route("DATA_", data);
    for (const auto& [name, pin] : controller->getAllInputPins()) {
        if (name.starts_with("MEM_")) w.connect(getInputPinDynamic(name), pin);
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
