#include "tests/MemoryDeviceTests.hpp"
#include "modules/memory/DRAM.hpp"
#include "modules/memory/NORFlash.hpp"
#include "modules/memory/ROM.hpp"
#include <random>
using namespace circuit;
using namespace circuit::test;
namespace {
constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH, X = LogicValue::UNKNOWN, Z = LogicValue::HIGH_Z;
void require(bool v, const std::string &m) {
    if (!v)
        throw std::runtime_error(m);
}
struct Scenario {
    ComponentTestSpec spec;
    NamedValues state;
    Scenario(std::string name, std::string id, ParameterMap p = {}, NamedValues inputs = {})
        : spec{std::move(name), std::move(id), "MEMORY_ROOT", std::move(p), {{"operations", {}, {}, {}}}},
          state(std::move(inputs)) {}
    void step(std::string label, NamedValues changes, NamedValues expected) {
        for (auto &[pin, v] : changes)
            state[pin] = v;
        spec.scenarios[0].actions.push_back(
            {std::move(label), CheckpointKind::Settled, state, std::move(expected), true, {}});
    }
    void cmd(std::string label, unsigned op, unsigned addr, NamedValues expected = {},
             NamedValues data = {}) {
        data["CLK"] = logicBit(L);
        data["CMD"] = logicBits(3, op);
        data[spec.contract_id.find("dram") != std::string::npos ? "ADDR" : "WORD_ADDR"] = logicBits(32, addr);
        step(label + "-setup", std::move(data), {});
        expected["BUSY"] = logicBit(L);
        step(label, {{"CLK", logicBit(H)}}, std::move(expected));
    }
    void rejectUnknownCommand(size_t width, bool control) {
        step("unknown-command-setup", {{"CLK", logicBit(L)}, {"CMD", LogicVector(3, X)}},
             {{"READ_VALID", logicBit(H)}});
        NamedValues expected{{"ERROR", logicBit(H)}, {"BUSY", logicBit(L)},
                             {"READ_VALID", logicBit(L)}};
        expected[control ? "OUTPUT_ENABLE" : "READ_DATA"] =
            control ? logicBit(L) : LogicVector(width, Z);
        step("unknown-command-rejected", {{"CLK", logicBit(H)}}, expected);
        cmd("nop-preserves-error", 0, 0, {{"ERROR", logicBit(H)}, {"READ_VALID", logicBit(L)}});
    }
};
ComponentTestSpec decoder() {
    Scenario s("MatrixRowDecoderTest", "memory.matrix.row-decoder", {{"rows", "3"}});
    for (unsigned a : {0u, 1u, 2u, 3u, 0xffffffffu})
        for (bool en : {false, true})
            s.step("row-" + std::to_string(a) + (en ? "-on" : "-off"),
                   {{"ROW_ADDR", logicBits(32, a)}, {"ENABLE", logicBit(en)}},
                   {{"WL_0", logicBit(en && a == 0)},
                    {"WL_1", logicBit(en && a == 1)},
                    {"WL_2", logicBit(en && a == 2)}});
    return s.spec;
}
ComponentTestSpec column() {
    Scenario s("MatrixColumnMuxTest", "memory.matrix.column-mux.8", {{"words_per_row", "2"}});
    for (unsigned c = 0; c < 16; ++c)
        s.state["COL_" + std::to_string(c)] = logicBit(((c < 8 ? 0xa5 : 0x3c) >> (c % 8)) & 1);
    for (unsigned a : {0u, 1u, 2u, 0xffffffffu})
        s.step("slot-" + std::to_string(a), {{"COLUMN_ADDR", logicBits(32, a)}},
               {{"DATA", logicBits(8, a == 0   ? 0xa5
                                      : a == 1 ? 0x3c
                                               : 0)}});
    return s.spec;
}
ComponentTestSpec driver() {
    Scenario s("MatrixWriteDistributorTest", "memory.matrix.write-distributor.8", {},
               {{"DATA", logicBits(8, 0xa5)}, {"MASK", logicBit(H)}});
    for (unsigned a : {0u, 1u, 2u})
        for (bool en : {false, true}) {
            NamedValues expected;
            for (unsigned c = 0; c < 16; ++c) {
                auto n = std::to_string(c);
                expected["DRIVE_" + n] = logicBit(en && a == c / 8);
                expected["DATA_" + n] = logicBit((0xa5 >> (c % 8)) & 1);
            }
            s.step("slot-" + std::to_string(a) + (en ? "-on" : "-off"),
                   {{"COLUMN_ADDR", logicBits(32, a)}, {"ENABLE", logicBit(en)}}, expected);
        }
    s.step("masked", {{"COLUMN_ADDR", logicBits(32, 0)}, {"MASK", logicBit(L)}}, {{"DRIVE_0", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec readColumn() {
    Scenario s("StaticReadColumnTest", "memory.matrix.read-column", {},
               {{"Q_0", logicBit(L)}, {"Q_1", logicBit(H)}, {"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}});
    s.step("none", {}, {{"OUT", logicBit(X)}});
    s.step("row-zero", {{"WL_0", logicBit(H)}}, {{"OUT", logicBit(L)}});
    s.step("conflict", {{"WL_1", logicBit(H)}}, {{"OUT", logicBit(X)}});
    s.step("row-one", {{"WL_0", logicBit(L)}}, {{"OUT", logicBit(H)}});
    s.step("unknown-select", {{"WL_0", logicBit(X)}}, {{"OUT", logicBit(X)}});
    return s.spec;
}
ComponentTestSpec output() {
    Scenario s("MemoryOutputBufferTest", "memory.matrix.output-buffer.8", {}, {{"DATA", logicBits(8, 0xa5)}});
    for (auto v : {L, H, X, Z})
        s.step("enable-" + std::to_string(int(v)), {{"ENABLE", logicBit(v)}},
               {{"OUT", v == H ? logicBits(8, 0xa5) : LogicVector(8, v == L ? Z : X)}});
    return s.spec;
}
ComponentTestSpec romCell() {
    Scenario s("ROMCellTest", "memory.rom.cell", {{"value", "1"}});
    s.step("fixed-one", {}, {{"Q", logicBit(H)}});
    return s.spec;
}
ComponentTestSpec romRow() {
    Scenario s("ROMRowTest", "memory.rom.row", {{"contents", "1010"}});
    s.step("fixed-row", {},
           {{"Q_0", logicBit(H)}, {"Q_1", logicBit(L)}, {"Q_2", logicBit(H)}, {"Q_3", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec romControl() {
    Scenario s("ROMAddressControlTest", "memory.rom.address-control", {},
               {{"WORD_ADDR", logicBits(32, 3)}, {"CE_N", logicBit(H)}, {"OE_N", logicBit(L)}});
    s.step("disabled", {}, {{"ROW_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(L)}});
    s.step("select-last-word", {{"CE_N", logicBit(L)}},
           {{"ROW_ADDR", logicBits(32, 1)},
            {"COLUMN_ADDR", logicBits(32, 1)},
            {"ROW_ENABLE", logicBit(H)},
            {"OUTPUT_ENABLE", logicBit(H)}});
    s.step("invalid-address", {{"WORD_ADDR", logicBits(32, 4)}},
           {{"ROW_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(X)}});
    s.step("output-disabled-row-selected", {{"WORD_ADDR", logicBits(32, 1)}, {"OE_N", logicBit(H)}},
           {{"ROW_ENABLE", logicBit(H)}, {"OUTPUT_ENABLE", logicBit(L)}});
    s.step("unknown-address-disabled", {{"WORD_ADDR", LogicVector(32, X)}},
           {{"ROW_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(L)}});
    s.step("unknown-enable", {{"WORD_ADDR", logicBits(32, 1)}, {"OE_N", logicBit(X)}},
           {{"ROW_ENABLE", logicBit(H)}, {"OUTPUT_ENABLE", logicBit(X)}});
    s.step("chip-disable-dominates", {{"CE_N", logicBit(H)}},
           {{"ROW_ENABLE", logicBit(L)}, {"OUTPUT_ENABLE", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec romBank() {
    Scenario s("ROMStorageBankTest", "memory.rom.bank.8", {{"image_hex", "a53c81ff"}},
               {{"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}});
    s.step("idle", {}, {{"COL_0", logicBit(X)}});
    for (unsigned r = 0; r < 2; ++r) {
        NamedValues expected;
        unsigned word = r ? 0xff81 : 0x3ca5;
        for (unsigned c = 0; c < 16; ++c)
            expected["COL_" + std::to_string(c)] = logicBit((word >> c) & 1);
        s.step("row-" + std::to_string(r), {{"WL_0", logicBit(r == 0)}, {"WL_1", logicBit(r == 1)}},
               expected);
    }
    return s.spec;
}
ComponentTestSpec romDevice(size_t width = 8, size_t depth = 4, size_t k = 2) {
    Scenario s("ROMTest", "memory.rom.device." + std::to_string(width),
               {{"depth_words", std::to_string(depth)},
                {"words_per_row", std::to_string(k)},
                {"image_hex", std::string("a53c81ff").substr(0, depth * (width / 8) * 2)}},
               {{"WORD_ADDR", logicBits(32, 0)}, {"CE_N", logicBit(H)}, {"OE_N", logicBit(L)}});
    s.step("disabled", {}, {{"READ_DATA", LogicVector(width, Z)}});
    for (unsigned a = 0; a < depth; ++a) {
        uint32_t v = 0;
        for (unsigned b = 0; b < width / 8; ++b) {
            unsigned i = a * (width / 8) + b;
            if (i < 4)
                v |= uint32_t(std::vector<unsigned>{0xa5, 0x3c, 0x81, 0xff}[i]) << (8 * b);
        }
        s.step("word-" + std::to_string(a), {{"WORD_ADDR", logicBits(32, a)}, {"CE_N", logicBit(L)}},
               {{"READ_DATA", logicBits(width, v)}});
    }
    s.step("out-of-range", {{"WORD_ADDR", logicBits(32, depth)}}, {{"READ_DATA", LogicVector(width, X)}});
    s.step("unknown-address", {{"WORD_ADDR", LogicVector(32, X)}}, {{"READ_DATA", LogicVector(width, X)}});
    s.step("output-disabled", {{"OE_N", logicBit(H)}}, {{"READ_DATA", LogicVector(width, Z)}});
    return s.spec;
}
ComponentTestSpec dramCell() {
    Scenario s("DRAMCellTest", "memory.dram.cell", {},
               {{"WL", logicBit(L)}, {"RESTORE", logicBit(L)}, {"D", logicBit(H)}});
    s.step("unknown-startup", {}, {{"Q", logicBit(X)}, {"SAMPLE", logicBit(X)}});
    s.step("activate-empty", {{"WL", logicBit(H)}}, {{"SAMPLE", logicBit(X)}});
    s.step("restore-one", {{"RESTORE", logicBit(H)}}, {{"Q", logicBit(H)}});
    s.step("close", {{"WL", logicBit(L)}, {"RESTORE", logicBit(L)}}, {{"Q", logicBit(H)}});
    s.step("destructive-read", {{"WL", logicBit(H)}}, {{"SAMPLE", logicBit(H)}, {"Q", logicBit(X)}});
    s.step("omit-restore", {{"WL", logicBit(L)}}, {{"Q", logicBit(X)}});
    s.step("activate-lost-bit", {{"WL", logicBit(H)}}, {{"SAMPLE", logicBit(X)}});
    s.step("write-zero", {{"RESTORE", logicBit(H)}, {"D", logicBit(L)}}, {{"Q", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec dramRow() {
    Scenario s("DRAMRowTest", "memory.dram.row", {{"columns", "2"}},
               {{"WL", logicBit(L)},
                {"RESTORE_0", logicBit(L)},
                {"RESTORE_1", logicBit(L)},
                {"D_0", logicBit(H)},
                {"D_1", logicBit(L)}});
    s.step("initial", {}, {{"Q_0", logicBit(X)}, {"Q_1", logicBit(X)}});
    s.step("activate", {{"WL", logicBit(H)}}, {{"SAMPLE_0", logicBit(X)}});
    s.step("write-row", {{"RESTORE_0", logicBit(H)}, {"RESTORE_1", logicBit(H)}},
           {{"Q_0", logicBit(H)}, {"Q_1", logicBit(L)}});
    s.step("close", {{"WL", logicBit(L)}, {"RESTORE_0", logicBit(L)}, {"RESTORE_1", logicBit(L)}},
           {{"Q_0", logicBit(H)}});
    s.step("sample", {{"WL", logicBit(H)}}, {{"SAMPLE_0", logicBit(H)}, {"SAMPLE_1", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec dramSense() {
    Scenario s("DRAMSenseColumnTest", "memory.dram.sense-column", {},
               {{"PRECHARGE", logicBit(H)},
                {"SENSE", logicBit(L)},
                {"WRITE", logicBit(L)},
                {"D", logicBit(L)},
                {"WL_0", logicBit(L)},
                {"WL_1", logicBit(L)},
                {"SAMPLE_0", logicBit(H)},
                {"SAMPLE_1", logicBit(L)}});
    s.step("precharge", {}, {{"Q", logicBit(X)}, {"RESTORE", logicBit(L)}});
    s.step("activate", {{"PRECHARGE", logicBit(L)}, {"WL_0", logicBit(H)}}, {{"RESTORE", logicBit(L)}});
    s.step("sense-and-restore", {{"SENSE", logicBit(H)}}, {{"Q", logicBit(H)}, {"RESTORE", logicBit(H)}});
    s.step("latch-survives-sample-change", {{"SAMPLE_0", logicBit(X)}}, {{"Q", logicBit(H)}});
    s.step("write-row-buffer", {{"WRITE", logicBit(H)}}, {{"Q", logicBit(L)}});
    s.step("conflicting-rows", {{"WL_1", logicBit(H)}}, {{"Q", logicBit(X)}, {"RESTORE", logicBit(L)}});
    return s.spec;
}
ComponentTestSpec dramBank() {
    Scenario s(
        "DRAMStorageBankTest", "memory.dram.bank.8", {},
        {{"PRECHARGE", logicBit(H)}, {"SENSE", logicBit(L)}, {"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}});
    for (unsigned c = 0; c < 16; ++c) {
        s.state["DRIVE_" + std::to_string(c)] = logicBit(L);
        s.state["DATA_" + std::to_string(c)] = logicBit(c % 2);
    }
    s.step("precharge", {}, {{"COL_0", logicBit(X)}});
    s.step("activate-row-zero", {{"PRECHARGE", logicBit(L)}, {"WL_0", logicBit(H)}}, {});
    s.step("sense-empty-row", {{"SENSE", logicBit(H)}}, {{"COL_0", logicBit(X)}});
    NamedValues drive, release, expected;
    for (unsigned c = 0; c < 16; ++c) {
        drive["DRIVE_" + std::to_string(c)] = logicBit(H);
        release["DRIVE_" + std::to_string(c)] = logicBit(L);
        expected["COL_" + std::to_string(c)] = logicBit(c % 2);
    }
    s.step("write-row", drive, expected);
    s.step("hold-row", release, expected);
    s.step("close-row", {{"WL_0", logicBit(L)}, {"SENSE", logicBit(L)}}, {});
    s.step("precharge-again", {{"PRECHARGE", logicBit(H)}}, {{"COL_0", logicBit(X)}});
    s.step("reopen-row", {{"PRECHARGE", logicBit(L)}, {"WL_0", logicBit(H)}}, {});
    s.step("read-restored-row", {{"SENSE", logicBit(H)}}, expected);
    return s.spec;
}
ComponentTestSpec dramDevice(size_t width = 8, size_t depth = 4, size_t k = 2, bool control = false) {
    Scenario s(control ? "DRAMCommandControlTest" : "DRAMTest",
               "memory.dram." + std::string(control ? "command-control." : "device.") + std::to_string(width),
               {{"depth_words", std::to_string(depth)}, {"words_per_row", std::to_string(k)}},
               {{"CLK", logicBit(L)},
                {"CMD", logicBits(3, 0)},
                {"ADDR", logicBits(32, 0)},
                {"WRITE_DATA", logicBits(width, 0xa53c9681)},
                {"WRITE_MASK", logicBits(width / 8, (1u << (width / 8)) - 1)}});
    s.step("closed-startup", {}, {{"BUSY", logicBit(L)}, {"ROW_OPEN", logicBit(L)}});
    s.cmd("read-while-closed", 2, 0, {{"ERROR", logicBit(H)}});
    s.cmd("activate-row-zero", 1, 0, {{"ERROR", logicBit(L)}, {"ROW_OPEN", logicBit(H)}});
    s.cmd("write-column-zero", 3, 0, {{"ERROR", logicBit(L)}});
    s.cmd("read-column-zero", 2, 0,
          control ? NamedValues{{"OUTPUT_ENABLE", logicBit(H)}}
                  : NamedValues{{"READ_DATA", logicBits(width, 0xa53c9681)}, {"READ_VALID", logicBit(H)}});
    s.cmd("masked-write", 3, 0, {},
          {{"WRITE_MASK", logicBits(width / 8, 1)}, {"WRITE_DATA", logicBits(width, 0x5a)}});
    s.cmd("read-masked-word", 2, 0,
          control ? NamedValues{} : NamedValues{{"READ_DATA", logicBits(width, 0xa53c965a)}});
    s.rejectUnknownCommand(width, control);
    s.cmd("bad-column", 2, k, {{"ERROR", logicBit(H)}});
    s.cmd("precharge", 4, 0, {{"ERROR", logicBit(L)}, {"ROW_OPEN", logicBit(L)}});
    s.cmd("refresh-next-row", 5, 0, {{"ERROR", logicBit(L)}, {"ROW_OPEN", logicBit(L)}});
    s.cmd("activate-restored-row", 1, 0, {{"ERROR", logicBit(L)}});
    s.cmd("read-persisted-word", 2, 0,
          control ? NamedValues{} : NamedValues{{"READ_DATA", logicBits(width, 0xa53c965a)}});
    s.cmd("reserved-command", 7, 0, {{"ERROR", logicBit(H)}});
    return s.spec;
}
ComponentTestSpec flashCell() {
    Scenario s("NORFlashCellTest", "memory.nor-flash.cell", {},
               {{"WL", logicBit(L)}, {"PROGRAM", logicBit(L)}, {"ERASE", logicBit(L)}, {"D", logicBit(H)}});
    s.step("erased", {}, {{"Q", logicBit(H)}});
    s.step("program-zero", {{"WL", logicBit(H)}, {"PROGRAM", logicBit(H)}, {"D", logicBit(L)}},
           {{"Q", logicBit(L)}});
    s.step("one-cannot-undo-program", {{"D", logicBit(H)}}, {{"Q", logicBit(L)}});
    s.step("release", {{"PROGRAM", logicBit(L)}, {"WL", logicBit(L)}}, {{"Q", logicBit(L)}});
    s.step("erase", {{"ERASE", logicBit(H)}}, {{"Q", logicBit(H)}});
    s.step("hold-erased", {{"ERASE", logicBit(L)}}, {{"Q", logicBit(H)}});
    s.step("unknown-program-data", {{"WL", logicBit(H)}, {"PROGRAM", logicBit(H)}, {"D", logicBit(X)}},
           {{"Q", logicBit(X)}});
    return s.spec;
}
ComponentTestSpec flashRow() {
    Scenario s("NORFlashRowTest", "memory.nor-flash.row", {{"columns", "2"}},
               {{"WL", logicBit(L)},
                {"ERASE", logicBit(L)},
                {"PROGRAM_0", logicBit(L)},
                {"PROGRAM_1", logicBit(L)},
                {"D_0", logicBit(L)},
                {"D_1", logicBit(L)}});
    s.step("erased", {}, {{"Q_0", logicBit(H)}, {"Q_1", logicBit(H)}});
    s.step("program-one-column", {{"WL", logicBit(H)}, {"PROGRAM_0", logicBit(H)}},
           {{"Q_0", logicBit(L)}, {"Q_1", logicBit(H)}});
    s.step("release", {{"WL", logicBit(L)}, {"PROGRAM_0", logicBit(L)}}, {{"Q_0", logicBit(L)}});
    s.step("erase-row", {{"ERASE", logicBit(H)}}, {{"Q_0", logicBit(H)}, {"Q_1", logicBit(H)}});
    return s.spec;
}
ComponentTestSpec flashBank() {
    Scenario s(
        "NORFlashStorageBankTest", "memory.nor-flash.bank.8", {},
        {{"WL_0", logicBit(L)}, {"WL_1", logicBit(L)}, {"ERASE_0", logicBit(L)}, {"ERASE_1", logicBit(L)}});
    for (unsigned c = 0; c < 16; ++c) {
        s.state["DRIVE_" + std::to_string(c)] = logicBit(L);
        s.state["DATA_" + std::to_string(c)] = logicBit(c % 2);
    }
    s.step("deselected", {}, {{"COL_0", logicBit(X)}});
    s.step("read-erased-row", {{"WL_0", logicBit(H)}}, {{"COL_0", logicBit(H)}, {"COL_1", logicBit(H)}});
    s.step("program-column-zero", {{"DRIVE_0", logicBit(H)}},
           {{"COL_0", logicBit(L)}, {"COL_1", logicBit(H)}});
    s.step("release-driver", {{"DRIVE_0", logicBit(L)}}, {{"COL_0", logicBit(L)}});
    s.step("other-row-unmodified", {{"WL_0", logicBit(L)}, {"WL_1", logicBit(H)}}, {{"COL_0", logicBit(H)}});
    s.step("erase-first-row", {{"ERASE_0", logicBit(H)}}, {{"COL_0", logicBit(H)}});
    s.step("read-first-row-again", {{"ERASE_0", logicBit(L)}, {"WL_1", logicBit(L)}, {"WL_0", logicBit(H)}},
           {{"COL_0", logicBit(H)}});
    return s.spec;
}
ComponentTestSpec flashDevice(size_t width = 8, size_t depth = 4, size_t k = 2, bool control = false) {
    Scenario s(control ? "NORFlashCommandControlTest" : "NORFlashTest",
               "memory.nor-flash." + std::string(control ? "command-control." : "device.") +
                   std::to_string(width),
               {{"depth_words", std::to_string(depth)},
                {"words_per_row", std::to_string(k)},
                {"block_words", std::to_string(k)}},
               {{"CLK", logicBit(L)},
                {"CMD", logicBits(3, 0)},
                {"WORD_ADDR", logicBits(32, 0)},
                {"WRITE_DATA", logicBits(width, 0xa53c9681)},
                {"WRITE_MASK", logicBits(width / 8, (1u << (width / 8)) - 1)}});
    s.step("ready-erased", {}, {{"BUSY", logicBit(L)}, {"WRITE_ENABLED", logicBit(L)}});
    auto value = [&](uint32_t v) {
        return control ? NamedValues{{"OUTPUT_ENABLE", logicBit(H)}}
                       : NamedValues{{"READ_DATA", logicBits(width, v)}, {"READ_VALID", logicBit(H)}};
    };
    s.cmd("read-erased", 1, 0, value(0xffffffff));
    s.cmd("program-without-enable", 2, 0, {{"ERROR", logicBit(H)}});
    s.cmd("write-enable", 4, 0, {{"WRITE_ENABLED", logicBit(H)}, {"ERROR", logicBit(L)}});
    s.cmd("program-word", 2, 0, {{"WRITE_ENABLED", logicBit(L)}, {"ERROR", logicBit(L)}});
    s.cmd("read-programmed", 1, 0, value(0xa53c9681));
    s.cmd("enable-again", 4, 0);
    s.cmd("cannot-program-zero-to-one", 2, 0, {}, {{"WRITE_DATA", logicBits(width, 0xffffffff)}});
    s.cmd("read-unchanged", 1, 0, value(0xa53c9681));
    s.cmd("enable-other-block", 4, 0);
    s.cmd("program-other-block", 2, k, {}, {{"WRITE_DATA", logicBits(width, 0x12345678)}});
    s.cmd("enable-erase", 4, 0);
    s.cmd("erase-block-zero", 3, 0, {{"WRITE_ENABLED", logicBit(L)}});
    s.cmd("read-erased-again", 1, 0, value(0xffffffff));
    s.cmd("other-block-survives", 1, k, value(0x12345678));
    s.rejectUnknownCommand(width, control);
    s.cmd("invalid-address", 1, depth, {{"ERROR", logicBit(H)}});
    s.cmd("reserved-command", 7, 0, {{"ERROR", logicBit(H)}});
    s.cmd("enable-then-disable", 4, 0);
    s.cmd("write-disable", 5, 0, {{"WRITE_ENABLED", logicBit(L)}});
    return s.spec;
}
void compare(ComponentTestSpec spec) {
    ComponentTestRunner runner;
    auto all = runner.runAll(spec, spec.scenarios.front());
    require(!all.targets.empty(), "No implementation tested");
    for (size_t i = 1; i < all.targets.size(); ++i)
        runner.compare(all.targets[0], all.targets[i]);
}
} // namespace
#define DEFINE_TEST(Name, Expr)                                                                              \
    Name::Name() : ComponentScenarioTest(Expr) {                                                             \
    }
DEFINE_TEST(MatrixRowDecoderTest, decoder())
DEFINE_TEST(MatrixColumnMuxTest, column())
DEFINE_TEST(MatrixWriteDistributorTest, driver())
DEFINE_TEST(StaticReadColumnTest, readColumn())
DEFINE_TEST(MemoryOutputBufferTest, output())
DEFINE_TEST(ROMCellTest, romCell())
DEFINE_TEST(ROMRowTest, romRow())
DEFINE_TEST(ROMAddressControlTest, romControl())
DEFINE_TEST(ROMStorageBankTest, romBank())
DEFINE_TEST(ROMTest, romDevice())
DEFINE_TEST(DRAMCellTest, dramCell())
DEFINE_TEST(DRAMRowTest, dramRow())
DEFINE_TEST(DRAMSenseColumnTest, dramSense())
DEFINE_TEST(DRAMStorageBankTest, dramBank())
DEFINE_TEST(DRAMCommandControlTest, dramDevice(8, 4, 2, true))
DEFINE_TEST(DRAMTest, dramDevice())
DEFINE_TEST(NORFlashCellTest, flashCell())
DEFINE_TEST(NORFlashRowTest, flashRow())
DEFINE_TEST(NORFlashStorageBankTest, flashBank())
DEFINE_TEST(NORFlashCommandControlTest, flashDevice(8, 4, 2, true))
DEFINE_TEST(NORFlashTest, flashDevice())
#undef DEFINE_TEST
void MemoryDeviceContractTest::verifyResults() {
    for (auto spec : {romCell(), romRow(), romBank(), dramRow(), dramBank(), flashRow(), flashBank()})
        compare(std::move(spec));
    for (size_t width : {8, 16, 32})
        for (size_t k : {1, 2, 4}) {
            compare(romDevice(width, 2 * k, k));
            compare(dramDevice(width, 2 * k, k));
            compare(flashDevice(width, 2 * k, k));
        }
    // Mixed root/bank/row fidelity must preserve the same external device.
    for (auto spec : {romDevice(), dramDevice(), flashDevice()}) {
        ComponentTestRunner runner;
        auto baseline = runner.run(spec, spec.scenarios.front());
        for (auto path : {"MEMORY_ROOT.STORAGE", "MEMORY_ROOT.STORAGE.ROW_0"}) {
            auto mixed = runner.run(spec, spec.scenarios.front(),
                                    withExactFidelity(canonicalDefaultProfile(), path, Fidelity::Behavioral));
            runner.compare(baseline, mixed);
        }
    }
    for (auto family : {"dram", "rom", "nor-flash"}) {
        auto id = "memory." + std::string(family) + ".device.32";
        auto create = [&](ParameterMap p, Fidelity f) {
            return builtinComponentCatalog().createRoot(
                {id, "DUT", p, {}, {}, {}}, withExactFidelity(canonicalDefaultProfile(), "DUT", f));
        };
        for (auto p : std::vector<ParameterMap>{{{"depth_words", "0"}},
                                                {{"depth_words", "1048577"}},
                                                {{"words_per_row", "3"}},
                                                {{"misspelled", "1"}},
                                                {{"depth_words", "-1"}}}) {
            bool rejected = false;
            try {
                create(p, Fidelity::Behavioral);
            } catch (const std::exception &) {
                rejected = true;
            }
            require(rejected, "Invalid parameters accepted by " + id);
        }
        bool limited = false;
        try {
            create({{"depth_words", "4096"}}, Fidelity::Structural);
        } catch (const std::exception &) {
            limited = true;
        }
        require(limited, "Structural allocation not bounded");
        auto large = create({{"depth_words", "65536"}}, Fidelity::Behavioral);
        require(large.root->getChildren().empty(), "Compact device expanded cells");
        auto view = std::dynamic_pointer_cast<MemoryStateView>(large.root);
        require(view && view->memoryStorage().isHistoryRecordingEnabled(),
                "Missing compact device history view");
        bool read_only = false;
        try {
            view->loadMemoryBytes(0, {0});
        } catch (const std::logic_error &) {
            read_only = true;
        }
        require(read_only, "Debug loading bypassed device semantics");
        auto no_history = create({{"record_history", "0"}}, Fidelity::Behavioral);
        require(!std::dynamic_pointer_cast<MemoryStateView>(no_history.root)
                     ->memoryStorage()
                     .isHistoryRecordingEnabled(),
                "History opt-out ignored");
    }
    for (auto hex : {"0", "gg", "0011223344"}) {
        auto spec = romDevice();
        spec.parameters["image_hex"] = hex;
        require(ComponentTestRunner().availableFidelities(spec).empty(), "Invalid ROM image accepted");
    }
    for (auto parameter : {"image_hex", "record_history"}) {
        auto spec = romControl();
        spec.parameters[parameter] = "00";
        require(ComponentTestRunner().availableFidelities(spec).empty(),
                "ROM address controller accepted storage-only parameters");
    }
    // Exercise every row/slot in a non-power-of-two-capacity DRAM with an
    // independent byte-mask oracle, not just root-vs-root equality.
    Scenario d("DRAMMaskOracle", "memory.dram.device.16", {{"depth_words", "6"}},
               {{"CLK", logicBit(L)},
                {"CMD", logicBits(3, 0)},
                {"ADDR", logicBits(32, 0)},
                {"WRITE_DATA", logicBits(16, 0)},
                {"WRITE_MASK", logicBits(2, 3)}});
    d.step("initial", {}, {});
    std::vector<LogicVector> memory(6, LogicVector(16, X));
    std::mt19937 random(0x4452414d);
    for (unsigned i = 0; i < 24; ++i) {
        unsigned address = i < 6 ? i : random() % 6, data = random() & 65535, mask = i < 6 ? 3 : random() % 4;
        d.cmd("activate-" + std::to_string(i), 1, address / 2);
        d.cmd("write-" + std::to_string(i), 3, address % 2, {},
              {{"WRITE_DATA", logicBits(16, data)}, {"WRITE_MASK", logicBits(2, mask)}});
        for (unsigned bit = 0; bit < 16; ++bit)
            if (mask & (1u << (bit / 8)))
                memory[address][bit] = (data & (1u << bit)) ? H : L;
        for (unsigned slot = 0; slot < 2; ++slot)
            d.cmd("read-" + std::to_string(i) + "-" + std::to_string(slot), 2, slot,
                  {{"READ_DATA", memory[address / 2 * 2 + slot]}});
        d.cmd("close-" + std::to_string(i), 4, 0);
    }
    compare(d.spec);
    // Independent byte-mask oracle over a deterministic randomized NOR program stream.
    auto spec = flashDevice(16);
    spec.scenarios[0].actions.clear();
    Scenario s("NORFlashMaskOracle", "memory.nor-flash.device.16", {},
               {{"CLK", logicBit(L)},
                {"CMD", logicBits(3, 0)},
                {"WORD_ADDR", logicBits(32, 0)},
                {"WRITE_DATA", logicBits(16, 0)},
                {"WRITE_MASK", logicBits(2, 3)}});
    s.step("initial", {}, {});
    std::mt19937 rng(0x4d454d);
    uint32_t expected[4] = {65535, 65535, 65535, 65535};
    for (unsigned i = 0; i < 24; ++i) {
        auto a = rng() % 4, d = rng() & 65535, m = rng() % 4;
        s.cmd("enable-" + std::to_string(i), 4, 0);
        s.cmd("program-" + std::to_string(i), 2, a, {},
              {{"WRITE_DATA", logicBits(16, d)}, {"WRITE_MASK", logicBits(2, m)}});
        for (unsigned byte = 0; byte < 2; ++byte)
            if (m & (1u << byte))
                expected[a] &= ~(255u << (8 * byte)) | (d & (255u << (8 * byte)));
        s.cmd("read-" + std::to_string(i), 1, a, {{"READ_DATA", logicBits(16, expected[a])}});
    }
    compare(s.spec);
}
void MemoryDeviceTimingTest::verifyResults() {
    // Timed inputs are separate from documentation's settled checkpoints.
    for (auto fidelity : {Fidelity::Structural, Fidelity::Behavioral})
        for (bool flash : {false, true}) {
            auto id = flash ? "memory.nor-flash.device.8" : "memory.dram.device.8";
            auto built = builtinComponentCatalog().createRoot(
                {id, "DUT", flash ? ParameterMap{} : ParameterMap{{"retention_ticks", "1024"}}, {}, {}, {}},
                withExactFidelity(canonicalDefaultProfile(), "DUT", fidelity));
            auto io = std::dynamic_pointer_cast<IOComponent>(built.root);
            ComponentBuilder b(io);
            Simulator sim;
            std::map<std::string, std::shared_ptr<WireBase>> inputs;
            for (auto &[n, p] : io->getAllInputPins())
                inputs[n] = b.addNewWireDynamic("IN_" + n, p->getWidth(), nullptr, {p});
            SimulationTest::scheduleInitialEventsForTree(io, sim, 0);
            auto set = [&](size_t t, std::string p, TestValue v) {
                sim.scheduleEvent(makeTestWireUpdate(t, inputs.at(p), v));
            };
            auto cmd = [&](size_t t, unsigned op, unsigned addr) {
                set(t, "CLK", L);
                set(t, "CMD", uint64_t(op));
                set(t, flash ? "WORD_ADDR" : "ADDR", uint64_t(addr));
                set(t + 10, "CLK", H);
            };
            set(0, "CLK", L);
            set(0, "CMD", uint64_t(0));
            set(0, flash ? "WORD_ADDR" : "ADDR", uint64_t(0));
            set(0, "WRITE_DATA", uint64_t(0xa5));
            set(0, "WRITE_MASK", H);
            if (flash) {
                cmd(100, 4, 0);
                cmd(200, 2, 0);
                sim.advanceAndRecord(230);
                require(io->getOutputPin("BUSY")->getValue() == H, "Flash program completed too early");
                cmd(240, 1, 1);
                set(245, "WRITE_DATA", uint64_t(0));
                sim.advanceAndRecord(500);
                require(io->getOutputPin("ERROR")->getValue() == H, "Busy flash command was not rejected");
                cmd(600, 1, 0);
                sim.advanceAndRecord(900);
                require(io->getOutputPin<8>("READ_DATA")->getValueAsUInt64() == 0xa5,
                        "Flash request was not latched");
            } else {
                cmd(100, 1, 0);
                sim.advanceAndRecord(130);
                require(io->getOutputPin("BUSY")->getValue() == H &&
                            io->getOutputPin("ROW_OPEN")->getValue() == L,
                        "DRAM activated too early");
                cmd(300, 3, 0);
                cmd(500, 4, 0);
                cmd(800, 5, 0);
                cmd(1400, 1, 0);
                cmd(1600, 2, 0);
                sim.advanceAndRecord(1800);
                require(io->getOutputPin<8>("READ_DATA")->getValueAsUInt64() == 0xa5,
                        "Refresh failed to restore row before expiry");
                cmd(1900, 4, 0);
                cmd(3300, 1, 0);
                cmd(3500, 2, 0);
                sim.advanceAndRecord(3800);
                require(io->getOutputPin<8>("READ_DATA")->getValueAsVector() == LogicVector(8, X),
                        "Expired DRAM row did not become unknown");
            }
            require(!sim.hasPendingEvents(), "Memory left periodic events running");
        }
}
