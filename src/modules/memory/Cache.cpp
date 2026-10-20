#include "modules/memory/Cache.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/storage/Register.hpp"
#include "modules/storage/ClockedStateBit.hpp"
#include "MemoryHandshakeWiring.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "analysis/GateListModel.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <mutex>
#include <bit>
#include <functional>
#include <array>
#include <limits>
#include <tuple>
#include <vector>

namespace circuit::memory {
using circuit::memory_detail::StructuralWiring;
using PinPtr = StructuralWiring::PinPtr;

void CacheConfig::validate(size_t width) const {
    if (width != 8 && width != 16 && width != 32)
        throw std::invalid_argument("Cache requires an 8, 16 or 32-bit endpoint word");
    const size_t bytes = block_bytes ? block_bytes : width / 8;
    const size_t words_per_line = bytes / (width / 8);
    if (!std::has_single_bit(sets) || sets > (line_store ? (scalable_storage ? 4096 : 128) : (scalable_storage ? 64 : 4)) ||
        (ways != 1 && ways != 2 && ways != 4 && ways != 8) ||
        !std::has_single_bit(bytes) || bytes < width / 8 ||
        words_per_line > 4 || (!scalable_storage && sets * words_per_line > 4096 / width))
        throw std::invalid_argument("Cache supports power-of-two sets (flip-flop line state: up to 4 expanded or 64 scalable; "
                                    "line-state store: 128 or 4096), 1/2/4/8 ways and 1/2/4 words per line");
    if (replacement == CacheReplacement::LRU && ways > 2)
        throw std::invalid_argument("Exact LRU replacement is built for one or two ways; choose plru, fifo or random");
    if (!std::has_single_bit(backing_depth_words) || backing_depth_words < sets * words_per_line ||
        backing_depth_words > std::numeric_limits<uint32_t>::max() / (width / 8))
        throw std::invalid_argument("Cache backing depth must be a sufficient power of two");
    for (auto technology : {tag_technology, data_technology})
        if (technology != Technology::FF && technology != Technology::SRAM)
            throw std::invalid_argument("Cache tag/data banks require writable FF or SRAM endpoints");
    if (write_buffer != 0 && write_buffer != 1 && write_buffer != 2 && write_buffer != 4)
        throw std::invalid_argument("Cache write buffer must have 0, 1, 2 or 4 entries");
    if (write_buffer && !read_write)
        throw std::invalid_argument("A write buffer needs a read-write cache");
    if ((write_back || write_allocate) && !read_write)
        throw std::invalid_argument("Write-back and write-allocate need a read-write cache");
    if (write_back && write_buffer)
        throw std::invalid_argument("A write-back cache has no write buffer");
}

CacheConfig cacheConfig(const ParameterMap& p) {
    memory_device::keys(p, {"sets", "ways", "block_bytes", "backing_depth_words", "mode", "storage_form",
                            "tag_technology", "data_technology", "fill", "write_buffer", "write_policy", "write_miss",
                            "line_state", "replacement", "controller"});
    CacheConfig c;
    c.sets = memory_device::parameter(p, "sets", c.sets);
    c.ways = memory_device::parameter(p, "ways", c.ways);
    c.block_bytes = memory_device::parameter(p, "block_bytes", c.block_bytes);
    c.backing_depth_words = memory_device::parameter(p, "backing_depth_words", c.backing_depth_words);
    c.write_buffer = memory_device::parameter(p, "write_buffer", c.write_buffer);
    if (auto it = p.find("write_policy"); it != p.end()) {
        if (it->second == "through") c.write_back = false;
        else if (it->second == "back") c.write_back = true;
        else throw std::invalid_argument("Cache write_policy must be through or back");
    }
    if (auto it = p.find("line_state"); it != p.end()) {
        if (it->second == "registers") c.line_store = false;
        else if (it->second == "store") c.line_store = true;
        else throw std::invalid_argument("Cache line_state must be registers or store");
    }
    if (auto it = p.find("controller"); it != p.end()) {
        if (it->second == "structural") c.compact_controller = false;
        else if (it->second == "behavioral") c.compact_controller = true;
        else throw std::invalid_argument("Cache controller must be structural or behavioral");
    }
    if (auto it = p.find("replacement"); it != p.end()) {
        if (it->second == "lru") c.replacement = CacheReplacement::LRU;
        else if (it->second == "plru") c.replacement = CacheReplacement::PLRU;
        else if (it->second == "fifo") c.replacement = CacheReplacement::FIFO;
        else if (it->second == "random") c.replacement = CacheReplacement::Random;
        else throw std::invalid_argument("Cache replacement must be lru, plru, fifo or random");
    } else if (c.ways > 2) {
        c.replacement = CacheReplacement::PLRU;  // exact LRU stops at two ways
    }
    if (auto it = p.find("write_miss"); it != p.end()) {
        if (it->second == "no-allocate") c.write_allocate = false;
        else if (it->second == "allocate") c.write_allocate = true;
        else throw std::invalid_argument("Cache write_miss must be no-allocate or allocate");
    }
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
    if (auto it = p.find("fill"); it != p.end()) {
        if (it->second == "words") c.burst_fill = false;
        else if (it->second == "burst") c.burst_fill = true;
        else throw std::invalid_argument("Cache fill must be words or burst");
    }
    return c;
}

ParameterMap cacheParameters(const CacheConfig& c) {
    return {{"sets", std::to_string(c.sets)}, {"ways", std::to_string(c.ways)},
            {"block_bytes", std::to_string(c.block_bytes)},
            {"backing_depth_words", std::to_string(c.backing_depth_words)},
            {"mode", c.read_write ? "read_write" : "read_only"},
            {"storage_form", c.scalable_storage ? "scalable" : "expanded"},
            {"tag_technology", technologyName(c.tag_technology)},
            {"data_technology", technologyName(c.data_technology)},
            {"fill", c.burst_fill ? "burst" : "words"},
            {"write_buffer", std::to_string(c.write_buffer)},
            {"write_policy", c.write_back ? "back" : "through"},
            {"write_miss", c.write_allocate ? "allocate" : "no-allocate"},
            {"line_state", c.line_store ? "store" : "registers"},
            {"replacement", cacheReplacementName(c.replacement)},
            {"controller", c.compact_controller ? "behavioral" : "structural"}};
}

const char* cacheReplacementName(CacheReplacement r) {
    switch (r) {
    case CacheReplacement::PLRU: return "plru";
    case CacheReplacement::FIFO: return "fifo";
    case CacheReplacement::Random: return "random";
    default: return "lru";
    }
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
    bool settled(const Values& in, Values& out, Deciders& deciders) const override {
        out["FIELDS_KNOWN"] = {memory_device::known(in.at("OP")) && memory_device::known(in.at("ADDR"))
            ? memory_device::H : memory_device::L};
        out["MASK_KNOWN"] = {memory_device::known(in.at("MASK")) ? memory_device::H : memory_device::L};
        deciders["FIELDS_KNOWN"] = {"OP", "ADDR"};
        deciders["MASK_KNOWN"] = {"MASK"};
        return true;
    }
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
              self->addPin("BURST", PinType::OUTPUT);
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
        // Read (0), write (1) and burst read (4); program, erase and the
        // other codes are reserved here.
        auto reserved = w.either("RESERVED_OPCODE", o1, w.both("NOT_A_CACHE_OPCODE", o2, o0));
        auto not_o1 = w.invert("NOT_OP_BIT_1", o1);
        auto read_opcode = w.reduce("READ_OPCODE", {w.invert("NOT_WRITE", o0), not_o1,
            w.invert("NOT_BURST_OPCODE", o2)}, true);
        auto write_opcode = w.both("WRITE_OPCODE", o0, w.invert("WRITE_NOT_RESERVED", reserved));
        auto burst_opcode = w.reduce("BURST_OPCODE", {w.invert("BURST_NOT_WRITE", o0), not_o1, o2}, true);
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
        auto burst = w.both("VALID_BURST", valid, burst_opcode);
        w.connect(burst, getOutputPin("BURST"));
        w.connect(w.reduce("ACCEPT", {read, write, burst}, false), getOutputPin("ACCEPT"));
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

// The valid bit of every line of one way, and with `dirty` its dirty bit (a
// write-back cache): CLEAR (eviction) and MARK (a completed fill) act on the
// set selected by ADDRESS; MARK leaves the line clean, SET_DIRTY (a write
// into it) and CLEAN (it was written back) change only its dirty bit.
class CacheLineState : public IOComponent {
public:
    CacheLineState(std::string name, size_t sets, size_t address_offset, bool dirty = false)
        : IOComponent(std::move(name), [dirty](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "INVALIDATE", "CLEAR", "MARK"})
                  self->addPin(pin, PinType::INPUT);
              self->addPin<32>("ADDRESS", PinType::INPUT);
              self->addPin("VALID", PinType::OUTPUT);
              if (dirty) {
                  for (auto pin : {"SET_DIRTY", "CLEAN"}) self->addPin(pin, PinType::INPUT);
                  self->addPin("DIRTY", PinType::OUTPUT);
              }
          }), sets_(sets), offset_(address_offset), dirty_(dirty),
          type_(std::string(dirty ? "CacheDirtyLineState<" : "CacheLineState<") + std::to_string(sets) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto addr = builder.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(getInputPin<32>("ADDRESS"), addr->getInputPin<32>("IN"));
        auto clear_all = w.either("RESET_OR_INVALIDATE", getInputPin("RST"), getInputPin("INVALIDATE"));
        auto low = w.constant("LOW", false);
        std::vector<PinPtr> selected_valid, selected_dirty;
        for (size_t set = 0; set < sets_; ++set) {
            const auto n = std::to_string(set);
            std::vector<PinPtr> bits;
            for (size_t i = 0; i < std::countr_zero(sets_); ++i) {
                auto bit = addr->getOutputPin("OUT_" + std::to_string(offset_ + i));
                bits.push_back((set & (size_t{1} << i)) ? bit :
                    w.invert("NOT_SET_" + n + "_" + std::to_string(i), bit));
            }
            auto selected = bits.empty() ? w.invert("SELECT_ZERO", low)
                                         : w.reduce("SELECT_" + n, bits, true);
            auto cell = builder.add(circuit::families::ClockedStateBit, "VALID_" + n);
            w.connect(getInputPin("CLK"), cell->getInputPin("CLK"));
            w.connect(clear_all, cell->getInputPin("RST"));
            auto clear = w.both("CLEAR_" + n, getInputPin("CLEAR"), selected);
            auto mark = w.both("MARK_" + n, getInputPin("MARK"), selected);
            auto keep = w.both("KEEP_" + n, cell->getOutputPin("Q"), w.invert("NOT_CLEAR_" + n, clear));
            w.connect(w.either("NEXT_" + n, keep, mark), cell->getInputPin("D"));
            selected_valid.push_back(w.both("SELECTED_VALID_" + n, selected, cell->getOutputPin("Q")));
            if (dirty_) {
                auto bit = builder.add(circuit::families::ClockedStateBit, "DIRTY_" + n);
                w.connect(getInputPin("CLK"), bit->getInputPin("CLK"));
                w.connect(clear_all, bit->getInputPin("RST"));
                auto clean = w.reduce("CLEANED_" + n, {w.either("CLEAN_OR_REFILL_" + n,
                    w.either("CLEAN_OR_CLEAR_" + n, getInputPin("CLEAN"), getInputPin("CLEAR")), getInputPin("MARK")), selected}, true);
                auto set_dirty = w.both("SET_DIRTY_" + n, getInputPin("SET_DIRTY"), selected);
                w.connect(w.either("NEXT_DIRTY_" + n, set_dirty,
                    w.both("KEEP_DIRTY_" + n, bit->getOutputPin("Q"), w.invert("NOT_CLEANED_" + n, clean))), bit->getInputPin("D"));
                selected_dirty.push_back(w.both("SELECTED_DIRTY_" + n, selected, bit->getOutputPin("Q")));
            }
        }
        w.connect(w.reduce("VALID", selected_valid, false), getOutputPin("VALID"));
        if (dirty_) w.connect(w.reduce("DIRTY", selected_dirty, false), getOutputPin("DIRTY"));
        w.finish();
    }
private:
    size_t sets_, offset_;
    bool dirty_;
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

// The replacement bits of every set (pseudo-LRU tree bits or a FIFO
// pointer), for the set selected by ADDRESS: BIT_i shows them, UPDATE loads
// NEXT_i into them at the edge. Reset and invalidation clear them.
class CachePolicyBits : public IOComponent {
public:
    CachePolicyBits(std::string name, size_t sets, size_t address_offset, size_t bits)
        : IOComponent(std::move(name), [bits](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "INVALIDATE", "UPDATE"}) self->addPin(pin, PinType::INPUT);
              self->addPin<32>("ADDRESS", PinType::INPUT);
              for (size_t i = 0; i < bits; ++i) {
                  self->addPin("NEXT_" + std::to_string(i), PinType::INPUT);
                  self->addPin("BIT_" + std::to_string(i), PinType::OUTPUT);
              }
          }), sets_(sets), offset_(address_offset), bits_(bits),
          type_("CachePolicyBits<" + std::to_string(sets) + "," + std::to_string(bits) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto addr = builder.addNewComponent<BitSplitter<32>>("ADDRESS_BITS");
        w.connect(getInputPin<32>("ADDRESS"), addr->getInputPin<32>("IN"));
        auto reset = w.either("RESET", getInputPin("RST"), getInputPin("INVALIDATE"));
        auto low = w.constant("LOW", false);
        std::vector<std::vector<PinPtr>> shown(bits_);
        for (size_t set = 0; set < sets_; ++set) {
            const auto n = std::to_string(set);
            std::vector<PinPtr> index;
            for (size_t i = 0; i < std::countr_zero(sets_); ++i) {
                auto bit = addr->getOutputPin("OUT_" + std::to_string(offset_ + i));
                index.push_back((set & (size_t{1} << i)) ? bit : w.invert("NOT_SET_" + n + "_" + std::to_string(i), bit));
            }
            auto selected = index.empty() ? w.invert("SELECT_ZERO", low) : w.reduce("SELECT_" + n, index, true);
            auto update = w.both("UPDATE_" + n, getInputPin("UPDATE"), selected);
            auto keep = w.invert("KEEP_" + n, update);
            for (size_t i = 0; i < bits_; ++i) {
                const auto b = n + "_" + std::to_string(i);
                auto cell = builder.add(circuit::families::ClockedStateBit, "BIT_" + b);
                w.connect(getInputPin("CLK"), cell->getInputPin("CLK"));
                w.connect(reset, cell->getInputPin("RST"));
                w.connect(w.either("NEXT_" + b, w.both("HOLD_" + b, cell->getOutputPin("Q"), keep),
                                   w.both("LOAD_" + b, update, getInputPin("NEXT_" + std::to_string(i)))),
                          cell->getInputPin("D"));
                shown[i].push_back(w.both("SELECTED_" + b, selected, cell->getOutputPin("Q")));
            }
        }
        for (size_t i = 0; i < bits_; ++i)
            w.connect(w.reduce("SHOWN_" + std::to_string(i), shown[i], false), getOutputPin("BIT_" + std::to_string(i)));
        w.finish();
    }
private:
    size_t sets_, offset_, bits_;
    std::string type_;
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

// The controller's state bits: LOOKUP (the cycle after a request is taken:
// every way's tag and data arrive and are compared), MEMORY_SEND and
// MEMORY_WAIT (a backing-memory request: a fill, or the write of a
// write-through), STREAM (the further words of a burst read that hit) and
// DONE (a response is shown until the front end finishes it). The
// controller computes each bit's next value.
class CacheLookupPhases : public IOComponent {
public:
    static constexpr std::array<const char*, 5> names = {"LOOKUP", "MEMORY_SEND", "MEMORY_WAIT", "STREAM", "DONE"};
    explicit CacheLookupPhases(std::string name)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST"}) self->addPin(pin, PinType::INPUT);
              for (auto name : names) {
                  self->addPin(std::string("NEXT_") + name, PinType::INPUT);
                  self->addPin(name, PinType::OUTPUT);
              }
          }) {}
    const char* getTypeName() const override { return "CacheLookupPhases"; }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        for (auto name : names) {
            auto bit = builder.add(circuit::families::ClockedStateBit, name);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bit->getInputPin(pin));
            w.connect(getInputPin(std::string("NEXT_") + name), bit->getInputPin("D"));
            w.connect(bit->getOutputPin("Q"), getOutputPin(name));
        }
        w.finish();
    }
};

// The write buffer: `entries` slots filled from slot 0, each a valid bit and
// the write's address, data and mask. Slot 0 is the oldest; it is sent to the
// backing memory while GRANT is high (no fill uses the lower port), and when
// its response arrives every slot moves down one. A new write enters the first
// free slot after that move, so one write can enter and one leave at an edge.
// The backing memory's status for a buffered write is not returned.
template<size_t Width>
class CacheWriteQueue : public IOComponent {
public:
    CacheWriteQueue(std::string name, size_t entries)
        : IOComponent(std::move(name), [](IOComponent* self) {
              for (auto pin : {"CLK", "RST", "ENQUEUE", "GRANT", "MEM_REQ_READY", "MEM_RSP_VALID"})
                  self->addPin(pin, PinType::INPUT);
              self->addPin<32>("ADDR", PinType::INPUT);
              self->addPinDynamic("DATA", PinType::INPUT, Width);
              self->addPinDynamic("MASK", PinType::INPUT, Width / 8);
              for (auto pin : {"FULL", "IDLE", "REQ_VALID", "RSP_READY"})
                  self->addPin(pin, PinType::OUTPUT);
              self->addPin<32>("REQ_ADDR", PinType::OUTPUT);
              self->addPinDynamic("REQ_DATA", PinType::OUTPUT, Width);
              self->addPinDynamic("REQ_MASK", PinType::OUTPUT, Width / 8);
          }), entries_(entries), type_("CacheWriteQueue<" + std::to_string(Width) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override {
        StructuralWiring w(builder);
        auto low = w.constant("LOW", false);
        auto state_bit = [&](const std::string& name) {
            auto bit = builder.add(circuit::families::ClockedStateBit, name);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bit->getInputPin(pin));
            return bit;
        };
        auto reg = [&](const std::string& name, size_t width) {
            auto r = builder.add(circuit::families::registerWord(width), name);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), r->getInputPin(pin));
            return r;
        };
        struct Slot { std::shared_ptr<IOComponent> valid, addr, data, mask; };
        std::vector<Slot> slots;
        for (size_t i = 0; i < entries_; ++i) {
            const auto n = std::to_string(i);
            slots.push_back({state_bit("VALID_" + n), reg("ADDRESS_" + n, 32), reg("DATA_" + n, Width),
                             reg("MASK_" + n, Width / 8)});
        }
        auto waiting = state_bit("WAITING");
        auto oldest = slots[0].valid->getOutputPin("Q");
        auto request = w.reduce("SEND", {oldest, w.invert("NOT_WAITING", waiting->getOutputPin("Q")),
                                         getInputPin("GRANT")}, true);
        auto sent = w.both("SENT", request, getInputPin("MEM_REQ_READY"));
        auto response_ready = w.either("RESPONSE_READY", sent, waiting->getOutputPin("Q"));
        auto done = w.both("DONE", response_ready, getInputPin("MEM_RSP_VALID"));
        w.connect(w.both("STILL_WAITING", response_ready, w.invert("NOT_DONE", done)), waiting->getInputPin("D"));
        // After the move (done): what each slot would hold, then where the new write goes.
        std::vector<PinPtr> moved_valid;
        for (size_t i = 0; i < entries_; ++i) {
            const auto n = std::to_string(i);
            auto above = i + 1 < entries_ ? PinPtr(slots[i + 1].valid->getOutputPin("Q")) : low;
            moved_valid.push_back(memory_detail::payloadMux(builder, w, "MOVED_VALID_" + n,
                slots[i].valid->getOutputPin("Q"), above, done));
        }
        for (size_t i = 0; i < entries_; ++i) {
            const auto n = std::to_string(i);
            auto first_free = w.both("FIRST_FREE_" + n, w.invert("FREE_" + n, moved_valid[i]),
                                     i == 0 ? w.constant("BOTTOM", true) : moved_valid[i - 1]);
            auto put = w.both("PUT_" + n, getInputPin("ENQUEUE"), first_free);
            w.connect(w.either("NEXT_VALID_" + n, moved_valid[i], put), slots[i].valid->getInputPin("D"));
            auto load = w.either("LOAD_" + n, done, put);
            const auto& upper = i + 1 < entries_ ? slots[i + 1] : slots[i];
            for (auto [field, source, input] : {std::tuple{slots[i].addr, upper.addr, PinPtr(this->template getInputPin<32>("ADDR"))},
                                                std::tuple{slots[i].data, upper.data, PinPtr(getInputPinDynamic("DATA"))},
                                                std::tuple{slots[i].mask, upper.mask, PinPtr(getInputPinDynamic("MASK"))}}) {
                auto moved = memory_detail::payloadMux(builder, w, field->getName() + "_MOVED",
                    field->getOutputPinDynamic("Q"), source->getOutputPinDynamic("Q"), done);
                w.connect(memory_detail::payloadMux(builder, w, field->getName() + "_NEXT", moved, input, put),
                          field->getInputPinDynamic("D"));
                w.connect(load, field->getInputPin("WE"));
            }
        }
        w.connect(slots.back().valid->getOutputPin("Q"), getOutputPin("FULL"));
        w.connect(w.both("IDLE", w.invert("EMPTY", oldest), w.invert("NOT_SENDING", waiting->getOutputPin("Q"))),
                  getOutputPin("IDLE"));
        w.connect(request, getOutputPin("REQ_VALID"));
        w.connect(response_ready, getOutputPin("RSP_READY"));
        w.connect(slots[0].addr->template getOutputPin<32>("Q"), this->template getOutputPin<32>("REQ_ADDR"));
        w.connect(slots[0].data->getOutputPinDynamic("Q"), getOutputPinDynamic("REQ_DATA"));
        w.connect(slots[0].mask->getOutputPinDynamic("Q"), getOutputPinDynamic("REQ_MASK"));
        w.finish();
    }
private:
    size_t entries_;
    std::string type_;
};

// Bank pins of one way's tag or data bank, as the controller drives them:
// READ (read at the next edge), WRITE (write at the next edge), WORD_ADDR,
// WRITE_DATA, WRITE_MASK; READ_DATA comes back.
void bankPins(IOComponent* self, const std::string& prefix, size_t width) {
    self->addPin(prefix + "READ", PinType::OUTPUT);
    self->addPin(prefix + "WRITE", PinType::OUTPUT);
    self->addPinDynamic(prefix + "WORD_ADDR", PinType::OUTPUT, 32);
    self->addPinDynamic(prefix + "WRITE_DATA", PinType::OUTPUT, width);
    self->addPinDynamic(prefix + "WRITE_MASK", PinType::OUTPUT, width / 8);
    self->addPinDynamic(prefix + "READ_DATA", PinType::INPUT, width);
}

// The cache controller: a request is looked up in every way at once (one
// cycle); a read miss fills the victim line from the backing memory (word
// by word, or in one burst); a write goes through to the backing memory. A
// burst read (op 4) is answered with the words of the line from the
// requested one on, wrapping: from the data bank one word per cycle after a
// hit, or forwarded as the fill brings them after a miss. The banks are
// native devices driven directly (Cache<Width>::buildInternals).
void controllerPins(IOComponent* self, size_t width, const CacheConfig& config) {
    cachePins(self, width);
    for (size_t way = 0; way < config.ways; ++way) {
        bankPins(self, "TAG" + std::to_string(way) + "_", 32);
        bankPins(self, "DATA" + std::to_string(way) + "_", width);
    }
    if (config.line_store) bankPins(self, "META_", 32);
}

template<size_t Width>
class CacheBankController : public IOComponent {
public:
    CacheBankController(std::string name, CacheConfig config)
        : IOComponent(std::move(name), [config](IOComponent* self) { controllerPins(self, Width, config); }),
          config_(config), type_("CacheBankController<" + std::to_string(Width) + ">") {}
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
    auto high = w.constant("HIGH", true);
    auto zero_data = builder.addNewComponent<ConstantValue<Width>>("ZERO_DATA", 0)->template getOutputPin<Width>("OUT");
    auto zero3 = builder.addNewComponent<ConstantValue<3>>("ZERO_STATUS", 0)->getOutputPin<3>("OUT");
    auto read_op = builder.addNewComponent<ConstantValue<3>>("READ_OP", config_.burst_fill ? 4 : 0)->getOutputPin<3>("OUT");
    auto write_op = builder.addNewComponent<ConstantValue<3>>("WRITE_OP", 1)->getOutputPin<3>("OUT");
    auto full_tag_mask = builder.addNewComponent<ConstantValue<4>>("FULL_TAG_MASK", 15)->getOutputPin<4>("OUT");
    auto full_data_mask = builder.addNewComponent<ConstantValue<Width / 8>>("FULL_DATA_MASK", (1u << (Width / 8)) - 1)
                              ->template getOutputPin<Width / 8>("OUT");
    auto zero_mask = builder.addNewComponent<ConstantValue<Width / 8>>("ZERO_MASK", 0)
                         ->template getOutputPin<Width / 8>("OUT");
    const size_t words_per_line = config_.block_bytes / (Width / 8);
    const bool streams = words_per_line > 1;

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
    auto valid_start = w.both("VALID_START", start, check->getOutputPin("ACCEPT"));
    auto invalid = w.both("INVALID_REQUEST", start, w.invert("REQUEST_REJECTED", check->getOutputPin("ACCEPT")));

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
    auto state_bit = [&](const std::string& name) {
        auto bit = builder.add(circuit::families::ClockedStateBit, name);
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bit->getInputPin(pin));
        return bit;
    };
    // The request's kind, held while it is served (a clocked state bit has
    // no enable).
    auto held = [&](const std::string& name, PinPtr decoded) {
        auto bit = state_bit(name);
        w.connect(w.either("NEXT_" + name, w.both("ACCEPT_" + name, start, decoded),
            w.both("HOLD_" + name, bit->getOutputPin("Q"), w.invert("NOT_NEW_" + name, start))), bit->getInputPin("D"));
        return bit->getOutputPin("Q");
    };
    auto is_write = held("IS_WRITE", check->getOutputPin("WRITE"));
    auto is_burst = held("IS_BURST", check->getOutputPin("BURST"));
    auto is_read = w.invert("IS_READ", is_write);
    auto child_clock = memory_detail::bridge(builder, w, "CHILD_CLOCK", getInputPin("CLK"));
    auto child_reset = memory_detail::bridge(builder, w, "CHILD_RESET", getInputPin("RST"));
    w.connect(child_clock, getOutputPin("MEM_CLK"));
    w.connect(child_reset, getOutputPin("MEM_RST"));

    const size_t offset = std::countr_zero(config_.block_bytes);
    const size_t set_bits = std::countr_zero(config_.sets);
    const size_t word_shift = std::countr_zero(Width / 8);
    const size_t line_bits = offset - word_shift;
    auto address_path = builder.addNewComponent<CacheAddressPath<Width>>("ADDRESS_PATH", config_);
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), address_path->getInputPin(pin));
    w.connect(addr_reg->getOutputPin<32>("Q"), address_path->template getInputPin<32>("REQUEST_ADDRESS"));
    auto tag_value = address_path->template getOutputPin<32>("TAG_VALUE");
    auto bits_of = [&](const std::string& name, PinPtr source) {
        auto split = builder.addNewComponent<BitSplitter<32>>(name);
        w.connect(source, split->getInputPin<32>("IN"));
        std::vector<PinPtr> out;
        for (size_t bit = 0; bit < 32; ++bit) out.push_back(split->getOutputPin("OUT_" + std::to_string(bit)));
        return out;
    };
    auto join = [&](const std::string& name, const std::vector<PinPtr>& bits) -> PinPtr {
        auto j = builder.addNewComponent<BitJoiner<32>>(name);
        for (size_t bit = 0; bit < 32; ++bit) w.connect(bit < bits.size() ? bits[bit] : low, j->getInputPin("IN_" + std::to_string(bit)));
        return j->getOutputPin<32>("OUT");
    };
    // The sum of two line-word offsets, wrapping within the line.
    auto add_offsets = [&](const std::string& name, const std::vector<PinPtr>& a, const std::vector<PinPtr>& b) {
        std::vector<PinPtr> sum;
        PinPtr carry = low;
        for (size_t i = 0; i < line_bits; ++i) {
            const auto n = name + "_" + std::to_string(i);
            auto half = w.binary<XORGate>(n + "_HALF", a[i], b[i]);
            sum.push_back(w.binary<XORGate>(n + "_SUM", half, carry));
            carry = w.either(n + "_CARRY", w.both(n + "_BOTH", a[i], b[i]), w.both(n + "_PASS", half, carry));
        }
        return sum;
    };
    auto request_bits = bits_of("REQUEST_ADDRESS_BITS", addr_reg->getOutputPin<32>("Q"));
    std::vector<PinPtr> wanted_word(request_bits.begin() + word_shift, request_bits.begin() + offset);
    std::vector<PinPtr> set_index(request_bits.begin() + offset, request_bits.begin() + offset + set_bits);
    // A data-bank word address: the set and a word of the line.
    auto data_address = [&](const std::string& name, const std::vector<PinPtr>& word) {
        std::vector<PinPtr> bits = word;
        bits.insert(bits.end(), set_index.begin(), set_index.end());
        return join(name, bits);
    };
    // The banks' addresses for a lookup: the front end's address in the
    // accepting cycle (a clocked bank registers it there), the held address
    // after it (a flip-flop bank reads it then).
    auto read_address = memory_detail::payloadMux(builder, w, "READ_ADDRESS",
        addr_reg->getOutputPin<32>("Q"), front->getOutputPin<32>("ADDR"), start);
    auto read_bits = bits_of("READ_ADDRESS_BITS", read_address);
    auto tag_read_index = join("TAG_READ_INDEX", std::vector<PinPtr>(read_bits.begin() + offset, read_bits.begin() + offset + set_bits));
    auto lookup_index = join("DATA_READ_INDEX", std::vector<PinPtr>(read_bits.begin() + word_shift, read_bits.begin() + offset + set_bits));
    auto tag_index = join("TAG_INDEX", set_index);
    auto data_index = data_address("DATA_INDEX", wanted_word);

    auto phases = builder.addNewComponent<CacheLookupPhases>("PHASES");
    for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), phases->getInputPin(pin));
    const auto L = phases->getOutputPin("LOOKUP"), MS = phases->getOutputPin("MEMORY_SEND"),
               MW = phases->getOutputPin("MEMORY_WAIT"), ST = phases->getOutputPin("STREAM"), D = phases->getOutputPin("DONE");
    // Write-back and write-allocate state. EVICT_READ/SEND/WAIT write a dirty
    // victim back word by word (EVICT_WORD counts the words; EVICT_FLUSH: the
    // eviction belongs to the flush walk). FLUSH_READ and FLUSH_CHECK walk
    // every line (FLUSH_INDEX: way, then set) for invalidation; FLUSHED says
    // the walk is complete. REFILLED_WRITE: a write-allocate fill just ended,
    // the write is looked up again.
    const bool back = config_.write_back, allocate = config_.write_allocate;
    const size_t way_bits = std::countr_zero(config_.ways);
    std::shared_ptr<IOComponent> evict_read_bit, evict_send_bit, evict_wait_bit, evict_flush_bit,
        flush_read_bit, flush_check_bit, flushed_bit, refilled_bit;
    std::vector<std::shared_ptr<IOComponent>> evict_word_bits, flush_index_bits;
    PinPtr ER = low, ES = low, EW = low, EF = low, FR = low, FC = low, FLUSHED = low, REFILLED = low;
    std::vector<PinPtr> evict_word, flush_index;
    if (allocate) { refilled_bit = state_bit("REFILLED_WRITE"); REFILLED = refilled_bit->getOutputPin("Q"); }
    if (back) {
        evict_read_bit = state_bit("EVICT_READ"); ER = evict_read_bit->getOutputPin("Q");
        evict_send_bit = state_bit("EVICT_SEND"); ES = evict_send_bit->getOutputPin("Q");
        evict_wait_bit = state_bit("EVICT_WAIT"); EW = evict_wait_bit->getOutputPin("Q");
        evict_flush_bit = state_bit("EVICT_FLUSH"); EF = evict_flush_bit->getOutputPin("Q");
        flush_read_bit = state_bit("FLUSH_READ"); FR = flush_read_bit->getOutputPin("Q");
        flush_check_bit = state_bit("FLUSH_CHECK"); FC = flush_check_bit->getOutputPin("Q");
        flushed_bit = state_bit("FLUSHED"); FLUSHED = flushed_bit->getOutputPin("Q");
        for (size_t i = 0; i < line_bits; ++i) {
            evict_word_bits.push_back(state_bit("EVICT_WORD_" + std::to_string(i)));
            evict_word.push_back(evict_word_bits.back()->getOutputPin("Q"));
        }
        for (size_t i = 0; i < way_bits + set_bits; ++i) {
            flush_index_bits.push_back(state_bit("FLUSH_INDEX_" + std::to_string(i)));
            flush_index.push_back(flush_index_bits.back()->getOutputPin("Q"));
        }
    }
    // Line-state store: CLEAR_INDEX walks every set writing an empty word,
    // after reset (INITIALIZED low) and for an invalidation (INVALIDATION_WALK,
    // then INVALIDATION_CLEARED until it is taken).
    std::shared_ptr<IOComponent> initialized_bit, inv_walk_bit, inv_cleared_bit;
    std::vector<std::shared_ptr<IOComponent>> clear_index_bits;
    std::vector<PinPtr> clear_index;
    PinPtr clearing = low, INV_WALK = low, INV_CLEARED = low;
    if (config_.line_store) {
        initialized_bit = state_bit("INITIALIZED");
        inv_walk_bit = state_bit("INVALIDATION_WALK"); INV_WALK = inv_walk_bit->getOutputPin("Q");
        inv_cleared_bit = state_bit("INVALIDATION_CLEARED"); INV_CLEARED = inv_cleared_bit->getOutputPin("Q");
        clearing = w.invert("CLEARING", initialized_bit->getOutputPin("Q"));
        for (size_t i = 0; i < set_bits; ++i) {
            clear_index_bits.push_back(state_bit("CLEAR_INDEX_" + std::to_string(i)));
            clear_index.push_back(clear_index_bits.back()->getOutputPin("Q"));
        }
    }
    const std::vector<PinPtr> flush_set(flush_index.begin() + std::min(way_bits, flush_index.size()), flush_index.end());
    auto flushing = back ? w.reduce("FLUSHING", {FR, FC, EF}, false) : low;
    // The line state reads the request's set, or the walked set while flushing.
    PinPtr line_address = addr_reg->getOutputPin<32>("Q");
    if (back) {
        std::vector<PinPtr> walked(32, low);
        for (size_t i = 0; i < set_bits; ++i) walked[offset + i] = flush_set[i];
        line_address = memory_detail::payloadMux(builder, w, "LINE_ADDRESS", line_address, join("FLUSH_ADDRESS", walked), flushing);
    }
    // Each way's valid and dirty bit and the set's replacement bits, for the
    // set being looked up or walked: from flip-flops (CacheLineState, and
    // CacheReplacementState or CachePolicyBits), or from the line-state store,
    // whose word for the set is read with the tags and kept in LINE_WORD while
    // a request lasts (bits 0..7 valid, 8..15 dirty, 16 on replacement bits).
    // Replacement bits: two-way lru, the most recently used way; plru, the
    // tree (node n's children are 2n+1 and 2n+2; the root decides the way
    // index's top bit, 1 pointing to the upper half); fifo, the pointer.
    // random keeps one LFSR for all sets instead.
    const bool store = config_.line_store;
    const auto policy = config_.replacement;
    const bool two_way_lru = config_.ways == 2 && policy == CacheReplacement::LRU;
    const size_t policy_bits = config_.ways == 1 ? 0 : two_way_lru ? 1
        : policy == CacheReplacement::PLRU ? config_.ways - 1 : policy == CacheReplacement::FIFO ? way_bits : 0;
    std::vector<std::shared_ptr<CacheLineState>> lines;
    std::shared_ptr<CacheReplacementState> replacement;
    std::shared_ptr<CachePolicyBits> policy_store;
    std::vector<PinPtr> way_valid, way_dirty, policy_state;
    std::shared_ptr<IOComponent> line_word_reg;
    std::vector<PinPtr> old_line_bits;
    if (store) {
        line_word_reg = builder.add(circuit::families::registerWord(32), "LINE_WORD");
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), line_word_reg->getInputPin(pin));
        // In the lookup or walk check the word arrives from the store; after it, the kept copy.
        auto old_word = memory_detail::payloadMux(builder, w, "LINE_WORD_NOW", line_word_reg->getOutputPin<32>("Q"),
            getInputPinDynamic("META_READ_DATA"), w.either("LINE_WORD_ARRIVES", L, FC));
        old_line_bits = bits_of("LINE_WORD_BITS", old_word);
        for (size_t way = 0; way < config_.ways; ++way) {
            way_valid.push_back(old_line_bits[way]);
            way_dirty.push_back(back ? old_line_bits[8 + way] : low);
        }
        for (size_t i = 0; i < policy_bits; ++i) policy_state.push_back(old_line_bits[16 + i]);
    } else {
        for (size_t way = 0; way < config_.ways; ++way) {
            auto line = builder.addNewComponent<CacheLineState>("LINE_STATE_" + std::to_string(way), config_.sets, offset, back);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), line->getInputPin(pin));
            w.connect(line_address, line->getInputPin<32>("ADDRESS"));
            lines.push_back(line);
            way_valid.push_back(line->getOutputPin("VALID"));
            way_dirty.push_back(back ? PinPtr(line->getOutputPin("DIRTY")) : low);
        }
        if (two_way_lru) {
            replacement = builder.addNewComponent<CacheReplacementState>("REPLACEMENT", config_.sets, offset);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), replacement->getInputPin(pin));
            w.connect(addr_reg->getOutputPin<32>("Q"), replacement->getInputPin<32>("ADDRESS"));
            policy_state.push_back(replacement->getOutputPin("MRU"));
        } else if (policy_bits) {
            policy_store = builder.addNewComponent<CachePolicyBits>("REPLACEMENT", config_.sets, offset, policy_bits);
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), policy_store->getInputPin(pin));
            w.connect(addr_reg->getOutputPin<32>("Q"), policy_store->getInputPin<32>("ADDRESS"));
            for (size_t i = 0; i < policy_bits; ++i) policy_state.push_back(policy_store->getOutputPin("BIT_" + std::to_string(i)));
        }
    }
    // random: a 16-bit LFSR (the XNOR of bits 15, 14, 12 and 3 shifted in at
    // bit 0), stepped by every completed fill; its low bits name the victim.
    std::vector<std::shared_ptr<IOComponent>> random_bits;
    if (policy == CacheReplacement::Random && config_.ways > 1)
        for (size_t i = 0; i < 16; ++i) random_bits.push_back(state_bit("RANDOM_" + std::to_string(i)));
    std::vector<PinPtr> matches;
    for (size_t way = 0; way < config_.ways; ++way) {
        auto match = builder.addNewComponent<CacheTagMatch32>("TAG_MATCH_" + std::to_string(way));
        w.connect(tag_value, match->getInputPin<32>("EXPECTED"));
        w.connect(getInputPinDynamic("TAG" + std::to_string(way) + "_READ_DATA"), match->getInputPin<32>("STORED"));
        w.connect(way_valid[way], match->getInputPin("VALID"));
        matches.push_back(match->getOutputPin("HIT"));
    }
    auto hit = config_.ways == 1 ? matches[0] : config_.ways == 2 ? w.either("HIT", matches[0], matches[1])
                                                                 : w.reduce("HIT", matches, false);
    // Way indexes are vectors of way_bits bits, least significant first. The
    // hit way's index (at most one tag matches):
    std::vector<PinPtr> hit_way;
    for (size_t b = 0; b < way_bits; ++b) {
        std::vector<PinPtr> terms;
        for (size_t way = 0; way < config_.ways; ++way) if ((way >> b) & 1) terms.push_back(matches[way]);
        hit_way.push_back(w.reduce("HIT_WAY_BIT_" + std::to_string(b), terms, false));
    }
    // values[i] for the way index `index`: a tree of multiplexers.
    std::function<PinPtr(const std::string&, const std::vector<PinPtr>&, const std::vector<PinPtr>&)> select =
        [&](const std::string& name, const std::vector<PinPtr>& values, const std::vector<PinPtr>& index) -> PinPtr {
            if (values.size() == 1) return values[0];
            if (values.size() == 2) return memory_detail::payloadMux(builder, w, name, values[0], values[1], index[0]);
            const auto half = values.size() / 2;
            const std::vector<PinPtr> lower(index.begin(), index.end() - 1);
            return memory_detail::payloadMux(builder, w, name,
                select(name + "_LOW", std::vector<PinPtr>(values.begin(), values.begin() + half), lower),
                select(name + "_HIGH", std::vector<PinPtr>(values.begin() + half, values.end()), lower), index.back());
        };
    // One line per way, high for the way an index names.
    auto decode = [&](const std::string& zero_name, const std::string& prefix, const std::vector<PinPtr>& index) -> std::vector<PinPtr> {
        if (index.empty()) return {high};
        if (index.size() == 1) return {w.invert(zero_name, index[0]), index[0]};
        std::vector<PinPtr> inverted, out;
        for (size_t b = 0; b < index.size(); ++b) inverted.push_back(w.invert(prefix + "_NOT_" + std::to_string(b), index[b]));
        for (size_t way = 0; way < config_.ways; ++way) {
            std::vector<PinPtr> terms;
            for (size_t b = 0; b < index.size(); ++b) terms.push_back(((way >> b) & 1) ? index[b] : inverted[b]);
            out.push_back(w.reduce(prefix + "_" + std::to_string(way), terms, true));
        }
        return out;
    };
    // A name per index bit: the two-way name as it was, else with _BIT and the bit.
    auto bit_name = [&](std::string base, size_t b) {
        if (way_bits == 1) return base;
        if (auto at = base.find("_ONE"); at != std::string::npos) base.replace(at, 4, "_BIT");
        return base + "_" + std::to_string(b);
    };
    // The victim of a miss: the first invalid way, else the policy's way.
    std::vector<PinPtr> victim_way;
    if (two_way_lru) {
        victim_way.push_back(w.both("VICTIM_WAY_ONE", way_valid[0],
            w.either("WAY_ONE_INVALID_OR_LRU", w.invert("WAY_ONE_INVALID", way_valid[1]), w.invert("WAY_ONE_LRU", policy_state[0]))));
    } else if (config_.ways > 1) {
        std::vector<PinPtr> chosen;
        if (policy == CacheReplacement::PLRU) {
            std::vector<PinPtr> path;  // the decisions from the root: the index's top bit first
            for (size_t level = 0; level < way_bits; ++level) {
                const size_t first = (size_t{1} << level) - 1;
                path.push_back(select("PLRU_LEVEL_" + std::to_string(level),
                    std::vector<PinPtr>(policy_state.begin() + first, policy_state.begin() + 2 * first + 1),
                    std::vector<PinPtr>(path.rbegin(), path.rend())));
            }
            chosen.assign(path.rbegin(), path.rend());
        } else if (policy == CacheReplacement::FIFO) {
            chosen = policy_state;
        } else {
            for (size_t b = 0; b < way_bits; ++b) chosen.push_back(random_bits[b]->getOutputPin("Q"));
        }
        PinPtr valid_so_far = way_valid[0];
        std::vector<PinPtr> first_invalid{w.invert("WAY_0_INVALID", way_valid[0])};
        for (size_t way = 1; way < config_.ways; ++way) {
            const auto n = std::to_string(way);
            first_invalid.push_back(w.both("FIRST_INVALID_" + n, w.invert("WAY_" + n + "_INVALID", way_valid[way]), valid_so_far));
            valid_so_far = w.both("VALID_UP_TO_" + n, valid_so_far, way_valid[way]);
        }
        auto some_invalid = w.invert("SOME_WAY_INVALID", valid_so_far);
        for (size_t b = 0; b < way_bits; ++b) {
            std::vector<PinPtr> terms;
            for (size_t way = 0; way < config_.ways; ++way) if ((way >> b) & 1) terms.push_back(first_invalid[way]);
            victim_way.push_back(memory_detail::payloadMux(builder, w, "VICTIM_WAY_BIT_" + std::to_string(b), chosen[b],
                w.reduce("FIRST_INVALID_BIT_" + std::to_string(b), terms, false), some_invalid));
        }
    }
    auto status_ok = [&](const std::string& name, PinPtr status) -> PinPtr {
        auto split = builder.addNewComponent<BitSplitter<3>>(name + "_BITS");
        w.connect(status, split->getInputPin<3>("IN"));
        return w.invert(name + "_OK", w.reduce(name + "_ERROR", {split->getOutputPin("OUT_0"),
            split->getOutputPin("OUT_1"), split->getOutputPin("OUT_2")}, false));
    };

    // The write buffer (write_buffer > 0): a write is queued in its lookup
    // cycle; a read miss waits (MISS_WAIT) until the queue has sent everything.
    const bool buffered = config_.write_buffer > 0;
    std::shared_ptr<IOComponent> queue, miss_wait_bit;
    PinPtr drained = high, queue_full = low, miss_wait = low;
    if (buffered) {
        queue = builder.addNewComponent<CacheWriteQueue<Width>>("WRITE_QUEUE", config_.write_buffer);
        for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), queue->getInputPin(pin));
        w.connect(w.invert("LOWER_PORT_FREE", w.either("FILL_USES_LOWER_PORT", MS, MW)), queue->getInputPin("GRANT"));
        w.connect(getInputPin("MEM_REQ_READY"), queue->getInputPin("MEM_REQ_READY"));
        w.connect(getInputPin("MEM_RSP_VALID"), queue->getInputPin("MEM_RSP_VALID"));
        w.connect(addr_reg->getOutputPin<32>("Q"), queue->getInputPin<32>("ADDR"));
        w.connect(write_data_reg->getOutputPinDynamic("Q"), queue->getInputPinDynamic("DATA"));
        w.connect(write_mask_reg->getOutputPinDynamic("Q"), queue->getInputPinDynamic("MASK"));
        drained = queue->getOutputPin("IDLE");
        queue_full = queue->getOutputPin("FULL");
        miss_wait_bit = state_bit("MISS_WAIT");
        miss_wait = miss_wait_bit->getOutputPin("Q");
    }
    auto lookup_read = w.both("LOOKUP_READ", L, is_read);
    auto read_hit = w.both("READ_HIT", lookup_read, hit);
    auto read_only_miss = w.both("READ_MISS", lookup_read, w.invert("NOT_HIT", hit));
    auto write_lookup = w.both("WRITE_LOOKUP", L, is_write);
    auto burst_hit = w.both("BURST_HIT", read_hit, is_burst);
    // A miss that fills its line: a read miss, or with write-allocate a write
    // miss (the write is looked up again once the line is in).
    auto read_miss = allocate ? w.either("FILL_MISS", read_only_miss,
        w.both("WRITE_MISS_ALLOCATES", write_lookup, w.invert("WRITE_NOT_HIT", hit))) : read_only_miss;
    // A write that goes ahead now: every write, or with write-allocate a hit.
    auto write_now = allocate ? w.both("WRITE_GOES_AHEAD", write_lookup, hit) : write_lookup;
    auto queued_write = buffered ? write_now : low;
    auto queued_update = buffered ? w.both("QUEUED_WRITE_HIT", write_now, hit) : low;
    // Write-back: a write hit updates its line, marks it dirty and is answered now.
    auto back_write = back ? w.both("WRITE_BACK_HIT", write_now, hit) : low;
    auto lookup_update = w.either("LOOKUP_UPDATE", queued_update, back_write);
    // A write sent to the backing memory now: write-through, or a write-back
    // write miss without allocation (write-around).
    auto write_to_lower = buffered ? low : back ? w.both("WRITE_AROUND", write_now, w.invert("AROUND_MISS", hit)) : write_now;

    // Write-back: a dirty victim is written back, word by word, before the
    // fill; the flush walk writes back every dirty line before invalidation.
    PinPtr needs_evict = low, evict_last_done = low, evict_rsp_ready = low, flush_found = low,
           evict_address = low, flush_step = low, flush_last = low;
    const std::vector<PinPtr> flush_way(flush_index.begin(), flush_index.begin() + std::min(way_bits, flush_index.size()));
    std::shared_ptr<IOComponent> victim_index_reg;
    if (back) {
        auto way_signal = [&](const std::string& name, const std::string& pin, const std::vector<PinPtr>& index) -> PinPtr {
            return select(name, pin == "VALID" ? way_valid : way_dirty, index);
        };
        std::vector<PinPtr> tags;
        for (size_t way = 0; way < config_.ways; ++way) tags.push_back(getInputPinDynamic("TAG" + std::to_string(way) + "_READ_DATA"));
        auto way_tag = [&](const std::string& name, const std::vector<PinPtr>& index) -> PinPtr { return select(name, tags, index); };
        needs_evict = w.reduce("NEEDS_EVICTION", {read_miss, way_signal("VICTIM_VALID", "VALID", victim_way),
                                                  way_signal("VICTIM_DIRTY", "DIRTY", victim_way)}, true);
        auto flush_dirty = w.both("WALKED_LINE_DIRTY", way_signal("WALKED_VALID", "VALID", flush_way),
                                  way_signal("WALKED_DIRTY", "DIRTY", flush_way));
        flush_found = w.both("FLUSH_FOUND", FC, flush_dirty);
        auto flush_skip = w.both("FLUSH_SKIP", FC, w.invert("WALKED_LINE_CLEAN", flush_dirty));
        auto start_evict = w.either("START_EVICTION", needs_evict, flush_found);
        // The victim's tag and set: from the lookup, or from the walk.
        auto victim_tag_reg = builder.add(circuit::families::registerWord(32), "VICTIM_TAG");
        victim_index_reg = builder.add(circuit::families::registerWord(32), "VICTIM_SET");
        for (const auto& reg : {victim_tag_reg, victim_index_reg}) {
            for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), reg->getInputPin(pin));
            w.connect(start_evict, reg->getInputPin("WE"));
        }
        w.connect(memory_detail::payloadMux(builder, w, "VICTIM_TAG_SOURCE", way_tag("VICTIM_TAG_NOW", victim_way),
                                            way_tag("WALKED_TAG", flush_way), flush_found), victim_tag_reg->getInputPin<32>("D"));
        w.connect(memory_detail::payloadMux(builder, w, "VICTIM_SET_SOURCE", join("REQUEST_SET", set_index),
                                            join("WALKED_SET", flush_set), flush_found), victim_index_reg->getInputPin<32>("D"));
        auto evict_sent = w.both("EVICT_SENT", ES, getInputPin("MEM_REQ_READY"));
        evict_rsp_ready = w.either("EVICT_RESPONSE_READY", ES, EW);
        auto evict_done = w.both("EVICT_WORD_DONE", w.either("EVICT_ACTIVE", evict_sent, EW), getInputPin("MEM_RSP_VALID"));
        auto evict_last = line_bits ? w.reduce("EVICT_LAST_WORD", evict_word, true) : high;
        evict_last_done = w.both("EVICTION_DONE", evict_done, evict_last);
        w.connect(w.either("NEXT_EVICT_READ", start_evict, w.both("EVICT_NEXT_WORD", evict_done, w.invert("EVICT_MORE", evict_last))),
                  evict_read_bit->getInputPin("D"));
        w.connect(w.either("NEXT_EVICT_SEND", ER, w.both("EVICT_STILL_SENDING", ES, w.invert("EVICT_NOT_TAKEN", getInputPin("MEM_REQ_READY")))),
                  evict_send_bit->getInputPin("D"));
        w.connect(w.both("NEXT_EVICT_WAIT", w.either("EVICT_IN_FLIGHT", evict_sent, EW), w.invert("EVICT_NO_RESPONSE", evict_done)),
                  evict_wait_bit->getInputPin("D"));
        w.connect(w.either("NEXT_EVICT_FLUSH", flush_found, w.both("EVICT_FLUSH_HOLD", EF, w.invert("EVICT_FLUSH_ENDS", evict_last_done))),
                  evict_flush_bit->getInputPin("D"));
        // EVICT_WORD: 0 when an eviction starts, one more per word written back.
        PinPtr carry = high;
        for (size_t i = 0; i < line_bits; ++i) {
            const auto n = std::to_string(i);
            auto sum = w.binary<XORGate>("EVICT_WORD_SUM_" + n, evict_word[i], carry);
            carry = w.both("EVICT_WORD_CARRY_" + n, evict_word[i], carry);
            auto stepped = memory_detail::payloadMux(builder, w, "EVICT_WORD_STEP_" + n, evict_word[i], sum, evict_done);
            w.connect(w.both("NEXT_EVICT_WORD_" + n, stepped, w.invert("EVICT_WORD_RESTART_" + n, start_evict)),
                      evict_word_bits[i]->getInputPin("D"));
        }
        // The victim word's backing address: its tag, its set, the word.
        auto victim_set_bits = bits_of("VICTIM_SET_BITS", victim_index_reg->getOutputPin<32>("Q"));
        auto victim_tag_bits = bits_of("VICTIM_TAG_BITS", victim_tag_reg->getOutputPin<32>("Q"));
        std::vector<PinPtr> address(32, low);
        for (size_t i = 0; i < line_bits; ++i) address[word_shift + i] = evict_word[i];
        for (size_t i = 0; i < set_bits; ++i) address[offset + i] = victim_set_bits[i];
        for (size_t i = 0; offset + set_bits + i < 32; ++i) address[offset + set_bits + i] = victim_tag_bits[i];
        evict_address = join("EVICT_ADDRESS", address);
        // The walk: the next line after a clean one or a finished write-back.
        flush_step = w.either("FLUSH_STEP", flush_skip, w.both("FLUSH_EVICTED", evict_last_done, EF));
        flush_last = flush_index.empty() ? high : w.reduce("FLUSH_LAST_LINE", flush_index, true);
        auto busy = w.reduce("CONTROLLER_BUSY", {L, MS, MW, ST, ER, ES, EW, FR, FC, REFILLED, D, clearing}, false);
        auto flush_begin = w.reduce("FLUSH_BEGIN", {getInputPin("INV_VALID"), w.invert("CONTROLLER_FREE", busy),
                                                    w.invert("NOT_FLUSHED", FLUSHED)}, true);
        w.connect(w.either("NEXT_FLUSH_READ", flush_begin, w.both("FLUSH_CONTINUES", flush_step, w.invert("FLUSH_MORE", flush_last))),
                  flush_read_bit->getInputPin("D"));
        w.connect(FR, flush_check_bit->getInputPin("D"));
        carry = high;
        for (size_t i = 0; i < flush_index.size(); ++i) {
            const auto n = std::to_string(i);
            auto sum = w.binary<XORGate>("FLUSH_INDEX_SUM_" + n, flush_index[i], carry);
            carry = w.both("FLUSH_INDEX_CARRY_" + n, flush_index[i], carry);
            auto stepped = memory_detail::payloadMux(builder, w, "FLUSH_INDEX_STEP_" + n, flush_index[i], sum, flush_step);
            w.connect(w.both("NEXT_FLUSH_INDEX_" + n, stepped, w.invert("FLUSH_INDEX_RESTART_" + n, flush_begin)),
                      flush_index_bits[i]->getInputPin("D"));
        }
    }
    auto miss_fills = back ? w.both("MISS_FILLS_NOW", read_miss, w.invert("MISS_EVICTS_FIRST", needs_evict)) : read_miss;
    auto fill_after_eviction = back ? w.both("FILL_AFTER_EVICTION", evict_last_done, w.invert("NOT_A_FLUSH", EF)) : low;
    // A fill starts once the queue is drained, or once the victim is written back.
    auto fill_start = buffered ? w.both("FILL_START", w.either("MISS_OR_WAITING", read_miss, miss_wait), drained)
                               : back ? w.either("FILL_START", miss_fills, fill_after_eviction) : read_miss;
    if (buffered) {
        w.connect(queued_write, queue->getInputPin("ENQUEUE"));
        w.connect(w.both("NEXT_MISS_WAIT", w.either("MISS_STILL_WAITING", read_miss, miss_wait),
                         w.invert("NOT_DRAINED", drained)), miss_wait_bit->getInputPin("D"));
    }
    // Backing memory: accepted while MEMORY_SEND; answered then or later, a
    // burst fill one word per response.
    // What MEMORY_SEND/WAIT is doing: a write (write-through or write-around),
    // or a fill (also of a write-allocate miss).
    auto sending_write_bit = state_bit("SENDING_WRITE");
    auto sending_write = sending_write_bit->getOutputPin("Q");
    auto sent = w.both("MEMORY_SENT", MS, getInputPin("MEM_REQ_READY"));
    // A response leaves the result register as the front end finishes it
    // (no response held: SPACE, registered); a burst's next word is taken
    // only then. A burst miss forwards each fill word, so its backing
    // responses wait for that room too.
    auto room = w.either("RESULT_ROOM", w.invert("NOTHING_SHOWN", D), front->getOutputPin("SPACE"));
    auto rsp_ready = w.both("MEMORY_RESPONSE_READY", w.either("MEMORY_PHASE", MS, MW),
        w.either("FORWARD_ROOM", w.invert("NOT_FORWARDING", is_burst), room));
    auto rsp = w.both("MEMORY_RESPONSE_TAKEN", getInputPin("MEM_RSP_VALID"), rsp_ready);
    auto mem_done = w.both("MEMORY_DONE", w.either("MEMORY_ACTIVE", sent, MW), rsp);
    auto mem_ok = status_ok("MEMORY_STATUS", getInputPin<3>("MEM_RSP_STATUS"));
    w.connect(w.either("NEXT_SENDING_WRITE", write_to_lower, w.reduce("STILL_SENDING_WRITE", {sending_write,
        w.either("WRITE_IN_MEMORY_PHASE", MS, MW), w.invert("WRITE_NOT_DONE", mem_done)}, true)), sending_write_bit->getInputPin("D"));
    auto fill_done = w.both("FILL_DONE", mem_done, w.invert("NOT_SENDING_WRITE", sending_write));
    auto fill_success = w.both("FILL_SUCCESS", fill_done, mem_ok);
    auto fill_failure = w.both("FILL_FAILURE", fill_done, w.invert("FILL_NOT_OK", mem_ok));
    auto last_fill = address_path->getOutputPin("LAST_FILL_WORD");
    auto fill_complete = w.both("FILL_COMPLETE", fill_success, last_fill);
    auto more_fill = w.both("MORE_FILL_WORDS", fill_success, w.invert("NOT_LAST_FILL_WORD", last_fill));
    auto write_done = w.both("WRITE_DONE", mem_done, sending_write);
    auto write_ok = w.both("WRITE_OK", write_done, mem_ok);
    // A burst hit's further words: STREAM_OFFSET counts the words from the
    // requested one; one is taken when there is room.
    std::vector<std::shared_ptr<IOComponent>> stream_count;
    std::vector<PinPtr> stream_q;
    for (size_t i = 0; i < line_bits; ++i) { stream_count.push_back(state_bit("STREAM_OFFSET_" + std::to_string(i))); stream_q.push_back(stream_count.back()->getOutputPin("Q")); }
    auto stream_take = w.both("STREAM_TAKE", ST, room);
    std::vector<PinPtr> stream_last_terms;
    for (size_t i = 0; i < line_bits; ++i) stream_last_terms.push_back(stream_q[i]);
    auto stream_last = streams ? w.reduce("STREAM_LAST_WORD", stream_last_terms, true) : high;
    // STREAM_OFFSET: 1 when a burst hit starts streaming, one more per word taken.
    {
        PinPtr carry = high;
        for (size_t i = 0; i < line_bits; ++i) {
            const auto n = std::to_string(i);
            auto sum = w.binary<XORGate>("STREAM_SUM_" + n, stream_q[i], carry);
            carry = w.both("STREAM_CARRY_" + n, stream_q[i], carry);
            auto stepped = w.either("STREAM_NEXT_" + n, w.both("STREAM_STEP_" + n, stream_take, sum),
                                    w.both("STREAM_KEEP_" + n, w.invert("STREAM_NO_STEP_" + n, stream_take), stream_q[i]));
            auto start_value = w.both("STREAM_START_" + n, burst_hit, w.constant("STREAM_FIRST_" + n, i == 0));
            w.connect(w.either("STREAM_OFFSET_NEXT_" + n, start_value, w.both("STREAM_HOLD_" + n, w.invert("STREAM_NOT_STARTING_" + n, burst_hit), stepped)),
                      stream_count[i]->getInputPin("D"));
        }
    }
    // A fill brings the line from the requested word on, wrapping (as a
    // burst does, and word by word in the same order): the fill word's
    // offset is the requested one plus the address path's count, and the
    // requested word is the first.
    auto fill_bits = bits_of("FILL_COUNT_BITS", address_path->template getOutputPin<32>("FILL_BACKING_ADDRESS"));
    std::vector<PinPtr> fill_count(fill_bits.begin() + word_shift, fill_bits.begin() + offset);
    auto fill_word = add_offsets("FILL_WORD", wanted_word, fill_count);
    auto fill_index = data_address("FILL_INDEX", fill_word);
    PinPtr requested = high;
    if (streams) {
        std::vector<PinPtr> first;
        for (size_t i = 0; i < line_bits; ++i) first.push_back(w.invert("FIRST_FILL_WORD_" + std::to_string(i), fill_count[i]));
        requested = w.reduce("REQUESTED_WORD", first, true);
    }

    // Responses. Each sets DONE, LAST (this one ends the transaction) and the
    // result and status registers.
    auto forward = w.both("FORWARD_FILL_WORD", fill_success, is_burst);
    auto read_answered = w.reduce("READ_ANSWERED", {fill_complete, w.invert("NOT_A_BURST", is_burst), is_read}, true);
    auto respond = w.reduce("RESPOND", {invalid, read_hit, stream_take, forward, read_answered, fill_failure, write_done,
                                        queued_write, back_write}, false);
    auto ends = w.reduce("RESPONSE_ENDS", {invalid, w.both("SINGLE_WORD_HIT", read_hit, streams ? w.invert("HIT_NOT_BURST", is_burst) : high),
        w.both("LAST_STREAM_WORD", stream_take, stream_last), w.both("LAST_FORWARDED", forward, last_fill),
        read_answered, fill_failure, write_done, queued_write, back_write}, false);
    auto last_bit = state_bit("LAST");
    w.connect(w.either("NEXT_LAST", w.both("NEW_LAST", respond, ends),
        w.both("HOLD_LAST", last_bit->getOutputPin("Q"), w.invert("NO_NEW_RESPONSE", respond))), last_bit->getInputPin("D"));
    w.connect(last_bit->getOutputPin("Q"), front->getInputPin("LAST"));
    w.connect(D, front->getInputPin("DONE"));

    // Next states.
    if (allocate) w.connect(w.both("WRITE_LINE_FILLED", fill_complete, is_write), refilled_bit->getInputPin("D"));
    w.connect(allocate ? w.either("NEXT_LOOKUP_VALUE", valid_start, REFILLED) : valid_start, phases->getInputPin("NEXT_LOOKUP"));
    w.connect(w.reduce("NEXT_MEMORY_SEND_VALUE", {fill_start, write_to_lower,
        config_.burst_fill ? low : more_fill,
        w.both("STAY_MEMORY_SEND", MS, w.invert("NOT_SENT", getInputPin("MEM_REQ_READY")))}, false),
        phases->getInputPin("NEXT_MEMORY_SEND"));
    auto waiting = w.both("STILL_WAITING", w.either("WAITING", sent, MW), w.invert("NO_MEMORY_RESPONSE", rsp));
    w.connect(config_.burst_fill ? w.either("NEXT_MEMORY_WAIT_VALUE", waiting, more_fill) : waiting, phases->getInputPin("NEXT_MEMORY_WAIT"));
    w.connect(streams ? w.either("NEXT_STREAM_VALUE", burst_hit, w.both("STAY_STREAM", ST, w.invert("STREAM_ENDS", w.both("STREAM_DONE", stream_take, stream_last))))
                      : low, phases->getInputPin("NEXT_STREAM"));
    w.connect(w.either("NEXT_DONE_VALUE", respond, w.both("STAY_DONE", D, w.invert("NOT_FINISHED", front->getOutputPin("FINISH")))),
              phases->getInputPin("NEXT_DONE"));

    // The way a fill, a write-through or a stream uses: the victim of a read
    // miss, the hit way of a write or a burst hit; and whether that write hit.
    std::vector<PinPtr> current_way;
    if (way_bits) {
        auto choose = w.reduce("CHOOSE_WAY", {read_miss, write_now, burst_hit, flush_found}, false);
        auto hit_choice = w.either("WRITE_OR_BURST_HIT", write_now, burst_hit);
        auto not_choosing = w.invert("NOT_CHOOSING", choose);
        for (size_t b = 0; b < way_bits; ++b) {
            auto bit = state_bit(bit_name("CURRENT_WAY", b));
            PinPtr q = bit->getOutputPin("Q");
            std::vector<PinPtr> chosen{w.both(bit_name("VICTIM_IS_ONE", b), read_miss, victim_way[b]),
                                       w.both(bit_name("HIT_WAY_ONE", b), hit_choice, hit_way[b])};
            if (back) chosen.push_back(w.both(bit_name("WALKED_WAY_ONE", b), flush_found, flush_way[b]));
            w.connect(w.either(bit_name("NEXT_CURRENT_WAY", b), w.reduce(bit_name("CHOSEN_WAY_ONE", b), chosen, false),
                               w.both(bit_name("HOLD_CURRENT_WAY", b), q, not_choosing)), bit->getInputPin("D"));
            current_way.push_back(q);
        }
    }
    auto write_hit_bit = state_bit("WRITE_HIT");
    w.connect(w.either("NEXT_WRITE_HIT", w.both("WRITE_HIT_NOW", write_lookup, hit),
        w.both("HOLD_WRITE_HIT", write_hit_bit->getOutputPin("Q"), w.invert("NOT_LOOKING_UP", L))),
        write_hit_bit->getInputPin("D"));
    auto update_written = w.both("UPDATE_WRITTEN_LINE", write_ok, write_hit_bit->getOutputPin("Q"));

    // Data-bank reads during a burst hit. A flip-flop bank is read in the
    // cycle its word is taken. A clocked bank shows at an edge the word it
    // was given before it: the second word is given in the lookup cycle,
    // then the next word as one is taken, or the same word again.
    auto stream_word = add_offsets("STREAM_WORD", wanted_word, stream_q);
    const bool clocked_data = config_.data_technology == Technology::SRAM;
    PinPtr stream_index = data_address("STREAM_INDEX", stream_word);
    auto lookup_burst = streams ? w.both("LOOKUP_BURST", L, is_burst) : low;
    PinPtr data_read = w.reduce("DATA_READ_START", {valid_start, REFILLED, ER}, false), reading_stream = ST;
    if (clocked_data && streams) {
        std::vector<PinPtr> one_bits(line_bits, low);
        one_bits[0] = high;
        auto next_index = data_address("NEXT_STREAM_INDEX", add_offsets("NEXT_STREAM_WORD", stream_word, one_bits));
        stream_index = memory_detail::payloadMux(builder, w, "STREAM_HOLD_OR_NEXT", stream_index, next_index, stream_take);
        stream_index = memory_detail::payloadMux(builder, w, "STREAM_OR_SECOND_INDEX", stream_index,
            data_address("SECOND_INDEX", add_offsets("SECOND_WORD", wanted_word, one_bits)), L);
        auto stream_read = w.both("STREAM_READ", ST, w.invert("STREAM_FINISHING", w.both("STREAM_LAST_TAKEN", stream_take, stream_last)));
        data_read = w.reduce("DATA_READ_NOW", {valid_start, lookup_burst, stream_read, REFILLED, ER}, false);
        reading_stream = w.either("READING_STREAM", lookup_burst, ST);
    }

    // A data-bank write: the requester's word (a write into the line) or a fill word.
    auto write_update = w.either("WRITE_UPDATE", update_written, lookup_update);
    auto data_write_word = memory_detail::payloadMux(builder, w, "DATA_WRITE_WORD",
        getInputPinDynamic("MEM_RSP_READ_DATA"), write_data_reg->getOutputPinDynamic("Q"), write_update);
    auto data_write_mask = memory_detail::payloadMux(builder, w, "DATA_WRITE_MASK_SOURCE",
        full_data_mask, write_mask_reg->getOutputPinDynamic("Q"), write_update);
    auto data_write_index = memory_detail::payloadMux(builder, w, "DATA_WRITE_INDEX", fill_index, data_index, write_update);
    auto data_writing = w.reduce("DATA_WRITING", {fill_success, update_written, lookup_update}, false);
    // A queued or write-back write updates its line in the lookup cycle, in the hit way.
    auto write_way = current_way;
    if (buffered || back)
        for (size_t b = 0; b < way_bits; ++b)
            write_way[b] = memory_detail::payloadMux(builder, w, bit_name("UPDATE_WAY", b), current_way[b], hit_way[b], lookup_update);
    const auto current_ways = decode("CURRENT_WAY_ZERO", "CURRENT_WAY_IS", current_way);
    const auto write_ways = decode("WRITE_WAY_ZERO", "WRITE_WAY_IS", write_way);
    const auto victim_ways = decode("VICTIM_ZERO", "VICTIM_IS", victim_way);
    const auto walked_ways = decode("WALKED_WAY_ZERO", "WALKED_WAY_IS", flush_way);
    auto data_read_index = memory_detail::payloadMux(builder, w, "DATA_LOOKUP_OR_STREAM", lookup_index, stream_index, reading_stream);
    if (back) {
        // A write-back reads the victim's words from its set.
        auto victim_set_bits = bits_of("EVICT_SET_BITS", victim_index_reg->getOutputPin<32>("Q"));
        std::vector<PinPtr> index = evict_word;
        index.insert(index.end(), victim_set_bits.begin(), victim_set_bits.begin() + set_bits);
        data_read_index = memory_detail::payloadMux(builder, w, "DATA_READ_OR_EVICT", data_read_index, join("EVICT_INDEX", index),
                                                    w.reduce("EVICTING", {ER, ES, EW}, false));
    }
    for (size_t way = 0; way < config_.ways; ++way) {
        const auto n = std::to_string(way);
        auto this_way = current_ways[way];
        auto data_write = w.both("DATA_WRITE_" + n, data_writing, write_ways[way]);
        auto tag_write = w.both("TAG_WRITE_" + n, fill_complete, this_way);
        w.connect(w.reduce("TAG_READ_" + n, {valid_start, REFILLED, FR}, false), getOutputPin("TAG" + n + "_READ"));
        w.connect(tag_write, getOutputPin("TAG" + n + "_WRITE"));
        w.connect(data_read, getOutputPin("DATA" + n + "_READ"));
        w.connect(data_write, getOutputPin("DATA" + n + "_WRITE"));
        auto tag_address = memory_detail::payloadMux(builder, w, "TAG_ADDRESS_" + n, tag_read_index, tag_index, tag_write);
        if (back) tag_address = memory_detail::payloadMux(builder, w, "TAG_OR_WALKED_" + n, tag_address,
            join("WALKED_TAG_INDEX_" + n, flush_set), w.either("WALK_READS_TAG_" + n, FR, FC));
        w.connect(tag_address, getOutputPin<32>("TAG" + n + "_WORD_ADDR"));
        w.connect(tag_value, getOutputPin<32>("TAG" + n + "_WRITE_DATA"));
        w.connect(full_tag_mask, getOutputPin<4>("TAG" + n + "_WRITE_MASK"));
        w.connect(memory_detail::payloadMux(builder, w, "DATA_ADDRESS_" + n, data_read_index, data_write_index, data_write),
                  getOutputPin<32>("DATA" + n + "_WORD_ADDR"));
        w.connect(data_write_word, getOutputPinDynamic("DATA" + n + "_WRITE_DATA"));
        w.connect(data_write_mask, getOutputPinDynamic("DATA" + n + "_WRITE_MASK"));
        if (store) continue;
        w.connect(w.both("EVICT_LINE_" + n, read_miss, victim_ways[way]), lines[way]->getInputPin("CLEAR"));
        w.connect(tag_write, lines[way]->getInputPin("MARK"));
        if (back) {
            w.connect(w.both("MARK_DIRTY_" + n, back_write, matches[way]), lines[way]->getInputPin("SET_DIRTY"));
            w.connect(w.reduce("WRITTEN_BACK_" + n, {evict_last_done, EF, walked_ways[way]}, true),
                      lines[way]->getInputPin("CLEAN"));
        }
    }

    // Idle: no lookup, backing request or stream; a shown response still lets
    // the front end take the next request as it is finished.
    auto working = w.reduce("WORKING", {L, MS, MW, ST, miss_wait, ER, ES, EW, FR, FC, REFILLED, clearing}, false);
    // Invalidation waits for the write buffer too: afterwards the backing
    // memory holds every write.
    auto idle = w.reduce("IDLE", {w.invert("NOT_WORKING", working), w.invert("NOT_DONE", D), drained}, true);
    w.connect(w.reduce("AVAILABLE", {w.invert("NO_WORK", working), w.invert("NO_INVALIDATION", getInputPin("INV_VALID")),
                                     w.invert("QUEUE_NOT_FULL", queue_full)}, true),
              front->getInputPin("BACK_READY"));
    // A write-back cache takes an invalidation once its walk has written
    // every dirty line back.
    auto inv_ready = store ? w.both("INVALIDATION_READY", idle, INV_CLEARED)
                   : back ? w.both("INVALIDATION_READY", idle, FLUSHED) : idle;
    w.connect(inv_ready, getOutputPin("INV_READY"));
    auto invalidate = w.both("ACCEPT_INVALIDATION", getInputPin("INV_VALID"), inv_ready);
    if (back) w.connect(w.either("NEXT_FLUSHED", w.both("WALK_COMPLETE", flush_step, flush_last),
                                 w.both("STAY_FLUSHED", FLUSHED, w.invert("NOT_INVALIDATED", invalidate))), flushed_bit->getInputPin("D"));
    for (const auto& line : lines) w.connect(invalidate, line->getInputPin("INVALIDATE"));
    // Replacement updates: lru and plru follow every access to a line (a read
    // hit, a completed fill, a write into it), fifo and random every completed
    // fill. policy_next: the set's bits after the update.
    PinPtr touch = low, policy_update = low;
    std::vector<PinPtr> touched_way, policy_next;
    if (config_.ways > 1 || store)
        touch = w.reduce("TOUCH_REPLACEMENT", {read_hit, fill_complete, update_written, lookup_update}, false);
    if (config_.ways > 1) {
        auto hit_touch = w.either("HIT_TOUCH", read_hit, lookup_update);
        for (size_t b = 0; b < way_bits; ++b)
            touched_way.push_back(memory_detail::payloadMux(builder, w, bit_name("TOUCHED_WAY", b), current_way[b], hit_way[b], hit_touch));
        const bool by_fill = policy == CacheReplacement::FIFO || policy == CacheReplacement::Random;
        policy_update = by_fill ? fill_complete : touch;
        if (two_way_lru) {
            policy_next = touched_way;
        } else if (policy == CacheReplacement::PLRU) {
            // The nodes on the touched way's path point away from it.
            std::vector<PinPtr> away;
            for (size_t level = 0; level < way_bits; ++level)
                away.push_back(w.invert("PLRU_AWAY_" + std::to_string(level), touched_way[way_bits - 1 - level]));
            for (size_t level = 0; level < way_bits; ++level) {
                const size_t first = (size_t{1} << level) - 1;
                for (size_t j = 0; j <= first; ++j) {
                    const auto node = std::to_string(first + j);
                    if (level == 0) { policy_next.push_back(away[0]); continue; }
                    std::vector<PinPtr> on_path;  // the touched way's top `level` bits are j
                    for (size_t i = 0; i < level; ++i)
                        on_path.push_back(((j >> (level - 1 - i)) & 1) ? touched_way[way_bits - 1 - i] : away[i]);
                    policy_next.push_back(memory_detail::payloadMux(builder, w, "PLRU_NEXT_" + node, policy_state[first + j],
                        away[level], w.reduce("PLRU_ON_PATH_" + node, on_path, true)));
                }
            }
        } else if (policy == CacheReplacement::FIFO) {
            PinPtr carry = high;
            for (size_t b = 0; b < way_bits; ++b) {
                const auto n = std::to_string(b);
                policy_next.push_back(w.binary<XORGate>("FIFO_NEXT_" + n, policy_state[b], carry));
                carry = w.both("FIFO_CARRY_" + n, policy_state[b], carry);
            }
        } else {
            auto q = [&](size_t i) -> PinPtr { return random_bits[i]->getOutputPin("Q"); };
            auto feedback = w.invert("RANDOM_FEEDBACK", w.binary<XORGate>("RANDOM_TAPS",
                w.binary<XORGate>("RANDOM_TAPS_HIGH", q(15), q(14)), w.binary<XORGate>("RANDOM_TAPS_LOW", q(12), q(3))));
            for (size_t i = 0; i < 16; ++i)
                w.connect(memory_detail::payloadMux(builder, w, "RANDOM_NEXT_" + std::to_string(i), q(i), i == 0 ? feedback : q(i - 1),
                                                    fill_complete), random_bits[i]->getInputPin("D"));
        }
    }
    if (store) {
        // The line-state word of the set: each change the flip-flop version
        // makes is applied to the kept copy and written to the store at the same
        // edge. Valid: cleared for an evicted way, set by a completed fill.
        // Dirty: set by a write-back write, cleared by a fill, an eviction or a
        // completed write-back. Most recently used way: every touch.
        std::vector<PinPtr> next(32, low);
        PinPtr changes = w.reduce("LINE_WORD_CHANGES", {read_miss, fill_complete, touch, back_write,
            back ? w.both("WRITE_BACK_ENDS", evict_last_done, EF) : low}, false);
        for (size_t way = 0; way < config_.ways; ++way) {
            const auto n = std::to_string(way);
            auto evicted = w.both("WORD_EVICTS_" + n, read_miss, victim_ways[way]);
            auto filled = w.both("WORD_FILLS_" + n, fill_complete, current_ways[way]);
            next[way] = w.either("NEXT_WORD_VALID_" + n, w.both("WORD_KEEPS_VALID_" + n, way_valid[way],
                w.invert("WORD_NOT_EVICTED_" + n, evicted)), filled);
            if (back) {
                auto cleaned = w.reduce("WORD_CLEANED_" + n, {evicted, filled, w.reduce("WORD_WRITTEN_BACK_" + n, {evict_last_done, EF,
                                         walked_ways[way]}, true)}, false);
                auto dirtied = w.both("WORD_DIRTIED_" + n, back_write, matches[way]);
                next[8 + way] = w.either("NEXT_WORD_DIRTY_" + n, dirtied,
                    w.both("WORD_KEEPS_DIRTY_" + n, way_dirty[way], w.invert("WORD_NOT_CLEANED_" + n, cleaned)));
            }
        }
        for (size_t i = 0; i < policy_bits; ++i)
            next[16 + i] = memory_detail::payloadMux(builder, w, two_way_lru ? std::string("NEXT_WORD_MRU") : "NEXT_WORD_POLICY_" + std::to_string(i),
                                                     policy_state[i], policy_next[i], policy_update);
        auto changed_word = join("CHANGED_LINE_WORD", next);
        auto zero_word = builder.addNewComponent<ConstantValue<32>>("EMPTY_LINE_WORD", 0)->getOutputPin<32>("OUT");
        auto meta_write = w.either("LINE_WORD_WRITE", clearing, changes);
        auto meta_data = memory_detail::payloadMux(builder, w, "LINE_WORD_DATA", changed_word, zero_word, clearing);
        // Store address: the cleared set, the walked set, or the request's set
        // (the lookup's read, or a write after it).
        auto request_set = join("LINE_SET", set_index);
        auto meta_address = memory_detail::payloadMux(builder, w, "LINE_ADDRESS_REQUEST", tag_read_index, request_set,
                                                      w.invert("NOT_STARTING", valid_start));
        if (back) meta_address = memory_detail::payloadMux(builder, w, "LINE_ADDRESS_WALKED", meta_address, join("LINE_WALKED_SET", flush_set), flushing);
        meta_address = memory_detail::payloadMux(builder, w, "LINE_ADDRESS_CLEARED", meta_address, join("LINE_CLEARED_SET", clear_index), clearing);
        w.connect(meta_address, getOutputPin<32>("META_WORD_ADDR"));
        w.connect(meta_data, getOutputPin<32>("META_WRITE_DATA"));
        w.connect(full_tag_mask, getOutputPin<4>("META_WRITE_MASK"));
        w.connect(meta_write, getOutputPin("META_WRITE"));
        w.connect(w.reduce("LINE_WORD_READ", {valid_start, REFILLED, FR}, false), getOutputPin("META_READ"));
        w.connect(w.reduce("LINE_WORD_LOAD", {L, FC, meta_write}, false), line_word_reg->getInputPin("WE"));
        w.connect(memory_detail::payloadMux(builder, w, "LINE_WORD_SOURCE", getInputPinDynamic("META_READ_DATA"), meta_data, meta_write),
                  line_word_reg->getInputPin<32>("D"));
        // The clearing walk.
        auto clear_last = clear_index.empty() ? high : w.reduce("CLEAR_LAST_SET", clear_index, true);
        auto clear_done = w.both("CLEAR_DONE", clearing, clear_last);
        auto inv_start = w.reduce("INVALIDATION_STARTS", {getInputPin("INV_VALID"), idle,
            back ? FLUSHED : high, w.invert("NOT_CLEARED_YET", INV_CLEARED), w.invert("NOT_WALKING", INV_WALK)}, true);
        w.connect(w.either("NEXT_INITIALIZED", w.both("STAY_INITIALIZED", initialized_bit->getOutputPin("Q"),
                    w.invert("NOT_INVALIDATING", inv_start)), clear_done), initialized_bit->getInputPin("D"));
        w.connect(w.either("NEXT_INVALIDATION_WALK", inv_start, w.both("STILL_INVALIDATING", INV_WALK, w.invert("CLEAR_NOT_DONE", clear_done))),
                  inv_walk_bit->getInputPin("D"));
        w.connect(w.either("NEXT_INVALIDATION_CLEARED", w.both("INVALIDATION_CLEARED_NOW", clear_done, INV_WALK),
                    w.both("STAY_CLEARED", INV_CLEARED, w.invert("NOT_TAKEN_YET", invalidate))), inv_cleared_bit->getInputPin("D"));
        PinPtr carry = high;
        for (size_t i = 0; i < clear_index.size(); ++i) {
            const auto n = std::to_string(i);
            auto sum = w.binary<XORGate>("CLEAR_INDEX_SUM_" + n, clear_index[i], carry);
            carry = w.both("CLEAR_INDEX_CARRY_" + n, clear_index[i], carry);
            w.connect(w.reduce("NEXT_CLEAR_INDEX_" + n, {sum, clearing, w.invert("CLEAR_INDEX_ENDS_" + n, clear_done)}, true),
                      clear_index_bits[i]->getInputPin("D"));
        }
    }
    if (replacement) {
        w.connect(invalidate, replacement->getInputPin("INVALIDATE"));
        w.connect(touch, replacement->getInputPin("TOUCH"));
        w.connect(touched_way[0], replacement->getInputPin("TOUCH_WAY"));
    }
    if (policy_store) {
        w.connect(invalidate, policy_store->getInputPin("INVALIDATE"));
        w.connect(policy_update, policy_store->getInputPin("UPDATE"));
        for (size_t i = 0; i < policy_bits; ++i) w.connect(policy_next[i], policy_store->getInputPin("NEXT_" + std::to_string(i)));
    }

    // Result: the hit way's word, a streamed word of the current way, or a
    // backing-memory word as it passes (the requested one, or every word of
    // a burst miss).
    std::vector<PinPtr> data_words;
    for (size_t way = 0; way < config_.ways; ++way) data_words.push_back(getInputPinDynamic("DATA" + std::to_string(way) + "_READ_DATA"));
    auto hit_word = select("HIT_WORD", data_words, hit_way);
    auto current_word = select("CURRENT_WAY_WORD", data_words, current_way);
    auto bank_word = memory_detail::payloadMux(builder, w, "BANK_WORD", current_word, hit_word, read_hit);
    w.connect(w.reduce("CAPTURE_READ_RESULT", {read_hit, stream_take, forward,
        w.reduce("CAPTURE_FILL_WORD", {fill_success, requested, w.invert("NOT_BURST_REQUEST", is_burst)}, true)}, false),
        result_reg->getInputPin("WE"));
    w.connect(memory_detail::payloadMux(builder, w, "READ_DATA_SOURCE", getInputPinDynamic("MEM_RSP_READ_DATA"), bank_word,
        w.either("FROM_BANK", read_hit, stream_take)), result_reg->getInputPinDynamic("D"));
    w.connect(respond, status_reg->getInputPin("WE"));
    auto status = memory_detail::payloadMux(builder, w, "STATUS_FROM_MEMORY", zero3, getInputPin<3>("MEM_RSP_STATUS"),
                                            w.either("MEMORY_STATUS_SELECTED", fill_failure, write_done));
    status = memory_detail::payloadMux(builder, w, "STATUS_FROM_REQUEST", status, check->getOutputPin<3>("STATUS"), invalid);
    w.connect(status, status_reg->getInputPin<3>("D"));
    w.connect(status_reg->getOutputPin<3>("Q"), front->getInputPin<3>("STATUS"));
    auto return_read_data = w.both("RETURN_READ_DATA", status_ok("COMPLETED_STATUS", status_reg->getOutputPin<3>("Q")), is_read);
    w.connect(memory_detail::payloadMux(builder, w, "ZERO_FAILED_RESULT", zero_data, result_reg->getOutputPinDynamic("Q"), return_read_data),
              front->getInputPinDynamic("RESULT"));

    // Backing memory: the fill (word by word, or one burst from the requested
    // word), or the write of a write-through.
    w.connect(read_miss, address_path->getInputPin("READ_MISS"));
    w.connect(fill_done, address_path->getInputPin("FILL_DATA_DONE"));
    w.connect(fill_success, address_path->getInputPin("FILL_DATA_SUCCESS"));
    w.connect(fill_failure, address_path->getInputPin("MEMORY_FAILURE"));
    std::vector<PinPtr> fill_address_bits(request_bits.begin(), request_bits.end());
    for (size_t i = 0; i < word_shift; ++i) fill_address_bits[i] = low;
    for (size_t i = 0; i < line_bits; ++i) fill_address_bits[word_shift + i] = fill_word[i];
    auto fill_address = config_.burst_fill ? PinPtr(addr_reg->getOutputPin<32>("Q")) : join("FILL_ADDRESS", fill_address_bits);
    auto lower_address = memory_detail::payloadMux(builder, w, "LOWER_ADDRESS", fill_address, addr_reg->getOutputPin<32>("Q"), sending_write);
    auto lower_op = memory_detail::payloadMux(builder, w, "LOWER_OP", read_op, write_op, sending_write);
    auto lower_data = memory_detail::payloadMux(builder, w, "LOWER_WRITE_DATA", zero_data, write_data_reg->getOutputPinDynamic("Q"), sending_write);
    auto lower_mask = memory_detail::payloadMux(builder, w, "LOWER_WRITE_MASK", zero_mask, write_mask_reg->getOutputPinDynamic("Q"), sending_write);
    PinPtr lower_valid = MS, lower_ready = rsp_ready;
    if (buffered) {
        // With a write buffer, MEMORY_SEND is always a fill; otherwise the
        // queue's oldest write uses the lower port.
        lower_address = memory_detail::payloadMux(builder, w, "LOWER_ADDRESS_OR_QUEUE", queue->getOutputPin<32>("REQ_ADDR"), fill_address, MS);
        lower_op = memory_detail::payloadMux(builder, w, "LOWER_OP_OR_QUEUE", write_op, read_op, MS);
        lower_data = memory_detail::payloadMux(builder, w, "LOWER_DATA_OR_QUEUE", queue->getOutputPinDynamic("REQ_DATA"), zero_data, MS);
        lower_mask = memory_detail::payloadMux(builder, w, "LOWER_MASK_OR_QUEUE", queue->getOutputPinDynamic("REQ_MASK"), zero_mask, MS);
        lower_valid = w.either("LOWER_REQUEST", MS, queue->getOutputPin("REQ_VALID"));
        lower_ready = w.either("LOWER_RESPONSE_READY", rsp_ready, queue->getOutputPin("RSP_READY"));
    }
    if (back) {
        // A write-back word: the victim word from the data bank.
        lower_address = memory_detail::payloadMux(builder, w, "LOWER_ADDRESS_OR_EVICT", lower_address, evict_address, ES);
        lower_op = memory_detail::payloadMux(builder, w, "LOWER_OP_OR_EVICT", lower_op, write_op, ES);
        lower_data = memory_detail::payloadMux(builder, w, "LOWER_DATA_OR_EVICT", lower_data, current_word, ES);
        lower_mask = memory_detail::payloadMux(builder, w, "LOWER_MASK_OR_EVICT", lower_mask, full_data_mask, ES);
        lower_valid = w.either("LOWER_REQUEST_OR_EVICT", lower_valid, ES);
        lower_ready = w.either("LOWER_RESPONSE_OR_EVICT", lower_ready, evict_rsp_ready);
    }
    w.connect(lower_address, getOutputPin<32>("MEM_REQ_ADDR"));
    w.connect(lower_op, getOutputPin<3>("MEM_REQ_OP"));
    w.connect(lower_data, getOutputPinDynamic("MEM_REQ_WRITE_DATA"));
    w.connect(lower_mask, getOutputPinDynamic("MEM_REQ_WRITE_MASK"));
    w.connect(lower_valid, getOutputPin("MEM_REQ_VALID"));
    w.connect(lower_ready, getOutputPin("MEM_RSP_READY"));
    w.finish();
}

// The gate-level controller of one parameter set as the compact form runs it:
// its gate list and the delay table proven on the same gates. Built once per
// width, parameters and timing profile. Storage inside stays compact, as in
// every structural view of a core (and as the delay tables are proven).
struct CompactControllerProgram {
    std::shared_ptr<const circuit::analysis::GateListProgram> gates;
    PartDelays delays;
};

BuildProfile controllerGateProfile() {
    std::vector<ProfileRule> rules{preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("storage.state-bit"),
                                                  "compact state bits")};
    for (size_t width : {1, 2, 3, 4, 5, 8, 16, 32})
        rules.push_back(preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("storage.register." + std::to_string(width)),
                                       "compact registers"));
    return withProfileOverrides(maximallyStructural(), std::move(rules), "cache-controller-gates-v1");
}

std::shared_ptr<const CompactControllerProgram> compactControllerProgram(size_t width, const ParameterMap& parameters) {
    std::string key = timing::activeTimingProfile().name + "|" + std::to_string(width);
    for (const auto& [name, value] : parameters) if (name != "controller") key += "|" + name + "=" + value;
    static std::mutex guard;
    static std::map<std::string, std::shared_ptr<const CompactControllerProgram>> programs;
    {
        const std::lock_guard<std::mutex> lock(guard);
        if (const auto found = programs.find(key); found != programs.end()) return found->second;
    }
    auto gate_parameters = parameters;
    gate_parameters["controller"] = "structural";
    const auto gates = builtinComponentCatalog().createRoot(
        {"memory.cache.controller." + std::to_string(width), "GATES", gate_parameters}, controllerGateProfile()).root;
    std::string why;
    auto program = std::make_shared<CompactControllerProgram>();
    program->gates = circuit::analysis::GateListModel::compile(gates, &why);
    if (!program->gates) throw std::runtime_error("Cache controller gate list: " + why);
    // RST and INV_VALID are held at 0, as in the clock analysis of a run
    // (the invalidation clears line state through the flip-flops' reset).
    circuit::analysis::SettlingOptions options;
    options.fixed_inputs["RST"] = 0;
    options.fixed_inputs["INV_VALID"] = 0;
    std::vector<std::string> diagnostics;
    program->delays = circuit::analysis::CircuitTimingAnalyzer::delays(gates, options, "", &diagnostics);
    if (program->delays.empty())
        throw std::runtime_error("Cache controller delays: " + (diagnostics.empty() ? std::string("none") : diagnostics.front()));
    program->delays.evidence = "proven at build time on the gate-level CacheBankController<" + std::to_string(width) + ">";
    const std::lock_guard<std::mutex> lock(guard);
    return programs.emplace(key, std::move(program)).first->second;
}

// The compact cache controller: one part that runs the gate-level
// controller's own gate list (GateListModel) on the delays proven on it.
template<size_t Width>
class CacheControllerBehavioral : public memory_device::Primitive {
public:
    CacheControllerBehavioral(std::string name, CacheConfig config, const ParameterMap& parameters)
        : Primitive(std::move(name), "CacheBankController<" + std::to_string(Width) + ">",
                    [config](IOComponent* self) { controllerPins(self, Width, config); }),
          program_(compactControllerProgram(Width, parameters)), model_(program_->gates) {}
    // Pins exist once the part is created: then its delay table applies.
    void attachDelays() { setDelays(program_->delays); }
    void evaluate(size_t time, Simulator& simulator) override {
        if (input_pins_.empty()) {
            for (const auto& [name, width] : model_.inputs()) input_pins_.push_back(getInputPinDynamic(name));
            values_.resize(input_pins_.size());
        }
        for (size_t pin = 0; pin < input_pins_.size(); ++pin) values_[pin] = input_pins_[pin]->getValueAsVector();
        model_.step(values_);
        const auto& outputs = model_.outputs();
        for (size_t pin = 0; pin < outputs.size(); ++pin) {
            const auto& name = outputs[pin].first;
            if (model_.passesClock(pin)) {
                // The gates pass the clock on through a zero-delay bridge.
                const auto clock = getOutputPinDynamic(name);
                clock->setValueFromVector(read("CLK"));
                if (auto wire = clock->getExternalWireBase()) simulator.scheduleEvent(makeWireUpdateEvent(time, wire, read("CLK")));
                continue;
            }
            publish(simulator, time, name, model_.output(pin));
        }
    }
private:
    std::shared_ptr<const CompactControllerProgram> program_;
    circuit::analysis::GateListModel model_;
    std::vector<std::shared_ptr<PinBase>> input_pins_;
    std::vector<std::vector<LogicValue>> values_;
};

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
    // The controller: gates, or the compact part that runs their list.
    auto controller = memory_device::child(builder, *this, "memory.cache.controller." + std::to_string(Width), "CONTROLLER",
        cacheParameters(config_), config_.compact_controller ? Fidelity::Behavioral : Fidelity::Structural);
    for (const auto& [name, pin] : getAllInputPins()) w.connect(pin, controller->getInputPinDynamic(name));
    for (const auto& [name, pin] : getAllOutputPins()) w.connect(controller->getOutputPinDynamic(name), pin);
    const size_t words_per_line = config_.block_bytes / (Width / 8);
    // Each bank's helpers are named after it (TAG_0_..., DATA_1_..., META_),
    // so that the floorplan places them under their bank. The line-state
    // store, if any, uses the tag banks' technology.
    struct Bank { std::string prefix, name, helper; size_t width, depth; Technology technology; };
    std::vector<Bank> banks;
    for (size_t way = 0; way < config_.ways; ++way) {
        const auto n = std::to_string(way);
        banks.push_back({"TAG" + n + "_", "TAG_STORE_" + n, "TAG_" + n + "_", 32, config_.sets, config_.tag_technology});
        banks.push_back({"DATA" + n + "_", "DATA_STORE_" + n, "DATA_" + n + "_", Width, config_.sets * words_per_line, config_.data_technology});
    }
    if (config_.line_store) banks.push_back({"META_", "LINE_STORE", "META_", 32, config_.sets, config_.tag_technology});
    for (const auto& [prefix, name, helper, width, depth, technology] : banks) {
        {
            ParameterMap parameters{{"depth_words", std::to_string(depth)}};
            if (technology == Technology::SRAM) {
                parameters["words_per_row"] = std::to_string(std::min<size_t>(2, depth));
                parameters["clocked"] = "1";
            }
            auto bank = memory_device::child(builder, *this, memoryContractId(technology, "device", width), name, parameters);
            auto read = controller->getOutputPin(prefix + "READ"), write = controller->getOutputPin(prefix + "WRITE");
            if (technology == Technology::SRAM) {
                w.connect(getInputPin("CLK"), bank->getInputPin("CLK"));
                w.connect(w.invert(helper + "SELECT", w.either(helper + "ACCESS", read, write)), bank->getInputPin("CE_N"));
                w.connect(w.invert(helper + "WRITE_STROBE", write), bank->getInputPin("WE_N"));
                w.connect(w.constant(helper + "OUTPUT_ENABLED", false), bank->getInputPin("OE_N"));
            } else {
                for (auto pin : {"CLK", "RST"}) w.connect(getInputPin(pin), bank->getInputPin(pin));
                w.connect(write, bank->getInputPin("WE"));
                w.connect(w.constant(helper + "READ_ENABLED", true), bank->getInputPin("READ_EN"));
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
    // The controller of a cache: the cache's own parameters (every setting
    // changes its pins or its gates); `controller` picks the implementation.
    for (size_t width : {8, 16, 32}) {
        const auto id = "memory.cache.controller." + std::to_string(width);
        const auto config_of = [width](const ParameterMap& parameters) {
            auto config = cacheConfig(parameters);
            if (!config.block_bytes) config.block_bytes = width / 8;
            config.validate(width);
            return config;
        };
        auto schema = [width, config_of](const ParameterMap& parameters) {
            const auto config = config_of(parameters);
            return memory_device::describe([width, config](IOComponent* self) { controllerPins(self, width, config); });
        };
        catalog.registerContract({id, 1, "CacheBankController<" + std::to_string(width) + ">",
            schema(cacheParameters(CacheConfig{})), {}, "four-state", "transaction-completion-v2", schema});
        for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
            ImplementationDescriptor implementation;
            const bool compact = fidelity == Fidelity::Behavioral;
            implementation.id = id + (compact ? ".behavioral" : ".structural");
            implementation.contract_id = id;
            implementation.fidelity = fidelity;
            implementation.concrete_type_name = "CacheBankController<" + std::to_string(width) + ">";
            implementation.supports = [schema](const ParameterMap& parameters) {
                try { schema(parameters); return true; }
                catch (const std::exception&) { return false; }
            };
            implementation.factory = [width, compact, config_of](const std::string& name, const ParameterMap& parameters,
                                                                const std::shared_ptr<BuildContext>& context) -> std::shared_ptr<Component> {
                const auto config = config_of(parameters);
                if (compact) {
                    const auto made = [&](auto part) -> std::shared_ptr<Component> { part->attachDelays(); return part; };
                    if (width == 8) return made(Component::createWithContext<CacheControllerBehavioral<8>>(context, name, config, parameters));
                    if (width == 16) return made(Component::createWithContext<CacheControllerBehavioral<16>>(context, name, config, parameters));
                    return made(Component::createWithContext<CacheControllerBehavioral<32>>(context, name, config, parameters));
                }
                if (width == 8) return Component::createWithContext<CacheBankController<8>>(context, name, config);
                if (width == 16) return Component::createWithContext<CacheBankController<16>>(context, name, config);
                return Component::createWithContext<CacheBankController<32>>(context, name, config);
            };
            implementation.evidence = {VerificationStatus::Characterized,
                {compact ? "runs the gate-level controller's own gate list; delays proven on it at build time"
                         : "gate-level cache controller"},
                {"CacheReadTest"}, compact ? std::vector<std::string>{"CacheControllerEquivalenceTest"} : std::vector<std::string>{},
                {}, "four-state", "transaction-completion-v2"};
            catalog.registerImplementation(std::move(implementation));
        }
    }
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
