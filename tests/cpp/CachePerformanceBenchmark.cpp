// Cache plan Step 2: the same request traces through a DRAM endpoint directly
// (S0), through an SRAM L1 (S1) and through an SRAM L1 and L2 (S2). One
// request at a time, so the paths see identical traces; every response is
// checked against a reference memory, and the stores' final DRAM bytes too.
// Prints one benchmark RunRecord per trace phase and path (JSON lines).
//
// Geometry and memory are the campaign's frozen setup (progress log,
// 2026-10-17): L1 8 sets x 2 ways x 16-byte lines, L2 64 sets, burst fills,
// DRAM of 65,536 words with the profile's row and refresh, four-word bursts.
// The clock is the profile's certified core clock; `unit` uses 400 ticks
// (a cache needs more than 200 there). Parts are built with the CPU
// campaign's profile: compact banks, structural cache controllers.
#include "analysis/RV32IExperiment.hpp"
#include "benchmark/Benchmark.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "modules/memory/Cache.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/TestHelpers.hpp"
#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <cstdlib>

using namespace circuit;
using namespace circuit::memory;

namespace {
constexpr size_t Words = 65536;

uint32_t initialWord(uint32_t word) { return word * 2654435761u ^ 0x5a5a5a5au; }

class Path : public IOComponent {
public:
    Path(std::string name, std::optional<CacheConfig> l1, std::optional<CacheConfig> l2, EndpointConfig memory)
        : IOComponent(std::move(name), [](IOComponent* self) { protocolPins(self, 32); }),
          l1_(std::move(l1)), l2_(std::move(l2)), memory_config_(std::move(memory)) {}
    const char* getTypeName() const override { return "CachePerformancePath"; }
    void buildInternals(ComponentBuilder& b) override {
        circuit::detail::StructuralWiring w(b);
        memory_ = b.addNewComponent<BehavioralMemoryEndpoint>("DRAM", 32, Technology::DRAM, memory_config_);
        if (l1_) chain_.push_back(b.addNewComponent<Cache<32>>("L1", *l1_));
        if (l2_) chain_.push_back(b.addNewComponent<Cache<32>>("L2", *l2_));
        for (auto& c : chain_) w.connect(w.constant(c->getName() + "_NO_INVALIDATION", false), c->getInputPin("INV_VALID"));
        std::shared_ptr<IOComponent> top = chain_.empty() ? std::shared_ptr<IOComponent>(memory_) : chain_.front();
        for (const auto& [name, pin] : getAllInputPins()) w.connect(pin, top->getInputPinDynamic(name));
        for (const auto& [name, pin] : getAllOutputPins()) w.connect(top->getOutputPinDynamic(name), pin);
        for (size_t i = 0; i < chain_.size(); ++i) {
            std::shared_ptr<IOComponent> lower = i + 1 < chain_.size() ? chain_[i + 1] : std::shared_ptr<IOComponent>(memory_);
            for (const auto& [name, pin] : chain_[i]->getAllOutputPins())
                if (name.starts_with("MEM_")) w.connect(pin, lower->getInputPinDynamic(name.substr(4)));
            for (const auto& [name, pin] : chain_[i]->getAllInputPins())
                if (name.starts_with("MEM_")) w.connect(lower->getOutputPinDynamic(name.substr(4)), pin);
        }
        w.finish();
    }
    std::shared_ptr<BehavioralMemoryEndpoint> memory_;
    std::vector<std::shared_ptr<IOComponent>> chain_;
private:
    std::optional<CacheConfig> l1_, l2_;
    EndpointConfig memory_config_;
};

struct Access { bool write; uint32_t address; uint32_t data; unsigned mask; };
struct Phase { std::string name; std::vector<Access> accesses; };
struct Trace { std::string id, description; std::vector<Phase> phases; };

std::vector<Access> reads(uint32_t first_word, size_t count, size_t passes = 1) {
    std::vector<Access> out;
    for (size_t pass = 0; pass < passes; ++pass)
        for (size_t i = 0; i < count; ++i) out.push_back({false, uint32_t((first_word + i) * 4), 0, 0});
    return out;
}

// The same number of reads as the stream with poorer locality: one word per
// line (every cache access a miss, the DRAM row still open), and one word per
// DRAM row (every DRAM access a row change), wrapping within the memory.
std::vector<Access> strided(uint32_t first_word, size_t count, size_t stride_words) {
    std::vector<Access> out;
    const size_t rows = Words / stride_words;
    for (size_t i = 0; i < count; ++i) {
        const size_t word = first_word + (i % rows) * stride_words + (i / rows) * 4;
        out.push_back({false, uint32_t((word % Words) * 4), 0, 0});
    }
    return out;
}

std::vector<Trace> traces(size_t row_words) {
    std::vector<Trace> t;
    t.push_back({"small-region", "32 words (half the L1) read once cold, then twice warm",
        {{"cold", reads(0, 32)}, {"warm", reads(0, 32, 2)}}});
    t.push_back({"medium-region", "128 words (twice the L1, a quarter of the L2), two passes",
        {{"cold", reads(4096, 128)}, {"warm", reads(4096, 128)}}});
    t.push_back({"stream", "2048 successive words (four times the L2) read once", {{"cold", reads(8192, 2048)}}});
    t.push_back({"stride-line", "2048 reads, one word per 16-byte line", {{"cold", strided(16384, 2048, 4)}}});
    t.push_back({"stride-row", "2048 reads, one word per DRAM row (every read changes row)", {{"cold", strided(0, 2048, row_words)}}});
    // Masked stores in the small region: a hit after a read, a byte, a half
    // word, a write miss (no allocation), each read back.
    std::vector<Access> stores = reads(0, 8);
    stores.push_back({true, 0x04, 0x11223344, 0xf});
    stores.push_back({true, 0x08, 0x000000aa, 0x1});
    stores.push_back({true, 0x0c, 0xbbcc0000, 0xc});
    stores.push_back({true, 0x2000, 0xdeadbeef, 0xf});
    for (uint32_t a : {0x04u, 0x08u, 0x0cu, 0x2000u, 0x00u}) stores.push_back({false, a, 0, 0});
    t.push_back({"masked-stores", "masked stores and reads in the small region, one write miss", {{"mixed", stores}}});
    // Diagnosis only: reads of the byte addresses in CACHE_BENCH_ADDRESSES.
    if (const char* list = std::getenv("CACHE_BENCH_ADDRESSES")) {
        std::vector<Access> chosen;
        std::stringstream in(list);
        for (std::string item; std::getline(in, item, ',');) chosen.push_back({false, uint32_t(std::stoul(item, nullptr, 0)), 0, 0});
        t.push_back({"addresses", "chosen addresses", {{"reads", chosen}}});
    }
    return t;
}

struct Counts { size_t reads = 0, bursts = 0, writes = 0; };

struct Result {
    size_t cycles = 0, requests = 0, events = 0;
    std::map<std::string, Counts> boundary;
    double host_seconds = 0;
};

class Runner {
public:
    Runner(std::shared_ptr<Path> root, size_t period) : root_(std::move(root)), period_(period) {
        ComponentBuilder b(root_);
        for (const auto& [n, q] : root_->getAllInputPins()) in_[n] = b.addNewWireDynamic("IN_" + n, q->getWidth(), nullptr, {q});
        for (const auto& [n, q] : root_->getAllOutputPins()) b.addNewWireDynamic("OUT_" + n, q->getWidth(), q, {});
        sim_.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root_, sim_, 0);
        set(0, "CLK", 0); set(0, "RST", 1); set(0, "REQ_VALID", 0); set(0, "REQ_OP", 0); set(0, "REQ_ADDR", 0);
        set(0, "REQ_WRITE_DATA", 0); set(0, "REQ_WRITE_MASK", 0); set(0, "RSP_READY", 1);
        for (int i = 0; i < 6; ++i) edge();
        set(now_, "RST", 0); settle(); edge(); edge();
    }
    // One access, offered after the previous response: the cycles from the
    // edge that takes it to the edge that takes its response.
    size_t access(const Access& a, uint32_t& data, unsigned& status, Result& result) {
        set(now_, "REQ_OP", a.write ? 1 : 0); set(now_, "REQ_ADDR", a.address);
        set(now_, "REQ_WRITE_DATA", a.data); set(now_, "REQ_WRITE_MASK", a.write ? a.mask : 0); set(now_, "REQ_VALID", 1);
        settle();
        for (size_t i = 0; !high("REQ_READY"); ++i) { if (i > 100000) throw std::runtime_error("request never taken"); count(result); edge(); }
        count(result); edge();
        set(now_, "REQ_VALID", 0); settle();
        size_t cycles = 1;
        for (; !high("RSP_VALID"); ++cycles) { if (cycles > 100000) throw std::runtime_error("no response"); count(result); edge(); }
        data = uint32_t(root_->getOutputPinDynamic("RSP_READ_DATA")->getValueAsUInt64());
        status = unsigned(root_->getOutputPinDynamic("RSP_STATUS")->getValueAsUInt64());
        count(result); edge();
        return cycles;
    }
    size_t events() const { return sim_.getPerformanceCounters().processed_events; }
private:
    void set(size_t at, const std::string& n, uint64_t v) {
        sim_.scheduleEvent(makeTestWireUpdate(at, in_.at(n), TestValue(n == "CLK" || n == "RST" || n == "REQ_VALID" || n == "RSP_READY"
            ? TestValue(v ? LogicValue::HIGH : LogicValue::LOW) : TestValue(v))));
    }
    void settle() { sim_.advanceAndRecord(now_ + period_ / 2 - 1); }
    void edge() { set(now_ + period_ / 2, "CLK", 1); set(now_ + period_, "CLK", 0); now_ += period_; settle(); }
    bool high(const char* pin) const { return root_->getOutputPinDynamic(pin)->getValueAsVector()[0] == LogicValue::HIGH; }
    // Requests each boundary takes at the coming edge.
    void count(Result& result) {
        const auto bit = [](const std::shared_ptr<PinBase>& p) { return p && p->getBit(0) == LogicValue::HIGH; };
        const auto take = [&](const std::string& name, const std::shared_ptr<IOComponent>& part, const std::string& prefix, bool requester) {
            const auto valid = requester ? part->getOutputPinDynamic(prefix + "REQ_VALID") : part->getInputPinDynamic(prefix + "REQ_VALID");
            const auto ready = requester ? part->getInputPinDynamic(prefix + "REQ_READY") : part->getOutputPinDynamic(prefix + "REQ_READY");
            if (!bit(valid) || !bit(ready)) return;
            const auto op = (requester ? part->getOutputPinDynamic(prefix + "REQ_OP") : part->getInputPinDynamic(prefix + "REQ_OP"))->getValueAsUInt64();
            auto& c = result.boundary[name];
            (op == 1 ? c.writes : op == 4 ? c.bursts : c.reads)++;
        };
        take("DRAM", root_->memory_, "", false);
        for (const auto& c : root_->chain_) take(c->getName() + "_DOWN", c, "MEM_", true);
    }
    std::shared_ptr<Path> root_;
    size_t period_, now_ = 0;
    Simulator sim_;
    std::map<std::string, std::shared_ptr<WireBase>> in_;
};

size_t profilePeriod() {
    const auto& name = timing::activeTimingProfile().name;
    if (name == "embedded") return 4104;
    if (name == "desktop") return 2858;
    return 400;
}
} // namespace

int main(int argc, char** argv) {
    try {
        // Optional: a period, then a trace and a path to run alone.
        const size_t period = argc > 1 && std::string(argv[1]) != "-" ? std::stoul(argv[1]) : profilePeriod();
        const std::string only_trace = argc > 2 ? argv[2] : "", only_path = argc > 3 ? argv[3] : "";
        const auto& reference = timing::activeTimingProfile().memory;
        EndpointConfig memory;
        memory.depth_words = Words;
        memory.words_per_row = std::max<size_t>(16, reference.dram_row_words);
        memory.block_words = 4;
        memory.refresh_interval_cycles = reference.dram_refresh_interval ? std::max<size_t>(24, reference.dram_refresh_interval / period) : 0;
        memory.record_history = false;
        if (std::getenv("CACHE_BENCH_NO_REFRESH")) memory.refresh_interval_cycles = 0;  // diagnosis only
        auto l1 = analysis::campaignCache(8, true), l2 = analysis::campaignCache(64, true);
        // CACHE_BENCH_COMPACT_CONTROLLER: the compact cache controllers, which
        // must give the same cycles.
        if (std::getenv("CACHE_BENCH_COMPACT_CONTROLLER")) l1.compact_controller = l2.compact_controller = true;
        struct PathSpec { std::string id; std::optional<CacheConfig> l1, l2; };
        const std::vector<PathSpec> paths = {{"S0", std::nullopt, std::nullopt}, {"S1", l1, std::nullopt}, {"S2", l1, l2}};
        for (const auto& trace : traces(memory.words_per_row)) {
            if (!only_trace.empty() && trace.id != only_trace) continue;
            for (const auto& path : paths) {
                if (!only_path.empty() && path.id != only_path) continue;
                // The CPU campaign's profile: compact parts wherever one exists
                // (the cache banks); the cache controllers as the setting says.
                auto profile = std::make_shared<const BuildProfile>(strictAllBehavioral());
                auto manifest = std::make_shared<BuildManifest>(profile->name(), profile->fingerprint());
                auto context = BuildContext::rootScope(builtinComponentCatalog(), profile, manifest)->child("CACHE_BENCH", std::nullopt);
                // CACHE_BENCH_GATE_BANKS: the default profile instead, with
                // gate-level banks (a check of the lower-level form).
                auto root = std::getenv("CACHE_BENCH_GATE_BANKS")
                    ? Component::create<Path>("CACHE_BENCH", path.l1, path.l2, memory)
                    : Component::createWithContext<Path>(context, "CACHE_BENCH", path.l1, path.l2, memory);
                std::vector<uint8_t> image(Words * 4);
                for (uint32_t word = 0; word < Words; ++word)
                    for (unsigned byte = 0; byte < 4; ++byte) image[word * 4 + byte] = uint8_t(initialWord(word) >> (8 * byte));
                std::vector<uint8_t> expected = image;
                root->memory_->initializeBytes(0, image);
                Runner runner(root, period);
                for (const auto& phase : trace.phases) {
                    Result result;
                    const size_t events_before = runner.events();
                    const auto started = std::chrono::steady_clock::now();
                    for (const auto& a : phase.accesses) {
                        uint32_t data = 0; unsigned status = 0;
                        result.cycles += runner.access(a, data, status, result);
                        ++result.requests;
                        if (status != 0) throw std::runtime_error(trace.id + "/" + path.id + ": status " + std::to_string(status));
                        if (a.write) {
                            for (unsigned byte = 0; byte < 4; ++byte) if (a.mask >> byte & 1) expected[a.address + byte] = uint8_t(a.data >> (8 * byte));
                        } else {
                            uint32_t want = 0;
                            for (unsigned byte = 0; byte < 4; ++byte) want |= uint32_t(expected[a.address + byte]) << (8 * byte);
                            if (data != want) {
                                std::ostringstream message;
                                message << trace.id << "/" << path.id << ": wrong data at " << a.address << " (request "
                                        << result.requests << "): got 0x" << std::hex << data << ", expected 0x" << want;
                                for (uint32_t near = a.address & ~0x3fu; near < (a.address & ~0x3fu) + 0x40; near += 4) {
                                    uint32_t v = 0;
                                    for (unsigned byte = 0; byte < 4; ++byte) v |= uint32_t(expected[near + byte]) << (8 * byte);
                                    if (v == data) message << "; that is the word at " << std::dec << near;
                                }
                                throw std::runtime_error(message.str());
                            }
                        }
                    }
                    result.host_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
                    result.events = runner.events() - events_before;
                    benchmark::RunRecord record;
                    record.workload = {"cache-step2/" + trace.id + "/" + phase.name, "memory", trace.description,
                        {"cache", "standalone"}, {{"requests", std::to_string(result.requests)}}};
                    record.target = {"cache/" + path.id, "memory", {{"path", path.id},
                        {"profile", timing::activeTimingProfile().name}, {"period_ticks", std::to_string(period)},
                        {"dram_row_words", std::to_string(memory.words_per_row)}, {"block_words", "4"}}};
                    record.status = benchmark::RunStatus::Completed;
                    record.validated = true;
                    record.simulated_ticks = result.cycles * period;
                    record.host_seconds = result.host_seconds;
                    benchmark::addMetric(record, "cycles", double(result.cycles), "cycles");
                    benchmark::addMetric(record, "cycles_per_request", double(result.cycles) / double(result.requests), "cycles");
                    benchmark::addMetric(record, "processed_events", double(result.events), "events");
                    for (const auto& [name, c] : result.boundary) {
                        benchmark::addMetric(record, name + "_reads", double(c.reads), "requests");
                        benchmark::addMetric(record, name + "_burst_reads", double(c.bursts), "requests");
                        benchmark::addMetric(record, name + "_writes", double(c.writes), "requests");
                    }
                    std::cout << benchmark::toJson(record) << std::endl;
                }
                if (root->memory_->readBytes(0, Words * 4) != expected)
                    throw std::runtime_error(trace.id + "/" + path.id + ": final DRAM image differs");
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CachePerformanceBenchmark: " << error.what() << '\n';
        return 1;
    }
}
