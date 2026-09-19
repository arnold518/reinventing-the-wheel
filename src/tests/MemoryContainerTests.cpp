// This header must stand alone without declaring any behavioral model first.
#include "modules/memory/MemoryArray.hpp"
#include "tests/MemoryContainerTests.hpp"
#include "modules/memory/LegacyMemoryContainer.hpp"
#include "modules/memory/MemoryContainerCatalog.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "basic/Wire.hpp"
#include <random>
#include <limits>
#include <set>

namespace {
using namespace circuit::test;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
    bool threw = false;
    try { f(); } catch (const std::exception&) { threw = true; }
    require(threw, "Expected memory configuration/access rejection");
}
ComponentTestSpec specFor(std::string model, size_t width, size_t depth = 4) {
    ComponentTestSpec spec;
    spec.id = model == "sram" ? "SRAMContainerTest" : model == "rom" ? "ROMContainerTest" : "FFMemoryContainerTest";
    spec.contract_id = "memory.container." + model + "." + std::to_string(width);
    spec.instance_name = "MEMORY_CONTAINER_ROOT";
    spec.parameters = {{"depth_words", std::to_string(depth)}};
    ActionScenario scenario;
    scenario.id = "array-contract";
    const auto bytes = width / 8;
    NamedValues state{{"WORD_ADDR", logicBits(32, 0)}, {"WRITE_DATA", logicBits(width, 0)},
        {"WRITE_MASK", logicBits(bytes, (1u << bytes) - 1)}, {"READ_EN", logicBit(false)},
        {"WE", logicBit(false)}, {"CLK", logicBit(false)}, {"RST", logicBit(true)}};
    auto action = [&](std::string id, NamedValues changes, uint32_t data, bool fault = false) {
        for (auto& [k,v] : changes) state[k] = v;
        scenario.actions.push_back({std::move(id), CheckpointKind::Settled, state,
            {{"READ_DATA", logicBits(width, data)}, {"FAULT", logicBit(fault)}}, true, {}});
    };
    action("initial-disabled", {}, 0);
    action("release-reset", {{"RST", logicBit(false)}}, 0);
    const auto mask = width == 32 ? 0xffffffffu : (1u << width) - 1;
    for (size_t word = 0; word < depth; ++word) {
        const uint32_t value = (0xa5f03c81u ^ (uint32_t(word) * 0x01010101u)) & mask;
        action("setup-" + std::to_string(word), {{"WORD_ADDR", logicBits(32, word)}, {"WE", logicBit(false)},
            {"CLK", logicBit(false)}, {"READ_EN", logicBit(false)}, {"WRITE_DATA", logicBits(width, value)}}, 0);
        if (model != "rom") {
            action("write-enable-" + std::to_string(word), {{"WE", logicBit(true)}}, 0);
            action("write-edge-" + std::to_string(word), {{"CLK", logicBit(true)}}, 0);
        }
        action("finish-write-" + std::to_string(word), {{"WE", logicBit(false)}, {"CLK", logicBit(false)}}, 0);
        action("read-" + std::to_string(word), {{"READ_EN", logicBit(true)}}, model == "rom" ? 0 : value);
    }
    // Re-read after every word was written: selection must not alias another
    // word, including across multiple mux levels and non-power-of-two tails.
    auto readAll = [&](const std::string& prefix) {
        for (size_t i = depth; i > 0; --i) {
            const size_t word = i - 1;
            const uint32_t value = (0xa5f03c81u ^ (uint32_t(word) * 0x01010101u)) & mask;
            action(prefix + std::to_string(word), {{"WORD_ADDR", logicBits(32, word)},
                {"READ_EN", logicBit(true)}}, model == "rom" ? 0 : value);
        }
    };
    readAll("read-back-");
    size_t slots = 1;
    while (slots < depth) slots *= 2;
    for (uint32_t address : std::set<uint32_t>{uint32_t(depth), uint32_t(slots), 0x80000000u, 0xffffffffu}) {
        const auto prefix = "invalid-" + std::to_string(address) + "-";
        action(prefix + "read", {{"WORD_ADDR", logicBits(32, address)}, {"READ_EN", logicBit(true)}}, 0, true);
        action(prefix + "disabled", {{"READ_EN", logicBit(false)}}, 0);
        action(prefix + "write-enable", {{"WRITE_DATA", logicBits(width, 0)}, {"WE", logicBit(true)}}, 0, true);
        action(prefix + "write-edge", {{"CLK", logicBit(true)}}, 0, true);
        action(prefix + "write-end", {{"WE", logicBit(false)}, {"CLK", logicBit(false)}}, 0);
    }
    readAll("after-invalid-writes-");
    action("disable-after-range-checks", {{"READ_EN", logicBit(false)}}, 0);
    if (model == "rom") action("reject-rom-write", {{"WORD_ADDR", logicBits(32, 0)}, {"WE", logicBit(true)}}, 0, true);
    if (model != "rom") {
        action("masked-setup", {{"WORD_ADDR", logicBits(32, 0)}, {"WRITE_DATA", logicBits(width, 0x12345678)},
            {"WRITE_MASK", logicBits(bytes, 1)}, {"WE", logicBit(false)}, {"CLK", logicBit(false)}}, 0);
        action("masked-enable", {{"WE", logicBit(true)}}, 0);
        action("masked-edge", {{"CLK", logicBit(true)}}, 0);
        action("masked-end", {{"WE", logicBit(false)}, {"CLK", logicBit(false)}}, 0);
        action("masked-read", {{"READ_EN", logicBit(true)}}, 0xa5f03c78u & mask);
        action("reset-retention", {{"RST", logicBit(true)}}, model == "sram" ? (0xa5f03c78u & mask) : 0);
    }
    spec.scenarios.push_back(std::move(scenario));
    return spec;
}
}
FFMemoryContainerTest::FFMemoryContainerTest() : ComponentScenarioTest(specFor("ff", 8)) {}
SRAMContainerTest::SRAMContainerTest() : ComponentScenarioTest(specFor("sram", 8)) {}
ROMContainerTest::ROMContainerTest() : ComponentScenarioTest(specFor("rom", 8)) {}

circuit::test::ComponentTestSpec ffMemorySpec(size_t width, size_t depth) {
    return specFor("ff", width, depth);
}

circuit::BuildProfile ffMemoryProfile(const std::string& mode, size_t width, const std::string& root) {
    using namespace circuit;
    auto profile = withExactFidelity(canonicalDefaultProfile(), root,
        mode == "behavioral" ? Fidelity::Behavioral : Fidelity::Structural, "ff-memory-" + mode);
    if (mode == "cells") return withProfileOverrides(profile,
        {{ProfileSelector::contract(families::FlipFlopCell), Fidelity::Behavioral, "compact FF cells"}});
    if (mode == "words") return withProfileOverrides(profile,
        {{ProfileSelector::contract("memory.ff-word." + std::to_string(width)), Fidelity::Behavioral, "compact FF words"}});
    if (mode == "bank") return withExactFidelity(profile, root + ".STORAGE", Fidelity::Behavioral, "compact FF bank");
    if (mode != "structural" && mode != "behavioral") throw std::invalid_argument("Unknown FF profile mode");
    return profile;
}

namespace {
ComponentTestSpec bankSpec(size_t width, size_t depth) {
    ComponentTestSpec spec;
    spec.id = "FFStorageBankTest";
    spec.contract_id = "memory.ff-storage-bank." + std::to_string(width);
    spec.instance_name = "FF_STORAGE_BANK_ROOT";
    spec.parameters = {{"depth_words", std::to_string(depth)}};
    ActionScenario scenario; scenario.id = "parallel-words";
    NamedValues state{{"WRITE_DATA", logicBits(width, 0)}, {"WRITE_MASK", logicBits(width / 8, 0)},
        {"CLK", logicBit(false)}, {"RST", logicBit(true)}};
    for (size_t n = 0; n < depth; ++n) state["WRITE_" + std::to_string(n)] = logicBit(false);
    std::vector<uint32_t> expected(depth, 0);
    auto action = [&](const std::string& label, NamedValues changes) {
        for (auto& [name, value] : changes) state[name] = value;
        NamedValues outputs;
        for (size_t n = 0; n < depth; ++n) outputs["WORD_" + std::to_string(n)] = logicBits(width, expected[n]);
        scenario.actions.push_back({label, CheckpointKind::Settled, state, outputs, true, {}});
    };
    action("reset", {});
    action("release-reset", {{"RST", logicBit(false)}});
    std::set<uint32_t> selections{0, (1u << depth) - 1};
    if (depth <= 3) for (uint32_t select = 0; select < (1u << depth); ++select) selections.insert(select);
    else {
        for (size_t n = 0; n < depth; ++n) selections.insert(1u << n);
        selections.insert(0x55u & ((1u << depth) - 1));
        selections.insert(0xaau & ((1u << depth) - 1));
    }
    for (auto select : selections) for (uint32_t mask = 0; mask < (1u << (width / 8)); ++mask) {
        const auto prefix = "select-" + std::to_string(select) + "-mask-" + std::to_string(mask) + "-";
        const uint32_t value = 0xa5f03c81u ^ (select * 0x11223345u) ^ (mask * 0x01020408u);
        NamedValues setup{{"CLK", logicBit(false)}, {"WRITE_DATA", logicBits(width, value)},
                          {"WRITE_MASK", logicBits(width / 8, mask)}};
        for (size_t n = 0; n < depth; ++n) setup["WRITE_" + std::to_string(n)] = logicBit((select & (1u << n)) != 0);
        action(prefix + "setup", setup);
        for (size_t n = 0; n < depth; ++n) if (select & (1u << n)) {
            for (size_t byte = 0; byte < width / 8; ++byte) if (mask & (1u << byte)) {
                const auto lane = uint32_t{0xff} << (8 * byte);
                expected[n] = (expected[n] & ~lane) | (value & lane);
            }
        }
        action(prefix + "write-edge", {{"CLK", logicBit(true)}});
        action(prefix + "high-is-not-edge", {{"WRITE_DATA", logicBits(width, ~value)}});
        action(prefix + "falling-is-not-write", {{"CLK", logicBit(false)}});
    }
    expected.assign(depth, 0);
    action("asynchronous-reset", {{"RST", logicBit(true)}});
    action("reset-priority-edge", {{"CLK", logicBit(true)}});
    action("release-without-edge", {{"RST", logicBit(false)}});
    action("reset-again", {{"RST", logicBit(true)}});
    spec.scenarios.push_back(std::move(scenario));
    return spec;
}

ComponentTestSpec wordSpec(size_t width) {
    ComponentTestSpec spec;
    spec.id = "FFWordTest";
    spec.contract_id = "memory.ff-word." + std::to_string(width);
    spec.instance_name = "FF_WORD_ROOT";
    ActionScenario scenario; scenario.id = "masked-word";
    NamedValues state{{"WRITE_DATA", logicBits(width, 0)}, {"WRITE_MASK", logicBits(width / 8, 0)},
        {"WE", logicBit(false)}, {"CLK", logicBit(false)}, {"RST", logicBit(true)}};
    uint32_t expected = 0;
    auto action = [&](const std::string& id, NamedValues changes) {
        for (auto& [name, value] : changes) state[name] = value;
        scenario.actions.push_back({id, CheckpointKind::Settled, state,
            {{"Q", logicBits(width, expected)}}, true, {}});
    };
    action("reset", {});
    action("release-reset", {{"RST", logicBit(false)}});
    for (uint32_t mask = 0; mask < (1u << (width / 8)); ++mask) {
        const auto id = "mask-" + std::to_string(mask) + "-";
        const auto value = 0xa5f03c81u ^ (mask * 0x11223345u);
        action(id + "setup", {{"CLK", logicBit(false)}, {"WE", logicBit(false)},
            {"WRITE_DATA", logicBits(width, value)}, {"WRITE_MASK", logicBits(width / 8, mask)}});
        action(id + "enable", {{"WE", logicBit(true)}});
        for (size_t byte = 0; byte < width / 8; ++byte) {
            const auto lane = uint32_t{0xff} << (byte * 8);
            if (mask & (1u << byte)) expected = (expected & ~lane) | (value & lane);
        }
        action(id + "write-edge", {{"CLK", logicBit(true)}});
        action(id + "high-is-not-edge", {{"WRITE_DATA", logicBits(width, ~value)}});
        action(id + "falling-is-not-write", {{"CLK", logicBit(false)}, {"WE", logicBit(false)}});
        action(id + "disabled-edge", {{"CLK", logicBit(true)}});
    }
    expected = 0;
    action("asynchronous-reset", {{"RST", logicBit(true)}, {"WE", logicBit(true)}});
    action("reset-priority-low", {{"CLK", logicBit(false)}});
    action("reset-priority-edge", {{"CLK", logicBit(true)}});
    action("release-without-edge", {{"RST", logicBit(false)}});
    spec.scenarios.push_back(std::move(scenario));
    return spec;
}
std::shared_ptr<Component> childNamed(const std::shared_ptr<Component>& root, const std::string& name) {
    for (const auto& child : root->getChildren()) if (child->getName() == name) return child;
    throw std::runtime_error("Missing child " + name);
}
}

FFWordTest::FFWordTest() : ComponentScenarioTest(wordSpec(16)) {}
FFStorageBankTest::FFStorageBankTest() : ComponentScenarioTest(bankSpec(16, 3)) {}

void FFStorageBankContractTest::verifyResults() {
    using namespace circuit;
    ComponentTestRunner runner;
    const auto& catalog = builtinComponentCatalog();
    for (size_t width : {8, 16, 32}) for (size_t depth : {1, 3, 7}) {
        const auto spec = bankSpec(width, depth);
        const auto results = runner.runAll(spec, spec.scenarios.front());
        require(results.targets.size() == 2, "FF bank must expose both implementations");
        runner.compare(results.targets[0], results.targets[1]);
        for (const auto& artifact : results.targets) {
            auto io = std::dynamic_pointer_cast<IOComponent>(artifact.root);
            require(io->getAllInputPins().size() == 4 + depth && io->getAllOutputPins().size() == depth,
                    "Bank parameters did not define the exact pin count");
        }
        for (const auto& child : std::vector<std::string>{"memory.ff-word." + std::to_string(width), "memory.write-enabled-bit"}) {
            auto profile = withProfileOverrides(canonicalDefaultProfile(),
                {{ProfileSelector::contract(child), Fidelity::Behavioral, "compact bank children"}});
            const auto mixed = runner.run(spec, spec.scenarios.front(), profile);
            runner.compare(results.targets[0], mixed);
            for (const auto& word : mixed.root->getChildren()) {
                if (child.starts_with("memory.ff-word.")) {
                    require(word->getSelectedFidelity() == "behavioral" && word->getChildren().empty(),
                            "Bank bypassed word fidelity selection");
                } else for (size_t bit = 0; bit < width; ++bit)
                    require(childNamed(word, "CELL_" + std::to_string(bit))->getSelectedFidelity() == "behavioral",
                            "Bank bypassed cell fidelity selection");
            }
        }
        // Capture X data without poisoning unselected words, then recover with reset.
        auto unknown = spec;
        auto& actions = unknown.scenarios.front().actions;
        actions.resize(2);
        auto state = actions.back().inputs;
        auto outputs = actions.back().expected_outputs;
        const LogicVector x(width, LogicValue::UNKNOWN);
        state["WRITE_0"] = logicBit(true);
        state["WRITE_MASK"] = logicBits(width / 8, 15);
        state["WRITE_DATA"] = x;
        actions.push_back({"x-setup", CheckpointKind::Settled, state, outputs, true, {}});
        state["CLK"] = logicBit(true); outputs["WORD_0"] = x;
        actions.push_back({"x-edge", CheckpointKind::Settled, state, outputs, true, {}});
        state["RST"] = logicBit(true); outputs["WORD_0"] = logicBits(width, 0);
        actions.push_back({"x-reset", CheckpointKind::Settled, state, outputs, true, {}});
        const auto xResults = runner.runAll(unknown, unknown.scenarios.front());
        runner.compare(xResults.targets[0], xResults.targets[1]);
        actions.resize(1); actions[0].inputs["RST"] = logicBit(false);
        for (auto& [pin, bits] : actions[0].expected_outputs) bits = x;
        const auto startup = runner.runAll(unknown, unknown.scenarios.front());
        runner.compare(startup.targets[0], startup.targets[1]);
    }
    for (size_t width : {8, 16, 32}) {
        const auto id = "memory.ff-storage-bank." + std::to_string(width);
        for (const auto& p : std::vector<ParameterMap>{{{"depth_words", "0"}}, {{"depth_words", "-1"}},
                {{"depth_words", "3x"}}, {{"depth_words", std::to_string(4096 / width + 1)}},
                {{"read_delay_ticks", "1"}}}) {
            for (const auto fidelity : {Fidelity::Structural, Fidelity::Behavioral})
                rejects([&] { catalog.createRoot({id, "BANK", p, {}, {}, {}},
                    withExactFidelity(canonicalDefaultProfile(), "BANK", fidelity)); });
        }
        // Both implementations accept the upper boundary; the behavioral bank still has N output ports.
        const auto max = std::to_string(4096 / width);
        for (const auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
            auto root = catalog.createRoot({id, "BANK", {{"depth_words", max}}, {}, {}, {}},
                withExactFidelity(canonicalDefaultProfile(), "BANK", fidelity)).root;
            require(std::dynamic_pointer_cast<IOComponent>(root)->getAllOutputPins().size() == 4096 / width,
                    "Maximum bank capacity lost output pins");
        }
    }
    // A wrong-depth factory must be rejected by the parameterized contract schema,
    // not accidentally accepted because its default four-word pin list looks valid.
    auto contract = catalog.contract("memory.ff-storage-bank.8");
    auto impl = catalog.implementationsFor(contract.id).front();
    impl.fidelity = Fidelity::Structural;
    impl.id = "bank.wrong-depth";
    // Use word pins without registered descendants to isolate schema validation.
    // The deliberately wrong factory below is still a structural IOComponent.
    ComponentCatalog malformed;
    malformed.registerContract(contract);
    impl.factory = [](const std::string& name, const ParameterMap&, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<IOComponent>(context, name,
            [](IOComponent* self) { defineFFStorageBankPins<8>(self, 4); });
    };
    malformed.registerImplementation(impl); malformed.freeze();
    bool schema_rejected = false;
    try {
        malformed.createRoot({contract.id, "BANK", {{"depth_words", "3"}}, {}, {}, {}}, canonicalDefaultProfile());
    } catch (const std::runtime_error& error) {
        schema_rejected = std::string(error.what()).find("Pin schema mismatch") != std::string::npos;
    }
    require(schema_rejected, "Malformed bank was not rejected by parameterized pin validation");
}

void FFWordContractTest::verifyResults() {
    using namespace circuit;
    ComponentTestRunner runner;
    for (size_t width : {8, 16, 32}) {
        const auto spec = wordSpec(width);
        const auto result = runner.runAll(spec, spec.scenarios.front());
        require(result.targets.size() == 2, "FFWord must expose both implementations");
        runner.compare(result.targets[0], result.targets[1]);
        auto profile = withProfileOverrides(canonicalDefaultProfile(),
            {{ProfileSelector::contract(families::FlipFlopCell), Fidelity::Behavioral, "test compact cells"}});
        const auto mixed = runner.run(spec, spec.scenarios.front(), profile);
        runner.compare(result.targets[0], mixed);
        for (size_t bit = 0; bit < width; ++bit) {
            auto cell = childNamed(mixed.root, "CELL_" + std::to_string(bit));
            require(cell->getImplementationId() == "memory.write-enabled-bit.behavioral.direct", "FFWord bypassed cell selection");
        }
        // Unknown power-up state and X data are not silently coerced to zero.
        auto unknown = spec;
        unknown.scenarios.front().actions.resize(1);
        auto& initial = unknown.scenarios.front().actions.front();
        initial.inputs["RST"] = logicBit(false);
        initial.expected_outputs["Q"] = std::vector<LogicValue>(width, LogicValue::UNKNOWN);
        auto startup = runner.runAll(unknown, unknown.scenarios.front());
        runner.compare(startup.targets[0], startup.targets[1]);
        auto dataX = spec;
        auto& actions = dataX.scenarios.front().actions;
        actions.resize(2);
        auto append = [&](const char* id, NamedValues changes, const LogicVector& expected) {
            auto state = actions.back().inputs;
            for (const auto& [pin, value] : changes) state[pin] = value;
            actions.push_back({id, CheckpointKind::Settled, state, {{"Q", expected}}, true, {}});
        };
        const LogicVector unknownBits(width, LogicValue::UNKNOWN);
        append("unknown-data-setup", {{"WRITE_DATA", unknownBits}, {"WRITE_MASK", logicBits(width / 8, 15)},
            {"WE", logicBit(true)}}, logicBits(width, 0));
        append("unknown-data-edge", {{"CLK", logicBit(true)}}, unknownBits);
        auto xResult = runner.runAll(dataX, dataX.scenarios.front());
        runner.compare(xResult.targets[0], xResult.targets[1]);
    }
}

void FFMemoryFidelityTest::verifyResults() {
    using namespace circuit;
    ComponentTestRunner runner;
    for (size_t width : {8, 16, 32}) for (size_t depth : {1, 3, 7}) {
        const auto spec = ffMemorySpec(width, depth);
        const auto baseline = runner.run(spec, spec.scenarios.front());
        require(baseline.root->getTypeName() == "FFMemory" + std::to_string(width), "Missing FF device identity");
        for (const std::string mode : {"cells", "words", "bank", "behavioral"}) {
            const auto result = runner.run(spec, spec.scenarios.front(), ffMemoryProfile(mode, width, spec.instance_name));
            runner.compare(baseline, result);
            if (mode == "behavioral") {
                require(result.root->getChildren().empty(), "Behavioral device expanded storage cells");
                continue;
            }
            const auto bank = childNamed(result.root, "STORAGE");
            require(bank->getTypeName() == "FFStorageBank<" + std::to_string(width) + "," + std::to_string(depth) + ">", "Wrong FF bank");
            if (mode == "bank") {
                require(bank->getChildren().empty(), "Behavioral bank expanded words");
                require(bank->getContractId() == "memory.ff-storage-bank." + std::to_string(width), "Bank not selectable");
                require(result.root->getChildren().size() == 4, "Bank selection collapsed surrounding circuitry");
                continue;
            }
            for (const auto& word : bank->getChildren()) {
                require(word->getContractId() == "memory.ff-word." + std::to_string(width), "Word has no public contract");
                if (mode == "words") require(word->getChildren().empty(), "Behavioral word expanded cells");
                else for (size_t bit = 0; bit < width; ++bit)
                    require(childNamed(word, "CELL_" + std::to_string(bit))->getImplementationId() ==
                        "memory.write-enabled-bit.behavioral.direct", "Cell override ignored inside array");
            }
        }
        auto profile = withExactFidelity(canonicalDefaultProfile(), spec.instance_name + ".STORAGE.WORD_0", Fidelity::Behavioral);
        if (depth > 1) profile = withExactFidelity(profile, spec.instance_name + ".STORAGE.WORD_1.CELL_0", Fidelity::Behavioral);
        const auto mixed = runner.run(spec, spec.scenarios.front(), profile);
        runner.compare(baseline, mixed);
        require(childNamed(childNamed(mixed.root, "STORAGE"), "WORD_0")->getChildren().empty(), "Exact word override ignored");
    }
    // Family construction works without a profile, and with catalog parameters.
    auto parent = Component::create<IOComponent>("PARENT");
    ComponentBuilder builder(parent);
    require(builder.add(families::FFMemory<32>(), "MEM")->getTypeName() == std::string("FFMemory32"), "FF family default failed");
    auto device = builtinComponentCatalog().createRoot(families::FFMemory<16>().request("MEM", {{"depth_words", "7"}}), canonicalDefaultProfile()).root;
    require(childNamed(device, "STORAGE")->getChildren().size() == 7, "FF family lost runtime depth");
    auto compact = builtinComponentCatalog().createRoot(families::FFMemory<32>().request("LARGE", {{"depth_words", "4096"}}),
        withExactFidelity(canonicalDefaultProfile(), "LARGE", Fidelity::Behavioral)).root;
    require(compact->getChildren().empty(), "Large FF memory expanded structure");
    auto direct = Component::create<FFMemory<16>>("DIRECT", MemoryConfig{.depth_words = 3});
    require(childNamed(direct, "STORAGE")->getChildren().size() == 3, "Context-free device lost bank depth");
}

void MemoryContainerContractTest::verifyResults() {
    circuit::test::ComponentTestRunner runner;
    for (const std::string model : {"ff", "sram", "rom", "ideal-reference"}) {
        for (const size_t width : {8,16,32}) {
            for (const size_t depth : {1,2,3,4,7,8,16,32,33}) {
                auto spec = specFor(model, width, depth);
                auto result = runner.runAll(spec, spec.scenarios.front());
                require(result.targets.size() == (model == "ideal-reference" ? 1 : 2), "Wrong memory fidelity availability");
                if (result.targets.size() == 2) runner.compare(result.targets[0], result.targets[1]);
                for (const auto& target : result.targets) {
                    if (target.root_fidelity != circuit::Fidelity::Structural) continue;
                    std::map<std::string, std::shared_ptr<IOComponent>> blocks;
                    for (const auto& child : target.root->getChildren()) {
                        auto io = std::dynamic_pointer_cast<IOComponent>(child);
                        require(io && !std::dynamic_pointer_cast<BasicComponent>(child), "Array block must be structural");
                        blocks.emplace(child->getName(), io);
                    }
                    require(blocks.size() == 4 && blocks.contains("ADDRESS_DECODE") && blocks.contains("ACCESS_CONTROL") &&
                            blocks.contains("STORAGE") && blocks.contains("READ_SELECT"), "MemoryArray lost its four-block hierarchy");
                    const auto& words = blocks.at("STORAGE")->getChildren();
                    require(words.size() == depth, "Storage bank allocated the wrong number of words");
                    for (size_t word = 0; word < depth; ++word) {
                        require(words[word]->getName() == "WORD_" + std::to_string(word), "Storage word has unstable identity");
                        size_t cells = 0, byteEnables = 0;
                        for (const auto& child : words[word]->getChildren()) {
                            const auto& name = child->getName();
                            if (name.starts_with("CELL_")) {
                                ++cells;
                                require(child->getTypeName() == std::string(model == "ff" ? "FlipFlopCell" :
                                    model == "sram" ? "GatedDLatch" : "ConstantValue"), "MemoryWord built the wrong cell type");
                            }
                            if (name.starts_with("BYTE_")) ++byteEnables;
                            if (model == "rom") require(name != "DATA" && name != "MASK", "ROM has unused write splitters");
                        }
                        require(cells == width, "MemoryWord allocated the wrong number of storage cells");
                        require(byteEnables == (model == "rom" ? 0 : width / 8), "MemoryWord has incorrect byte enables");
                    }
                    for (const auto& child : blocks.at("READ_SELECT")->getChildren()) {
                        require(child->getTypeName() != std::string("BitSplitter"), "Read grouping added a splitter delay");
                        if (auto rewire = std::dynamic_pointer_cast<Rewire>(child)) require(rewire->getDelay() == 0, "Word unpacking adds delay");
                        if (depth == 1) require(child->getTypeName() != std::string("Mux2to1"), "One-word array has a read mux");
                    }
                    if (depth == 1) for (const auto& child : blocks.at("ADDRESS_DECODE")->getChildren())
                        require(child->getName().find("_SELECT") == std::string::npos, "One-word array has a word decoder");
                    if (model == "rom") {
                        require(blocks.at("STORAGE")->getAllInputPins().empty(), "ROM storage has write/control inputs");
                        require(blocks.at("ACCESS_CONTROL")->getAllOutputPins().size() == 2, "ROM has unused word write gates");
                    }
                }
            }
            // Guard the structural expansion boundary without simulating a
            // huge full-address sweep. Compact storage remains available.
            auto spec = specFor(model, width);
            spec.scenarios.front().actions.resize(2);
            const size_t maxDepth = 4096 / width;
            spec.parameters["depth_words"] = std::to_string(maxDepth);
            require(runner.availableFidelities(spec).size() == (model == "ideal-reference" ? 1 : 2),
                    "Structural maximum depth rejected");
            spec.parameters["depth_words"] = std::to_string(maxDepth + 1);
            auto result = runner.runAll(spec, spec.scenarios.front());
            require(result.targets.size() == 1 && result.targets[0].root->getChildren().empty(), "Oversized array expanded into cells");
        }
    }
    using Array = MemoryArray<32, FlipFlopCell>;
    static_assert(std::is_same_v<Array::CellType, FlipFlopCell>);
    auto single = Component::create<Array>("ONE_WORD", MemoryConfig{.depth_words = 1});
    require(single->configuration().depth_words == 1, "Direct MemoryArray construction ignored depth");
    auto latchArray = Component::create<MemoryArray<8, GatedDLatch>>("LATCH_ARRAY", MemoryConfig{.depth_words = 3});
    auto zeroArray = Component::create<MemoryArray<8, ConstantValue<1>>>("ZERO_ARRAY");
    require(latchArray->configuration().depth_words == 3 && latchArray->getTypeName() == std::string("SRAM8"),
            "Direct latch array lost its geometry or logical name");
    require(zeroArray->configuration().initial_value == LogicValue::LOW && zeroArray->getTypeName() == std::string("ROM8"),
            "Direct constant array lost its default image or logical name");
    rejects([] { Component::create<Array>("EMPTY", MemoryConfig{.depth_words = 0}); });
    rejects([] { Component::create<Array>("OVERSIZED", MemoryConfig{.depth_words = Array::MaxDepthWords + 1}); });
    rejects([] { Component::create<Array>("OVERFLOW", MemoryConfig{.depth_words = std::numeric_limits<size_t>::max()}); });
    rejects([] { Component::create<Array>("DELAY", MemoryConfig{.read_delay_ticks = 2}); });
    rejects([] { Component::create<Array>("FILL", MemoryConfig{.initial_value = LogicValue::HIGH}); });
    auto config = MemoryConfig{.depth_words = 4096, .read_delay_ticks = 7};
    auto memory = Component::create<LegacyMemoryContainer<32, SRAMModel>>("SCALABLE", config);
    require(memory->memoryStorage().capacityBytes() == 16384, "Runtime depth incorrect");
    memory->loadMemoryBytes(16380, {1,2,3,4});
    require(memory->memoryStorage().readBytes(16380, 4) == std::vector<uint8_t>({1,2,3,4}), "Image loading failed");
    rejects([&] { memory->loadMemoryBytes(16384, {1}); });
    rejects([] { memoryConfigFromParameters({{"depth_words", "0"}}); });
    rejects([] { memoryConfigFromParameters({{"depth_words", "4x"}}); });
    rejects([] { memoryConfigFromParameters({{"refresh_interval", "100"}}); });
    rejects([] { Component::create<LegacyMemoryContainer<32, SRAMModel>>("BAD", MemoryConfig{.depth_words = 0}); });
    rejects([] { MemoryStorage(std::numeric_limits<size_t>::max(), 32); });

    // An output wire, not the eager local pin value, defines propagation delay.
    auto timed = Component::create<LegacyMemoryContainer<32, SRAMModel>>("TIMED", config);
    timed->loadMemoryBytes(0, {0x81, 0x23, 0x45, 0x67});
    for (const auto& [name, pin] : timed->getAllInputPins()) pin->setValueFromUInt64(0);
    timed->getInputPin("READ_EN")->setValueFromUInt64(1);
    auto wire = std::make_shared<Wire<32>>("READ_DATA");
    wire->setSourcePin(timed->getOutputPin<32>("READ_DATA"));
    timed->getOutputPin<32>("READ_DATA")->connectExternal(wire);
    Simulator sim;
    timed->evaluate(0, sim);
    sim.advanceAndRecord(6);
    require(wire->getValueVector() == std::vector<LogicValue>(32, LogicValue::UNKNOWN), "Read arrived before configured delay");
    sim.advanceAndRecord(7);
    require(wire->getValue() == 0x67452381, "Read missing after configured delay");
    rejects([&] { timed->loadMemoryBytes(0, {1}); });
    rejects([&] { timed->evaluate(std::numeric_limits<size_t>::max(), sim); });

    // Same public port, different write/reset semantics. Inputs remain stable
    // around writes; array access is not a request/response transaction.
    auto ff = Component::create<LegacyMemoryContainer<16, FlipFlopArrayModel>>("FF", MemoryConfig{.initial_value = LogicValue::LOW});
    auto sram = Component::create<LegacyMemoryContainer<16, SRAMModel>>("SRAM", MemoryConfig{.initial_value = LogicValue::LOW});
    auto rom = Component::create<LegacyMemoryContainer<16, ROMModel>>("ROM");
    rom->loadMemoryBytes(0, {0x80, 0x12});
    for (const auto& device : std::vector<std::shared_ptr<BasicComponent>>{ff, sram, rom}) {
        Simulator local;
        for (const auto& [name, pin] : device->getAllInputPins()) pin->setValueFromUInt64(0);
        auto set = [&](const std::string& name, uint64_t value) { device->getInputPinDynamic(name)->setValueFromUInt64(value); };
        auto step = [&](size_t time) { device->evaluate(time, local); local.advanceAndRecord(time + 1); };
        auto view = std::dynamic_pointer_cast<MemoryStateView>(device);
        const auto read = [&] { return view->memoryStorage().readBytes(0, 2); };
        step(0);
        set("WRITE_DATA", 0xabcd); set("WRITE_MASK", 3); set("WE", 1); step(2);
        require(read() == (device == sram ? std::vector<uint8_t>{0xcd,0xab} : device == rom ? std::vector<uint8_t>{0x80,0x12} : std::vector<uint8_t>{0,0}), "Write trigger model mismatch");
        set("CLK", 1); step(4);
        set("WRITE_DATA", 0x5678); step(6);
        require(read() == (device == sram ? std::vector<uint8_t>{0x78,0x56} : device == rom ? std::vector<uint8_t>{0x80,0x12} : std::vector<uint8_t>{0xcd,0xab}), "Repeated high clock caused an FF write");
        const auto retained = read();
        set("WRITE_MASK", 0); set("CLK", 0); step(8); set("CLK", 1); step(10);
        require(read() == retained, "Zero write mask changed data");
        set("WRITE_MASK", 3); set("CLK", 0); step(12);
        device->getInputPinDynamic("WORD_ADDR")->setBit(0, LogicValue::UNKNOWN);
        set("CLK", 1); step(14);
        require(read() == retained, "Unknown address corrupted a known word");
        require(device->getOutputPin("FAULT")->getValue() == LogicValue::UNKNOWN, "Unknown address not reported");
        set("WE", 0); set("READ_EN", 0); step(16);
        require(device->getOutputPin("FAULT")->getValue() == LogicValue::LOW, "Disabled access depends on ignored address");
        require(device->getOutputPinDynamic("READ_DATA")->getValueAsUInt64() == 0, "Disabled read not zero");
        set("RST", 1); step(18);
        require(read() == (device == ff ? std::vector<uint8_t>{0,0} : retained), "Reset retention contract mismatch");
    }
}

void MemoryStorageTest::verifyResults() {
    MemoryStorage storage(17, 16, LogicValue::LOW);
    require(storage.capacityBytes() == 34 && storage.allocatedDataBytes() == 68, "Packed geometry incorrect");
    require(storage.historicalByteCount() == 0, "History allocated for untouched bytes");
    std::vector<uint8_t> reference(34, 0);
    std::mt19937 rng(42);
    for (size_t time = 1; time <= 200; ++time) {
        const auto address = uint32_t(rng() % reference.size());
        reference[address] = uint8_t(rng());
        storage.writeLogic(time, address, circuit::test::logicBits(8, reference[address]), true);
        require(storage.readBytes(0, 34) == reference, "Storage random write mismatch");
        auto snapshot = storage.getWordsAtTime(time, 0, 17);
        for (size_t word = 0; word < 17; ++word)
            require(snapshot[word].second == circuit::test::logicBits(16, reference[2*word] | (reference[2*word+1] << 8)), "Historical snapshot mismatch");
    }
    storage.fill(201, LogicValue::UNKNOWN);
    require(storage.getTouchedWordCountAtTime(201) == 0, "Reset did not hide old touched state");
    require(storage.getWordsAtTime(200, 0, 1)[0].second != std::vector<LogicValue>(16, LogicValue::UNKNOWN), "Reset overwrote earlier history");
    rejects([&] { storage.readBytes(0, 1); });
    storage.writeLogic(201, 0, circuit::test::logicBits(16, 0x1234), true);
    require(storage.getWordsAtTime(201, 0, 1)[0].second == circuit::test::logicBits(16, 0x1234), "Same-time reset/write order wrong");
    auto unknown = circuit::test::logicBits(8, 0xff); unknown[3] = LogicValue::HIGH_Z;
    storage.writeLogic(202, 2, unknown);
    require(storage.readLogic(2, 1)[3] == LogicValue::UNKNOWN, "Z was not normalized");
    require(storage.getByteWritesInTimeRange(200, 201).at(0) == 0x34, "Bus write trace mismatch");
    rejects([&] { storage.getByteWritesInTimeRange(201, 200); });
    rejects([&] { storage.writeLogic(203, 34, circuit::test::logicBits(8, 1)); });
    rejects([&] { storage.readBytes(0, std::numeric_limits<size_t>::max()); });
    storage.setHistoryRecordingEnabled(false);
    require(storage.historicalByteCount() == 0, "History was not released");
    rejects([&] { storage.getWordsAtTime(201, 0, 1); });
    storage.writeLogic(204, 0, circuit::test::logicBits(8, 42));
    require(storage.readBytes(0, 1)[0] == 42, "History-off write failed");
    storage.setHistoryRecordingEnabled(true);
    require(storage.getWordsAtTime(0, 0, 1)[0].second == storage.readLogic(0, 2),
            "Re-enabled history did not preserve the recording epoch baseline");
    storage.fill(1, LogicValue::LOW);
    require(storage.getWordsAtTime(1, 0, 1)[0].second == circuit::test::logicBits(16, 0),
            "Reset did not supersede recording epoch baseline");
}
