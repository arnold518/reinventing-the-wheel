#include "components/TimingProfile.hpp"
#include "tests/CacheTests.hpp"
#include "tests/ComponentTestModel.hpp"
#include "modules/memory/Cache.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "simulator/Event.hpp"
#include <initializer_list>
#include <iostream>
#include <map>
#include <optional>
#include <utility>
#include <vector>

using namespace circuit;
using namespace circuit::memory;
using namespace circuit::test;

namespace {
ComponentTestSpec cacheScenarioSpec() {
    ComponentTestSpec spec{"CacheComponentScenarioTest", "memory.cache.8", "CACHE",
        {{"sets", "2"}, {"ways", "2"}, {"block_bytes", "2"},
         {"backing_depth_words", "16"}, {"mode", "read_write"}},
        {{"cache-operations", {}, {}, {100'000, 500'000}}}};
    auto& actions = spec.scenarios[0].actions;
    NamedValues state{{"CLK", logicBit(false)}, {"RST", logicBit(true)},
        {"REQ_VALID", logicBit(false)}, {"REQ_OP", logicBits(3, 0)},
        {"REQ_ADDR", logicBits(32, 0)}, {"REQ_WRITE_DATA", logicBits(8, 0)},
        {"REQ_WRITE_MASK", logicBits(1, 0)}, {"RSP_READY", logicBit(false)},
        {"INV_VALID", logicBit(false)}, {"MEM_REQ_READY", logicBit(false)},
        {"MEM_RSP_VALID", logicBit(false)}, {"MEM_RSP_READ_DATA", logicBits(8, 0)},
        {"MEM_RSP_STATUS", logicBits(3, 0)}};
    auto step = [&](std::string id, NamedValues changes = {}, NamedValues expected = {}, bool emit = true) {
        for (auto& [name, value] : changes) state[name] = value;
        actions.push_back({std::move(id), CheckpointKind::Settled, state, std::move(expected), emit, {}});
    };
    auto clocks = [&](size_t count) {
        for (size_t i = 0; i < count; ++i) {
            step({}, {{"CLK", logicBit(false)}}, {}, false);
            step({}, {{"CLK", logicBit(true)}}, {}, false);
        }
    };
    // A request the cache rejects finishes in its accepting cycle; the next
    // request is then taken while that response waits.
    auto request = [&](const std::string& id, unsigned op, uint32_t address,
                       uint8_t data = 0, unsigned mask = 0, bool rejected = false) {
        step(id + "-setup", {{"CLK", logicBit(false)}, {"REQ_VALID", logicBit(true)},
            {"REQ_OP", logicBits(3, op)}, {"REQ_ADDR", logicBits(32, address)},
            {"REQ_WRITE_DATA", logicBits(8, data)}, {"REQ_WRITE_MASK", logicBits(1, mask)}},
            {{"REQ_READY", logicBit(true)}});
        step(id + "-accepted", {{"CLK", logicBit(true)}}, {{"REQ_READY", logicBit(rejected)}});
        step({}, {{"CLK", logicBit(false)}, {"REQ_VALID", logicBit(false)}}, {}, false);
    };
    auto lower = [&](const std::string& id, unsigned op, uint32_t address,
                     uint8_t result = 0, uint8_t data = 0, unsigned mask = 0) {
        clocks(20); // A blocked lower request remains visible until accepted.
        NamedValues expected{{"MEM_REQ_VALID", logicBit(true)},
            {"MEM_REQ_OP", logicBits(3, op)}, {"MEM_REQ_ADDR", logicBits(32, address)},
            {"RSP_VALID", logicBit(false)}};
        if (op == 1) {
            expected["MEM_REQ_WRITE_DATA"] = logicBits(8, data);
            expected["MEM_REQ_WRITE_MASK"] = logicBits(1, mask);
        }
        step(id + "-lower-blocked", {{"CLK", logicBit(false)}}, expected);
        step(id + "-lower-ready", {{"MEM_REQ_READY", logicBit(true)}}, expected);
        step(id + "-lower-accepted", {{"CLK", logicBit(true)}},
             {{"MEM_REQ_VALID", logicBit(false)}});
        step(id + "-lower-response", {{"CLK", logicBit(false)},
            {"MEM_REQ_READY", logicBit(false)}, {"MEM_RSP_VALID", logicBit(true)},
            {"MEM_RSP_READ_DATA", logicBits(8, result)}},
            {{"MEM_RSP_READY", logicBit(true)}});
        step({}, {{"CLK", logicBit(true)}}, {}, false);
        step({}, {{"CLK", logicBit(false)}, {"MEM_RSP_VALID", logicBit(false)}}, {}, false);
    };
    auto response = [&](const std::string& id, uint8_t data = 0, Status status = Status::Ok) {
        clocks(20);
        step(id + "-response", {{"CLK", logicBit(false)}},
             {{"RSP_VALID", logicBit(true)}, {"RSP_READ_DATA", logicBits(8, data)},
              {"RSP_STATUS", logicBits(3, unsigned(status))},
              {"MEM_REQ_VALID", logicBit(false)}});
        step(id + "-held", {{"CLK", logicBit(true)}},
             {{"RSP_VALID", logicBit(true)}, {"RSP_READ_DATA", logicBits(8, data)},
              {"RSP_STATUS", logicBits(3, unsigned(status))}});
        step({}, {{"CLK", logicBit(false)}, {"RSP_READY", logicBit(true)}}, {}, false);
        step({}, {{"CLK", logicBit(true)}}, {}, false);
        step({}, {{"CLK", logicBit(false)}, {"RSP_READY", logicBit(false)}}, {}, false);
    };
    auto hit = [&](const std::string& id, uint32_t address, uint8_t value) {
        request(id, 0, address);
        clocks(8);
        step(id + "-no-lower-read", {{"CLK", logicBit(false)}},
             {{"MEM_REQ_VALID", logicBit(false)}});
        response(id, value);
    };
    auto miss = [&](const std::string& id, uint32_t address,
                    uint32_t block_base, uint8_t first, uint8_t second) {
        request(id, 0, address);
        lower(id + "-word-0", 0, block_base, first);
        lower(id + "-word-1", 0, block_base + 1, second);
        response(id, address == block_base ? first : second);
    };

    step("reset", {}, {{"REQ_READY", logicBit(false)}, {"RSP_VALID", logicBit(false)}});
    step("ready", {{"RST", logicBit(false)}},
         {{"REQ_READY", logicBit(true)}, {"INV_READY", logicBit(true)}});
    miss("cold", 0, 0, 0xa0, 0xa1);
    hit("same-line", 1, 0xa1); // Spatial hit in the other word.
    miss("second-way", 4, 4, 0xa4, 0xa5); // Same set, different tag.
    hit("touch-first-way", 0, 0xa0);
    miss("replace-lru", 8, 8, 0xa8, 0xa9);
    hit("retained-way", 0, 0xa0);
    miss("evicted-way", 4, 4, 0xa4, 0xa5);

    request("write-hit", 1, 4, 0x5c, 1);
    lower("write-hit", 1, 4, 0, 0x5c, 1);
    response("write-hit");
    hit("read-after-write", 4, 0x5c);

    request("write-miss", 1, 12, 0x7d, 1);
    lower("write-miss", 1, 12, 0, 0x7d, 1);
    response("write-miss");
    miss("read-after-write-miss", 12, 12, 0x7d, 0xad);

    step("invalidate-ready", {{"CLK", logicBit(false)}, {"INV_VALID", logicBit(true)}},
         {{"INV_READY", logicBit(true)}});
    step("invalidated", {{"CLK", logicBit(true)}},
         {{"RSP_VALID", logicBit(false)}});
    step({}, {{"CLK", logicBit(false)}, {"INV_VALID", logicBit(false)}}, {}, false);
    miss("refill-after-invalidate", 4, 4, 0x5c, 0xa5);

    request("out-of-range", 0, 16, 0, 0, true);
    response("out-of-range", 0, Status::Range);
    return spec;
}

struct ScriptedCacheExample {
    ComponentTestSpec spec;
    NamedValues state;
    size_t width;

    ScriptedCacheExample(std::string id, size_t word_width, CacheConfig config)
        : spec{std::move(id),
               std::string(config.scalable_storage ? "memory.cache.scalable." : "memory.cache.")
                   + std::to_string(word_width), "CACHE", cacheParameters(config),
               {{"operations", {}, {}, {100'000, 500'000}}}},
          width(word_width) {
        state = {{"CLK", logicBit(false)}, {"RST", logicBit(true)},
                 {"REQ_VALID", logicBit(false)}, {"REQ_OP", logicBits(3, 0)},
                 {"REQ_ADDR", logicBits(32, 0)},
                 {"REQ_WRITE_DATA", logicBits(width, 0)},
                 {"REQ_WRITE_MASK", logicBits(width / 8, 0)},
                 {"RSP_READY", logicBit(false)}, {"INV_VALID", logicBit(false)},
                 {"MEM_REQ_READY", logicBit(false)},
                 {"MEM_RSP_VALID", logicBit(false)},
                 {"MEM_RSP_READ_DATA", logicBits(width, 0)},
                 {"MEM_RSP_STATUS", logicBits(3, 0)}};
    }

    void step(std::string id, NamedValues changes = {}, NamedValues expected = {}, bool emit = true) {
        for (auto& [name, value] : changes) state[name] = value;
        spec.scenarios.front().actions.push_back(
            {std::move(id), CheckpointKind::Settled, state, std::move(expected), emit, {}});
    }
    void clocks(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            step({}, {{"CLK", logicBit(false)}}, {}, false);
            step({}, {{"CLK", logicBit(true)}}, {}, false);
        }
    }
    void begin() {
        step("reset", {}, {{"REQ_READY", logicBit(false)}});
        step("ready", {{"RST", logicBit(false)}}, {{"REQ_READY", logicBit(true)}});
    }
    // A rejected request finishes in its accepting cycle (REQ_READY high again).
    void request(const std::string& id, uint32_t address, bool rejected = false) {
        step(id + "-setup", {{"CLK", logicBit(false)}, {"REQ_VALID", logicBit(true)},
            {"REQ_OP", logicBits(3, 0)}, {"REQ_ADDR", logicBits(32, address)}},
            {{"REQ_READY", logicBit(true)}});
        step(id + "-accepted", {{"CLK", logicBit(true)}},
            {{"REQ_READY", logicBit(rejected)}});
        step({}, {{"CLK", logicBit(false)}, {"REQ_VALID", logicBit(false)}}, {}, false);
    }
    void lowerRead(const std::string& id, uint32_t address, uint32_t value) {
        clocks(20);
        step(id + "-lower-request", {{"CLK", logicBit(false)}},
            {{"MEM_REQ_VALID", logicBit(true)}, {"MEM_REQ_OP", logicBits(3, 0)},
             {"MEM_REQ_ADDR", logicBits(32, address)}});
        step(id + "-lower-ready", {{"MEM_REQ_READY", logicBit(true)}},
            {{"MEM_REQ_VALID", logicBit(true)}});
        step(id + "-lower-accepted", {{"CLK", logicBit(true)}},
            {{"MEM_REQ_VALID", logicBit(false)}});
        step(id + "-lower-response", {{"CLK", logicBit(false)},
            {"MEM_REQ_READY", logicBit(false)}, {"MEM_RSP_VALID", logicBit(true)},
            {"MEM_RSP_READ_DATA", logicBits(width, value)}},
            {{"MEM_RSP_READY", logicBit(true)}});
        step({}, {{"CLK", logicBit(true)}}, {}, false);
        step({}, {{"CLK", logicBit(false)}, {"MEM_RSP_VALID", logicBit(false)}}, {}, false);
    }
    void response(const std::string& id, uint32_t value, Status status = Status::Ok) {
        clocks(20);
        step(id + "-response", {{"CLK", logicBit(false)}},
            {{"RSP_VALID", logicBit(true)}, {"RSP_READ_DATA", logicBits(width, value)},
             {"RSP_STATUS", logicBits(3, unsigned(status))},
             {"MEM_REQ_VALID", logicBit(false)}});
        step({}, {{"CLK", logicBit(false)}, {"RSP_READY", logicBit(true)}}, {}, false);
        step({}, {{"CLK", logicBit(true)}}, {}, false);
        step({}, {{"CLK", logicBit(false)}, {"RSP_READY", logicBit(false)}}, {}, false);
    }
    void miss(const std::string& id, uint32_t address,
              std::initializer_list<std::pair<uint32_t, uint32_t>> words,
              uint32_t value) {
        request(id, address);
        size_t index = 0;
        for (auto [at, data] : words)
            lowerRead(id + "-word-" + std::to_string(index++), at, data);
        response(id, value);
    }
    void hit(const std::string& id, uint32_t address, uint32_t value) {
        request(id, address);
        clocks(8);
        step(id + "-no-lower-read", {{"CLK", logicBit(false)}},
             {{"MEM_REQ_VALID", logicBit(false)}});
        response(id, value);
    }
};

ComponentTestSpec directMappedCacheScenarioSpec() {
    CacheConfig config;
    config.sets = 2;
    config.ways = 1;
    config.block_bytes = 4;
    config.backing_depth_words = 16;
    ScriptedCacheExample example("CacheDirectMappedScenarioTest", 32, config);
    example.begin();
    example.miss("cold", 0, {{0, 0xa3a2a1a0}}, 0xa3a2a1a0);
    example.hit("same-word", 0, 0xa3a2a1a0);
    example.miss("conflict", 8, {{8, 0xabaaa9a8}}, 0xabaaa9a8);
    example.miss("evicted-word", 0, {{0, 0xa3a2a1a0}}, 0xa3a2a1a0);
    example.request("out-of-range", 64, true);
    example.response("out-of-range", 0, Status::Range);
    return std::move(example.spec);
}

ComponentTestSpec scalableCacheScenarioSpec() {
    CacheConfig config;
    config.sets = 8;
    config.ways = 2;
    config.block_bytes = 8;
    config.backing_depth_words = 64;
    config.tag_technology = Technology::SRAM;
    config.data_technology = Technology::SRAM;
    config.scalable_storage = true;
    ScriptedCacheExample example("CacheScalableScenarioTest", 32, config);
    example.begin();
    // A fill starts at the requested word and wraps within the line.
    example.miss("cold", 4, {{4, 0xa7a6a5a4}, {0, 0xa3a2a1a0}}, 0xa7a6a5a4);
    example.hit("neighbor", 0, 0xa3a2a1a0);
    example.miss("second-way", 64, {{64, 0xe3e2e1e0}, {68, 0xe7e6e5e4}}, 0xe3e2e1e0);
    example.hit("touch-first-way", 4, 0xa7a6a5a4);
    example.miss("third-conflict", 128, {{128, 0x23222120}, {132, 0x27262524}}, 0x23222120);
    example.hit("retained-way", 0, 0xa3a2a1a0);
    example.miss("evicted-way", 64, {{64, 0xe3e2e1e0}, {68, 0xe7e6e5e4}}, 0xe3e2e1e0);
    return std::move(example.spec);
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<size_t Width>
class CacheFixture : public IOComponent {
public:
    CacheFixture(std::string name, CacheConfig config, Technology backing_technology = Technology::ROM,
                 size_t backing_depth_override = 0,
                 std::optional<CacheConfig> lower_cache_config = std::nullopt)
        : IOComponent(std::move(name), [](IOComponent* self) {
              protocolPins(self, Width);
              self->addPin("INV_VALID", PinType::INPUT);
              self->addPin("INV_READY", PinType::OUTPUT);
              self->addPin("LOWER_REQ_VALID", PinType::OUTPUT);
              self->addPin("LOWER_REQ_READY", PinType::OUTPUT);
              self->addPin<32>("LOWER_REQ_ADDR", PinType::OUTPUT);
              self->addPin<3>("LOWER_REQ_OP", PinType::OUTPUT);
          }), config_(config), backing_technology_(backing_technology),
          backing_depth_override_(backing_depth_override),
          lower_cache_config_(std::move(lower_cache_config)) {}
    const char* getTypeName() const override { return "CacheFixture"; }
    void initializeBacking() {
        if (!backing_ff_) return;
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < config_.backing_depth_words * (Width / 8); ++i)
            bytes.push_back(uint8_t(0xa0 + i));
        backing_ff_->initializeBytes(0, bytes);
    }
    std::vector<uint8_t> backingBytes(uint32_t address, size_t count) const {
        if (!backing_ff_) throw std::logic_error("Writable backing required");
        return backing_ff_->readBytes(address, count);
    }
    void buildInternals(ComponentBuilder& builder) override {
        detail::StructuralWiring w(builder);
        auto cache = builder.addNewComponent<Cache<Width>>("CACHE", config_);
        EndpointConfig backing_config;
        backing_config.depth_words = backing_depth_override_ ? backing_depth_override_ : config_.backing_depth_words;
        backing_config.words_per_row = 1;
        // A burst-filling cache needs a backing memory whose bursts are its
        // lines.
        const auto& filler = lower_cache_config_ ? *lower_cache_config_ : config_;
        backing_config.block_words = filler.burst_fill ? filler.block_bytes / (Width / 8) : 1;
        std::shared_ptr<IOComponent> backing;
        if (backing_technology_ == Technology::ROM) {
            for (size_t i = 0; i < backing_config.depth_words * (Width / 8); ++i) {
                constexpr char hex[] = "0123456789abcdef";
                const uint8_t value = uint8_t(0xa0 + i);
                backing_config.image_hex.push_back(hex[value >> 4]);
                backing_config.image_hex.push_back(hex[value & 15]);
            }
            backing = builder.addNewComponent<MemoryEndpoint>("BACKING", Width, Technology::ROM, backing_config);
        } else {
            backing_ff_ = builder.addNewComponent<BehavioralMemoryEndpoint>("BACKING", Width,
                backing_technology_, backing_config);
            backing = backing_ff_;
        }
        std::shared_ptr<IOComponent> lowest_requester = cache;
        if (lower_cache_config_) {
            auto lower = builder.addNewComponent<Cache<Width>>("LOWER_CACHE", *lower_cache_config_);
            for (const auto& [name, pin] : cache->getAllOutputPins())
                if (name.starts_with("MEM_"))
                    w.connect(pin, lower->getInputPinDynamic(name.substr(4)));
            for (const auto& [name, pin] : cache->getAllInputPins())
                if (name.starts_with("MEM_"))
                    w.connect(lower->getOutputPinDynamic(name.substr(4)), pin);
            // The fixture broadcasts invalidation only after both levels are idle.
            w.connect(getInputPin("INV_VALID"), lower->getInputPin("INV_VALID"));
            lowest_requester = lower;
        }
        for (const auto& [name, pin] : getAllInputPins())
            w.connect(pin, cache->getInputPinDynamic(name));
        for (const auto& [name, pin] : getAllOutputPins())
            if (!name.starts_with("LOWER_")) w.connect(cache->getOutputPinDynamic(name), pin);
        for (const auto& [name, pin] : lowest_requester->getAllOutputPins()) {
            if (!name.starts_with("MEM_")) continue;
            w.connect(pin, backing->getInputPinDynamic(name.substr(4)));
            if (name == "MEM_REQ_VALID") w.connect(pin, getOutputPin("LOWER_REQ_VALID"));
            if (name == "MEM_REQ_ADDR") w.connect(pin, getOutputPin<32>("LOWER_REQ_ADDR"));
            if (name == "MEM_REQ_OP") w.connect(pin, getOutputPin<3>("LOWER_REQ_OP"));
        }
        for (const auto& [name, pin] : lowest_requester->getAllInputPins()) {
            if (!name.starts_with("MEM_")) continue;
            auto source = backing->getOutputPinDynamic(name.substr(4));
            w.connect(source, pin);
            if (name == "MEM_REQ_READY") w.connect(source, getOutputPin("LOWER_REQ_READY"));
        }
        w.finish();
    }
private:
    CacheConfig config_;
    Technology backing_technology_;
    size_t backing_depth_override_;
    std::optional<CacheConfig> lower_cache_config_;
    std::shared_ptr<BehavioralMemoryEndpoint> backing_ff_;
};

template<size_t Width>
uint32_t imageWord(uint32_t address);

template<size_t Width>
class Rig {
public:
    explicit Rig(CacheConfig config, Technology backing_technology = Technology::ROM,
                 size_t backing_depth_override = 0,
                 std::optional<CacheConfig> lower_cache_config = std::nullopt)
        : context_("W=" + std::to_string(Width) + " S=" + std::to_string(config.sets) +
                   " tag=" + technologyName(config.tag_technology) +
                   " data=" + technologyName(config.data_technology)) {
        root_ = Component::create<CacheFixture<Width>>("CACHE_FIXTURE", config,
            backing_technology, backing_depth_override, std::move(lower_cache_config));
        root_->initializeBacking();
        ComponentBuilder builder(root_);
        for (const auto& [name, pin] : root_->getAllInputPins())
            inputs_[name] = builder.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root_->getAllOutputPins())
            builder.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
        for (const auto& [name, wire] : inputs_)
            sim_.scheduleEvent(makeTestWireUpdate(0, wire, TestValue(uint64_t(name == "RST"))));
        SimulationTest::scheduleInitialEventsForTree(root_, sim_, 0);
        sim_.advanceAndRecord(now_);
        drive({{"RST", 0}});
    }
    void drive(const std::map<std::string, uint64_t>& values, size_t wait = 200) {
        // Every rising edge this rig drives: count the backing request it takes.
        if (auto clk = values.find("CLK"); clk != values.end()) {
            if (clk->second == 1 && !clock_high_) countLower();
            clock_high_ = clk->second == 1;
        }
        const auto at = now_ + 1;
        for (const auto& [name, value] : values)
            sim_.scheduleEvent(makeTestWireUpdate(at, inputs_.at(name), TestValue(value)));
        now_ = at + wait;
        sim_.advanceAndRecord(now_);
    }
    LogicVector out(const char* pin) const { return root_->getOutputPinDynamic(pin)->getValueAsVector(); }
    // A backing request taken at the coming edge.
    void countLower() {
        if (out("LOWER_REQ_VALID") == logicBit(true) && out("LOWER_REQ_READY") == logicBit(true)) {
            if (out("LOWER_REQ_OP") == logicBits(3, 0) || out("LOWER_REQ_OP") == logicBits(3, 4)) ++lower_reads_;
            else if (out("LOWER_REQ_OP") == logicBits(3, 1)) ++lower_writes_;
        }
    }
    void cycle() {
        drive({{"CLK", 0}});
        drive({{"CLK", 1}});
    }
    unsigned lowerReads() const { return lower_reads_; }
    unsigned lowerWrites() const { return lower_writes_; }
    std::vector<uint8_t> backingBytes(uint32_t address, size_t count) const {
        return root_->backingBytes(address, count);
    }
    void read(uint32_t address, uint32_t expected, unsigned expected_reads) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 0}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}, {"RSP_READY", 0}});
        for (size_t i = 0; out("REQ_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("REQ_READY") == logicBit(true), context_ + ": no request capacity");
        drive({{"CLK", 0}, {"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0xffffffff}});
        for (size_t i = 0; out("RSP_VALID") != logicBit(true) && i < 100; ++i) cycle();
        require(out("RSP_VALID") == logicBit(true), context_ + ": response deadline exceeded at " + std::to_string(address));
        require(out("RSP_STATUS") == logicBits(3, 0), context_ + ": nonzero read status at " + std::to_string(address));
        require(out("RSP_READ_DATA") == logicBits(Width, expected),
                context_ + ": wrong data at " + std::to_string(address) +
                " got " + formatLogicVector(out("RSP_READ_DATA")));
        require(lower_reads_ == expected_reads, context_ + ": wrong lower read count at " + std::to_string(address) +
                ": got " + std::to_string(lower_reads_) + ", expected " + std::to_string(expected_reads));
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    }
    // A burst read (op 4): the words of the line from `address` on,
    // wrapping. The consumer holds back every `stall`-th word for a cycle.
    void burst(uint32_t address, size_t words_per_line, unsigned expected_reads, size_t stall = 0) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 4}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}, {"RSP_READY", 0}});
        for (size_t i = 0; out("REQ_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("REQ_READY") == logicBit(true), context_ + ": no burst request capacity");
        drive({{"CLK", 0}, {"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0xffffffff}, {"REQ_OP", 0}});
        const uint32_t bytes = uint32_t(Width / 8), line = uint32_t(words_per_line) * bytes;
        for (size_t k = 0; k < words_per_line; ++k) {
            for (size_t i = 0; out("RSP_VALID") != logicBit(true) && i < 100; ++i) cycle();
            const auto word = (address & ~(line - 1)) | ((address + uint32_t(k) * bytes) & (line - 1));
            const auto where = context_ + ": burst from " + std::to_string(address) + " word " + std::to_string(k);
            require(out("RSP_VALID") == logicBit(true), where + ": response deadline exceeded");
            require(out("RSP_STATUS") == logicBits(3, 0), where + ": nonzero status");
            require(out("RSP_READ_DATA") == logicBits(Width, imageWord<Width>(word)),
                    where + ": got " + formatLogicVector(out("RSP_READ_DATA")));
            if (stall && k % stall == 0) {
                cycle();
                require(out("RSP_VALID") == logicBit(true) && out("RSP_READ_DATA") == logicBits(Width, imageWord<Width>(word)),
                        where + ": response changed under backpressure");
            }
            drive({{"CLK", 0}, {"RSP_READY", 1}});
            drive({{"CLK", 1}});
            drive({{"CLK", 0}, {"RSP_READY", 0}});
        }
        for (size_t i = 0; i < 3; ++i) cycle();
        require(out("RSP_VALID") != logicBit(true), context_ + ": burst gave more words than its line");
        require(lower_reads_ == expected_reads, context_ + ": wrong lower read count after burst from " +
                std::to_string(address) + ": got " + std::to_string(lower_reads_) + ", expected " + std::to_string(expected_reads));
    }
    void write(uint32_t address, uint32_t data, unsigned mask, Status expected_status,
               unsigned expected_reads, unsigned expected_writes) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 1}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", data}, {"REQ_WRITE_MASK", mask}, {"RSP_READY", 0}});
        for (size_t i = 0; out("REQ_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("REQ_READY") == logicBit(true), context_ + ": no write request capacity");
        drive({{"CLK", 0}, {"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0xffffffff},
               {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}});
        for (size_t i = 0; out("RSP_VALID") != logicBit(true) && i < 100; ++i) cycle();
        require(out("RSP_VALID") == logicBit(true), context_ + ": write response deadline exceeded");
        require(out("RSP_STATUS") == logicBits(3, unsigned(expected_status)),
                context_ + ": wrong write status at " + std::to_string(address) +
                " got " + formatLogicVector(out("RSP_STATUS")));
        require(out("RSP_READ_DATA") == logicBits(Width, 0), context_ + ": write returned stale read data");
        require(lower_reads_ == expected_reads && lower_writes_ == expected_writes,
                context_ + ": wrong lower traffic after write at " + std::to_string(address) +
                " reads=" + std::to_string(lower_reads_) + " writes=" + std::to_string(lower_writes_));
        const auto held_status = out("RSP_STATUS");
        cycle();
        require(out("RSP_VALID") == logicBit(true) && out("RSP_STATUS") == held_status,
                context_ + ": write response changed under backpressure");
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    }
    // A buffered write: answered OK within a few cycles of being taken,
    // whatever the backing memory is doing; its lower write comes later.
    void postedWrite(uint32_t address, uint32_t data, unsigned mask) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 1}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", data}, {"REQ_WRITE_MASK", mask}, {"RSP_READY", 0}});
        for (size_t i = 0; out("REQ_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("REQ_READY") == logicBit(true), context_ + ": no buffered-write capacity");
        drive({{"CLK", 0}, {"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0xffffffff}, {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}});
        size_t waited = 0;
        for (; out("RSP_VALID") != logicBit(true) && waited < 100; ++waited) cycle();
        require(out("RSP_VALID") == logicBit(true) && waited <= 2,
                context_ + ": buffered write not answered promptly at " + std::to_string(address));
        require(out("RSP_STATUS") == logicBits(3, 0), context_ + ": buffered write status not OK");
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    }
    void idle(size_t cycles) { for (size_t i = 0; i < cycles; ++i) cycle(); }
    // A write whose lower traffic is checked by the caller's model: answered OK.
    void writeOk(uint32_t address, uint32_t data, unsigned mask) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 1}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", data}, {"REQ_WRITE_MASK", mask}, {"RSP_READY", 0}});
        for (size_t i = 0; out("REQ_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("REQ_READY") == logicBit(true), context_ + ": no write capacity");
        drive({{"CLK", 0}, {"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0xffffffff}, {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}});
        for (size_t i = 0; out("RSP_VALID") != logicBit(true) && i < 200; ++i) cycle();
        require(out("RSP_VALID") == logicBit(true) && out("RSP_STATUS") == logicBits(3, 0),
                context_ + ": write not answered OK at " + std::to_string(address));
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    }
    void reject(uint32_t address, unsigned op, Status expected_status) {
        const auto before_reads = lower_reads_, before_writes = lower_writes_;
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", op}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}, {"RSP_READY", 0}});
        for (size_t i = 0; out("REQ_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("REQ_READY") == logicBit(true), context_ + ": no rejected-request capacity");
        drive({{"CLK", 0}, {"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0}});
        for (size_t i = 0; out("RSP_VALID") != logicBit(true) && i < 100; ++i) cycle();
        require(out("RSP_VALID") == logicBit(true) &&
                out("RSP_STATUS") == logicBits(3, unsigned(expected_status)),
                context_ + ": wrong rejected-request status at " + std::to_string(address));
        require(lower_reads_ == before_reads && lower_writes_ == before_writes,
                context_ + ": rejected request accessed backing");
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    }
    void readFailure(uint32_t address, Status expected_status, unsigned expected_reads) {
        drive({{"CLK", 0}, {"REQ_VALID", 1}, {"REQ_OP", 0}, {"REQ_ADDR", address},
               {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}, {"RSP_READY", 0}});
        require(out("REQ_READY") == logicBit(true), context_ + ": no fault-test request capacity");
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}});
        for (size_t i = 0; out("RSP_VALID") != logicBit(true) && i < 100; ++i) cycle();
        require(out("RSP_VALID") == logicBit(true) &&
                out("RSP_STATUS") == logicBits(3, unsigned(expected_status)),
                context_ + ": incorrect backing fault status");
        require(out("RSP_READ_DATA") == logicBits(Width, 0),
                context_ + ": failed read returned partial fill data");
        require(lower_reads_ == expected_reads,
                context_ + ": failed fill lower traffic differs");
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    }
    void invalidate() {
        // A write-back cache first walks its lines and writes the dirty ones back.
        drive({{"CLK", 0}, {"INV_VALID", 1}});
        for (size_t i = 0; out("INV_READY") != logicBit(true) && i < 5000; ++i) cycle();
        require(out("INV_READY") == logicBit(true), context_ + ": no invalidation capacity");
        drive({{"CLK", 0}, {"INV_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"INV_VALID", 0}});
    }
    void resetDuringMiss() {
        drive({{"CLK", 0}, {"REQ_VALID", 1}, {"REQ_OP", 0}, {"REQ_ADDR", 0},
               {"REQ_WRITE_DATA", 0}, {"REQ_WRITE_MASK", 0}, {"RSP_READY", 0}});
        require(out("REQ_READY") == logicBit(true), context_ + ": no reset-test request capacity");
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}});
        for (size_t i = 0; lower_reads_ == 0 && i < 40; ++i) cycle();
        require(lower_reads_ == 1, context_ + ": reset test never reached backing");
        drive({{"CLK", 0}, {"RST", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RST", 0}});
        require(out("RSP_VALID") != logicBit(true), context_ + ": reset retained abandoned response");
        read(0, imageWord<Width>(0), 2);
    }
private:
    std::shared_ptr<CacheFixture<Width>> root_;
    Simulator sim_;
    size_t now_ = 100;
    std::map<std::string, std::shared_ptr<WireBase>> inputs_;
    std::string context_;
    unsigned lower_reads_ = 0;
    unsigned lower_writes_ = 0;
    bool clock_high_ = false;
};

template<size_t Width>
uint32_t imageWord(uint32_t address) {
    uint32_t value = 0;
    for (size_t byte = 0; byte < Width / 8; ++byte)
        value |= uint32_t(uint8_t(0xa0 + address + byte)) << (8 * byte);
    return value;
}

template<size_t Width>
void exerciseLines(size_t sets, size_t words_per_line) {
    CacheConfig config;
    config.sets = sets;
    config.block_bytes = words_per_line * (Width / 8);
    config.backing_depth_words = 32;
    Rig<Width> rig(config);
    const auto last_word = uint32_t((words_per_line - 1) * (Width / 8));
    const auto conflict = uint32_t(sets * config.block_bytes);
    rig.read(last_word, imageWord<Width>(last_word), unsigned(words_per_line));
    rig.read(0, imageWord<Width>(0), unsigned(words_per_line));
    rig.read(last_word, imageWord<Width>(last_word), unsigned(words_per_line));
    rig.read(conflict, imageWord<Width>(conflict), unsigned(2 * words_per_line));
    rig.read(0, imageWord<Width>(0), unsigned(3 * words_per_line));
}

template<size_t Width>
void exerciseWays(size_t sets, size_t words_per_line) {
    CacheConfig config;
    config.sets = sets;
    config.ways = 2;
    config.block_bytes = words_per_line * (Width / 8);
    config.backing_depth_words = 64;
    Rig<Width> rig(config);
    const auto b = uint32_t(sets * config.block_bytes);
    const auto c = uint32_t(2 * sets * config.block_bytes);
    const auto reads = unsigned(words_per_line);
    rig.read(0, imageWord<Width>(0), reads);
    rig.read(b, imageWord<Width>(b), 2 * reads);
    rig.read(0, imageWord<Width>(0), 2 * reads);
    rig.read(c, imageWord<Width>(c), 3 * reads);
    rig.read(0, imageWord<Width>(0), 3 * reads);
    rig.read(b, imageWord<Width>(b), 4 * reads);
}

// Burst reads (op 4) and burst fills: a burst miss forwards the fill, a burst
// hit streams the line from the data bank; a word fill and a burst fill bring
// the same line.
template<size_t Width>
void exerciseBursts(size_t words_per_line, bool burst_fill, size_t ways, Technology data) {
    CacheConfig config;
    config.sets = 2;
    config.ways = ways;
    config.block_bytes = words_per_line * (Width / 8);
    config.backing_depth_words = 64;
    config.burst_fill = burst_fill;
    config.data_technology = data;
    Rig<Width> rig(config);
    const auto per_miss = unsigned(burst_fill ? 1 : words_per_line);
    const auto bytes = uint32_t(Width / 8), line = uint32_t(config.block_bytes);
    const auto last = line - bytes, conflict = uint32_t(config.sets) * line;
    rig.burst(last, words_per_line, per_miss);                 // miss, forwarded
    rig.burst(0, words_per_line, per_miss, 2);                 // hit, streamed with backpressure
    rig.read(bytes % line, imageWord<Width>(bytes % line), per_miss);
    rig.burst(conflict + bytes % line, words_per_line, 2 * per_miss, 1);
    rig.read(conflict, imageWord<Width>(conflict), 2 * per_miss);
    const auto expected = ways == 2 ? 2 * per_miss : 3 * per_miss;
    rig.burst(line - bytes, words_per_line, expected);         // a way-0 hit, or a refill
    rig.read(line, imageWord<Width>(line), expected + per_miss);  // the other set
}

template<size_t Width>
void exerciseReferenceStream() {
    constexpr size_t sets = 4, ways = 2, words_per_line = 2;
    constexpr size_t word_bytes = Width / 8, block_bytes = words_per_line * word_bytes;
    CacheConfig config;
    config.sets = sets;
    config.ways = ways;
    config.block_bytes = block_bytes;
    config.backing_depth_words = 64;
    config.read_write = true;
    Rig<Width> rig(config, Technology::FF);
    struct Line {
        bool valid = false;
        size_t tag = 0, age = 0;
        std::vector<uint8_t> bytes = std::vector<uint8_t>(block_bytes);
    };
    std::vector<std::vector<Line>> lines(sets, std::vector<Line>(ways));
    std::vector<uint8_t> backing(config.backing_depth_words * word_bytes);
    for (size_t i = 0; i < backing.size(); ++i) backing[i] = uint8_t(0xa0 + i);
    size_t age = 0;
    unsigned reads = 0, writes = 0;
    auto find = [&](size_t address) -> Line* {
        const auto set = (address / block_bytes) % sets;
        const auto tag = (address / block_bytes) / sets;
        for (auto& line : lines[set])
            if (line.valid && line.tag == tag) return &line;
        return nullptr;
    };
    auto read = [&](size_t address) {
        auto* line = find(address);
        if (!line) {
            const auto set = (address / block_bytes) % sets;
            const auto base = address - address % block_bytes;
            line = &lines[set][0];
            for (auto& candidate : lines[set]) {
                if (!candidate.valid) { line = &candidate; break; }
                if (line->valid && candidate.age < line->age) line = &candidate;
            }
            line->valid = true;
            line->tag = (address / block_bytes) / sets;
            for (size_t i = 0; i < block_bytes; ++i) line->bytes[i] = backing[base + i];
            reads += words_per_line;
        }
        line->age = ++age;
        uint32_t value = 0;
        for (size_t i = 0; i < word_bytes; ++i)
            value |= uint32_t(line->bytes[address % block_bytes + i]) << (8 * i);
        rig.read(uint32_t(address), value, reads);
    };
    auto write = [&](size_t address, uint32_t data, unsigned mask) {
        ++writes;
        for (size_t i = 0; i < word_bytes; ++i) {
            if (!(mask & (1u << i))) continue;
            const auto value = uint8_t(data >> (8 * i));
            backing[address + i] = value;
            if (auto* line = find(address)) line->bytes[address % block_bytes + i] = value;
        }
        if (auto* line = find(address)) line->age = ++age;
        rig.write(uint32_t(address), data, mask, Status::Ok, reads, writes);
    };
    read(0);
    read(sets * block_bytes);
    read(0);
    read(2 * sets * block_bytes);
    read(0);
    uint32_t random = 0x12c0ffee;
    for (size_t i = 0; i < 20; ++i) {
        random = 1664525u * random + 1013904223u;
        const auto address = size_t((random >> 16) % 20) * word_bytes;
        if (i % 3 == 0) write(address, random ^ 0x81bad042u,
                              unsigned((random >> 8) & ((1u << word_bytes) - 1)));
        else read(address);
    }
    rig.invalidate();
    for (auto& set : lines) for (auto& line : set) line.valid = false;
    read(0);
    require(rig.backingBytes(0, backing.size()) == backing,
            "Reference stream final backing bytes differ");
}

// The write buffer: writes are answered at once and reach the backing memory
// later, in order; a read always sees the latest write (a hit reads the
// updated line, a miss waits for the queue to drain first).
template<size_t Width>
void exerciseWriteBuffer(size_t entries, size_t ways, Technology data, bool store = false) {
    constexpr size_t sets = 4, words_per_line = 2;
    constexpr size_t word_bytes = Width / 8, block_bytes = words_per_line * word_bytes;
    CacheConfig config;
    config.sets = sets;
    config.ways = ways;
    config.block_bytes = block_bytes;
    config.backing_depth_words = 64;
    config.read_write = true;
    config.write_buffer = entries;
    config.data_technology = data;
    config.line_store = store;
    Rig<Width> rig(config, Technology::FF);
    struct Line { bool valid = false; size_t tag = 0, age = 0; std::vector<uint8_t> bytes = std::vector<uint8_t>(block_bytes); };
    std::vector<std::vector<Line>> lines(sets, std::vector<Line>(ways));
    std::vector<uint8_t> backing(config.backing_depth_words * word_bytes);
    for (size_t i = 0; i < backing.size(); ++i) backing[i] = uint8_t(0xa0 + i);
    size_t age = 0;
    unsigned reads = 0, writes = 0;
    auto find = [&](size_t address) -> Line* {
        const auto set = (address / block_bytes) % sets, tag = (address / block_bytes) / sets;
        for (auto& line : lines[set]) if (line.valid && line.tag == tag) return &line;
        return nullptr;
    };
    auto read = [&](size_t address) {
        auto* line = find(address);
        if (!line) {
            const auto set = (address / block_bytes) % sets, base = address - address % block_bytes;
            line = &lines[set][0];
            for (auto& candidate : lines[set]) {
                if (!candidate.valid) { line = &candidate; break; }
                if (line->valid && candidate.age < line->age) line = &candidate;
            }
            line->valid = true;
            line->tag = (address / block_bytes) / sets;
            for (size_t i = 0; i < block_bytes; ++i) line->bytes[i] = backing[base + i];
            reads += words_per_line;
        }
        line->age = ++age;
        uint32_t value = 0;
        for (size_t i = 0; i < word_bytes; ++i) value |= uint32_t(line->bytes[address % block_bytes + i]) << (8 * i);
        rig.read(uint32_t(address), value, reads);
    };
    auto write = [&](size_t address, uint32_t value, unsigned mask) {
        ++writes;
        auto* line = find(address);
        for (size_t i = 0; i < word_bytes; ++i) {
            if (!(mask & (1u << i))) continue;
            backing[address + i] = uint8_t(value >> (8 * i));
            if (line) line->bytes[address % block_bytes + i] = uint8_t(value >> (8 * i));
        }
        if (line) line->age = ++age;
        rig.postedWrite(uint32_t(address), value, mask);
    };
    const auto all = (1u << word_bytes) - 1;
    write(0, 0x11223344u, all);                         // not cached: queued only
    read(0);                                            // the miss drains the queue first
    write(0, 0x55667788u, all);                         // cached: the line and the queue
    read(0);                                            // a hit, no backing read
    for (size_t i = 0; i < entries + 2; ++i)            // more writes than slots
        write((i % 3) * word_bytes + sets * block_bytes, 0x01010101u * uint32_t(i + 1), all);
    read(sets * block_bytes);
    uint32_t random = 0x5eed1234;
    for (size_t i = 0; i < 24; ++i) {
        random = 1664525u * random + 1013904223u;
        const auto address = size_t((random >> 16) % 24) * word_bytes;
        if (i % 2 == 0) write(address, random ^ 0x0badf00du, unsigned((random >> 8) & all));
        else read(address);
    }
    rig.idle(40);
    require(rig.lowerWrites() == writes, "Write buffer: each write must reach the backing memory once");
    require(rig.backingBytes(0, backing.size()) == backing, "Write buffer: final backing bytes differ");
}

// Write-back and write-allocate against a reference model: read values and
// fills, then (after a flush by invalidation) the backing bytes and the number
// of backing writes.
template<size_t Width>
void exerciseWritePolicy(bool back, bool allocate, size_t ways, Technology data, bool store = false, size_t sets = 4) {
    constexpr size_t words_per_line = 2;
    constexpr size_t word_bytes = Width / 8, block_bytes = words_per_line * word_bytes;
    CacheConfig config;
    config.sets = sets;
    config.ways = ways;
    config.block_bytes = block_bytes;
    config.backing_depth_words = std::max<size_t>(64, 4 * sets * words_per_line);
    config.line_store = store;
    config.scalable_storage = sets > 4;
    config.read_write = true;
    config.write_back = back;
    config.write_allocate = allocate;
    config.data_technology = data;
    Rig<Width> rig(config, Technology::FF);
    struct Line { bool valid = false, dirty = false; size_t tag = 0, age = 0; std::vector<uint8_t> bytes = std::vector<uint8_t>(block_bytes); };
    std::vector<std::vector<Line>> lines(sets, std::vector<Line>(ways));
    std::vector<uint8_t> backing(config.backing_depth_words * word_bytes);
    for (size_t i = 0; i < backing.size(); ++i) backing[i] = uint8_t(0xa0 + i);
    size_t age = 0;
    unsigned reads = 0, writes = 0;
    auto find = [&](size_t address) -> Line* {
        const auto set = (address / block_bytes) % sets, tag = (address / block_bytes) / sets;
        for (auto& line : lines[set]) if (line.valid && line.tag == tag) return &line;
        return nullptr;
    };
    auto write_back = [&](Line& line, size_t set) {
        const auto base = (line.tag * sets + set) * block_bytes;
        for (size_t i = 0; i < block_bytes; ++i) backing[base + i] = line.bytes[i];
        writes += words_per_line;
        line.dirty = false;
    };
    auto fill = [&](size_t address) -> Line& {
        const auto set = (address / block_bytes) % sets, base = address - address % block_bytes;
        Line* line = &lines[set][0];
        for (auto& candidate : lines[set]) {
            if (!candidate.valid) { line = &candidate; break; }
            if (line->valid && candidate.age < line->age) line = &candidate;
        }
        if (line->valid && line->dirty) write_back(*line, set);
        line->valid = true;
        line->dirty = false;
        line->tag = (address / block_bytes) / sets;
        for (size_t i = 0; i < block_bytes; ++i) line->bytes[i] = backing[base + i];
        reads += words_per_line;
        line->age = ++age;
        return *line;
    };
    auto read = [&](size_t address) {
        auto* line = find(address);
        if (!line) line = &fill(address);
        else line->age = ++age;
        uint32_t value = 0;
        for (size_t i = 0; i < word_bytes; ++i) value |= uint32_t(line->bytes[address % block_bytes + i]) << (8 * i);
        rig.read(uint32_t(address), value, reads);
    };
    auto write = [&](size_t address, uint32_t value, unsigned mask) {
        auto* line = find(address);
        if (!line && allocate) line = &fill(address);
        for (size_t i = 0; i < word_bytes; ++i) {
            if (!(mask & (1u << i))) continue;
            if (line) line->bytes[address % block_bytes + i] = uint8_t(value >> (8 * i));
            if (!back || !line) backing[address + i] = uint8_t(value >> (8 * i));
        }
        if (line) { line->age = ++age; if (back) line->dirty = true; }
        if (!back || !line) ++writes;
        rig.writeOk(uint32_t(address), value, mask);
    };
    const auto all = (1u << word_bytes) - 1;
    const auto conflict = sets * block_bytes;
    read(0);
    write(0, 0x11223344u, all);                 // a hit
    write(word_bytes, 0x55667788u, all);        // the same line
    read(conflict);                             // another line of set 0
    read(2 * conflict);                         // evicts (two ways: the older line)
    read(0);
    write(3 * conflict, 0x01020304u, all);      // a write miss
    read(3 * conflict);
    uint32_t random = 0xcafe0042;
    for (size_t i = 0; i < 30; ++i) {
        random = 1664525u * random + 1013904223u;
        const auto address = size_t((random >> 16) % 40) * word_bytes;
        if (i % 2 == 0) write(address, random ^ 0x5a5a1234u, unsigned((random >> 8) & all));
        else read(address);
    }
    rig.invalidate();
    for (size_t set = 0; set < sets; ++set)
        for (auto& line : lines[set]) { if (line.valid && line.dirty) write_back(line, set); line.valid = false; }
    require(rig.backingBytes(0, backing.size()) == backing, "Write policy: final backing bytes differ");
    require(rig.lowerWrites() == writes, "Write policy: wrong number of backing writes: got " +
            std::to_string(rig.lowerWrites()) + ", expected " + std::to_string(writes));
    read(0);
}
} // namespace

namespace {
circuit::BuildProfile cachePublicScenarioProfile() {
    return circuit::withProfileOverrides(circuit::canonicalDefaultProfile(),
            {circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("memory.ff.endpoint.8"),
                "compact cache data bank in the public scenario"),
             circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("memory.ff.endpoint.32"),
                "compact cache tag bank in the public scenario"),
             circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("storage.state-bit"),
                "compact flip-flop cells in the public scenario"),
             circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("storage.register.1"),
                "compact one-bit registers in the public scenario"),
             circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("storage.register.3"),
                "compact status registers in the public scenario"),
             circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("storage.register.8"),
                "compact data registers in the public scenario"),
             circuit::preferFidelity(circuit::Fidelity::Behavioral,
                circuit::ProfileSelector::contract("storage.register.32"),
                "compact address and tag registers in the public scenario")},
            "cache-structural-control");
}
} // namespace

CacheComponentScenarioTest::CacheComponentScenarioTest()
    : ComponentScenarioTest(cacheScenarioSpec(), "cache-operations",
        cachePublicScenarioProfile()) {}

CacheDirectMappedScenarioTest::CacheDirectMappedScenarioTest()
    : ComponentScenarioTest(directMappedCacheScenarioSpec(), "operations",
        cachePublicScenarioProfile()) {}

CacheScalableScenarioTest::CacheScalableScenarioTest()
    : ComponentScenarioTest(scalableCacheScenarioSpec(), "operations",
        cachePublicScenarioProfile()) {}

void CacheTwoLevelScenarioTest::setupCircuit() {
    CacheConfig upper;
    upper.sets = 1;
    upper.ways = 1;
    upper.block_bytes = 1;
    upper.backing_depth_words = 16;
    upper.read_write = true;
    CacheConfig lower;
    lower.sets = 2;
    lower.ways = 2;
    lower.block_bytes = 2;
    lower.backing_depth_words = 16;
    lower.read_write = true;

    auto profile = std::make_shared<const circuit::BuildProfile>(
        cachePublicScenarioProfile());
    auto manifest = std::make_shared<circuit::BuildManifest>(
        profile->name(), profile->fingerprint());
    auto context = circuit::BuildContext::rootScope(
        circuit::builtinComponentCatalog(), profile, manifest)
        ->child("CACHE_HIERARCHY", std::nullopt);
    auto fixture = Component::createWithContext<CacheFixture<8>>(
        context, "CACHE_HIERARCHY", upper, Technology::FF, 0, lower);
    fixture->initializeBacking();
    root = fixture;
    sim = std::make_shared<Simulator>();
    checkpoints_.clear();
    prepared_ = false;

    ComponentBuilder wiring(fixture);
    inputs_.clear();
    for (const auto& [name, pin] : fixture->getAllInputPins())
        inputs_[name] = wiring.addNewWireDynamic(
            "IN_" + name, pin->getWidth(), nullptr, {pin});
    for (const auto& [name, pin] : fixture->getAllOutputPins())
        wiring.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
}

void CacheTwoLevelScenarioTest::preparePrecomputedSimulation() {
    if (prepared_) return;
    auto fixture = std::dynamic_pointer_cast<CacheFixture<8>>(root);
    require(fixture != nullptr && sim != nullptr,
            "Two-level scenario must be set up before playback");
    // Times are written in gate delays: one inverter is 1 tick in `unit`
    // and one FO4 in a realistic profile, so every profile settles alike.
    const size_t gate = circuit::timing::leafDelay(circuit::timing::Leaf::Not);
    size_t now = 100 * gate;
    for (const auto& [name, wire] : inputs_)
        sim->scheduleEvent(makeTestWireUpdate(0, wire,
            TestValue(uint64_t(name == "RST"))));
    scheduleInitialEventsForTree(fixture, *sim);
    sim->advanceAndRecord(now);
    auto drive = [&](const std::map<std::string, uint64_t>& values) {
        const auto at = now + 1;
        for (const auto& [name, value] : values)
            sim->scheduleEvent(makeTestWireUpdate(at, inputs_.at(name), TestValue(value)));
        now = at + 200 * gate;
        sim->advanceAndRecord(now);
    };
    auto output = [&](const char* pin) {
        return fixture->getOutputPinDynamic(pin)->getValueAsVector();
    };
    auto mark = [&](std::string name, std::string explanation) {
        checkpoints_.push_back({now, std::move(name), std::move(explanation),
            checkpoints_.size()});
    };
    unsigned ram_reads = 0, ram_writes = 0;
    auto cycle = [&] {
        drive({{"CLK", 0}});
        if (output("LOWER_REQ_VALID") == logicBit(true) &&
            output("LOWER_REQ_READY") == logicBit(true)) {
            if (output("LOWER_REQ_OP") == logicBits(3, 0)) ++ram_reads;
            else if (output("LOWER_REQ_OP") == logicBits(3, 1)) ++ram_writes;
        }
        drive({{"CLK", 1}});
    };
    auto awaitReady = [&] {
        for (size_t i = 0; output("REQ_READY") != logicBit(true) && i < 200; ++i)
            cycle();
        require(output("REQ_READY") == logicBit(true),
                "Two-level scenario request was not accepted");
    };
    auto awaitResponse = [&] {
        for (size_t i = 0; output("RSP_VALID") != logicBit(true) && i < 200; ++i)
            cycle();
        require(output("RSP_VALID") == logicBit(true),
                "Two-level scenario response deadline exceeded");
        require(output("RSP_STATUS") == logicBits(3, unsigned(Status::Ok)),
                "Two-level scenario returned a memory error");
    };
    auto consume = [&] {
        drive({{"CLK", 0}, {"RSP_READY", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"RSP_READY", 0}});
    };
    auto read = [&](const std::string& label, uint32_t address,
                    uint8_t expected, unsigned expected_ram_reads,
                    std::string detail) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 0},
            {"REQ_ADDR", address}, {"REQ_WRITE_DATA", 0},
            {"REQ_WRITE_MASK", 0}, {"RSP_READY", 0}});
        awaitReady();
        drive({{"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_ADDR", 0xffffffff}});
        awaitResponse();
        require(output("RSP_READ_DATA") == logicBits(8, expected),
                "Two-level scenario returned wrong data at " + std::to_string(address));
        require(ram_reads == expected_ram_reads,
                "Two-level scenario RAM read count differs at " + std::to_string(address));
        mark(label, std::move(detail));
        consume();
    };
    auto write = [&](const std::string& label, uint32_t address,
                     uint8_t data, unsigned expected_ram_writes,
                     std::string detail) {
        drive({{"CLK", 0}, {"REQ_VALID", 0}, {"REQ_OP", 1},
            {"REQ_ADDR", address}, {"REQ_WRITE_DATA", data},
            {"REQ_WRITE_MASK", 1}, {"RSP_READY", 0}});
        awaitReady();
        drive({{"REQ_VALID", 1}});
        drive({{"CLK", 1}});
        drive({{"CLK", 0}, {"REQ_VALID", 0}});
        awaitResponse();
        require(ram_writes == expected_ram_writes,
                "Two-level scenario RAM write count differs");
        require(fixture->backingBytes(address, 1) == std::vector<uint8_t>{data},
                "Two-level scenario write did not reach backing RAM");
        mark(label, std::move(detail));
        consume();
    };

    drive({{"RST", 0}});
    mark("ready", "L1 and L2 are empty; FF backing RAM contains the test image.");
    read("cold-both-levels", 0, 0xa0, 2,
         "L1 and L2 miss. L2 fills its two-word line from RAM addresses 0 and 1.");
    read("l1-hit", 0, 0xa0, 2,
         "L1 answers the repeated read without a new RAM request.");
    read("l2-hit-neighbor", 1, 0xa1, 2,
         "L1 misses its one-word line; L2 already holds the neighboring word.");
    read("l2-hit-first", 0, 0xa0, 2,
         "L1 misses again, while the same L2 line still supplies address 0.");
    read("second-l2-line", 4, 0xa4, 4,
         "A different L2 line fills from RAM addresses 4 and 5.");
    write("write-through", 0, 0x55, 1,
          "The write reaches RAM and updates any resident cached copies.");
    read("read-after-write", 0, 0x55, 4,
         "The updated value is visible through the cache path without a RAM read.");
    for (size_t i = 0; output("INV_READY") != logicBit(true) && i < 20; ++i)
        cycle();
    require(output("INV_READY") == logicBit(true),
            "Two-level scenario could not invalidate while idle");
    drive({{"CLK", 0}, {"INV_VALID", 1}});
    drive({{"CLK", 1}});
    drive({{"CLK", 0}, {"INV_VALID", 0}});
    mark("both-invalidated", "The fixture broadcasts invalidation to idle L1 and L2.");
    read("refill-after-invalidation", 0, 0x55, 6,
         "Both levels miss and L2 fetches the changed RAM value again.");
    prepared_ = true;
}

bool CacheTwoLevelScenarioTest::run() {
    try {
        setupCircuit();
        preparePrecomputedSimulation();
        std::cout << "Two-level cache visual scenario passed ("
                  << checkpoints_.size() << " checkpoints)\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "Two-level cache visual scenario failed: " << error.what() << '\n';
        return false;
    }
}

size_t CacheTwoLevelScenarioTest::getRunDuration() const {
    return sim ? sim->getCurrentTime() : 0;
}

std::vector<SimulationTest::SimulationCheckpoint>
CacheTwoLevelScenarioTest::getCheckpoints() const {
    return checkpoints_;
}

void CacheReadTest::verifyResults() {
    auto invalid = [](CacheConfig config, size_t width) {
        try { config.validate(width); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("Invalid cache geometry or technology was accepted");
    };
    exerciseWritePolicy<8>(false, true, 1, Technology::FF);    // write-through, write-allocate
    exerciseWritePolicy<8>(true, false, 1, Technology::FF);    // write-back, write-around
    exerciseWritePolicy<32>(true, true, 2, Technology::FF);    // write-back, write-allocate
    exerciseWritePolicy<32>(true, true, 2, Technology::SRAM);
    exerciseWritePolicy<16>(true, false, 2, Technology::SRAM);
    // The line-state store: the same streams, and a cache too large for flip-flop line state.
    exerciseWritePolicy<8>(false, false, 1, Technology::FF, true);
    exerciseWritePolicy<32>(true, true, 2, Technology::SRAM, true);
    exerciseWritePolicy<16>(true, false, 2, Technology::FF, true);
    exerciseWritePolicy<32>(true, true, 2, Technology::SRAM, true, 256);
    exerciseWriteBuffer<32>(2, 2, Technology::SRAM, true);
    exerciseWriteBuffer<8>(1, 1, Technology::FF);
    exerciseWriteBuffer<32>(2, 2, Technology::SRAM);
    exerciseWriteBuffer<16>(4, 2, Technology::FF);
    {
        CacheConfig config;
        config.write_buffer = 1; invalid(config, 8);   // read-only cache
        config.read_write = true; config.write_buffer = 3; invalid(config, 8);
        config.write_buffer = 2; config.write_back = true; invalid(config, 8);   // back has no buffer
        config.write_buffer = 0; config.read_write = false; invalid(config, 8);  // back needs read-write
        config = CacheConfig{}; config.scalable_storage = true; config.sets = 128; config.backing_depth_words = 1024;
        invalid(config, 8);  // registers: 64
        config.line_store = true; config.validate(8);
        config.sets = 8192; invalid(config, 8);
    }
    {
        CacheConfig config;
        config.sets = 0; invalid(config, 8);
        config.sets = 8; invalid(config, 8);
        config.scalable_storage = true;
        config.sets = 128; invalid(config, 8);
        config.sets = 4;
        config.ways = 3; invalid(config, 8);
        config.ways = 1;
        config.block_bytes = 3; invalid(config, 8);
        config.block_bytes = 4;
        config.backing_depth_words = 8; invalid(config, 8);
        config.backing_depth_words = 16;
        config.tag_technology = Technology::ROM; invalid(config, 8);
        config.tag_technology = Technology::FF;
        config.data_technology = Technology::DRAM; invalid(config, 8);
        config.data_technology = Technology::SRAM;
        config.validate(8);
        require(cacheConfig(cacheParameters(config)).scalable_storage,
                "Cache storage form did not round-trip through catalog parameters");
    }
    for (size_t sets : {1u, 2u, 4u}) {
        CacheConfig config;
        config.sets = sets;
        config.backing_depth_words = 16;
        Rig<8> rig(config);
        rig.read(0, 0xa0, 1);
        rig.read(0, 0xa0, 1);
        if (sets > 1) {
            rig.read(1, 0xa1, 2);
            rig.read(0, 0xa0, 2);
        }
        rig.read(uint32_t(sets), uint32_t(0xa0 + sets), sets > 1 ? 3 : 2);
        rig.read(0, 0xa0, sets > 1 ? 4 : 3);
        rig.invalidate();
        rig.read(0, 0xa0, sets > 1 ? 5 : 4);
    }
    for (unsigned swap = 0; swap < 2; ++swap) {
        CacheConfig config;
        config.sets = 4;
        if (swap == 0) config.tag_technology = Technology::SRAM;
        else config.data_technology = Technology::SRAM;
        Rig<8> rig(config);
        rig.read(0, 0xa0, 1);
        rig.read(0, 0xa0, 1);
        rig.read(4, 0xa4, 2);
        rig.read(0, 0xa0, 3);
    }
    for (size_t sets : {1u, 2u, 4u}) {
        CacheConfig config;
        config.sets = sets;
        config.backing_depth_words = 16;
        Rig<16> half(config);
        half.read(0, 0xa1a0, 1);
        half.read(0, 0xa1a0, 1);
        half.read(uint32_t(2 * sets), uint32_t(0xa1a0 + 0x0202 * sets), 2);
        half.read(0, 0xa1a0, 3);
        Rig<32> word(config);
        word.read(0, 0xa3a2a1a0, 1);
        word.read(0, 0xa3a2a1a0, 1);
        word.read(uint32_t(4 * sets), uint32_t(0xa3a2a1a0 + 0x04040404 * sets), 2);
        word.read(0, 0xa3a2a1a0, 3);
    }
    for (size_t sets : {1u, 2u, 4u}) {
        exerciseLines<8>(sets, 2);
        exerciseLines<16>(sets, 2);
        exerciseLines<32>(sets, 2);
    }
    for (size_t width : {8u, 16u, 32u}) {
        if (width == 8) exerciseLines<8>(4, 4);
        else if (width == 16) exerciseLines<16>(4, 4);
        else exerciseLines<32>(4, 4);
    }
    for (size_t sets : {1u, 4u}) {
        exerciseWays<8>(sets, 1);
        exerciseWays<16>(sets, 1);
        exerciseWays<32>(sets, 1);
    }
    exerciseWays<8>(4, 2);
    exerciseWays<32>(4, 2);
    for (bool burst_fill : {false, true})
        for (auto data : {Technology::FF, Technology::SRAM}) {
            exerciseBursts<8>(4, burst_fill, 1, data);
            exerciseBursts<32>(2, burst_fill, 2, data);
            exerciseBursts<16>(1, burst_fill, 2, data);
        }
    for (size_t sets : {1u, 4u}) {
        CacheConfig config;
        config.sets = sets;
        config.block_bytes = 4;
        config.backing_depth_words = 32;
        config.burst_fill = true;
        Rig<8> rig(config);
        rig.read(3, imageWord<8>(3), 1);
        rig.read(0, imageWord<8>(0), 1);
        rig.read(uint32_t(4 * sets + 2), imageWord<8>(uint32_t(4 * sets + 2)), 2);
        rig.read(1, imageWord<8>(1), 3);
    }
    {
        CacheConfig config;
        config.read_write = true;
        config.sets = 2;
        config.ways = 2;
        config.block_bytes = 2;
        config.backing_depth_words = 16;
        Rig<8> rig(config, Technology::FF);
        rig.read(0, 0xa0, 2);
        rig.write(0, 0x5a, 1, Status::Ok, 2, 1);
        rig.read(0, 0x5a, 2);
        rig.write(4, 0x7b, 1, Status::Ok, 2, 2);
        require(rig.backingBytes(0, 1) == std::vector<uint8_t>{0x5a} &&
                rig.backingBytes(4, 1) == std::vector<uint8_t>{0x7b},
                "Write-through backing bytes differ");
        rig.read(4, 0x7b, 4); // Write miss did not allocate the line.
        rig.write(4, 0x00, 0, Status::Ok, 4, 3);
        rig.read(4, 0x7b, 4);
    }
    {
        CacheConfig config;
        config.read_write = true;
        config.sets = 2;
        config.ways = 2;
        config.block_bytes = 4;
        config.backing_depth_words = 32;
        config.data_technology = Technology::SRAM;
        Rig<16> rig(config, Technology::FF);
        rig.read(0, 0xa1a0, 2);
        rig.write(0, 0x5a7b, 1, Status::Ok, 2, 1);
        rig.read(0, 0xa17b, 2);
        require(rig.backingBytes(0, 2) == (std::vector<uint8_t>{0x7b, 0xa1}),
                "Halfword masked write changed the wrong byte");
        rig.write(8, 0xcafe, 3, Status::Ok, 2, 2);
        rig.read(8, 0xcafe, 4);
    }
    {
        CacheConfig config;
        config.read_write = true;
        config.sets = 2;
        config.ways = 2;
        config.block_bytes = 8;
        config.backing_depth_words = 32;
        config.tag_technology = Technology::SRAM;
        Rig<32> rig(config, Technology::FF);
        rig.read(0, 0xa3a2a1a0, 2);
        rig.write(0, 0x11223344, 0b0101, Status::Ok, 2, 1);
        rig.read(0, 0xa322a144, 2);
        require(rig.backingBytes(0, 4) == (std::vector<uint8_t>{0x44, 0xa1, 0x22, 0xa3}),
                "Word masked write changed the wrong byte lanes");
    }
    {
        CacheConfig config;
        config.read_write = true;
        Rig<8> rig(config, Technology::ROM);
        rig.read(0, 0xa0, 1);
        rig.write(0, 0x55, 1, Status::ReadOnly, 1, 1);
        rig.read(0, 0xa0, 1);
    }
    {
        CacheConfig config;
        Rig<8> rig(config, Technology::ROM);
        rig.write(0, 0x55, 1, Status::Unsupported, 0, 0);
        rig.reject(16, 0, Status::Range);
        rig.reject(0, 2, Status::Unsupported);
        rig.read(0, 0xa0, 1);
    }
    {
        CacheConfig config;
        config.backing_depth_words = 16;
        Rig<32> rig(config);
        rig.reject(2, 0, Status::Alignment);
        rig.reject(64, 0, Status::Range);
        rig.read(0, 0xa3a2a1a0, 1);
    }
    {
        CacheConfig config;
        config.backing_depth_words = 16;
        Rig<32> rig(config);
        rig.resetDuringMiss();
    }
    for (auto technology : {Technology::SRAM, Technology::DRAM}) {
        CacheConfig config;
        config.sets = 2;
        config.block_bytes = 2;
        config.backing_depth_words = 16;
        Rig<8> rig(config, technology);
        rig.read(0, 0xa0, 2);
        rig.read(1, 0xa1, 2);
        rig.read(4, 0xa4, 4);
    }
    {
        CacheConfig config;
        config.sets = 1;
        config.block_bytes = 2;
        config.backing_depth_words = 16;
        // The intentionally shorter lower endpoint fails on fill word 1.
        Rig<8> rig(config, Technology::ROM, 1);
        rig.readFailure(0, Status::Range, 2);
        rig.readFailure(0, Status::Range, 4); // Partial line was not marked valid.
    }
    {
        CacheConfig config;
        config.scalable_storage = true;
        config.sets = 8;
        config.ways = 2;
        config.block_bytes = 8;
        config.backing_depth_words = 128;
        config.tag_technology = Technology::SRAM;
        Rig<32> rig(config);
        rig.read(4, 0xa7a6a5a4, 2);
        rig.read(0, 0xa3a2a1a0, 2);
        rig.read(64, 0xe3e2e1e0, 4);
        rig.read(4, 0xa7a6a5a4, 4);
    }
    {
        CacheConfig config;
        config.scalable_storage = true;
        config.sets = 16;
        config.ways = 2;
        config.block_bytes = 2;
        config.backing_depth_words = 128;
        config.read_write = true;
        config.data_technology = Technology::SRAM;
        Rig<8> rig(config, Technology::FF);
        rig.read(0, 0xa0, 2);
        rig.write(0, 0x5a, 1, Status::Ok, 2, 1);
        rig.read(0, 0x5a, 2);
        rig.read(32, 0xc0, 4);
    }
    {
        CacheConfig config;
        config.scalable_storage = true;
        config.sets = 8;
        config.block_bytes = 4;
        config.backing_depth_words = 64;
        config.read_write = true;
        Rig<16> rig(config, Technology::FF);
        rig.read(2, 0xa3a2, 2);
        rig.write(2, 0x5678, 2, Status::Ok, 2, 1);
        rig.read(2, 0x56a2, 2);
    }
    {
        CacheConfig config;
        config.scalable_storage = true;
        config.sets = 64;
        config.ways = 2;
        config.backing_depth_words = 128;
        Rig<8> rig(config);
        rig.read(0, 0xa0, 1);
        rig.read(64, 0xe0, 2);
        rig.read(0, 0xa0, 2);
        rig.invalidate();
        rig.read(0, 0xa0, 3);
    }
    {
        CacheConfig upper;
        upper.sets = 1;
        upper.block_bytes = 1;
        upper.backing_depth_words = 16;
        upper.read_write = true;
        CacheConfig lower;
        lower.sets = 2;
        lower.ways = 2;
        lower.block_bytes = 2;
        lower.backing_depth_words = 16;
        lower.read_write = true;
        Rig<8> rig(upper, Technology::FF, 0, lower);
        rig.read(0, 0xa0, 2); // Both levels miss; lower fills two words from RAM.
        rig.read(1, 0xa1, 2); // Upper misses; lower has the neighboring word.
        rig.read(4, 0xa4, 4); // A different line fills a second lower way.
        rig.read(0, 0xa0, 4); // Upper misses again; lower still hits.
        rig.write(0, 0x55, 1, Status::Ok, 4, 1);
        require(rig.backingBytes(0, 1) == std::vector<uint8_t>{0x55},
                "Stacked cache write did not reach backing RAM");
        rig.read(4, 0xa4, 4); // Evict the upper line without evicting lower.
        rig.read(0, 0x55, 4); // Lower cached copy reflects the write.
        rig.invalidate(); // Both levels are idle at this point.
        rig.read(0, 0x55, 6); // Both levels refill from the changed RAM image.
    }
    {
        CacheConfig upper;
        upper.sets = 1;
        upper.block_bytes = 4;
        upper.backing_depth_words = 16;
        upper.read_write = true;
        CacheConfig lower;
        lower.sets = 2;
        lower.ways = 2;
        lower.block_bytes = 8;
        lower.backing_depth_words = 16;
        lower.read_write = true;
        lower.scalable_storage = true;
        lower.tag_technology = Technology::SRAM;
        lower.data_technology = Technology::SRAM;
        Rig<32> rig(upper, Technology::DRAM, 0, lower);
        rig.read(0, 0xa3a2a1a0, 2); // DRAM fills the SRAM L2 line.
        rig.read(4, 0xa7a6a5a4, 2); // L1 miss, L2 hit; no DRAM request.
        rig.read(0, 0xa3a2a1a0, 2);
        rig.read(8, 0xabaaa9a8, 4); // Another L2 set needs two DRAM words.
        rig.write(0, 0x11223344, 0b0101, Status::Ok, 4, 1);
        require(rig.backingBytes(0, 4) == (std::vector<uint8_t>{0x44, 0xa1, 0x22, 0xa3}),
                "Mixed-technology stack changed the wrong DRAM bytes");
        rig.read(0, 0xa322a144, 4);
        rig.invalidate();
        rig.read(0, 0xa322a144, 6);
    }
    exerciseReferenceStream<8>();
    exerciseReferenceStream<32>();
}
