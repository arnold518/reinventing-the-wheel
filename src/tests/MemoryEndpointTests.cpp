#include "tests/MemoryEndpointTests.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryContainer.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "tests/FFMemoryTests.hpp"
#include "tests/MemoryDeviceTests.hpp"
#include "tests/SRAMTests.hpp"
#include <random>
using namespace circuit;
using namespace circuit::test;
using namespace circuit::memory;
namespace {
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool failed = false; try { f(); } catch (const std::exception&) { failed = true; } require(failed,"Expected rejection"); }
struct Scenario {
    ComponentTestSpec spec;
    NamedValues state;
    size_t width;
    Scenario(std::string name, std::string contract, size_t w, ParameterMap config = {})
        : spec{std::move(name),std::move(contract),"MEMORY_ENDPOINT_ROOT",std::move(config),{{"transactions",{},{},{}}}},
          state{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"REQ_VALID",logicBit(false)},
                {"REQ_OP",logicBits(3,0)},{"REQ_ADDR",logicBits(32,0)},
                {"REQ_WRITE_DATA",logicBits(w,0)},{"REQ_WRITE_MASK",logicBits(w/8,0)},
                {"RSP_READY",logicBit(false)}}, width(w) {}
    void step(std::string label, NamedValues changes = {}, NamedValues expected = {}) {
        for (auto& [name,value] : changes) state[name] = value;
        spec.scenarios[0].actions.push_back({std::move(label),CheckpointKind::Settled,state,std::move(expected),true,{}});
    }
    void init() {
        step("reset", {}, {{"REQ_READY",logicBit(false)},{"RSP_VALID",logicBit(false)}});
        step("release-reset", {{"RST",logicBit(false)}}, {{"REQ_READY",logicBit(true)}});
    }
    void transaction(std::string label, unsigned op, uint32_t address, uint32_t data, unsigned mask,
                     uint32_t result, Status status = Status::Ok) {
        step(label+"-setup", {{"CLK",logicBit(false)},{"REQ_VALID",logicBit(true)},{"REQ_OP",logicBits(3,op)},
            {"REQ_ADDR",logicBits(32,address)},{"REQ_WRITE_DATA",logicBits(width,data)},
            {"REQ_WRITE_MASK",logicBits(width/8,mask)},{"RSP_READY",logicBit(false)}}, {{"REQ_READY",logicBit(true)}});
        step(label+"-accept", {{"CLK",logicBit(true)}}, {{"REQ_READY",logicBit(false)}});
        step(label+"-latched", {{"CLK",logicBit(false)},{"REQ_VALID",logicBit(false)},
            {"REQ_ADDR",logicBits(32,0xffffffff)},{"REQ_WRITE_DATA",logicBits(width,0)}});
        step(label+"-execute", {{"CLK",logicBit(true)}});
        // A relay has another transaction frontend behind it. Supply enough
        // clocks for either path; no response is consumed until RSP_READY.
        for (size_t cycle = 0; cycle < 5; ++cycle) {
            step(label+"-wait-low-"+std::to_string(cycle), {{"CLK",logicBit(false)}});
            step(label+"-wait-high-"+std::to_string(cycle), {{"CLK",logicBit(true)}});
        }
        NamedValues response{{"RSP_VALID",logicBit(true)},{"RSP_STATUS",logicBits(3,unsigned(status))},
                             {"RSP_READ_DATA",logicBits(width,result)},{"REQ_READY",logicBit(false)}};
        step(label+"-response", {{"CLK",logicBit(false)}}, response);
        step(label+"-hold", {{"CLK",logicBit(true)}}, response);
        response["REQ_READY"]=logicBit(true);
        step(label+"-ready", {{"CLK",logicBit(false)},{"RSP_READY",logicBit(true)}}, response);
        step(label+"-consumed", {{"CLK",logicBit(true)}}, {{"RSP_VALID",logicBit(false)},{"REQ_READY",logicBit(true)}});
    }
};
std::string testName(Technology t) {
    switch(t) {
    case Technology::FF: return "MemoryEndpointFFTest";
    case Technology::SRAM: return "MemoryEndpointSRAMTest";
    case Technology::DRAM: return "MemoryEndpointDRAMTest";
    case Technology::ROM: return "MemoryEndpointROMTest";
    case Technology::NORFlash: return "MemoryEndpointFlashTest";
    default: return "MemoryEndpointRelayTest";
    }
}
ComponentTestSpec endpointSpec(Technology t, size_t width = 8, bool container = false, size_t depth = 4, size_t row_words = 1) {
    ParameterMap config{{"depth_words",std::to_string(depth)},{"words_per_row",std::to_string(row_words)},{"block_words",std::to_string(row_words)}};
    if (t == Technology::ROM) config["image_hex"] = "a53c81ff";
    Scenario s(container ? "ScalableMemoryEndpointTest" : testName(t),
        memoryContractId(t, container ? "scalable-endpoint" : "endpoint", width), width, config);
    s.init(); const unsigned mask = (1u << (width/8))-1;
    if (t == Technology::ROM) {
        const uint32_t expected = width == 8 ? 0xa5 : width == 16 ? 0x3ca5 : 0xff813ca5;
        s.transaction("read-image",0,0,0,0,expected);
        s.transaction("reject-write",1,0,0,mask,0,Status::ReadOnly);
    } else {
        const unsigned write = t == Technology::NORFlash ? 2 : 1;
        const uint32_t first = uint32_t(0xa5a53c81ull & ((uint64_t(1)<<width)-1));
        s.transaction("write-first",write,0,first,mask,0);
        s.transaction("read-first",0,0,0,0,first);
        s.transaction("write-last",write,(depth-1)*(width/8),0x3c,mask,0);
        s.transaction("read-last",0,(depth-1)*(width/8),0,0,0x3c);
        s.transaction("zero-mask",write,0,0,0,0);
        s.transaction("unchanged",0,0,0,0,first);
        if (t != Technology::NORFlash) {
            s.transaction("masked-low-byte",1,0,0x5a,1,0);
            s.transaction("read-masked-byte",0,0,0,0,(first & ~0xffu)|0x5a);
        }
        if (t == Technology::NORFlash) {
            s.transaction("cannot-program-ones",2,0,0xffffffff,mask,0);
            s.transaction("still-programmed",0,0,0,0,first);
            s.transaction("reject-ram-write",1,0,0,mask,0,Status::Unsupported);
            s.transaction("erase-first-block",3,0,0,0,0);
            s.transaction("read-erased",0,0,0,0,width==32 ? 0xffffffff : (1u<<width)-1);
            s.transaction("other-block-retained",0,(depth-1)*(width/8),0,0,0x3c);
        } else s.transaction("unsupported-program",2,0,0,mask,0,Status::Unsupported);
    }
    s.transaction("range-error",0,depth*(width/8),0,0,0,Status::Range);
    s.transaction("reserved-opcode",7,0,0,0,0,Status::Unsupported);
    if (width > 8) s.transaction("unaligned",0,1,0,0,0,Status::Alignment);
    if (t != Technology::ROM) {
        const auto begin = s.spec.scenarios[0].actions.size();
        s.transaction("unknown-mask",t==Technology::NORFlash?2:1,0,0,0,0,Status::InvalidRequest);
        for (size_t i : {begin,begin+1})
            s.spec.scenarios[0].actions[i].inputs["REQ_WRITE_MASK"]=LogicVector(width/8,LogicValue::UNKNOWN);
        // Use a fresh word (erased ones for NOR); unknown selected data must
        // remain unknown instead of silently becoming a binary zero.
        const auto write = s.spec.scenarios[0].actions.size();
        s.transaction("unknown-data",t==Technology::NORFlash?2:1,width/8,0,mask,0);
        for (size_t i : {write,write+1})
            s.spec.scenarios[0].actions[i].inputs["REQ_WRITE_DATA"]=LogicVector(width,LogicValue::UNKNOWN);
        const auto read=s.spec.scenarios[0].actions.size();
        s.transaction("read-unknown",0,width/8,0,0,0);
        for (size_t i=read;i<s.spec.scenarios[0].actions.size();++i) {
            auto& expected=s.spec.scenarios[0].actions[i].expected_outputs;
            if (expected.count("RSP_READ_DATA")) expected["RSP_READ_DATA"]=LogicVector(width,LogicValue::UNKNOWN);
            if (i==read || i==read+1) {
                auto& inputs=s.spec.scenarios[0].actions[i].inputs;
                inputs["REQ_WRITE_DATA"]=LogicVector(width,LogicValue::UNKNOWN);
                inputs["REQ_WRITE_MASK"]=LogicVector(width/8,LogicValue::UNKNOWN);
            }
        }
    }
    return s.spec;
}
ComponentTestSpec frontendSpec() {
    Scenario s("MemoryTransactionPortTest","memory.shared.transaction-port.8",8);
    s.state["BACK_READY"] = logicBit(true); s.state["DONE"] = logicBit(false);
    s.state["RESULT"] = logicBits(8,0xa5); s.state["STATUS"] = logicBits(3,0);
    s.init(); s.step("request",{{"REQ_VALID",logicBit(true)},{"REQ_ADDR",logicBits(32,12)}});
    s.step("accept",{{"CLK",logicBit(true)}},{{"REQ_READY",logicBit(false)},{"ADDR",logicBits(32,12)}});
    s.step("backend-owns-payload",{{"CLK",logicBit(false)},{"REQ_ADDR",logicBits(32,0)},{"REQ_VALID",logicBit(false)}},{{"ADDR",logicBits(32,0)}});
    s.step("pending",{{"CLK",logicBit(true)}},{{"START",logicBit(false)},{"RSP_VALID",logicBit(false)}});
    s.step("done",{{"CLK",logicBit(false)},{"DONE",logicBit(true)}});
    s.step("capture-response",{{"CLK",logicBit(true)}},{{"RSP_VALID",logicBit(true)},{"RSP_READ_DATA",logicBits(8,0xa5)}});
    s.step("hold",{{"CLK",logicBit(false)},{"RESULT",logicBits(8,0)}},{{"RSP_READ_DATA",logicBits(8,0xa5)}});
    s.step("reset-response",{{"RST",logicBit(true)}},{{"RSP_VALID",logicBit(false)},{"REQ_READY",logicBit(false)}});
    return s.spec;
}
ComponentTestSpec responseBufferSpec(size_t width=8) {
    ComponentTestSpec spec{"MemoryResponseBufferTest","memory.shared.response-buffer."+std::to_string(width),
        "MEMORY_ENDPOINT_ROOT",{},{{"fall-through",{},{},{}}}};
    NamedValues state{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"IN_VALID",logicBit(false)},
        {"OUT_READY",logicBit(false)},{"IN_DATA",logicBits(width,0)},{"IN_STATUS",logicBits(3,0)}};
    auto step=[&](std::string name,NamedValues changes,NamedValues expected){
        for(auto& [k,v]:changes)state[k]=v;
        spec.scenarios[0].actions.push_back({name,CheckpointKind::Settled,state,expected,true,{}});
    };
    step("reset",{},{{"IN_READY",logicBit(false)},{"OUT_VALID",logicBit(false)},{"FULL",logicBit(false)}});
    step("empty-bypass",{{"RST",logicBit(false)},{"IN_VALID",logicBit(true)},{"OUT_READY",logicBit(true)},
        {"IN_DATA",logicBits(width,0x31)}},{{"IN_READY",logicBit(true)},{"OUT_VALID",logicBit(true)},{"OUT_DATA",logicBits(width,0x31)}});
    step("bypass-transfer",{{"CLK",logicBit(true)}},{{"FULL",logicBit(false)}});
    step("empty",{{"CLK",logicBit(false)},{"IN_VALID",logicBit(false)}},{{"OUT_VALID",logicBit(false)}});
    step("blocked-input",{{"IN_VALID",logicBit(true)},{"OUT_READY",logicBit(false)}},{{"IN_READY",logicBit(true)}});
    step("capture",{{"CLK",logicBit(true)}},{{"FULL",logicBit(true)},{"IN_READY",logicBit(false)}});
    step("hold",{{"CLK",logicBit(false)},{"IN_DATA",logicBits(width,0x42)},{"IN_STATUS",logicBits(3,5)}},
        {{"OUT_DATA",logicBits(width,0x31)},{"OUT_STATUS",logicBits(3,0)}});
    step("consume-and-replace",{{"OUT_READY",logicBit(true)}},{{"IN_READY",logicBit(true)},{"OUT_DATA",logicBits(width,0x31)}});
    step("replacement",{{"CLK",logicBit(true)}},{{"FULL",logicBit(true)},{"OUT_DATA",logicBits(width,0x42)},{"OUT_STATUS",logicBits(3,5)}});
    step("retain-new",{{"CLK",logicBit(false)},{"IN_VALID",logicBit(false)},{"OUT_READY",logicBit(false)}},{{"OUT_VALID",logicBit(true)}});
    step("reset-held-response",{{"RST",logicBit(true)}},{{"OUT_VALID",logicBit(false)},{"FULL",logicBit(false)}});
    return spec;
}
ComponentTestSpec controllerSpec() {
    Scenario s("MemoryControllerTest","memory.ff.controller.8",8);
    s.state["MEM_READ_DATA"]=logicBits(8,0xa5);s.state["MEM_FAULT"]=logicBit(false);
    s.init();s.transaction("read-attached-port",0,0,0,0,0xa5);
    s.transaction("write-attached-port",1,0,0x3c,1,0);
    s.state["MEM_FAULT"]=logicBit(true);
    s.transaction("native-fault",0,0,0,0,0,Status::Device);
    return s.spec;
}
}
MemoryTransactionPortTest::MemoryTransactionPortTest() : ComponentScenarioTest(frontendSpec()) {}
MemoryResponseBufferTest::MemoryResponseBufferTest() : ComponentScenarioTest(responseBufferSpec()) {}
void RegisterWidthsTest::verifyResults() {
    using namespace circuit::test;
    ComponentTestRunner runner;
    for (size_t width : {1,2,3,4,5,8,16,32}) {
        auto zero = logicBits(width, 0);
        auto pattern = logicBits(width, 0xa5a5a5a5U);
        auto unknown = pattern;
        unknown[width - 1] = LogicValue::UNKNOWN;
        ActionScenario scenario{"word-edges", {}, {}, {10'000, 200'000}};
        auto add = [&](std::string id, NamedValues inputs, NamedValues expected) {
            scenario.actions.push_back({std::move(id), CheckpointKind::Settled,
                                        std::move(inputs), std::move(expected), true, {}});
        };
        NamedValues state{{"D", zero}, {"WE", logicBit(false)},
                          {"CLK", logicBit(false)}, {"RST", logicBit(true)}};
        add("asynchronous-reset", state, {{"Q", zero}});
        state["RST"] = logicBit(false);
        state["D"] = pattern;
        state["CLK"] = logicBit(true);
        add("disabled-hold", state, {{"Q", zero}});
        state["CLK"] = logicBit(false);
        add("clock-low", state, {{"Q", zero}});
        state["WE"] = logicBit(true);
        add("write-enable-setup", state, {{"Q", zero}});
        state["CLK"] = logicBit(true);
        add("capture-word", state, {{"Q", pattern}});
        state["D"] = zero;
        add("no-level-capture", state, {{"Q", pattern}});
        state["CLK"] = logicBit(false);
        state["D"] = unknown;
        add("prepare-unknown", state, {{"Q", pattern}});
        state["CLK"] = logicBit(true);
        auto stored_unknown = unknown;
        stored_unknown[width - 1] = LogicValue::UNKNOWN;
        add("unknown-data-capture", state, {{"Q", stored_unknown}});
        state["RST"] = logicBit(true);
        add("reset-while-clock-high", state, {{"Q", zero}});
        ComponentTestSpec spec{"RegisterWidthsTest",
            std::string(circuit::families::registerWord(width).id()),
            "PAYLOAD_ROOT", {}, {scenario}};
        auto results = runner.runAll(spec, scenario);
        if (results.targets.size() != 2)
            throw std::runtime_error("Register<W> needs both structural and behavioral implementations");
        runner.compare(results.targets[0], results.targets[1]);
        if (results.targets[0].root->getChildren().size() < width ||
            !results.targets[1].root->getChildren().empty())
            throw std::runtime_error("Register<W> fidelity did not change the storage boundary");
    }
}
void ClockedStateBitTest::verifyResults() {
    using namespace circuit::test;
    ActionScenario scenario{"clocked-state", {}, {}, {10'000, 200'000}};
    auto add = [&](std::string label, NamedValues inputs, LogicValue q) {
        scenario.actions.push_back({std::move(label), CheckpointKind::Settled,
                                    std::move(inputs), {{"Q", logicBit(q)}}, true, {}});
    };
    NamedValues state{{"D", logicBit(false)}, {"CLK", logicBit(false)}, {"RST", logicBit(true)}};
    add("reset", state, LogicValue::LOW);
    state["RST"] = logicBit(false);
    state["D"] = logicBit(true);
    add("prepare", state, LogicValue::LOW);
    state["CLK"] = logicBit(true);
    add("capture", state, LogicValue::HIGH);
    state["D"] = logicBit(false);
    add("hold-high", state, LogicValue::HIGH);
    state["CLK"] = logicBit(false);
    add("falling-edge", state, LogicValue::HIGH);
    state["CLK"] = logicBit(true);
    add("capture-low", state, LogicValue::LOW);
    state["RST"] = logicBit(true);
    add("asynchronous-reset", state, LogicValue::LOW);
    ComponentTestSpec spec{"ClockedStateBitTest", std::string(circuit::families::ClockedStateBit.id()),
                           "STATE_ROOT", {}, {scenario}};
    ComponentTestRunner runner;
    auto results = runner.runAll(spec, scenario);
    require(results.targets.size() == 2, "ClockedStateBit needs both fidelities");
    runner.compare(results.targets[0], results.targets[1]);
}
MemoryControllerTest::MemoryControllerTest() : ComponentScenarioTest(controllerSpec()) {}
namespace {
ComponentTestSpec alwaysReadyControllerSpec() {
    Scenario s("FFAlwaysReadyControllerTest","memory.ff.controller.8",8,
        {{"depth_words","4"},{"always_ready","1"}});
    s.state["RSP_READY"]=logicBit(true);
    s.state["MEM_READ_DATA"]=logicBits(8,0xa5);
    s.state["MEM_FAULT"]=logicBit(false);
    s.init();
    s.step("read",{{"REQ_VALID",logicBit(true)}},
        {{"REQ_READY",logicBit(true)},{"RSP_VALID",logicBit(true)},
         {"RSP_READ_DATA",logicBits(8,0xa5)},{"RSP_STATUS",logicBits(3,0)},
         {"MEM_READ_EN",logicBit(true)}});
    s.step("read-edge",{{"CLK",logicBit(true)}},{{"RSP_VALID",logicBit(true)}});
    s.step("idle",{{"CLK",logicBit(false)},{"REQ_VALID",logicBit(false)}},
        {{"REQ_READY",logicBit(true)},{"RSP_VALID",logicBit(false)}});
    s.step("write",{{"REQ_VALID",logicBit(true)},{"REQ_OP",logicBits(3,1)},
        {"REQ_WRITE_DATA",logicBits(8,0x3c)},{"REQ_WRITE_MASK",logicBits(1,1)}},
        {{"RSP_VALID",logicBit(true)},{"MEM_WE",logicBit(true)},
         {"RSP_READ_DATA",logicBits(8,0)},{"RSP_STATUS",logicBits(3,0)}});
    s.step("range",{{"REQ_OP",logicBits(3,0)},{"REQ_ADDR",logicBits(32,4)}},
        {{"RSP_VALID",logicBit(true)},{"MEM_READ_EN",logicBit(false)},
         {"RSP_STATUS",logicBits(3,unsigned(Status::Range))}});
    s.step("reset",{{"RST",logicBit(true)}},
        {{"REQ_READY",logicBit(false)},{"RSP_VALID",logicBit(false)}});
    return s.spec;
}
}
FFAlwaysReadyControllerTest::FFAlwaysReadyControllerTest()
    : ComponentScenarioTest(alwaysReadyControllerSpec()) {}
MemoryEndpointFFTest::MemoryEndpointFFTest() : ComponentScenarioTest(endpointSpec(Technology::FF)) {}
MemoryEndpointSRAMTest::MemoryEndpointSRAMTest() : ComponentScenarioTest(endpointSpec(Technology::SRAM)) {}
MemoryEndpointDRAMTest::MemoryEndpointDRAMTest() : ComponentScenarioTest(endpointSpec(Technology::DRAM)) {}
MemoryEndpointROMTest::MemoryEndpointROMTest() : ComponentScenarioTest(endpointSpec(Technology::ROM)) {}
MemoryEndpointFlashTest::MemoryEndpointFlashTest() : ComponentScenarioTest(endpointSpec(Technology::NORFlash)) {}
MemoryEndpointRelayTest::MemoryEndpointRelayTest() : ComponentScenarioTest(endpointSpec(Technology::Container)) {}
ScalableMemoryEndpointTest::ScalableMemoryEndpointTest() : ComponentScenarioTest(endpointSpec(Technology::SRAM,8,true)) {}
namespace {
ComponentTestSpec nativeContainerSpec() {
    auto spec=ffMemorySpec(8,4);
    spec.id="NativeMemoryContainerTest";
    spec.contract_id="memory.ff.scalable-device.8";
    spec.instance_name="MEMORY_CONTAINER_ROOT";
    return spec;
}

void verifyEndpointStoragePolicy(Technology technology) {
    // All three representations must preserve the caller's storage policy:
    // expanded endpoint with a behavioral device, compact device, fused endpoint.
    for (unsigned mode = 0; mode < 3; ++mode) {
        auto profile = withExactFidelity(canonicalDefaultProfile(), "POLICY_ROOT",
            mode == 2 ? Fidelity::Behavioral : Fidelity::Structural);
        if (mode == 0)
            profile = withExactFidelity(profile, "POLICY_ROOT.MEMORY", Fidelity::Behavioral);
        ParameterMap policy{{"depth_words", "4"}, {"words_per_row", "2"}, {"block_words", "2"},
            {"record_history", "0"},
            {"paged", "0"}, {"max_resident_bytes", "64"}, {"max_history_bytes", "1"}};
        const auto context = std::string(technologyName(technology)) + "/mode-" + std::to_string(mode);
        auto create = [&] {
            auto root = builtinComponentCatalog().createRoot(
                {memoryContractId(technology, mode == 1 ? "scalable-endpoint" : "endpoint", 8),
                 "POLICY_ROOT", policy, {}, {}, {}}, profile).root;
            auto view = std::dynamic_pointer_cast<MemoryStateView>(root);
            require(bool(view), context + ": missing memory state view");
            return view;
        };
        auto dense = create();
        require(!dense->memoryStorage().isHistoryRecordingEnabled(), context + ": history setting lost");
        require(dense->memoryStorage().allocatedDataBytes() > 0 &&
                dense->memoryStorage().allocatedDataBytes() <= 64, context + ": dense storage setting lost");
        dense->initializeMemoryBytes(0, {0x42});
        require(dense->memoryStorage().readBytes(0, 1) == std::vector<uint8_t>{0x42}, context + ": provisioning failed");
        require(dense->memoryStorage().historicalByteCount() == 0, context + ": disabled history still allocated");

        policy["max_resident_bytes"] = "1";
        rejects([&] { create(); }); // Dense allocation must respect the budget too.
        policy["paged"] = "1";
        auto paged = create();
        require(paged->memoryStorage().allocatedDataBytes() == 0, context + ": sparse storage allocated eagerly");
        rejects([&] { paged->initializeMemoryBytes(0, {0x42}); });
        require(paged->memoryStorage().allocatedDataBytes() == 0, context + ": resident budget exceeded");

        policy["max_resident_bytes"] = "16384";
        policy["record_history"] = "1";
        auto bounded_history = create();
        require(bounded_history->memoryStorage().isHistoryRecordingEnabled(), context + ": history not enabled");
        rejects([&] { bounded_history->initializeMemoryBytes(0, {0x42}); });
    }
}

void verifyEndpointWriteHistory(Technology technology, size_t width) {
    const size_t bytes = width / 8;
    const uint32_t address = 3 * bytes; // Nonzero row and column, next to untouched storage.
    const unsigned mask = (1u << bytes) - 1;
    const bool flash = technology == Technology::NORFlash;
    const unsigned write_op = flash ? 2 : 1;
    Scenario s("MemoryEndpointWriteHistory", memoryContractId(technology, "endpoint", width), width, {{"depth_words", "4"}, {"words_per_row", "2"},
        {"block_words", "2"}, {"refresh_interval_ticks", "0"}});
    using Writes = std::map<uint32_t, uint8_t>;
    std::map<std::string, Writes> expected;
    auto transaction = [&](std::string label, unsigned op, uint32_t at, uint32_t data,
                           unsigned lanes, uint32_t result, Writes writes, Status status = Status::Ok) {
        s.transaction(label, op, at, data, lanes, result, status);
        expected.emplace(label + "-consumed", std::move(writes));
    };
    auto wordWrites = [&](uint32_t data) {
        Writes writes;
        for (size_t byte = 0; byte < bytes; ++byte) writes[address + byte] = uint8_t(data >> (byte * 8));
        return writes;
    };
    s.init();
    // Other lanes and the neighboring DRAM word are unknown here. Neither may
    // leak into the binary bus-write query or cause it to throw.
    transaction("first-byte", write_op, address, 0x81, 1, 0, {{address, 0x81}});
    transaction("zero-mask", write_op, address, 0, 0, 0, {});
    uint32_t word = uint32_t(0xa53c7e81ull & ((uint64_t(1) << width) - 1));
    transaction("whole-word", write_op, address, word, mask, 0, wordWrites(word));
    transaction("read", 0, address, 0, 0, word, {});
    transaction("same-value-write", write_op, address, word, mask, 0, wordWrites(word));
    const unsigned shift = width - 8;
    const uint8_t high_byte = flash ? uint8_t((word >> shift) & 0x5a) : 0x5a;
    word = (word & ~(0xffu << shift)) | (uint32_t(high_byte) << shift);
    transaction("high-byte", write_op, address, 0x5au << shift, 1u << (bytes - 1), 0,
                {{address + uint32_t(bytes - 1), high_byte}});
    transaction("read-masked", 0, address, 0, 0, word, {});
    if (flash) {
        transaction("program-ones", write_op, address, 0xffffffff, mask, 0, wordWrites(word));
        Writes erased;
        for (uint32_t at = 2 * bytes; at < 4 * bytes; ++at) erased[at] = 0xff;
        transaction("erase-block", 3, 2 * bytes, 0, 0, 0, erased);
        transaction("read-erased", 0, address, 0, 0,
                    uint32_t((uint64_t(1) << width) - 1), {});
    }
    transaction("unsupported", 7, address, 0, mask, 0, {}, Status::Unsupported);

    ComponentTestRunner runner;
    for (unsigned mode = 0; mode < 3; ++mode) {
        auto profile = withExactFidelity(canonicalDefaultProfile(), s.spec.instance_name,
            mode == 2 ? Fidelity::Behavioral : Fidelity::Structural);
        if (mode == 0)
            profile = withExactFidelity(profile, s.spec.instance_name + ".MEMORY", Fidelity::Behavioral);
        auto spec = s.spec;
        if (mode == 1) spec.contract_id = memoryContractId(technology, "scalable-endpoint", width);
        auto run = runner.run(spec, spec.scenarios[0], profile);
        auto view = std::dynamic_pointer_cast<MemoryStateView>(run.root);
        require(bool(view), "Write-history endpoint has no memory state view");
        size_t previous = 0, checked = 0;
        for (const auto& checkpoint : run.checkpoints) if (auto it = expected.find(checkpoint.id); it != expected.end()) {
            const auto context = std::string(technologyName(technology)) + "/" + std::to_string(width) +
                "/mode-" + std::to_string(mode) + "/" + checkpoint.id;
            require(view->memoryStorage().getByteWritesInTimeRange(previous, checkpoint.actual_time) == it->second,
                    context + ": incorrect written byte addresses or values");
            previous = checkpoint.actual_time;
            ++checked;
        }
        require(checked == expected.size(), "Write-history checkpoints missing");
    }
}
}
NativeMemoryContainerTest::NativeMemoryContainerTest() : ComponentScenarioTest(nativeContainerSpec()) {}
void NativeMemoryDeviceContractTest::verifyResults() {
    ComponentTestRunner runner;
    auto compare=[&](ComponentTestSpec spec,Technology t,size_t width) {
        for (const auto& scenario:spec.scenarios) {
            auto expanded=runner.run(spec,scenario);
            auto native=spec;
            native.contract_id=memoryContractId(t,"scalable-device",width);
            auto compact=runner.run(native,scenario);
            runner.compare(expanded,compact);
            require(compact.root->getChildren().empty(),"Native container must be a leaf without controller state");
        }
    };
    for (size_t width:{8,16,32}) {
        compare(ffMemorySpec(width,4),Technology::FF,width);
        compare(sramDeviceSpec(width,4,2),Technology::SRAM,width);
    }
    compare(DRAMTest().getSpec(),Technology::DRAM,8);
    compare(ROMTest().getSpec(),Technology::ROM,8);
    compare(NORFlashTest().getSpec(),Technology::NORFlash,8);
    for (auto t:{Technology::FF,Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash}) {
        verifyEndpointStoragePolicy(t);
        // A visible structural controller can drive a compact device too.
        auto spec=endpointSpec(t);
        auto expanded=runner.run(spec,spec.scenarios[0]);
        spec.contract_id=memoryContractId(t,"scalable-endpoint",8);
        auto scalable=runner.run(spec,spec.scenarios[0]);
        runner.compare(expanded,scalable);
        EndpointConfig config;config.depth_words=1u<<28;config.record_history=false;
        config.max_resident_bytes=16384;config.max_history_bytes=0;config.refresh_interval_ticks=0;
        auto huge=std::dynamic_pointer_cast<MemoryDeviceStorage>(createMemoryContainer("HUGE",8,t,config));
        require(huge->memoryStorage().capacityBytes()==1u<<28,"Wrong large capacity");
        require(huge->memoryStorage().allocatedDataBytes()==0,"Eager content allocation");
        huge->initializeBytes((1u<<28)-1,{0x42});
        require(huge->readBytes((1u<<28)-1,1)==std::vector<uint8_t>{0x42},"Provision last address");
        rejects([&]{huge->initializeBytes(0,{0x42});});
        auto endpoint=Component::create<MemoryEndpoint>("LARGE_ENDPOINT",8,t,config,true);
        require(endpoint->getChildren().size()==2,"Endpoint should contain controller plus device");
    }
}
void MemoryEndpointContractTest::verifyResults() {
    ComponentTestRunner runner;
    for (auto t : {Technology::FF, Technology::SRAM, Technology::DRAM, Technology::NORFlash})
        for (size_t width : {8, 16, 32}) verifyEndpointWriteHistory(t, width);
    for(size_t width:{8,16,32}) {
        auto spec=responseBufferSpec(width);
        runner.run(spec,spec.scenarios[0]);
    }
    for (auto t : {Technology::FF,Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash})
        for (size_t width : {8,16,32}) {
            auto spec = endpointSpec(t,width);
            auto result = runner.runAll(spec,spec.scenarios[0]);
            require(result.targets.size()==2,"Missing endpoint fidelity");
            runner.compare(result.targets[0],result.targets[1]);
        }
    for (auto t : {Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash}) {
        auto spec=endpointSpec(t,16,false,6,2);
        auto result=runner.runAll(spec,spec.scenarios[0]);
        runner.compare(result.targets[0],result.targets[1]);
    }
    MemoryStorage dense(8192,8,LogicValue::UNKNOWN,false);
    MemoryStorage sparse(8192,8,LogicValue::UNKNOWN,false,{true,65536,65536});
    std::mt19937 rng(123);
    for (size_t i=0;i<256;++i) {
        auto address=rng()%8192; auto value=logicBits(8,rng());
        dense.writeLogic(i,address,value); sparse.writeLogic(i,address,value);
        require(dense.readLogic(address,1)==sparse.readLogic(address,1),"Paged contents mismatch");
    }
    sparse.fill(300,LogicValue::HIGH); require(sparse.allocatedDataBytes()==0,"Sparse fill allocated pages");
    require(sparse.readBytes(8000,1)[0]==255,"Sparse fill incorrect");
    MemoryStorage huge(1u<<28,8,LogicValue::UNKNOWN,false,{true,16384,0});
    require(huge.allocatedDataBytes()==0,"Sparse construction scales with capacity");
    huge.writeLogic(1,(1u<<28)-1,logicBits(8,42));
    require(huge.allocatedDataBytes()<16384 && huge.readBytes((1u<<28)-1,1)[0]==42,"Sparse last address failed");
    rejects([&]{huge.writeLogic(2,0,logicBits(8,1));});
    rejects([]{MemoryStorage(1u<<28,8,LogicValue::LOW,false,{false,1024,0});});
    MemoryStorage bounded(16,8,LogicValue::LOW,true,{true,16384,1});
    rejects([&]{bounded.writeLogic(1,0,logicBits(8,1));});
    EndpointConfig config; config.depth_words=1u<<28; config.record_history=false;
    auto root=createMemoryContainer<8,circuit::memory::SRAMModel>("LARGE",config);
    auto large=std::dynamic_pointer_cast<MemoryDeviceStorage>(root);
    require(large->memoryStorage().capacityBytes()==1u<<28 && root->getChildren().empty(),"Container not compact");
    require(large->memoryStorage().allocatedDataBytes()==0,"Container allocated untouched capacity");
    config.retention_ticks=1024;
    rejects([&]{config.validate(8,Technology::DRAM);});
}
void MemoryEndpointTimingTest::verifyResults() {
    struct Rig {
        std::shared_ptr<IOComponent> root;
        Simulator sim;
        size_t now = 100;
        std::map<std::string,std::shared_ptr<WireBase>> wires;
        std::string context;
        explicit Rig(Technology t, bool compact, ParameterMap config = {}, bool native_device = false) {
            context=std::string(technologyName(t))+(compact?" behavioral":native_device?" native device":" structural");
            config["words_per_row"]="1"; config["block_words"]="1";
            auto profile=withExactFidelity(canonicalDefaultProfile(),"DUT",compact?Fidelity::Behavioral:Fidelity::Structural);
            if (native_device) profile=withExactFidelity(profile,"DUT.MEMORY",Fidelity::Behavioral);
            root=std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(
                {memoryContractId(t,"endpoint",8),"DUT",config,{},{},{}},profile).root);
            ComponentBuilder b(root);
            for (auto& [name,pin]:root->getAllInputPins()) wires[name]=b.addNewWireDynamic("IN_"+name,pin->getWidth(),nullptr,{pin});
            for (auto& [name,pin]:root->getAllOutputPins()) b.addNewWireDynamic("OUT_"+name,pin->getWidth(),pin,{});
            for (auto& [name,wire]:wires) sim.scheduleEvent(makeTestWireUpdate(0,wire,TestValue(uint64_t(name=="RST"))));
            SimulationTest::scheduleInitialEventsForTree(root,sim,0); sim.advanceAndRecord(100);
            drive({{"RST",0}});
        }
        void drive(std::map<std::string,uint64_t> values, size_t wait=100) {
            const auto at=now+1;
            for (auto& [name,value]:values) sim.scheduleEvent(makeTestWireUpdate(at,wires.at(name),TestValue(value)));
            now=at+wait;sim.advanceAndRecord(now);
        }
        void unknown(const char* name,size_t width) {
            sim.scheduleEvent(makeTestWireUpdate(now+1,wires.at(name),TestValue(LogicVector(width,LogicValue::UNKNOWN))));
            now+=101;sim.advanceAndRecord(now);
        }
        void cycle() {drive({{"CLK",0}});drive({{"CLK",1}});}
        LogicVector out(const char* name) {return root->getOutputPinDynamic(name)->getValueAsVector();}
        void start(unsigned op,unsigned addr,unsigned data=0,unsigned mask=1) {
            drive({{"CLK",0},{"REQ_OP",op},{"REQ_ADDR",addr},{"REQ_WRITE_DATA",data},{"REQ_WRITE_MASK",mask},{"REQ_VALID",0},{"RSP_READY",0}});
            for (unsigned i=0;out("REQ_READY")!=logicBit(true)&&i<100;++i) cycle();
            require(out("REQ_READY")==logicBit(true),context+" request never ready at "+std::to_string(sim.getCurrentTime())+
                " op="+std::to_string(op)+" address="+std::to_string(addr)+" response="+formatLogicVector(out("RSP_VALID")));
            drive({{"CLK",0},{"REQ_VALID",1}});drive({{"CLK",1}});
            drive({{"CLK",0},{"REQ_VALID",0},{"REQ_ADDR",0xffffffff},{"REQ_WRITE_DATA",0}});
        }
        void response() {
            for (unsigned i=0;out("RSP_VALID")!=logicBit(true)&&i<100;++i) cycle();
            require(out("RSP_VALID")==logicBit(true),"Response deadline exceeded");
        }
        void consume() {drive({{"CLK",0},{"RSP_READY",1}});drive({{"CLK",1}});drive({{"CLK",0},{"RSP_READY",0}});}
        void transaction(unsigned op,unsigned address,unsigned value,unsigned expected) {
            start(op,address,value);response();
            require(out("RSP_STATUS")==logicBits(3,0),"Unexpected transaction error");
            require(out("RSP_READ_DATA")==logicBits(8,expected),"Timed response data mismatch");consume();
        }
    };
    for (unsigned mode = 0; mode < 3; ++mode) {
        const bool compact = mode == 2, native_device = mode == 1;
        Rig ff(Technology::FF,compact,{},native_device);
        // Zero-wait transfers: present during LOW, complete on the next edge,
        // with a different request every cycle and no idle bubble.
        for(unsigned i=0;i<24;++i) {
            const bool write=i<12;const unsigned address=i%4;
            ff.drive({{"CLK",0},{"REQ_VALID",1},{"RSP_READY",1},{"REQ_OP",write?1u:0u},
                {"REQ_ADDR",address},{"REQ_WRITE_DATA",0x40+address},{"REQ_WRITE_MASK",1}});
            require(ff.out("REQ_READY")==logicBit(true)&&ff.out("RSP_VALID")==logicBit(true),
                ff.context+" inserted a fast-path wait state");
            require(ff.out("RSP_STATUS")==logicBits(3,0),"Fast response status");
            require(ff.out("RSP_READ_DATA")==logicBits(8,write?0:0x40+address),"Fast response data");
            ff.drive({{"CLK",1}});
        }
        ff.drive({{"CLK",0},{"REQ_VALID",0},{"RSP_READY",0}});
        // Consume a held read while accepting another: old response wins on
        // that edge, the new response must be retained for the following one.
        ff.start(0,0);ff.response();
        ff.drive({{"CLK",0},{"RSP_READY",1},{"REQ_VALID",1},{"REQ_OP",0},{"REQ_ADDR",1}});
        require(ff.out("REQ_READY")==logicBit(true)&&ff.out("RSP_READ_DATA")==logicBits(8,0x40),"Held response was overtaken");
        ff.drive({{"CLK",1}});
        ff.drive({{"CLK",0},{"REQ_VALID",0},{"RSP_READY",0}});
        require(ff.out("RSP_VALID")==logicBit(true)&&ff.out("RSP_READ_DATA")==logicBits(8,0x41),"Replacement response lost");
        ff.consume();
        ff.transaction(1,0,0xa5,0);ff.transaction(0,0,0,0xa5);
        ff.drive({{"RST",1}});ff.drive({{"RST",0}});
        ff.transaction(0,0,0,0xa5); // Protocol reset is not FF storage reset.
        ff.drive({{"CLK",0},{"REQ_OP",0},{"REQ_VALID",1},{"REQ_ADDR",0}});
        ff.unknown("REQ_ADDR",32);ff.drive({{"CLK",1}});ff.drive({{"CLK",0},{"REQ_VALID",0}});ff.response();
        require(ff.out("RSP_STATUS")==logicBits(3,6),"Unknown address was coerced to zero");ff.consume();
        ff.start(0,0);ff.response();
        for (int i=0;i<5;++i) {ff.cycle();require(ff.out("RSP_READ_DATA")==logicBits(8,0xa5),"Response changed under backpressure");}
        ff.consume();
        Rig flash(Technology::NORFlash,compact,{{"program_ticks","4000"},{"erase_ticks","5000"}},native_device);
        flash.start(2,0,0x81);flash.cycle();
        require(flash.out("RSP_VALID")==logicBit(false),"Flash completed before its program interval");
        flash.drive({{"RST",1}});flash.drive({{"RST",0}});
        for(int i=0;i<35;++i) {flash.cycle();require(flash.out("RSP_VALID")==logicBit(false),"Reset resurrected a response");}
        flash.transaction(0,0,0,0x81); // Issued program drained, never rolled back.
        if (mode != 0) {
            auto view = std::dynamic_pointer_cast<MemoryStateView>(flash.root);
            require(view->memoryStorage().getByteWritesInTimeRange(0, flash.now) ==
                    std::map<uint32_t, uint8_t>{{0, 0x81}}, "Drained flash program missing from write history");
        }
        Rig dram(Technology::DRAM,compact,{{"retention_ticks","20000"},{"refresh_interval_ticks","1024"}},native_device);
        dram.transaction(1,0,0x5a,0);
        const auto last_write = dram.now;
        dram.start(0,0);dram.response();
        for(int i=0;i<220;++i) {dram.cycle();require(dram.out("RSP_READ_DATA")==logicBits(8,0x5a),"Refresh changed a held response");}
        dram.consume();dram.transaction(0,0,0,0x5a);
        if (mode != 0) {
            auto view = std::dynamic_pointer_cast<MemoryStateView>(dram.root);
            require(view->memoryStorage().getByteWritesInTimeRange(0, last_write) ==
                    std::map<uint32_t, uint8_t>{{0, 0x5a}}, "DRAM write missing from history");
            require(view->memoryStorage().getByteWritesInTimeRange(last_write, dram.now).empty(),
                    "DRAM read, precharge or refresh reported as a program write");
        }
        dram.transaction(1,3,0x3c,0);
        unsigned responses=0;
        // Keep VALID asserted: periodic refresh must still protect another row.
        for(unsigned cycle=0;cycle<220;++cycle) {
            dram.drive({{"CLK",0},{"REQ_VALID",1},{"RSP_READY",1},{"REQ_OP",0},{"REQ_ADDR",3}});
            if(dram.out("RSP_VALID")==logicBit(true)) {
                require(dram.out("RSP_STATUS")==logicBits(3,0)&&dram.out("RSP_READ_DATA")==logicBits(8,0x3c),
                    "Continuous DRAM traffic lost its data");
                ++responses;
            }
            dram.drive({{"CLK",1}});
        }
        require(responses>10,"Continuous DRAM traffic made no progress");
        dram.drive({{"CLK",0},{"REQ_VALID",0},{"RSP_READY",1}});
        for(unsigned i=0;i<30;++i)dram.cycle(); // Drain, then inspect the untouched row.
        dram.drive({{"RSP_READY",0}});
        dram.transaction(0,0,0,0x5a);
        // Close row zero, then stop the clock beyond retention. Expiry is not
        // repaired by a later refresh; both representations must return X.
        dram.transaction(1,3,0x3c,0);dram.drive({{"CLK",0}},30000);
        dram.start(0,0);dram.response();
        require(dram.out("RSP_READ_DATA")==LogicVector(8,LogicValue::UNKNOWN),"Expired DRAM data survived stopped-clock retention");
    }
}
