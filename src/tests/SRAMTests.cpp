#include "tests/SRAMTests.hpp"
#include "modules/memory/SRAM.hpp"
#include "components/ComponentBuilder.tpp"
#include <random>

namespace {
using namespace circuit;
using namespace circuit::test;
constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH, X = LogicValue::UNKNOWN, Z = LogicValue::HIGH_Z;
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
struct Scenario {
    ComponentTestSpec spec;
    NamedValues state;
    Scenario(std::string test, std::string kind, ParameterMap p = {}, NamedValues inputs = {})
        : spec{test, "memory.sram." + kind, "SRAM_ROOT", std::move(p), {{"operations", {}, {}, {}}}}, state(std::move(inputs)) {}
    void step(std::string label, NamedValues changes, NamedValues expected) {
        for (auto& [pin, value] : changes) state[pin] = value;
        spec.scenarios.front().actions.push_back({label, CheckpointKind::Settled, state, std::move(expected), true, {}});
    }
};
ComponentTestSpec cellSpec() {
    Scenario s("SRAMCellTest", "cell", {}, {{"WL", logicBit(L)}, {"DRIVE", logicBit(L)}, {"BL", logicBit(H)}, {"BL_BAR", logicBit(H)}});
    s.step("unknown-startup", {}, {{"Q", logicBit(X)}, {"Q_BAR", logicBit(X)}});
    s.step("write-zero", {{"WL", logicBit(H)}, {"DRIVE", logicBit(H)}, {"BL", logicBit(L)}}, {{"Q", logicBit(L)}, {"Q_BAR", logicBit(H)}});
    s.step("hold", {{"WL", logicBit(L)}, {"DRIVE", logicBit(L)}}, {{"Q", logicBit(L)}});
    s.step("read-is-not-write", {{"WL", logicBit(H)}, {"BL", logicBit(H)}, {"BL_BAR", logicBit(H)}}, {{"Q", logicBit(L)}});
    s.step("write-one", {{"DRIVE", logicBit(H)}, {"BL_BAR", logicBit(L)}}, {{"Q", logicBit(H)}, {"Q_BAR", logicBit(L)}});
    s.step("deselect-before-drive-change", {{"WL", logicBit(L)}, {"DRIVE", logicBit(L)}}, {{"Q", logicBit(H)}});
    s.step("unselected-write-drive", {{"DRIVE", logicBit(H)}, {"BL", logicBit(L)}, {"BL_BAR", logicBit(H)}}, {{"Q", logicBit(H)}});
    s.step("unknown-write", {{"WL", logicBit(H)}, {"BL", logicBit(X)}, {"BL_BAR", logicBit(X)}}, {{"Q", logicBit(X)}});
    s.step("recover-zero", {{"BL", logicBit(L)}, {"BL_BAR", logicBit(H)}}, {{"Q", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec rowSpec() {
    Scenario s("SRAMRowTest", "row", {{"columns", "2"}}, {{"WL", logicBit(L)}, {"DRIVE_0", logicBit(L)}, {"DRIVE_1", logicBit(L)},
        {"BL_0", logicBit(H)}, {"BL_BAR_0", logicBit(L)}, {"BL_1", logicBit(L)}, {"BL_BAR_1", logicBit(H)}});
    s.step("unknown-startup", {}, {{"Q_0", logicBit(X)}, {"Q_1", logicBit(X)}});
    s.step("write-two-columns", {{"WL", logicBit(H)}, {"DRIVE_0", logicBit(H)}, {"DRIVE_1", logicBit(H)}}, {{"Q_0", logicBit(H)}, {"Q_1", logicBit(L)}});
    s.step("release", {{"WL", logicBit(L)}, {"DRIVE_0", logicBit(L)}, {"DRIVE_1", logicBit(L)}}, {{"Q_0", logicBit(H)}, {"Q_1", logicBit(L)}});
    s.step("prepare-masked-write", {{"BL_0", logicBit(L)}, {"BL_BAR_0", logicBit(H)}, {"BL_1", logicBit(H)}, {"BL_BAR_1", logicBit(L)}, {"DRIVE_1", logicBit(H)}}, {{"Q_0", logicBit(H)}, {"Q_1", logicBit(L)}});
    s.step("write-only-column-one", {{"WL", logicBit(H)}}, {{"Q_0", logicBit(H)}, {"Q_1", logicBit(H)}});
    return s.spec;
}
ComponentTestSpec bitlineSpec() {
    Scenario s("SRAMBitlinePairTest", "bitline-pair", {{"rows", "2"}}, {{"PRECHARGE", logicBit(L)}, {"DRIVE", logicBit(L)},
        {"WRITE_BL", logicBit(L)}, {"WRITE_BL_BAR", logicBit(H)}, {"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}, {"Q_0", logicBit(L)}, {"Q_1", logicBit(H)}});
    s.step("unknown-startup", {}, {{"BL", logicBit(X)}, {"BL_BAR", logicBit(X)}, {"VALID", logicBit(L)}});
    s.step("precharge", {{"PRECHARGE", logicBit(H)}}, {{"BL", logicBit(H)}, {"BL_BAR", logicBit(H)}, {"CELL_DRIVE", logicBit(L)}});
    s.step("retain-charge", {{"PRECHARGE", logicBit(L)}}, {{"BL", logicBit(H)}, {"BL_BAR", logicBit(H)}});
    s.step("read-row-zero", {{"WL_0", logicBit(H)}}, {{"BL", logicBit(L)}, {"BL_BAR", logicBit(H)}, {"VALID", logicBit(H)}, {"CELL_DRIVE", logicBit(L)}});
    s.step("release-row", {{"WL_0", logicBit(L)}}, {{"VALID", logicBit(L)}});
    s.step("missing-precharge", {{"WL_1", logicBit(H)}}, {{"BL", logicBit(X)}, {"VALID", logicBit(L)}});
    s.step("deselect", {{"WL_1", logicBit(L)}}, {{"VALID", logicBit(L)}});
    s.step("precharge-again", {{"PRECHARGE", logicBit(H)}}, {{"BL", logicBit(H)}, {"BL_BAR", logicBit(H)}});
    s.step("release-precharge", {{"PRECHARGE", logicBit(L)}}, {{"BL", logicBit(H)}});
    s.step("read-row-one", {{"WL_1", logicBit(H)}}, {{"BL", logicBit(H)}, {"BL_BAR", logicBit(L)}, {"VALID", logicBit(H)}});
    s.step("two-row-conflict", {{"WL_0", logicBit(H)}}, {{"BL", logicBit(X)}, {"VALID", logicBit(L)}, {"CELL_DRIVE", logicBit(L)}});
    s.step("write-drive", {{"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}, {"DRIVE", logicBit(H)}}, {{"BL", logicBit(L)}, {"BL_BAR", logicBit(H)}, {"CELL_DRIVE", logicBit(H)}, {"CELL_BL", logicBit(L)}});
    s.step("precharge-write-conflict", {{"PRECHARGE", logicBit(H)}}, {{"BL", logicBit(X)}, {"CELL_DRIVE", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec decoderSpec() {
    Scenario s("SRAMRowDecoderTest", "row-decoder", {{"depth_words", "6"}, {"words_per_row", "2"}}, {{"WORD_ADDR", logicBits(32, 0)}, {"ENABLE", logicBit(L)}});
    s.step("disabled", {}, {{"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}, {"WL_2", logicBit(L)}});
    for (uint32_t a : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 0xffffffffu}) s.step("address-" + std::to_string(a), {{"WORD_ADDR", logicBits(32, a)}, {"ENABLE", logicBit(H)}},
        {{"WL_0", logicBit(a < 2)}, {"WL_1", logicBit(a >= 2 && a < 4)}, {"WL_2", logicBit(a >= 4 && a < 6)}});
    return s.spec;
}
ComponentTestSpec prechargeSpec() {
    Scenario s("SRAMPrechargeTest", "precharge", {{"columns", "2"}}, {{"ENABLE", logicBit(L)}});
    for (const auto& [label, v] : std::vector<std::pair<std::string, LogicValue>>{{"disabled", L}, {"enabled", H}, {"released", L}, {"unknown-enable", X}})
        s.step(label, {{"ENABLE", logicBit(v)}}, {{"PRECHARGE_0", logicBit(v)}, {"PRECHARGE_1", logicBit(v)}});
    return s.spec;
}
ComponentTestSpec driverSpec() {
    Scenario s("SRAMWriteDriverTest", "write-driver");
    for (unsigned d = 0; d < 2; ++d) for (unsigned en = 0; en < 2; ++en) for (unsigned mask = 0; mask < 2; ++mask)
        s.step("data-" + std::to_string(d) + "-enable-" + std::to_string(en) + "-mask-" + std::to_string(mask),
            {{"D", logicBit(bool(d))}, {"ENABLE", logicBit(bool(en))}, {"MASK", logicBit(bool(mask))}},
            {{"BL", logicBit(bool(d))}, {"BL_BAR", logicBit(!d)}, {"DRIVE", logicBit(bool(en && mask))}});
    return s.spec;
}
ComponentTestSpec senseSpec() {
    Scenario s("SRAMSenseAmplifierTest", "sense-amplifier", {}, {{"BL", logicBit(H)}, {"BL_BAR", logicBit(H)}, {"VALID", logicBit(L)}, {"ENABLE", logicBit(L)}});
    s.step("disabled", {}, {{"Q", logicBit(X)}});
    s.step("equal-pair-is-not-data", {{"ENABLE", logicBit(H)}}, {{"Q", logicBit(X)}});
    s.step("sense-zero", {{"BL", logicBit(L)}, {"VALID", logicBit(H)}}, {{"Q", logicBit(L)}});
    s.step("sense-one", {{"BL", logicBit(H)}, {"BL_BAR", logicBit(L)}}, {{"Q", logicBit(H)}});
    s.step("invalid-pair", {{"BL_BAR", logicBit(H)}}, {{"Q", logicBit(X)}});
    return s.spec;
}
ComponentTestSpec outputSpec() {
    Scenario s("SRAMOutputBufferTest", "output-buffer.8", {}, {{"DATA", logicBits(8, 0xa5)}, {"ENABLE", logicBit(L)}});
    s.step("disabled", {}, {{"OUT", LogicVector(8, Z)}});
    s.step("enabled", {{"ENABLE", logicBit(H)}}, {{"OUT", logicBits(8, 0xa5)}});
    s.step("unknown-enable", {{"ENABLE", logicBit(X)}}, {{"OUT", LogicVector(8, X)}});
    s.step("disable-again", {{"ENABLE", logicBit(L)}}, {{"OUT", LogicVector(8, Z)}});
    return s.spec;
}
ComponentTestSpec columnSpec() {
    Scenario s("SRAMColumnSelectorTest", "column-selector.8", {{"words_per_row", "2"}}, {{"WORD_ADDR", logicBits(32, 0)}});
    for (size_t bit = 0; bit < 8; ++bit) {
        const auto n = std::to_string(bit);
        s.state["WRITE_BL_" + n] = logicBit(H); s.state["WRITE_BL_BAR_" + n] = logicBit(L); s.state["WRITE_DRIVE_" + n] = logicBit(H);
    }
    for (size_t c = 0; c < 16; ++c) {
        const auto n = std::to_string(c);
        s.state["BL_" + n] = logicBit(c >= 8); s.state["BL_BAR_" + n] = logicBit(c < 8); s.state["VALID_" + n] = logicBit(H);
    }
    for (size_t slot = 0; slot < 2; ++slot) {
        NamedValues expected;
        for (size_t bit = 0; bit < 8; ++bit) {
            const auto n = std::to_string(bit);
            expected["READ_BL_" + n] = logicBit(bool(slot)); expected["READ_BL_BAR_" + n] = logicBit(!slot); expected["READ_VALID_" + n] = logicBit(H);
        }
        for (size_t c = 0; c < 16; ++c) {
            const auto n = std::to_string(c);
            expected["DRIVE_" + n] = logicBit(c / 8 == slot);
            expected["DRIVE_BL_" + n] = logicBit(H);
            expected["DRIVE_BL_BAR_" + n] = logicBit(L);
        }
        s.step("slot-" + std::to_string(slot), {{"WORD_ADDR", logicBits(32, slot)}}, expected);
    }
    return s.spec;
}
ComponentTestSpec controlSpec() {
    Scenario s("SRAMAccessControlTest", "access-control.8", {}, {{"WORD_ADDR", logicBits(32, 1)}, {"WRITE_DATA", logicBits(8, 0xa5)},
        {"WRITE_MASK", logicBits(1, 1)}, {"CE_N", logicBit(H)}, {"WE_N", logicBit(H)}, {"OE_N", logicBit(L)}});
    s.step("deselected", {}, {{"ROW_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(L)}});
    s.step("read-complete", {{"CE_N", logicBit(L)}}, {{"ADDRESS", logicBits(32, 1)}, {"PRECHARGE", logicBit(L)}, {"ROW_ENABLE", logicBit(H)}, {"SENSE_ENABLE", logicBit(H)}, {"WRITE_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(H)}});
    s.step("output-disabled", {{"OE_N", logicBit(H)}}, {{"OUTPUT_ENABLE", logicBit(L)}, {"ROW_ENABLE", logicBit(H)}});
    s.step("write-complete", {{"WE_N", logicBit(L)}}, {{"DATA", logicBits(8, 0xa5)}, {"MASK", logicBits(1, 1)}, {"WRITE_ENABLE", logicBit(H)}, {"SENSE_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(L)}});
    s.step("deselect", {{"CE_N", logicBit(H)}}, {{"ROW_ENABLE", logicBit(L)}, {"WRITE_ENABLE", logicBit(L)}, {"PRECHARGE", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec bankSpec(size_t width = 8, size_t depth = 4, size_t k = 2) {
    const auto rows = depth / k, columns = width * k;
    Scenario s("SRAMStorageBankTest", "bank." + std::to_string(width), {{"depth_words", std::to_string(depth)}, {"words_per_row", std::to_string(k)}});
    for (size_t r = 0; r < rows; ++r) s.state["WL_" + std::to_string(r)] = logicBit(L);
    for (size_t c = 0; c < columns; ++c) for (const auto* p : {"PRECHARGE_", "DRIVE_", "WRITE_BL_", "WRITE_BL_BAR_"}) s.state[p + std::to_string(c)] = logicBit(L);
    s.step("idle", {}, {{"VALID_0", logicBit(L)}});
    for (size_t r = 0; r < rows; ++r) {
        NamedValues prepare;
        for (size_t c = 0; c < columns; ++c) {
            const auto n = std::to_string(c); const bool v = (c + r) % 2;
            prepare["DRIVE_" + n] = logicBit(H); prepare["WRITE_BL_" + n] = logicBit(v); prepare["WRITE_BL_BAR_" + n] = logicBit(!v);
        }
        s.step("prepare-write-row-" + std::to_string(r), prepare, {{"VALID_0", logicBit(L)}});
        s.step("write-row-" + std::to_string(r), {{"WL_" + std::to_string(r), logicBit(H)}}, {{"VALID_0", logicBit(L)}});
        s.step("release-row-" + std::to_string(r), {{"WL_" + std::to_string(r), logicBit(L)}}, {{"VALID_0", logicBit(L)}});
    }
    NamedValues release;
    for (size_t c = 0; c < columns; ++c) release["DRIVE_" + std::to_string(c)] = logicBit(L);
    s.step("release-drivers", release, {{"VALID_0", logicBit(L)}});
    for (size_t r = 0; r < rows; ++r) {
        NamedValues charge, discharge, expected;
        for (size_t c = 0; c < columns; ++c) {
            const auto n = std::to_string(c);
            charge["PRECHARGE_" + n] = logicBit(H); discharge["PRECHARGE_" + n] = logicBit(L);
            expected["BL_" + n] = logicBit(bool((c + r) % 2)); expected["BL_BAR_" + n] = logicBit(!((c + r) % 2)); expected["VALID_" + n] = logicBit(H);
        }
        s.step("precharge-row-" + std::to_string(r), charge, {{"BL_0", logicBit(H)}, {"BL_BAR_0", logicBit(H)}});
        s.step("release-precharge-" + std::to_string(r), discharge, {{"VALID_0", logicBit(L)}});
        s.step("read-row-" + std::to_string(r), {{"WL_" + std::to_string(r), logicBit(H)}}, expected);
        s.step("end-read-" + std::to_string(r), {{"WL_" + std::to_string(r), logicBit(L)}}, {{"VALID_0", logicBit(L)}});
    }
    return s.spec;
}
}

circuit::test::ComponentTestSpec sramDeviceSpec(size_t width, size_t depth, size_t k) {
    Scenario s("SRAMTest", "device." + std::to_string(width), {{"depth_words", std::to_string(depth)}, {"words_per_row", std::to_string(k)}},
        {{"WORD_ADDR", logicBits(32, 0)}, {"WRITE_DATA", logicBits(width, 0)}, {"WRITE_MASK", logicBits(width / 8, (1u << (width / 8)) - 1)},
         {"CE_N", logicBit(H)}, {"WE_N", logicBit(H)}, {"OE_N", logicBit(L)}});
    const LogicVector z(width, Z), x(width, X);
    s.step("deselected-startup", {}, {{"READ_DATA", z}});
    s.step("unwritten-read", {{"CE_N", logicBit(L)}}, {{"READ_DATA", x}});
    std::vector<uint32_t> words(depth);
    for (size_t word = 0; word < depth; ++word) {
        words[word] = (0xa53c9681u ^ (uint32_t(word) * 0x17234567u));
        s.step("setup-word-" + std::to_string(word), {{"CE_N", logicBit(H)}, {"WE_N", logicBit(H)}, {"WORD_ADDR", logicBits(32, word)}, {"WRITE_DATA", logicBits(width, words[word])}}, {{"READ_DATA", z}});
        s.step("write-word-" + std::to_string(word), {{"CE_N", logicBit(L)}, {"WE_N", logicBit(L)}}, {{"READ_DATA", z}});
        s.step("read-word-" + std::to_string(word), {{"WE_N", logicBit(H)}}, {{"READ_DATA", logicBits(width, words[word])}});
    }
    for (size_t mask = 0; mask < (size_t{1} << (width / 8)); ++mask) {
        const size_t word = mask % depth;
        const uint32_t data = 0x76543210u ^ uint32_t(mask * 0x11223345u);
        s.step("mask-setup-" + std::to_string(mask), {{"CE_N", logicBit(H)}, {"WORD_ADDR", logicBits(32, word)}, {"WRITE_DATA", logicBits(width, data)}, {"WRITE_MASK", logicBits(width / 8, mask)}}, {{"READ_DATA", z}});
        for (size_t byte = 0; byte < width / 8; ++byte) if (mask & (size_t{1} << byte)) { auto m = 0xffu << (byte * 8); words[word] = (words[word] & ~m) | (data & m); }
        s.step("masked-write-" + std::to_string(mask), {{"CE_N", logicBit(L)}, {"WE_N", logicBit(L)}}, {{"READ_DATA", z}});
        s.step("masked-read-" + std::to_string(mask), {{"WE_N", logicBit(H)}}, {{"READ_DATA", logicBits(width, words[word])}});
    }
    s.step("disable-output", {{"OE_N", logicBit(H)}}, {{"READ_DATA", z}});
    s.step("read-last-word", {{"OE_N", logicBit(L)}, {"WORD_ADDR", logicBits(32, depth - 1)}}, {{"READ_DATA", logicBits(width, words.back())}});
    s.step("ignored-write-inputs-during-read", {{"WRITE_DATA", x}, {"WRITE_MASK", LogicVector(width / 8, X)}}, {{"READ_DATA", logicBits(width, words.back())}});
    s.step("invalid-read", {{"WORD_ADDR", logicBits(32, depth)}}, {{"READ_DATA", x}});
    s.step("invalid-write", {{"WE_N", logicBit(L)}, {"WRITE_DATA", logicBits(width, 0)}, {"WRITE_MASK", logicBits(width / 8, (1u << (width / 8)) - 1)}}, {{"READ_DATA", z}});
    s.step("end-invalid-write", {{"CE_N", logicBit(H)}, {"WE_N", logicBit(H)}}, {{"READ_DATA", z}});
    for (size_t word = 0; word < depth; ++word) s.step("readback-" + std::to_string(word), {{"CE_N", logicBit(L)}, {"WORD_ADDR", logicBits(32, word)}}, {{"READ_DATA", logicBits(width, words[word])}});
    s.step("prepare-unknown-write", {{"CE_N", logicBit(H)}, {"WORD_ADDR", logicBits(32, 0)}, {"WRITE_DATA", x}}, {{"READ_DATA", z}});
    s.step("write-unknown", {{"CE_N", logicBit(L)}, {"WE_N", logicBit(L)}}, {{"READ_DATA", z}});
    s.step("read-unknown", {{"WE_N", logicBit(H)}}, {{"READ_DATA", x}});
    s.step("unknown-address", {{"WORD_ADDR", LogicVector(32, X)}}, {{"READ_DATA", x}});
    s.step("deselect-retains", {{"CE_N", logicBit(H)}}, {{"READ_DATA", z}});
    return s.spec;
}

SRAMCellTest::SRAMCellTest() : ComponentScenarioTest(cellSpec()) {}
SRAMRowTest::SRAMRowTest() : ComponentScenarioTest(rowSpec()) {}
SRAMBitlinePairTest::SRAMBitlinePairTest() : ComponentScenarioTest(bitlineSpec()) {}
SRAMRowDecoderTest::SRAMRowDecoderTest() : ComponentScenarioTest(decoderSpec()) {}
SRAMPrechargeTest::SRAMPrechargeTest() : ComponentScenarioTest(prechargeSpec()) {}
SRAMWriteDriverTest::SRAMWriteDriverTest() : ComponentScenarioTest(driverSpec()) {}
SRAMSenseAmplifierTest::SRAMSenseAmplifierTest() : ComponentScenarioTest(senseSpec()) {}
SRAMColumnSelectorTest::SRAMColumnSelectorTest() : ComponentScenarioTest(columnSpec()) {}
SRAMAccessControlTest::SRAMAccessControlTest() : ComponentScenarioTest(controlSpec()) {}
SRAMOutputBufferTest::SRAMOutputBufferTest() : ComponentScenarioTest(outputSpec()) {}
SRAMStorageBankTest::SRAMStorageBankTest() : ComponentScenarioTest(bankSpec()) {}
SRAMTest::SRAMTest() : ComponentScenarioTest(sramDeviceSpec(8, 4, 2)) {}

void SRAMContractTest::verifyResults() {
    ComponentTestRunner runner;
    for (size_t width : {8, 16, 32}) for (const auto& [depth, k] : std::vector<std::pair<size_t, size_t>>{{1,1}, {4,2}, {6,2}, {8,4}}) {
        for (auto spec : {sramDeviceSpec(width, depth, k), bankSpec(width, depth, k)}) {
            auto results = runner.runAll(spec, spec.scenarios.front());
            require(results.targets.size() == 2, "SRAM device/bank must have both fidelities");
            runner.compare(results.targets[0], results.targets[1]);
            if (spec.contract_id.find("device") != std::string::npos) {
                auto mixed = withExactFidelity(canonicalDefaultProfile(), "SRAM_ROOT.STORAGE", Fidelity::Behavioral);
                runner.compare(results.targets[0], runner.run(spec, spec.scenarios.front(), mixed));
                if (width == 8 && depth == 4) for (const auto& path : {"SRAM_ROOT.STORAGE.ROW_0", "SRAM_ROOT.STORAGE.ROW_0.CELL_0"})
                    runner.compare(results.targets[0], runner.run(spec, spec.scenarios.front(),
                        withExactFidelity(canonicalDefaultProfile(), path, Fidelity::Behavioral)));
            }
        }
    }
    for (auto spec : {cellSpec(), rowSpec()}) {
        auto results = runner.runAll(spec, spec.scenarios.front());
        require(results.targets.size() == 2, "SRAM cell/row must have both fidelities");
        runner.compare(results.targets[0], results.targets[1]);
    }
    for (auto config : {SRAMDeviceConfig{0,1}, SRAMDeviceConfig{5,2}, SRAMDeviceConfig{4,3}, SRAMDeviceConfig{4,0}, SRAMDeviceConfig{4,2,1}}) {
        bool rejected = false; try { config.validate(8); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid SRAM geometry accepted");
    }
    SRAMDeviceConfig{size_t{1} << 25, 2}.validate(8); // Scalable devices exceed the old 16-Mword cap.
    bool oversized = false;
    try { SRAMDeviceConfig{size_t{1} << 30, 2}.validate(32); }
    catch (const std::invalid_argument&) { oversized = true; }
    require(oversized, "SRAM exceeded its 32-bit byte capacity");
    bool rejected = false; try { sramConfigFromParameters({{"typo", "4"}}); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Unknown SRAM parameter accepted");
    // Nontrivial geometry and independently maintained randomized byte-write oracle.
    Scenario random("SRAMRandom", "device.16", {{"depth_words", "6"}, {"words_per_row", "2"}},
        {{"WORD_ADDR", logicBits(32, 0)}, {"WRITE_DATA", logicBits(16, 0)}, {"WRITE_MASK", logicBits(2, 3)},
         {"CE_N", logicBit(H)}, {"WE_N", logicBit(H)}, {"OE_N", logicBit(L)}});
    std::vector<LogicVector> expected(6, LogicVector(16, X));
    std::mt19937 rng(0x5352414d);
    for (unsigned i = 0; i < 32; ++i) {
        const auto address = rng() % 6, data = rng() & 0xffff, mask = rng() % 4;
        random.step("setup-" + std::to_string(i), {{"CE_N", logicBit(H)}, {"WE_N", logicBit(H)},
            {"WORD_ADDR", logicBits(32, address)}, {"WRITE_DATA", logicBits(16, data)}, {"WRITE_MASK", logicBits(2, mask)}}, {{"READ_DATA", LogicVector(16, Z)}});
        for (size_t bit = 0; bit < 16; ++bit) if (mask & (1u << (bit / 8))) expected[address][bit] = data & (1u << bit) ? H : L;
        random.step("write-" + std::to_string(i), {{"CE_N", logicBit(L)}, {"WE_N", logicBit(L)}}, {{"READ_DATA", LogicVector(16, Z)}});
        random.step("read-" + std::to_string(i), {{"WE_N", logicBit(H)}}, {{"READ_DATA", expected[address]}});
    }
    auto randomRuns = runner.runAll(random.spec, random.spec.scenarios.front());
    runner.compare(randomRuns.targets[0], randomRuns.targets[1]);
}

void SRAMTimingTest::verifyResults() {
    // Explicit sub-phase tests; settled examples alone cannot catch stale timers.
    for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral}) {
        const auto built = builtinComponentCatalog().createRoot({"memory.sram.device.8", "DUT", {}, {}, {}, {}},
            withExactFidelity(canonicalDefaultProfile(), "DUT", fidelity));
        auto io = std::dynamic_pointer_cast<IOComponent>(built.root);
        ComponentBuilder builder(io); Simulator sim;
        SimulationTest::scheduleInitialEventsForTree(io, sim, 0);
        std::map<std::string, std::shared_ptr<WireBase>> inputs;
        for (const auto& [name, pin] : io->getAllInputPins()) inputs[name] = builder.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        auto set = [&](size_t t, const std::string& pin, TestValue value) { sim.scheduleEvent(makeTestWireUpdate(t, inputs.at(pin), value)); };
        set(0, "CE_N", H); set(0, "WE_N", H); set(0, "OE_N", L); set(0, "WORD_ADDR", uint64_t{0}); set(0, "WRITE_DATA", uint64_t{0xa5}); set(0, "WRITE_MASK", H);
        // Cancel a write before row activation; a stale event must never write it.
        set(10, "CE_N", L); set(10, "WE_N", L); set(20, "CE_N", H);
        set(24, "CE_N", L); set(24, "WE_N", H);
        sim.advanceAndRecord(1000);
        require(io->getOutputPin<8>("READ_DATA")->getValueAsVector() == LogicVector(8, X), "Short cancelled write changed SRAM contents");
        // Normal pulse, then address change during an in-flight read.
        set(1100, "WE_N", L); set(1400, "WE_N", H);
        sim.advanceAndRecord(1410);
        require(io->getOutputPin<8>("READ_DATA")->getValueAsVector() == LogicVector(8, Z), "Read returned before access completed");
        sim.advanceAndRecord(1700);
        require(io->getOutputPin<8>("READ_DATA")->getValueAsUInt64() == 0xa5, "Completed write not readable");
        set(1800, "WORD_ADDR", uint64_t{1}); set(1810, "WORD_ADDR", uint64_t{0});
        sim.advanceAndRecord(1820);
        require(io->getOutputPin<8>("READ_DATA")->getValueAsVector() == LogicVector(8, Z), "Superseded access exposed stale output");
        sim.advanceAndRecord(2100);
        require(io->getOutputPin<8>("READ_DATA")->getValueAsUInt64() == 0xa5, "Latest request lost to old timer");
        require(!sim.hasPendingEvents(), "SRAM should quiesce without a periodic clock");
    }
}
