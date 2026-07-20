#include "tests/MemoryComponentTests.hpp"

#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "modules/memory/BehavioralMemory64Kx32.hpp"
#include "modules/memory/BehavioralRegisterFile32x32.hpp"
#include "modules/memory/BehavioralMemoryBit.hpp"
#include "modules/memory/Memory4x32.hpp"
#include "modules/memory/Memory32x32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/memory/RegisterFile4x32.hpp"
#include "simulator/Event.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void requireMemory(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct ExpectedValue {
    size_t time;
    LogicValue value;
    const char* label;
};

struct ExpectedWord {
    size_t time;
    uint32_t value;
    std::string label;
};

struct ExpectedRegisterVector {
    size_t time;
    std::vector<LogicValue> values;
    std::string label;
};

LogicValue bit(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

void drive(Simulator& sim, size_t time, const std::shared_ptr<Wire<>>& wire, LogicValue value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(time, wire, value));
}

void drive(Simulator& sim, size_t time, const std::shared_ptr<Wire<>>& wire, bool value) {
    drive(sim, time, wire, bit(value));
}

void drive32(Simulator& sim, size_t time, const std::shared_ptr<Wire<32>>& wire, uint32_t value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(time, wire, static_cast<uint64_t>(value)));
}

void drive2(Simulator& sim, size_t time, const std::shared_ptr<Wire<2>>& wire, uint64_t value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<2>>(time, wire, value & 0x3U));
}

void drive5(Simulator& sim, size_t time, const std::shared_ptr<Wire<5>>& wire, uint64_t value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<5>>(time, wire, value & 0x1fU));
}

void drive5(Simulator& sim,
            size_t time,
            const std::shared_ptr<Wire<5>>& wire,
            const std::vector<LogicValue>& values) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<5>>(time, wire, values));
}

void drive32(Simulator& sim,
             size_t time,
             const std::shared_ptr<Wire<32>>& wire,
             const std::vector<LogicValue>& values) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(time, wire, values));
}

std::vector<LogicValue> bits32(uint32_t value) {
    std::vector<LogicValue> bits(32, LogicValue::LOW);
    for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
        bits[bit_index] = ((value >> bit_index) & 1U) ? LogicValue::HIGH : LogicValue::LOW;
    }
    return bits;
}

std::vector<LogicValue> unknownWord32() {
    return std::vector<LogicValue>(32, LogicValue::UNKNOWN);
}

void expectWireAt(Simulator& sim,
                  const std::shared_ptr<Wire<>>& wire,
                  const ExpectedValue& expected,
                  const char* test_name = "MemoryBitTest") {
    assert(wire && "Missing wire for MemoryBit timestamp verification");
    sim.setCircuitStateAtTime(expected.time);
    const auto actual = wire->getSingleValue();
    if (actual != expected.value) {
        std::cerr << test_name << " failed at t=" << expected.time
                  << " (" << expected.label << "): expected " << expected.value
                  << ", got " << actual << std::endl;
        assert(false && "Memory bit output mismatch");
    }
}

void expectRegisterAt(Simulator& sim,
                      const std::shared_ptr<Wire<32>>& wire,
                      const ExpectedWord& expected,
                      const char* test_name = "Register32Test") {
    assert(wire && "Missing Register32 output wire");
    sim.setCircuitStateAtTime(expected.time);
    const auto actual = static_cast<uint32_t>(wire->getValue());
    if (actual != expected.value) {
        std::cerr << test_name << " failed at t=" << expected.time
                  << " (" << expected.label << "): expected 0x" << std::hex << expected.value
                  << ", got 0x" << actual << std::dec << std::endl;
        assert(false && "32-bit output mismatch");
    }
}

void expectRegisterVectorAt(Simulator& sim,
                            const std::shared_ptr<Wire<32>>& wire,
                            const ExpectedRegisterVector& expected,
                            const char* test_name = "Register32Test") {
    assert(wire && "Missing Register32 output wire");
    assert(expected.values.size() == 32 && "Register32 expected vector must be 32 bits");
    sim.setCircuitStateAtTime(expected.time);
    for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
        const auto actual = wire->getBit(bit_index);
        if (actual != expected.values[bit_index]) {
            std::cerr << test_name << " failed at t=" << expected.time
                      << " (" << expected.label << "), bit " << bit_index
                      << ": expected " << expected.values[bit_index]
                      << ", got " << actual << std::endl;
            assert(false && "32-bit vector output mismatch");
        }
    }
}

void expectBehavioralRegisterStateAt(const BehavioralRegisterFile32x32& register_file,
                                     size_t time,
                                     size_t register_index,
                                     const std::vector<LogicValue>& expected,
                                     const char* label,
                                     const char* test_name) {
    assert(register_index < 32 && "Behavioral register index must be in x0..x31");
    assert(expected.size() == 32 && "Behavioral register expected vector must be 32 bits");
    const auto state = register_file.getRegisterStateAtTime(time);
    assert(state.size() == 32 && "Behavioral register API must return 32 words");
    assert(state[register_index].size() == 32 && "Behavioral register API word must be 32 bits");
    for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
        if (state[register_index][bit_index] != expected[bit_index]) {
            std::cerr << test_name << " API failed at t=" << time
                      << " (" << label << "), x" << register_index
                      << " bit " << bit_index
                      << ": expected " << expected[bit_index]
                      << ", got " << state[register_index][bit_index] << std::endl;
            assert(false && "Behavioral register API state mismatch");
        }
    }
}

void expectBehavioralRegisterStateAt(const BehavioralRegisterFile32x32& register_file,
                                     size_t time,
                                     size_t register_index,
                                     uint32_t expected,
                                     const char* label,
                                     const char* test_name) {
    expectBehavioralRegisterStateAt(register_file, time, register_index, bits32(expected), label, test_name);
}

void expectBehavioralMemoryWordAt(const BehavioralMemory64Kx32& memory,
                                  size_t time,
                                  uint32_t address,
                                  const std::vector<LogicValue>& expected,
                                  const char* label,
                                  const char* test_name) {
    assert(expected.size() == 32 && "Behavioral memory expected word must be 32 bits");
    const auto words = memory.getWordsAtTime(time, address, 1);
    assert(words.size() == 1 && "Behavioral memory API must return the requested word");
    assert(words[0].first == (address & ~uint32_t{0x3}) && "Behavioral memory API must return aligned word address");
    for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
        if (words[0].second[bit_index] != expected[bit_index]) {
            std::cerr << test_name << " API failed at t=" << time
                      << " (" << label << "), address 0x" << std::hex << address << std::dec
                      << " bit " << bit_index
                      << ": expected " << expected[bit_index]
                      << ", got " << words[0].second[bit_index] << std::endl;
            assert(false && "Behavioral memory API word mismatch");
        }
    }
}

void expectBehavioralMemoryWordAt(const BehavioralMemory64Kx32& memory,
                                  size_t time,
                                  uint32_t address,
                                  uint32_t expected,
                                  const char* label,
                                  const char* test_name) {
    expectBehavioralMemoryWordAt(memory, time, address, bits32(expected), label, test_name);
}

uint32_t registerFilePattern(uint32_t reg_index) {
    return 0x10000000U | (reg_index * 0x01010101U);
}

uint32_t memoryWordPattern(uint32_t word_index) {
    return 0x20000000U | (word_index * 0x01020304U);
}
}

std::string MemoryBitTest::getTestName() const {
    return "MemoryBitTest";
}

void MemoryBitTest::setupCircuit() {
    root = Component::create<MemoryBit>("MEMORY_BIT_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void MemoryBitTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "MemoryBitTest requires IOComponent root");

    auto d_wire = builder->addNewWire("D_IN", nullptr, {io_root->getInputPin("D")});
    auto we_wire = builder->addNewWire("WE_IN", nullptr, {io_root->getInputPin("WE")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire("Q_OUT", io_root->getOutputPin("Q"), {});

    drive(*sim, 0, d_wire, false);
    drive(*sim, 0, we_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);

    drive(*sim, 30, rst_wire, false);

    // WE=0: the rising edge must preserve reset value even while D=1.
    drive(*sim, 40, d_wire, true);
    drive(*sim, 60, clk_wire, true);
    drive(*sim, 100, clk_wire, false);

    // WE=1: capture D=1 on the next rising edge.
    drive(*sim, 110, we_wire, true);
    drive(*sim, 130, clk_wire, true);

    // D changes while CLK is high after the edge; Q must not change until another rising edge.
    drive(*sim, 180, d_wire, false);
    drive(*sim, 220, clk_wire, false);

    // Now capture D=0 on a clean rising edge.
    drive(*sim, 260, clk_wire, true);
    drive(*sim, 310, clk_wire, false);

    // WE=0: hold zero even when D=1.
    drive(*sim, 320, we_wire, false);
    drive(*sim, 330, d_wire, true);
    drive(*sim, 360, clk_wire, true);
    drive(*sim, 410, clk_wire, false);

    // Unknown data under WE=1 should be captured as UNKNOWN.
    drive(*sim, 420, we_wire, true);
    drive(*sim, 430, d_wire, LogicValue::UNKNOWN);
    drive(*sim, 460, clk_wire, true);
    drive(*sim, 510, clk_wire, false);

    // Reset dominates and clears unknown state.
    drive(*sim, 520, rst_wire, true);
    drive(*sim, 560, rst_wire, false);

    // Unknown WE on a write attempt should make the selected DFF input unknown.
    drive(*sim, 570, d_wire, true);
    drive(*sim, 580, we_wire, LogicValue::UNKNOWN);
    drive(*sim, 610, clk_wire, true);
}

void MemoryBitTest::verifyResults() {
    auto q_wire = builder->getWire("Q_OUT");
    const ExpectedValue expected[] = {
        {20, LogicValue::LOW, "reset drives Q low"},
        {95, LogicValue::LOW, "WE=0 holds reset value on rising edge"},
        {125, LogicValue::LOW, "WE=1 before rising edge has not captured yet"},
        {170, LogicValue::HIGH, "WE=1 captures D=1"},
        {210, LogicValue::HIGH, "D change while CLK high is not another edge"},
        {300, LogicValue::LOW, "next rising edge captures D=0"},
        {400, LogicValue::LOW, "WE=0 holds zero despite D=1"},
        {500, LogicValue::UNKNOWN, "WE=1 captures unknown D"},
        {550, LogicValue::LOW, "reset clears unknown state"},
        {650, LogicValue::UNKNOWN, "unknown WE captures unknown selected input"},
    };

    for (const auto& value : expected) {
        expectWireAt(*sim, q_wire, value);
    }
}

size_t MemoryBitTest::getRunDuration() const {
    return 660;
}

std::vector<SimulationTest::SimulationCheckpoint> MemoryBitTest::getCheckpoints() const {
    return {
        {20, "Reset clear", "RST=1 initializes Q=0", 0},
        {95, "Hold while disabled", "D=1, WE=0, rising CLK -> Q remains 0", 1},
        {170, "Write one", "D=1, WE=1, rising CLK -> Q becomes 1", 2},
        {210, "No level capture", "D changes while CLK is already high -> Q stays 1", 3},
        {300, "Write zero", "D=0, WE=1, rising CLK -> Q becomes 0", 4},
        {400, "Hold zero", "D=1, WE=0, rising CLK -> Q remains 0", 5},
        {500, "Capture unknown data", "D=X, WE=1, rising CLK -> Q becomes X", 6},
        {550, "Reset dominates", "RST=1 clears Q from X to 0", 7},
        {650, "Unknown write enable", "WE=X on a write attempt drives stored value to X", 8},
    };
}

std::string BehavioralMemoryBitTest::getTestName() const {
    return "BehavioralMemoryBitTest";
}

void BehavioralMemoryBitTest::setupCircuit() {
    root = Component::create<BehavioralMemoryBit>("BEHAVIORAL_MEMORY_BIT_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void BehavioralMemoryBitTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "BehavioralMemoryBitTest requires IOComponent root");

    auto d_wire = builder->addNewWire("D_IN", nullptr, {io_root->getInputPin("D")});
    auto we_wire = builder->addNewWire("WE_IN", nullptr, {io_root->getInputPin("WE")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire("Q_OUT", io_root->getOutputPin("Q"), {});

    drive(*sim, 0, d_wire, false);
    drive(*sim, 0, we_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);

    drive(*sim, 30, rst_wire, false);

    // WE=0: a rising edge preserves the reset value.
    drive(*sim, 40, d_wire, true);
    drive(*sim, 60, clk_wire, true);
    drive(*sim, 100, clk_wire, false);

    // WE=1: capture D=1 on the next rising edge.
    drive(*sim, 110, we_wire, true);
    drive(*sim, 130, clk_wire, true);

    // D changes while CLK is high; this is not another rising edge.
    drive(*sim, 180, d_wire, false);
    drive(*sim, 220, clk_wire, false);

    // Capture D=0 on a clean later rising edge.
    drive(*sim, 260, clk_wire, true);
    drive(*sim, 310, clk_wire, false);

    // WE=0: hold zero despite D=1.
    drive(*sim, 320, we_wire, false);
    drive(*sim, 330, d_wire, true);
    drive(*sim, 360, clk_wire, true);
    drive(*sim, 410, clk_wire, false);

    // Unknown data under WE=1 is captured as UNKNOWN.
    drive(*sim, 420, we_wire, true);
    drive(*sim, 430, d_wire, LogicValue::UNKNOWN);
    drive(*sim, 460, clk_wire, true);
    drive(*sim, 510, clk_wire, false);

    // Reset clears unknown state.
    drive(*sim, 520, rst_wire, true);
    drive(*sim, 560, rst_wire, false);

    // Unknown WE on a write edge stores UNKNOWN.
    drive(*sim, 570, d_wire, true);
    drive(*sim, 580, we_wire, LogicValue::UNKNOWN);
    drive(*sim, 610, clk_wire, true);
}

void BehavioralMemoryBitTest::verifyResults() {
    auto q_wire = builder->getWire("Q_OUT");
    const ExpectedValue expected[] = {
        {5, LogicValue::LOW, "reset drives Q low"},
        {65, LogicValue::LOW, "WE=0 holds reset value on rising edge"},
        {125, LogicValue::LOW, "WE=1 before rising edge has not captured yet"},
        {140, LogicValue::HIGH, "WE=1 captures D=1"},
        {190, LogicValue::HIGH, "D change while CLK high is not another edge"},
        {270, LogicValue::LOW, "next rising edge captures D=0"},
        {370, LogicValue::LOW, "WE=0 holds zero despite D=1"},
        {470, LogicValue::UNKNOWN, "WE=1 captures unknown D"},
        {530, LogicValue::LOW, "reset clears unknown state"},
        {620, LogicValue::UNKNOWN, "unknown WE captures unknown selected input"},
    };

    for (const auto& value : expected) {
        expectWireAt(*sim, q_wire, value, "BehavioralMemoryBitTest");
    }
}

size_t BehavioralMemoryBitTest::getRunDuration() const {
    return 630;
}

std::vector<SimulationTest::SimulationCheckpoint> BehavioralMemoryBitTest::getCheckpoints() const {
    return {
        {5, "Reset clear", "RST=1 initializes Q=0", 0},
        {65, "Hold while disabled", "D=1, WE=0, rising CLK -> Q remains 0", 1},
        {140, "Write one", "D=1, WE=1, rising CLK -> Q becomes 1", 2},
        {190, "No level capture", "D changes while CLK is already high -> Q stays 1", 3},
        {270, "Write zero", "D=0, WE=1, rising CLK -> Q becomes 0", 4},
        {370, "Hold zero", "D=1, WE=0, rising CLK -> Q remains 0", 5},
        {470, "Capture unknown data", "D=X, WE=1, rising CLK -> Q becomes X", 6},
        {530, "Reset dominates", "RST=1 clears Q from X to 0", 7},
        {620, "Unknown write enable", "WE=X on a write attempt drives stored value to X", 8},
    };
}

std::string BehavioralMemory64Kx32Test::getTestName() const {
    return "BehavioralMemory64Kx32Test";
}

void BehavioralMemory64Kx32Test::setupCircuit() {
    root = Component::create<BehavioralMemory64Kx32>("BEHAVIORAL_MEMORY64KX32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void BehavioralMemory64Kx32Test::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "BehavioralMemory64Kx32Test requires IOComponent root");

    auto addr_wire = builder->addNewWire<32>("ADDR_IN", nullptr, {io_root->getInputPin<32>("ADDR")});
    auto write_data_wire = builder->addNewWire<32>("WRITE_DATA_IN", nullptr, {io_root->getInputPin<32>("WRITE_DATA")});
    auto read_en_wire = builder->addNewWire("READ_EN_IN", nullptr, {io_root->getInputPin("READ_EN")});
    auto write_en_wire = builder->addNewWire("WRITE_EN_IN", nullptr, {io_root->getInputPin("WRITE_EN")});
    auto size_wire = builder->addNewWire<2>("SIZE_IN", nullptr, {io_root->getInputPin<2>("SIZE")});
    auto sign_extend_wire = builder->addNewWire("SIGN_EXTEND_IN", nullptr, {io_root->getInputPin("SIGN_EXTEND")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("READ_DATA_OUT", io_root->getOutputPin<32>("READ_DATA"), {});
    builder->addNewWire("READY_OUT", io_root->getOutputPin("READY"), {});
    builder->addNewWire("FAULT_OUT", io_root->getOutputPin("FAULT"), {});

    constexpr uint32_t LastWordAddress = 0x0003fffcU;
    constexpr uint32_t FirstOutOfRangeAddress = 0x00040000U;

    drive32(*sim, 0, addr_wire, 0U);
    drive32(*sim, 0, write_data_wire, 0U);
    drive(*sim, 0, read_en_wire, false);
    drive(*sim, 0, write_en_wire, false);
    drive2(*sim, 0, size_wire, 2);
    drive(*sim, 0, sign_extend_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);

    drive(*sim, 50, rst_wire, false);
    drive(*sim, 80, read_en_wire, true);

    drive32(*sim, 200, addr_wire, 0U);
    drive32(*sim, 200, write_data_wire, 0x12345678U);
    drive2(*sim, 200, size_wire, 2);
    drive(*sim, 200, write_en_wire, true);
    drive(*sim, 260, clk_wire, true);
    drive(*sim, 320, clk_wire, false);
    drive(*sim, 340, write_en_wire, false);
    drive32(*sim, 360, addr_wire, 0U);

    drive32(*sim, 500, addr_wire, LastWordAddress);
    drive32(*sim, 500, write_data_wire, 0x89abcdefU);
    drive2(*sim, 500, size_wire, 2);
    drive(*sim, 500, write_en_wire, true);
    drive(*sim, 560, clk_wire, true);
    drive(*sim, 620, clk_wire, false);
    drive(*sim, 640, write_en_wire, false);
    drive32(*sim, 660, addr_wire, LastWordAddress);

    drive32(*sim, 800, addr_wire, 4U);
    drive32(*sim, 800, write_data_wire, 0x01020304U);
    drive2(*sim, 800, size_wire, 2);
    drive(*sim, 800, write_en_wire, true);
    drive(*sim, 860, clk_wire, true);
    drive(*sim, 920, clk_wire, false);
    drive(*sim, 940, write_en_wire, false);
    drive32(*sim, 960, addr_wire, 4U);

    drive32(*sim, 1100, addr_wire, 4U);
    drive32(*sim, 1100, write_data_wire, 0xdeadbeefU);
    drive2(*sim, 1100, size_wire, 2);
    drive(*sim, 1100, write_en_wire, false);
    drive(*sim, 1160, clk_wire, true);
    drive(*sim, 1220, clk_wire, false);
    drive32(*sim, 1240, addr_wire, 4U);

    drive32(*sim, 1400, addr_wire, 1U);
    drive32(*sim, 1400, write_data_wire, 0x000000aaU);
    drive2(*sim, 1400, size_wire, 0);
    drive(*sim, 1400, write_en_wire, true);
    drive(*sim, 1460, clk_wire, true);
    drive(*sim, 1520, clk_wire, false);
    drive(*sim, 1540, write_en_wire, false);
    drive32(*sim, 1560, addr_wire, 0U);
    drive2(*sim, 1560, size_wire, 2);

    drive32(*sim, 1800, addr_wire, 2U);
    drive32(*sim, 1800, write_data_wire, 0x0000beefU);
    drive2(*sim, 1800, size_wire, 1);
    drive(*sim, 1800, write_en_wire, true);
    drive(*sim, 1860, clk_wire, true);
    drive(*sim, 1920, clk_wire, false);
    drive(*sim, 1940, write_en_wire, false);
    drive32(*sim, 1960, addr_wire, 0U);
    drive2(*sim, 1960, size_wire, 2);

    drive32(*sim, 2100, addr_wire, 8U);
    drive32(*sim, 2100, write_data_wire, 0x00000000U);
    drive2(*sim, 2100, size_wire, 2);
    drive(*sim, 2100, write_en_wire, true);
    drive(*sim, 2140, clk_wire, true);
    drive(*sim, 2160, clk_wire, false);
    drive(*sim, 2180, write_en_wire, false);

    drive32(*sim, 2200, addr_wire, 2U);
    drive2(*sim, 2200, size_wire, 0);
    drive(*sim, 2200, sign_extend_wire, false);
    drive(*sim, 2350, sign_extend_wire, true);

    drive32(*sim, 2500, addr_wire, 2U);
    drive2(*sim, 2500, size_wire, 1);
    drive(*sim, 2500, sign_extend_wire, false);
    drive(*sim, 2650, sign_extend_wire, true);

    drive32(*sim, 2900, addr_wire, 0U);
    drive2(*sim, 2900, size_wire, 3);
    drive(*sim, 2900, sign_extend_wire, false);
    drive(*sim, 2900, read_en_wire, true);

    drive32(*sim, 3050, addr_wire, 1U);
    drive2(*sim, 3050, size_wire, 1);

    drive32(*sim, 3200, addr_wire, 2U);
    drive2(*sim, 3200, size_wire, 2);

    drive32(*sim, 3350, addr_wire, FirstOutOfRangeAddress);
    drive2(*sim, 3350, size_wire, 0);

    drive(*sim, 3500, read_en_wire, false);
    drive(*sim, 3500, write_en_wire, false);
    drive32(*sim, 3500, addr_wire, FirstOutOfRangeAddress);
    drive2(*sim, 3500, size_wire, 2);

    drive32(*sim, 3650, addr_wire, 2U);
    drive32(*sim, 3650, write_data_wire, 0xffffffffU);
    drive2(*sim, 3650, size_wire, 2);
    drive(*sim, 3650, write_en_wire, true);
    drive(*sim, 3710, clk_wire, true);
    drive(*sim, 3770, clk_wire, false);
    drive(*sim, 3790, write_en_wire, false);
    drive(*sim, 3840, read_en_wire, true);
    drive32(*sim, 3840, addr_wire, 0U);

    drive(*sim, 4050, rst_wire, true);
    drive32(*sim, 4120, addr_wire, LastWordAddress);
    drive2(*sim, 4120, size_wire, 2);
}

void BehavioralMemory64Kx32Test::verifyResults() {
    auto read_data_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("READ_DATA_OUT"));
    auto ready_wire = builder->getWire("READY_OUT");
    auto fault_wire = builder->getWire("FAULT_OUT");
    assert(read_data_wire && "BehavioralMemory64Kx32Test requires 32-bit READ_DATA_OUT wire");
    assert(ready_wire && "BehavioralMemory64Kx32Test requires READY_OUT wire");
    assert(fault_wire && "BehavioralMemory64Kx32Test requires FAULT_OUT wire");

    expectWireAt(*sim, ready_wire, {120, LogicValue::HIGH, "READY is asserted"}, "BehavioralMemory64Kx32Test");
    expectWireAt(*sim, fault_wire, {120, LogicValue::LOW, "valid reset read has no fault"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {120, 0x00000000U, "reset clears word 0"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {430, 0x12345678U, "word store and load at base"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {760, 0x89abcdefU, "word store and load at final word"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {1040, 0x01020304U, "word 1 store"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {1320, 0x01020304U, "WRITE_EN=0 preserves word 1"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {1660, 0x1234aa78U, "byte store updates one byte lane"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {2060, 0xbeefaa78U, "halfword store updates two byte lanes"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {2300, 0x000000efU, "LBU-style byte read"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {2450, 0xffffffefU, "LB-style sign-extended byte read"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {2600, 0x0000beefU, "LHU-style halfword read"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {2750, 0xffffbeefU, "LH-style sign-extended halfword read"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {3940, 0xbeefaa78U, "faulted write does not alter word 0"}, "BehavioralMemory64Kx32Test");
    expectRegisterAt(*sim, read_data_wire, {4200, 0x00000000U, "reset clears final word"}, "BehavioralMemory64Kx32Test");

    const ExpectedValue expected_faults[] = {
        {3000, LogicValue::HIGH, "SIZE=11 faults"},
        {3150, LogicValue::HIGH, "misaligned halfword faults"},
        {3300, LogicValue::HIGH, "misaligned word faults"},
        {3450, LogicValue::HIGH, "out-of-range byte access faults"},
        {3600, LogicValue::LOW, "no access suppresses FAULT"},
        {3710, LogicValue::HIGH, "faulted write reports FAULT"},
        {3940, LogicValue::LOW, "valid read after fault clears FAULT"},
    };

    for (const auto& expected : expected_faults) {
        expectWireAt(*sim, fault_wire, expected, "BehavioralMemory64Kx32Test");
    }

    if (auto memory = std::dynamic_pointer_cast<BehavioralMemory64Kx32>(root)) {
        expectBehavioralMemoryWordAt(
            *memory,
            430,
            0x00000000U,
            0x12345678U,
            "API exposes word store at base",
            "BehavioralMemory64Kx32Test");
        expectBehavioralMemoryWordAt(
            *memory,
            760,
            0x0003fffcU,
            0x89abcdefU,
            "API exposes word store at memory limit",
            "BehavioralMemory64Kx32Test");
        expectBehavioralMemoryWordAt(
            *memory,
            2060,
            0x00000000U,
            0xbeefaa78U,
            "API exposes byte and halfword stores inside word 0",
            "BehavioralMemory64Kx32Test");
        expectBehavioralMemoryWordAt(
            *memory,
            2190,
            0x00000008U,
            0x00000000U,
            "API exposes zero store as a touched word",
            "BehavioralMemory64Kx32Test");
        expectBehavioralMemoryWordAt(
            *memory,
            4200,
            0x0003fffcU,
            0x00000000U,
            "API exposes reset-cleared final word",
            "BehavioralMemory64Kx32Test");
        const auto all_touched = memory->getTouchedWordsAtTime(2190, BehavioralMemory64Kx32::capacityWords());
        assert(all_touched.size() == 4 && "Behavioral memory API should return all touched words when uncapped");
        assert(all_touched[0].first == 0x00000000U && "Behavioral memory touched words should be sorted by address");
        assert(all_touched[1].first == 0x00000004U && "Behavioral memory touched words should include word 1");
        assert(all_touched[2].first == 0x00000008U && "Behavioral memory touched words should include zero writes");
        assert(all_touched[3].first == 0x0003fffcU && "Behavioral memory touched words should include the final word");
        assert(memory->getTouchedWordCountAtTime(2060) == 3 && "Behavioral memory API should count three touched words");
        assert(memory->getTouchedWordCountAtTime(2190) == 4 && "Behavioral memory API should count zero writes as touched");
        assert(memory->getTouchedWordCountAtTime(4200) == 0 && "Behavioral memory API should count no touched words after reset");
        assert(memory->getTouchedWordsAtTime(4200, BehavioralMemory64Kx32::capacityWords()).empty()
               && "Behavioral memory API should return no touched words after reset");

        const auto first_bus_write = memory->getByteWritesInTimeRange(200, 400);
        requireMemory(first_bus_write.size() == 4,
                      "word bus write history should include all four byte lanes");
        requireMemory(first_bus_write.at(0) == 0x78 && first_bus_write.at(1) == 0x56
                          && first_bus_write.at(2) == 0x34 && first_bus_write.at(3) == 0x12,
                      "word bus write history should preserve little-endian byte values");

        const auto zero_bus_write = memory->getByteWritesInTimeRange(2100, 2190);
        requireMemory(zero_bus_write.size() == 4,
                      "zero-valued word store must still record four physical byte writes");
        requireMemory(zero_bus_write.at(8) == 0 && zero_bus_write.at(9) == 0
                          && zero_bus_write.at(10) == 0 && zero_bus_write.at(11) == 0,
                      "zero-valued word bus write history should preserve all zero bytes");

        requireMemory(memory->getByteWritesInTimeRange(3650, 3840).empty(),
                      "faulted bus write must not appear in successful write history");

        memory->writeU32AtTime(5000, 0x00000120U, 0x0000002bU);
        assert(memory->getTouchedWordCountAtTime(4999) == 0 && "Future direct writes must not appear before their simulation time");
        const auto direct_write_touched = memory->getTouchedWordsAtTime(5000, BehavioralMemory64Kx32::capacityWords());
        assert(direct_write_touched.size() == 1 && "Timed direct write should touch one word after reset");
        assert(direct_write_touched[0].first == 0x00000120U && "Timed direct write should report its word address");
        expectBehavioralMemoryWordAt(
            *memory,
            5000,
            0x00000120U,
            0x0000002bU,
            "API exposes timed direct write at its simulation time",
            "BehavioralMemory64Kx32Test");
    }
}

size_t BehavioralMemory64Kx32Test::getRunDuration() const {
    return 4300;
}

std::vector<SimulationTest::SimulationCheckpoint> BehavioralMemory64Kx32Test::getCheckpoints() const {
    return {
        {120, "Reset read", "Word 0 reads as 0x00000000 and FAULT=0", 0},
        {430, "SW/LW base", "Word store/load at address 0x00000000", 1},
        {760, "SW/LW limit", "Word store/load at address 0x0003fffc", 2},
        {1320, "Write disabled", "WRITE_EN=0 blocks a word write", 3},
        {1660, "Byte store", "SB updates one byte lane inside word 0", 4},
        {2060, "Halfword store", "SH updates two byte lanes inside word 0", 5},
        {2300, "LBU", "Byte read zero-extends", 6},
        {2450, "LB", "Byte read sign-extends", 7},
        {2600, "LHU", "Halfword read zero-extends", 8},
        {2750, "LH", "Halfword read sign-extends", 9},
        {3000, "Invalid size", "SIZE=11 reports FAULT", 10},
        {3150, "Misaligned half", "ADDR[0]!=0 reports FAULT for halfword", 11},
        {3300, "Misaligned word", "ADDR[1:0]!=00 reports FAULT for word", 12},
        {3450, "Out of range", "Address 0x00040000 reports FAULT", 13},
        {3600, "No access", "READ_EN=0 and WRITE_EN=0 suppress FAULT", 14},
        {3940, "Faulted write blocked", "A misaligned write does not change word 0", 15},
        {4200, "Reset clear", "RST clears the memory contents", 16},
    };
}

std::string Register32Test::getTestName() const {
    return "Register32Test";
}

void Register32Test::setupCircuit() {
    root = Component::create<Register32>("REGISTER32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void Register32Test::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "Register32Test requires IOComponent root");

    auto d_wire = builder->addNewWire<32>("D_IN", nullptr, {io_root->getInputPin<32>("D")});
    auto we_wire = builder->addNewWire("WE_IN", nullptr, {io_root->getInputPin("WE")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("Q_OUT", io_root->getOutputPin<32>("Q"), {});

    drive32(*sim, 0, d_wire, 0x00000000U);
    drive(*sim, 0, we_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);
    drive(*sim, 60, rst_wire, false);

    // WE=0: hold reset value despite all-one D.
    drive32(*sim, 100, d_wire, 0xffffffffU);
    drive(*sim, 130, clk_wire, true);
    drive(*sim, 180, clk_wire, false);

    // WE=1: capture all ones.
    drive(*sim, 200, we_wire, true);
    drive(*sim, 230, clk_wire, true);
    drive32(*sim, 250, d_wire, 0x00000000U);
    drive(*sim, 300, clk_wire, false);

    // Next clean rising edge captures the zero that the master sampled while CLK was low.
    drive(*sim, 360, clk_wire, true);
    drive(*sim, 410, clk_wire, false);

    // Alternating patterns.
    drive32(*sim, 430, d_wire, 0xaaaaaaaaU);
    drive(*sim, 460, clk_wire, true);
    drive(*sim, 520, clk_wire, false);
    drive32(*sim, 540, d_wire, 0x55555555U);
    drive(*sim, 570, clk_wire, true);
    drive(*sim, 630, clk_wire, false);

    // Walking-bit writes across all lanes.
    for (uint32_t bit_index = 0; bit_index < 32; ++bit_index) {
        const size_t base_time = 700 + static_cast<size_t>(bit_index) * 100;
        drive32(*sim, base_time, d_wire, uint32_t{1} << bit_index);
        drive(*sim, base_time + 30, clk_wire, true);
        drive(*sim, base_time + 70, clk_wire, false);
    }

    // WE=0 hold after walking-bit writes.
    drive(*sim, 3950, we_wire, false);
    drive32(*sim, 3960, d_wire, 0xdeadbeefU);
    drive(*sim, 3990, clk_wire, true);
    drive(*sim, 4040, clk_wire, false);

    // A single unknown bit should be captured only in its own lane.
    auto unknown_bit_pattern = bits32(0x12345678U);
    unknown_bit_pattern[13] = LogicValue::UNKNOWN;
    drive(*sim, 4080, we_wire, true);
    drive32(*sim, 4090, d_wire, unknown_bit_pattern);
    drive(*sim, 4120, clk_wire, true);
    drive(*sim, 4180, clk_wire, false);

    // Reset clears all 32 bits after nontrivial content.
    drive(*sim, 4220, rst_wire, true);
}

void Register32Test::verifyResults() {
    auto q_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("Q_OUT"));
    assert(q_wire && "Register32Test requires 32-bit Q_OUT wire");

    const ExpectedWord expected_words[] = {
        {90, 0x00000000U, "reset clears all bits"},
        {170, 0x00000000U, "WE=0 holds reset value"},
        {225, 0x00000000U, "WE=1 before rising edge has not captured yet"},
        {280, 0xffffffffU, "WE=1 captures all ones"},
        {330, 0xffffffffU, "falling edge does not update Q"},
        {405, 0x00000000U, "next rising edge captures zero"},
        {510, 0xaaaaaaaaU, "captures alternating 1010 pattern"},
        {620, 0x55555555U, "captures alternating 0101 pattern"},
        {4070, 0x80000000U, "WE=0 holds highest walking bit value"},
        {4260, 0x00000000U, "reset clears after unknown pattern"},
    };

    for (const auto& expected : expected_words) {
        expectRegisterAt(*sim, q_wire, expected);
    }

    for (uint32_t bit_index = 0; bit_index < 32; ++bit_index) {
        const size_t base_time = 700 + static_cast<size_t>(bit_index) * 100;
        const std::string label = "walking bit " + std::to_string(bit_index);
        expectRegisterAt(*sim, q_wire, {base_time + 90, uint32_t{1} << bit_index, label});
    }

    auto expected_unknown = bits32(0x12345678U);
    expected_unknown[13] = LogicValue::UNKNOWN;
    expectRegisterVectorAt(*sim, q_wire, {4170, expected_unknown, "captures one unknown data lane"});
}

size_t Register32Test::getRunDuration() const {
    return 4280;
}

std::vector<SimulationTest::SimulationCheckpoint> Register32Test::getCheckpoints() const {
    std::vector<SimulationCheckpoint> checkpoints{
        {90, "Reset clear", "RST=1 clears Q to 0x00000000", 0},
        {170, "Hold disabled", "WE=0 blocks write of 0xffffffff", 1},
        {280, "Write all ones", "WE=1 and rising CLK capture 0xffffffff", 2},
        {405, "Write zero", "Next rising CLK captures 0x00000000", 3},
        {510, "Write 0xAAAAAAAA", "Alternating high-bit pattern captured", 4},
        {620, "Write 0x55555555", "Alternating low-bit pattern captured", 5},
    };

    for (uint32_t bit_index = 0; bit_index < 32; ++bit_index) {
        const size_t base_time = 700 + static_cast<size_t>(bit_index) * 100;
        checkpoints.push_back({
            base_time + 90,
            "Walking bit " + std::to_string(bit_index),
            "Only bit " + std::to_string(bit_index) + " is stored high",
            static_cast<size_t>(6 + bit_index),
        });
    }

    checkpoints.push_back({4070, "Hold final walking bit", "WE=0 preserves 0x80000000", 38});
    checkpoints.push_back({4170, "Capture unknown bit", "Only bit 13 is unknown in Q", 39});
    checkpoints.push_back({4260, "Reset clear", "RST=1 clears all 32 bits", 40});
    return checkpoints;
}

std::string RegisterFile4x32Test::getTestName() const {
    return "RegisterFile4x32Test";
}

void RegisterFile4x32Test::setupCircuit() {
    root = Component::create<RegisterFile4x32>("REGISTER_FILE4X32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void RegisterFile4x32Test::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "RegisterFile4x32Test requires IOComponent root");

    auto rs1_addr_wire = builder->addNewWire<2>("RS1_ADDR_IN", nullptr, {io_root->getInputPin<2>("RS1_ADDR")});
    auto rs2_addr_wire = builder->addNewWire<2>("RS2_ADDR_IN", nullptr, {io_root->getInputPin<2>("RS2_ADDR")});
    auto rd_addr_wire = builder->addNewWire<2>("RD_ADDR_IN", nullptr, {io_root->getInputPin<2>("RD_ADDR")});
    auto write_data_wire = builder->addNewWire<32>("WRITE_DATA_IN", nullptr, {io_root->getInputPin<32>("WRITE_DATA")});
    auto reg_write_wire = builder->addNewWire("REG_WRITE_IN", nullptr, {io_root->getInputPin("REG_WRITE")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("RS1_DATA_OUT", io_root->getOutputPin<32>("RS1_DATA"), {});
    builder->addNewWire<32>("RS2_DATA_OUT", io_root->getOutputPin<32>("RS2_DATA"), {});

    drive2(*sim, 0, rs1_addr_wire, 0);
    drive2(*sim, 0, rs2_addr_wire, 1);
    drive2(*sim, 0, rd_addr_wire, 0);
    drive32(*sim, 0, write_data_wire, 0x00000000U);
    drive(*sim, 0, reg_write_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);
    drive(*sim, 80, rst_wire, false);

    // Writes to x0 must be ignored even when REG_WRITE is asserted.
    drive32(*sim, 200, write_data_wire, 0xffffffffU);
    drive2(*sim, 200, rd_addr_wire, 0);
    drive(*sim, 200, reg_write_wire, true);
    drive(*sim, 230, clk_wire, true);
    drive(*sim, 290, clk_wire, false);
    drive2(*sim, 330, rs1_addr_wire, 0);
    drive2(*sim, 330, rs2_addr_wire, 1);

    // Write x1 and verify dual read can see x1 beside hardwired x0.
    drive32(*sim, 430, write_data_wire, 0x11111111U);
    drive2(*sim, 430, rd_addr_wire, 1);
    drive(*sim, 480, clk_wire, true);
    drive(*sim, 540, clk_wire, false);
    drive2(*sim, 570, rs1_addr_wire, 1);
    drive2(*sim, 570, rs2_addr_wire, 0);

    // Write x2 without disturbing x1.
    drive32(*sim, 700, write_data_wire, 0x22222222U);
    drive2(*sim, 700, rd_addr_wire, 2);
    drive(*sim, 750, clk_wire, true);
    drive(*sim, 810, clk_wire, false);
    drive2(*sim, 840, rs1_addr_wire, 1);
    drive2(*sim, 840, rs2_addr_wire, 2);

    // Write x3 and verify simultaneous reads of x2/x3.
    drive32(*sim, 980, write_data_wire, 0x33333333U);
    drive2(*sim, 980, rd_addr_wire, 3);
    drive(*sim, 1030, clk_wire, true);
    drive(*sim, 1090, clk_wire, false);
    drive2(*sim, 1120, rs1_addr_wire, 2);
    drive2(*sim, 1120, rs2_addr_wire, 3);

    // Read ports are independent and can select any two registers.
    drive2(*sim, 1260, rs1_addr_wire, 1);
    drive2(*sim, 1260, rs2_addr_wire, 3);

    // REG_WRITE=0 must hold all registers despite clock and write data changes.
    drive(*sim, 1380, reg_write_wire, false);
    drive32(*sim, 1380, write_data_wire, 0xaaaaaaaaU);
    drive2(*sim, 1380, rd_addr_wire, 1);
    drive(*sim, 1430, clk_wire, true);
    drive(*sim, 1490, clk_wire, false);
    drive2(*sim, 1520, rs1_addr_wire, 1);
    drive2(*sim, 1520, rs2_addr_wire, 2);

    // Overwrite x2 only.
    drive(*sim, 1640, reg_write_wire, true);
    drive32(*sim, 1640, write_data_wire, 0x12345678U);
    drive2(*sim, 1640, rd_addr_wire, 2);
    drive(*sim, 1690, clk_wire, true);
    drive(*sim, 1750, clk_wire, false);
    drive2(*sim, 1780, rs1_addr_wire, 2);
    drive2(*sim, 1780, rs2_addr_wire, 1);

    // Another x0 write attempt must not affect x0 or the selected real register.
    drive32(*sim, 1920, write_data_wire, 0xdeadbeefU);
    drive2(*sim, 1920, rd_addr_wire, 0);
    drive(*sim, 1970, clk_wire, true);
    drive(*sim, 2030, clk_wire, false);
    drive2(*sim, 2060, rs1_addr_wire, 0);
    drive2(*sim, 2060, rs2_addr_wire, 3);

    // Unknown data in one lane should stay confined to that lane.
    auto unknown_x3 = bits32(0xcafebabeU);
    unknown_x3[7] = LogicValue::UNKNOWN;
    drive32(*sim, 2180, write_data_wire, unknown_x3);
    drive2(*sim, 2180, rd_addr_wire, 3);
    drive(*sim, 2230, clk_wire, true);
    drive(*sim, 2290, clk_wire, false);
    drive2(*sim, 2320, rs1_addr_wire, 3);
    drive2(*sim, 2320, rs2_addr_wire, 0);

    // Reset clears x1..x3; x0 remains zero.
    drive(*sim, 2460, rst_wire, true);
    drive2(*sim, 2500, rs1_addr_wire, 1);
    drive2(*sim, 2500, rs2_addr_wire, 3);
}

void RegisterFile4x32Test::verifyResults() {
    auto rs1_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("RS1_DATA_OUT"));
    auto rs2_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("RS2_DATA_OUT"));
    assert(rs1_wire && "RegisterFile4x32Test requires 32-bit RS1_DATA_OUT wire");
    assert(rs2_wire && "RegisterFile4x32Test requires 32-bit RS2_DATA_OUT wire");

    const ExpectedWord rs1_expected[] = {
        {180, 0x00000000U, "reset reads x0 as zero"},
        {390, 0x00000000U, "write to x0 is ignored"},
        {650, 0x11111111U, "read port 1 sees x1 write"},
        {930, 0x11111111U, "x1 is preserved while writing x2"},
        {1210, 0x22222222U, "read port 1 selects x2"},
        {1330, 0x11111111U, "read port 1 independently selects x1"},
        {1600, 0x11111111U, "REG_WRITE=0 holds x1"},
        {1870, 0x12345678U, "x2 overwrite is visible"},
        {2140, 0x00000000U, "second x0 write is ignored"},
        {2600, 0x00000000U, "reset clears x1"},
    };

    const ExpectedWord rs2_expected[] = {
        {180, 0x00000000U, "reset reads x1 as zero"},
        {390, 0x00000000U, "x1 remains zero after x0 write"},
        {650, 0x00000000U, "read port 2 independently selects x0"},
        {930, 0x22222222U, "read port 2 sees x2 write"},
        {1210, 0x33333333U, "read port 2 sees x3 write"},
        {1330, 0x33333333U, "read port 2 independently selects x3"},
        {1600, 0x22222222U, "REG_WRITE=0 holds x2"},
        {1870, 0x11111111U, "x1 preserved while x2 is overwritten"},
        {2140, 0x33333333U, "x3 preserved while writing x0"},
        {2410, 0x00000000U, "read port 2 selects hardwired x0"},
        {2600, 0x00000000U, "reset clears x3"},
    };

    for (const auto& expected : rs1_expected) {
        expectRegisterAt(*sim, rs1_wire, expected);
    }

    for (const auto& expected : rs2_expected) {
        expectRegisterAt(*sim, rs2_wire, expected);
    }

    auto expected_unknown_x3 = bits32(0xcafebabeU);
    expected_unknown_x3[7] = LogicValue::UNKNOWN;
    expectRegisterVectorAt(*sim, rs1_wire, {2410, expected_unknown_x3, "x3 captures one unknown lane"});
}

size_t RegisterFile4x32Test::getRunDuration() const {
    return 2620;
}

std::vector<SimulationTest::SimulationCheckpoint> RegisterFile4x32Test::getCheckpoints() const {
    return {
        {180, "Reset reads", "x0 and x1 both read as 0x00000000", 0},
        {390, "Ignore x0 write", "RD=0 does not store 0xffffffff", 1},
        {650, "Write x1", "x1 captures 0x11111111 while x0 remains zero", 2},
        {930, "Write x2", "x2 captures 0x22222222 and x1 is preserved", 3},
        {1210, "Write x3", "Dual read returns x2 and x3", 4},
        {1330, "Independent reads", "RS1 selects x1 while RS2 selects x3", 5},
        {1600, "Hold disabled", "REG_WRITE=0 blocks a write to x1", 6},
        {1870, "Overwrite x2", "x2 updates to 0x12345678", 7},
        {2140, "Ignore x0 again", "x0 remains zero while x3 is preserved", 8},
        {2410, "Unknown lane", "x3 captures one unknown data bit", 9},
        {2600, "Reset clear", "RST clears x1 and x3 back to zero", 10},
    };
}

std::string RegisterFile32x32Test::getTestName() const {
    return "RegisterFile32x32Test";
}

void RegisterFile32x32Test::setupCircuit() {
    root = Component::create<RegisterFile32x32>("REGISTER_FILE32X32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void RegisterFile32x32Test::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "RegisterFile32x32Test requires IOComponent root");

    auto rs1_addr_wire = builder->addNewWire<5>("RS1_ADDR_IN", nullptr, {io_root->getInputPin<5>("RS1_ADDR")});
    auto rs2_addr_wire = builder->addNewWire<5>("RS2_ADDR_IN", nullptr, {io_root->getInputPin<5>("RS2_ADDR")});
    auto rd_addr_wire = builder->addNewWire<5>("RD_ADDR_IN", nullptr, {io_root->getInputPin<5>("RD_ADDR")});
    auto write_data_wire = builder->addNewWire<32>("WRITE_DATA_IN", nullptr, {io_root->getInputPin<32>("WRITE_DATA")});
    auto reg_write_wire = builder->addNewWire("REG_WRITE_IN", nullptr, {io_root->getInputPin("REG_WRITE")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("RS1_DATA_OUT", io_root->getOutputPin<32>("RS1_DATA"), {});
    builder->addNewWire<32>("RS2_DATA_OUT", io_root->getOutputPin<32>("RS2_DATA"), {});

    drive5(*sim, 0, rs1_addr_wire, 0);
    drive5(*sim, 0, rs2_addr_wire, 1);
    drive5(*sim, 0, rd_addr_wire, 0);
    drive32(*sim, 0, write_data_wire, 0x00000000U);
    drive(*sim, 0, reg_write_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);
    drive(*sim, 100, rst_wire, false);

    // x0 is hardwired and ignores write attempts.
    drive32(*sim, 220, write_data_wire, 0xffffffffU);
    drive5(*sim, 220, rd_addr_wire, 0);
    drive(*sim, 220, reg_write_wire, true);
    drive(*sim, 270, clk_wire, true);
    drive(*sim, 340, clk_wire, false);
    drive5(*sim, 380, rs1_addr_wire, 0);
    drive5(*sim, 380, rs2_addr_wire, 1);

    // Write every writable register and read it back beside the previous register.
    for (uint32_t reg = 1; reg < 32; ++reg) {
        const size_t base_time = 600 + static_cast<size_t>(reg - 1) * 260;
        drive32(*sim, base_time, write_data_wire, registerFilePattern(reg));
        drive5(*sim, base_time, rd_addr_wire, reg);
        drive(*sim, base_time + 50, clk_wire, true);
        drive(*sim, base_time + 120, clk_wire, false);
        drive5(*sim, base_time + 150, rs1_addr_wire, reg);
        drive5(*sim, base_time + 150, rs2_addr_wire, reg - 1);
    }

    // REG_WRITE=0 must block writes.
    drive(*sim, 8820, reg_write_wire, false);
    drive32(*sim, 8820, write_data_wire, 0xdeadbeefU);
    drive5(*sim, 8820, rd_addr_wire, 5);
    drive(*sim, 8870, clk_wire, true);
    drive(*sim, 8940, clk_wire, false);
    drive5(*sim, 8980, rs1_addr_wire, 5);
    drive5(*sim, 8980, rs2_addr_wire, 31);

    // Another x0 write attempt must not affect x0 or x31.
    drive(*sim, 9140, reg_write_wire, true);
    drive32(*sim, 9140, write_data_wire, 0xaaaaaaaaU);
    drive5(*sim, 9140, rd_addr_wire, 0);
    drive(*sim, 9190, clk_wire, true);
    drive(*sim, 9260, clk_wire, false);
    drive5(*sim, 9300, rs1_addr_wire, 0);
    drive5(*sim, 9300, rs2_addr_wire, 31);

    // Unknown data should stay lane-local in the selected register.
    auto unknown_x13 = bits32(0xcafebabeU);
    unknown_x13[7] = LogicValue::UNKNOWN;
    drive32(*sim, 9460, write_data_wire, unknown_x13);
    drive5(*sim, 9460, rd_addr_wire, 13);
    drive(*sim, 9510, clk_wire, true);
    drive(*sim, 9580, clk_wire, false);
    drive5(*sim, 9620, rs1_addr_wire, 13);
    drive5(*sim, 9620, rs2_addr_wire, 12);

    // Reset clears all writable registers; x0 remains zero.
    drive(*sim, 9780, rst_wire, true);
    drive5(*sim, 9820, rs1_addr_wire, 13);
    drive5(*sim, 9820, rs2_addr_wire, 31);
}

void RegisterFile32x32Test::verifyResults() {
    auto rs1_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("RS1_DATA_OUT"));
    auto rs2_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("RS2_DATA_OUT"));
    assert(rs1_wire && "RegisterFile32x32Test requires 32-bit RS1_DATA_OUT wire");
    assert(rs2_wire && "RegisterFile32x32Test requires 32-bit RS2_DATA_OUT wire");
    const auto test_name = getTestName();
    const auto* test_name_c = test_name.c_str();

    expectRegisterAt(*sim, rs1_wire, {180, 0x00000000U, "reset reads x0 as zero"}, test_name_c);
    expectRegisterAt(*sim, rs2_wire, {180, 0x00000000U, "reset reads x1 as zero"}, test_name_c);
    expectRegisterAt(*sim, rs1_wire, {520, 0x00000000U, "write to x0 is ignored"}, test_name_c);
    expectRegisterAt(*sim, rs2_wire, {520, 0x00000000U, "x1 remains zero after x0 write"}, test_name_c);

    for (uint32_t reg = 1; reg < 32; ++reg) {
        const size_t base_time = 600 + static_cast<size_t>(reg - 1) * 260;
        const std::string rs1_label = "RS1 reads x" + std::to_string(reg);
        expectRegisterAt(
            *sim,
            rs1_wire,
            {base_time + 250, registerFilePattern(reg), rs1_label},
            test_name_c);

        const uint32_t expected_rs2 = (reg == 1) ? 0x00000000U : registerFilePattern(reg - 1);
        const std::string rs2_label = "RS2 reads x" + std::to_string(reg - 1);
        expectRegisterAt(*sim, rs2_wire, {base_time + 250, expected_rs2, rs2_label}, test_name_c);
    }

    expectRegisterAt(*sim, rs1_wire, {9120, registerFilePattern(5), "REG_WRITE=0 holds x5"}, test_name_c);
    expectRegisterAt(*sim, rs2_wire, {9120, registerFilePattern(31), "REG_WRITE=0 preserves x31"}, test_name_c);
    expectRegisterAt(*sim, rs1_wire, {9440, 0x00000000U, "second write to x0 is ignored"}, test_name_c);
    expectRegisterAt(*sim, rs2_wire, {9440, registerFilePattern(31), "x31 survives x0 write"}, test_name_c);

    auto expected_unknown_x13 = bits32(0xcafebabeU);
    expected_unknown_x13[7] = LogicValue::UNKNOWN;
    expectRegisterVectorAt(
        *sim,
        rs1_wire,
        {9760, expected_unknown_x13, "x13 captures one unknown lane"},
        test_name_c);
    expectRegisterAt(*sim, rs2_wire, {9760, registerFilePattern(12), "x12 unaffected by unknown x13 write"}, test_name_c);

    expectRegisterAt(*sim, rs1_wire, {9960, 0x00000000U, "reset clears x13"}, test_name_c);
    expectRegisterAt(*sim, rs2_wire, {9960, 0x00000000U, "reset clears x31"}, test_name_c);

    if (auto behavioral = std::dynamic_pointer_cast<BehavioralRegisterFile32x32>(root)) {
        expectBehavioralRegisterStateAt(
            *behavioral,
            850,
            1,
            registerFilePattern(1),
            "API exposes x1 after first write",
            test_name_c);
        expectBehavioralRegisterStateAt(
            *behavioral,
            9760,
            13,
            expected_unknown_x13,
            "API preserves x13 unknown lane",
            test_name_c);
        expectBehavioralRegisterStateAt(
            *behavioral,
            9960,
            31,
            0x00000000U,
            "API exposes reset-cleared x31",
            test_name_c);
    }
}

size_t RegisterFile32x32Test::getRunDuration() const {
    return 10000;
}

std::vector<SimulationTest::SimulationCheckpoint> RegisterFile32x32Test::getCheckpoints() const {
    std::vector<SimulationCheckpoint> checkpoints{
        {180, "Reset reads", "x0 and x1 both read as 0x00000000", 0},
        {520, "Ignore x0 write", "RD=0 does not store 0xffffffff", 1},
    };

    for (uint32_t reg = 1; reg < 32; ++reg) {
        const size_t base_time = 600 + static_cast<size_t>(reg - 1) * 260;
        checkpoints.push_back({
            base_time + 250,
            "Write x" + std::to_string(reg),
            "RS1 reads x" + std::to_string(reg) + ", RS2 reads x" + std::to_string(reg - 1),
            static_cast<size_t>(1 + reg),
        });
    }

    checkpoints.push_back({9120, "Hold disabled", "REG_WRITE=0 blocks a write to x5", 33});
    checkpoints.push_back({9440, "Ignore x0 again", "x0 remains zero while x31 is preserved", 34});
    checkpoints.push_back({9760, "Unknown lane", "x13 captures one unknown data bit", 35});
    checkpoints.push_back({9960, "Reset clear", "RST clears x13 and x31 back to zero", 36});
    return checkpoints;
}

std::string BehavioralRegisterFile32x32Test::getTestName() const {
    return "BehavioralRegisterFile32x32Test";
}

void BehavioralRegisterFile32x32Test::setupCircuit() {
    root = Component::create<BehavioralRegisterFile32x32>("BEHAVIORAL_REGISTER_FILE32X32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

std::string BehavioralRegisterFile32x32UnknownTest::getTestName() const {
    return "BehavioralRegisterFile32x32UnknownTest";
}

void BehavioralRegisterFile32x32UnknownTest::setupCircuit() {
    root = Component::create<BehavioralRegisterFile32x32>("BEHAVIORAL_REGISTER_FILE32X32_UNKNOWN_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void BehavioralRegisterFile32x32UnknownTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "BehavioralRegisterFile32x32UnknownTest requires IOComponent root");

    auto rs1_addr_wire = builder->addNewWire<5>("RS1_ADDR_IN", nullptr, {io_root->getInputPin<5>("RS1_ADDR")});
    auto rs2_addr_wire = builder->addNewWire<5>("RS2_ADDR_IN", nullptr, {io_root->getInputPin<5>("RS2_ADDR")});
    auto rd_addr_wire = builder->addNewWire<5>("RD_ADDR_IN", nullptr, {io_root->getInputPin<5>("RD_ADDR")});
    auto write_data_wire = builder->addNewWire<32>("WRITE_DATA_IN", nullptr, {io_root->getInputPin<32>("WRITE_DATA")});
    auto reg_write_wire = builder->addNewWire("REG_WRITE_IN", nullptr, {io_root->getInputPin("REG_WRITE")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("RS1_DATA_OUT", io_root->getOutputPin<32>("RS1_DATA"), {});
    builder->addNewWire<32>("RS2_DATA_OUT", io_root->getOutputPin<32>("RS2_DATA"), {});

    const std::vector<LogicValue> maybe_x3_or_x7{
        LogicValue::HIGH,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::LOW,
        LogicValue::LOW,
    };

    drive5(*sim, 0, rs1_addr_wire, 0);
    drive5(*sim, 0, rs2_addr_wire, 1);
    drive5(*sim, 0, rd_addr_wire, 0);
    drive32(*sim, 0, write_data_wire, 0x00000000U);
    drive(*sim, 0, reg_write_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);
    drive(*sim, 50, rst_wire, false);

    drive32(*sim, 100, write_data_wire, 0xaaaaaaaaU);
    drive5(*sim, 100, rd_addr_wire, 3);
    drive(*sim, 100, reg_write_wire, true);
    drive(*sim, 130, clk_wire, true);
    drive(*sim, 170, clk_wire, false);
    drive5(*sim, 200, rs1_addr_wire, 3);

    drive32(*sim, 260, write_data_wire, 0xaaaaaaaaU);
    drive5(*sim, 260, rd_addr_wire, 7);
    drive(*sim, 290, clk_wire, true);
    drive(*sim, 330, clk_wire, false);
    drive5(*sim, 360, rs1_addr_wire, maybe_x3_or_x7);

    drive32(*sim, 450, write_data_wire, 0x55555555U);
    drive5(*sim, 450, rd_addr_wire, 7);
    drive(*sim, 480, clk_wire, true);
    drive(*sim, 520, clk_wire, false);
    drive5(*sim, 550, rs1_addr_wire, maybe_x3_or_x7);

    drive32(*sim, 650, write_data_wire, 0x12345678U);
    drive5(*sim, 650, rd_addr_wire, maybe_x3_or_x7);
    drive(*sim, 680, clk_wire, true);
    drive(*sim, 720, clk_wire, false);
    drive5(*sim, 750, rs1_addr_wire, 3);
    drive5(*sim, 750, rs2_addr_wire, 7);

    drive(*sim, 850, rst_wire, true);
    drive(*sim, 900, rst_wire, false);
    drive5(*sim, 910, rs1_addr_wire, 3);
    drive5(*sim, 910, rs2_addr_wire, 7);

    drive32(*sim, 980, write_data_wire, 0xffffffffU);
    drive5(*sim, 980, rd_addr_wire, 9);
    drive(*sim, 980, reg_write_wire, LogicValue::UNKNOWN);
    drive(*sim, 1010, clk_wire, true);
    drive(*sim, 1050, clk_wire, false);
    drive5(*sim, 1080, rs1_addr_wire, 9);
    drive5(*sim, 1080, rs2_addr_wire, 0);

    drive(*sim, 1180, rst_wire, LogicValue::UNKNOWN);
    drive5(*sim, 1200, rs1_addr_wire, 1);
    drive5(*sim, 1200, rs2_addr_wire, 0);
}

void BehavioralRegisterFile32x32UnknownTest::verifyResults() {
    auto rs1_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("RS1_DATA_OUT"));
    auto rs2_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("RS2_DATA_OUT"));
    assert(rs1_wire && "BehavioralRegisterFile32x32UnknownTest requires 32-bit RS1_DATA_OUT wire");
    assert(rs2_wire && "BehavioralRegisterFile32x32UnknownTest requires 32-bit RS2_DATA_OUT wire");

    expectRegisterAt(*sim, rs1_wire, {80, 0x00000000U, "unknown-free reset reads zero"}, "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterAt(*sim, rs1_wire, {230, 0xaaaaaaaaU, "x3 stores first pattern"}, "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterAt(*sim, rs1_wire, {390, 0xaaaaaaaaU, "ambiguous x3/x7 read agrees bitwise"}, "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterVectorAt(
        *sim,
        rs1_wire,
        {590, unknownWord32(), "ambiguous x3/x7 read disagrees bitwise"},
        "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterVectorAt(
        *sim,
        rs1_wire,
        {790, unknownWord32(), "ambiguous write contaminates x3"},
        "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterVectorAt(
        *sim,
        rs2_wire,
        {790, unknownWord32(), "ambiguous write contaminates x7"},
        "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterAt(*sim, rs1_wire, {940, 0x00000000U, "reset clears x3"}, "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterAt(*sim, rs2_wire, {940, 0x00000000U, "reset clears x7"}, "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterVectorAt(
        *sim,
        rs1_wire,
        {1120, unknownWord32(), "unknown REG_WRITE contaminates selected x9"},
        "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterAt(*sim, rs2_wire, {1120, 0x00000000U, "x0 remains zero after unknown REG_WRITE"}, "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterVectorAt(
        *sim,
        rs1_wire,
        {1240, unknownWord32(), "unknown reset contaminates writable x1"},
        "BehavioralRegisterFile32x32UnknownTest");
    expectRegisterAt(*sim, rs2_wire, {1240, 0x00000000U, "x0 remains zero after unknown reset"}, "BehavioralRegisterFile32x32UnknownTest");
}

size_t BehavioralRegisterFile32x32UnknownTest::getRunDuration() const {
    return 1260;
}

std::vector<SimulationTest::SimulationCheckpoint> BehavioralRegisterFile32x32UnknownTest::getCheckpoints() const {
    return {
        {80, "Reset", "x0 reads as zero after reset", 0},
        {230, "Write x3", "x3 stores 0xaaaaaaaa", 1},
        {390, "Ambiguous read agrees", "RS1 may select x3 or x7; both contain 0xaaaaaaaa", 2},
        {590, "Ambiguous read differs", "RS1 may select x3 or x7 with different values, so every lane is unknown", 3},
        {790, "Ambiguous write", "Unknown RD address contaminates both possible targets", 4},
        {940, "Reset clear", "Reset recovers x3 and x7 to zero", 5},
        {1120, "Unknown REG_WRITE", "Unknown write enable contaminates the selected writable register", 6},
        {1240, "Unknown reset", "Unknown reset contaminates writable registers while x0 remains zero", 7},
    };
}

std::string Memory4x32Test::getTestName() const {
    return "Memory4x32Test";
}

void Memory4x32Test::setupCircuit() {
    root = Component::create<Memory4x32>("MEMORY4X32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void Memory4x32Test::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "Memory4x32Test requires IOComponent root");

    auto addr_wire = builder->addNewWire<32>("ADDR_IN", nullptr, {io_root->getInputPin<32>("ADDR")});
    auto write_data_wire = builder->addNewWire<32>("WRITE_DATA_IN", nullptr, {io_root->getInputPin<32>("WRITE_DATA")});
    auto read_en_wire = builder->addNewWire("READ_EN_IN", nullptr, {io_root->getInputPin("READ_EN")});
    auto write_en_wire = builder->addNewWire("WRITE_EN_IN", nullptr, {io_root->getInputPin("WRITE_EN")});
    auto size_wire = builder->addNewWire<2>("SIZE_IN", nullptr, {io_root->getInputPin<2>("SIZE")});
    auto sign_extend_wire = builder->addNewWire("SIGN_EXTEND_IN", nullptr, {io_root->getInputPin("SIGN_EXTEND")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("READ_DATA_OUT", io_root->getOutputPin<32>("READ_DATA"), {});
    builder->addNewWire("READY_OUT", io_root->getOutputPin("READY"), {});
    builder->addNewWire("FAULT_OUT", io_root->getOutputPin("FAULT"), {});

    drive32(*sim, 0, addr_wire, 0x00000000U);
    drive32(*sim, 0, write_data_wire, 0x00000000U);
    drive(*sim, 0, read_en_wire, false);
    drive(*sim, 0, write_en_wire, false);
    drive2(*sim, 0, size_wire, 2);
    drive(*sim, 0, sign_extend_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);

    drive(*sim, 80, rst_wire, false);
    drive(*sim, 120, read_en_wire, true);

    const uint32_t patterns[] = {
        0x10203040U,
        0xa5a55a5aU,
        0x0badcafeU,
        0xf00dc0deU,
    };

    for (uint32_t word = 0; word < 4; ++word) {
        const size_t base_time = 300 + static_cast<size_t>(word) * 350;
        drive32(*sim, base_time, addr_wire, word * 4U);
        drive32(*sim, base_time, write_data_wire, patterns[word]);
        drive2(*sim, base_time, size_wire, 2);
        drive(*sim, base_time, write_en_wire, true);
        drive(*sim, base_time + 140, clk_wire, true);
        drive(*sim, base_time + 210, clk_wire, false);
        drive(*sim, base_time + 220, write_en_wire, false);
        drive32(*sim, base_time + 240, addr_wire, word * 4U);
    }

    for (uint32_t word = 0; word < 4; ++word) {
        const size_t base_time = 1750 + static_cast<size_t>(word) * 180;
        drive32(*sim, base_time, addr_wire, word * 4U);
        drive2(*sim, base_time, size_wire, 2);
        drive(*sim, base_time, read_en_wire, true);
        drive(*sim, base_time, write_en_wire, false);
    }

    // WRITE_EN=0 must not modify a word, even with a clock edge.
    drive32(*sim, 2480, addr_wire, 4U);
    drive32(*sim, 2480, write_data_wire, 0xdeadbeefU);
    drive2(*sim, 2480, size_wire, 2);
    drive(*sim, 2480, write_en_wire, false);
    drive(*sim, 2620, clk_wire, true);
    drive(*sim, 2680, clk_wire, false);
    drive32(*sim, 2710, addr_wire, 4U);

    // This first memory slice is word-granular: byte/halfword sizes raise FAULT.
    drive32(*sim, 2900, addr_wire, 0U);
    drive2(*sim, 2900, size_wire, 0);
    drive(*sim, 2900, read_en_wire, true);

    drive32(*sim, 3060, addr_wire, 0U);
    drive2(*sim, 3060, size_wire, 1);

    drive32(*sim, 3220, addr_wire, 2U);
    drive2(*sim, 3220, size_wire, 2);

    drive32(*sim, 3380, addr_wire, 16U);
    drive2(*sim, 3380, size_wire, 2);

    drive(*sim, 3600, read_en_wire, false);
    drive(*sim, 3600, write_en_wire, false);
    drive32(*sim, 3600, addr_wire, 16U);
    drive2(*sim, 3600, size_wire, 2);

    // A misaligned write request raises FAULT and must not alter the selected word.
    drive32(*sim, 3780, addr_wire, 2U);
    drive32(*sim, 3780, write_data_wire, 0xffffffffU);
    drive2(*sim, 3780, size_wire, 2);
    drive(*sim, 3780, write_en_wire, true);
    drive(*sim, 3920, clk_wire, true);
    drive(*sim, 3980, clk_wire, false);
    drive(*sim, 3990, write_en_wire, false);
    drive(*sim, 4060, read_en_wire, true);
    drive32(*sim, 4060, addr_wire, 0U);

    // Reset clears all words.
    drive(*sim, 4300, rst_wire, true);
    drive32(*sim, 4380, addr_wire, 12U);
    drive2(*sim, 4380, size_wire, 2);
}

void Memory4x32Test::verifyResults() {
    auto read_data_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("READ_DATA_OUT"));
    auto ready_wire = builder->getWire("READY_OUT");
    auto fault_wire = builder->getWire("FAULT_OUT");
    assert(read_data_wire && "Memory4x32Test requires 32-bit READ_DATA_OUT wire");
    assert(ready_wire && "Memory4x32Test requires READY_OUT wire");
    assert(fault_wire && "Memory4x32Test requires FAULT_OUT wire");

    expectWireAt(*sim, ready_wire, {160, LogicValue::HIGH, "READY is asserted"}, "Memory4x32Test");
    expectWireAt(*sim, fault_wire, {220, LogicValue::LOW, "valid reset read has no fault"}, "Memory4x32Test");
    expectRegisterAt(*sim, read_data_wire, {220, 0x00000000U, "reset clears word 0"}, "Memory4x32Test");

    const ExpectedWord expected_words[] = {
        {620, 0x10203040U, "word 0 write"},
        {970, 0xa5a55a5aU, "word 1 write"},
        {1320, 0x0badcafeU, "word 2 write"},
        {1670, 0xf00dc0deU, "word 3 write"},
        {1880, 0x10203040U, "readback word 0"},
        {2060, 0xa5a55a5aU, "readback word 1"},
        {2240, 0x0badcafeU, "readback word 2"},
        {2420, 0xf00dc0deU, "readback word 3"},
        {2840, 0xa5a55a5aU, "WRITE_EN=0 preserves word 1"},
        {4200, 0x10203040U, "misaligned write does not alter word 0"},
        {4500, 0x00000000U, "reset clears word 3"},
    };

    for (const auto& expected : expected_words) {
        expectRegisterAt(*sim, read_data_wire, expected, "Memory4x32Test");
    }

    const ExpectedValue expected_faults[] = {
        {3000, LogicValue::HIGH, "byte size is unsupported in the structural slice"},
        {3160, LogicValue::HIGH, "halfword size is unsupported in the structural slice"},
        {3320, LogicValue::HIGH, "word access must be 4-byte aligned"},
        {3520, LogicValue::HIGH, "address above the 16-byte window faults"},
        {3720, LogicValue::LOW, "no access suppresses fault reporting"},
        {3920, LogicValue::HIGH, "misaligned write reports fault"},
        {4200, LogicValue::LOW, "valid read after fault clears FAULT"},
    };

    for (const auto& expected : expected_faults) {
        expectWireAt(*sim, fault_wire, expected, "Memory4x32Test");
    }
}

size_t Memory4x32Test::getRunDuration() const {
    return 4550;
}

std::vector<SimulationTest::SimulationCheckpoint> Memory4x32Test::getCheckpoints() const {
    return {
        {220, "Reset read", "Word 0 reads as 0x00000000 and FAULT=0", 0},
        {620, "Write word 0", "Aligned SW stores 0x10203040", 1},
        {970, "Write word 1", "Aligned SW stores 0xa5a55a5a", 2},
        {1320, "Write word 2", "Aligned SW stores 0x0badcafe", 3},
        {1670, "Write word 3", "Aligned SW stores 0xf00dc0de", 4},
        {2420, "Read mux", "Address bits [3:2] select word 3", 5},
        {2840, "Write disabled", "WRITE_EN=0 holds word 1", 6},
        {3000, "Unsupported byte", "SIZE=00 reports FAULT in this word-granular slice", 7},
        {3160, "Unsupported halfword", "SIZE=01 reports FAULT in this word-granular slice", 8},
        {3320, "Misaligned word", "ADDR[1:0]!=00 reports FAULT", 9},
        {3520, "Out of range", "ADDR[31:4]!=0 reports FAULT", 10},
        {3720, "No access", "READ_EN=0 and WRITE_EN=0 suppress FAULT", 11},
        {4200, "Faulted write blocked", "A misaligned write does not change word 0", 12},
        {4500, "Reset clear", "RST clears the stored words", 13},
    };
}

std::string Memory32x32Test::getTestName() const {
    return "Memory32x32Test";
}

void Memory32x32Test::setupCircuit() {
    root = Component::create<Memory32x32>("MEMORY32X32_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void Memory32x32Test::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "Memory32x32Test requires IOComponent root");

    auto addr_wire = builder->addNewWire<32>("ADDR_IN", nullptr, {io_root->getInputPin<32>("ADDR")});
    auto write_data_wire = builder->addNewWire<32>("WRITE_DATA_IN", nullptr, {io_root->getInputPin<32>("WRITE_DATA")});
    auto read_en_wire = builder->addNewWire("READ_EN_IN", nullptr, {io_root->getInputPin("READ_EN")});
    auto write_en_wire = builder->addNewWire("WRITE_EN_IN", nullptr, {io_root->getInputPin("WRITE_EN")});
    auto size_wire = builder->addNewWire<2>("SIZE_IN", nullptr, {io_root->getInputPin<2>("SIZE")});
    auto sign_extend_wire = builder->addNewWire("SIGN_EXTEND_IN", nullptr, {io_root->getInputPin("SIGN_EXTEND")});
    auto clk_wire = builder->addNewWire("CLK_IN", nullptr, {io_root->getInputPin("CLK")});
    auto rst_wire = builder->addNewWire("RST_IN", nullptr, {io_root->getInputPin("RST")});
    builder->addNewWire<32>("READ_DATA_OUT", io_root->getOutputPin<32>("READ_DATA"), {});
    builder->addNewWire("READY_OUT", io_root->getOutputPin("READY"), {});
    builder->addNewWire("FAULT_OUT", io_root->getOutputPin("FAULT"), {});

    drive32(*sim, 0, addr_wire, 0x00000000U);
    drive32(*sim, 0, write_data_wire, 0x00000000U);
    drive(*sim, 0, read_en_wire, false);
    drive(*sim, 0, write_en_wire, false);
    drive2(*sim, 0, size_wire, 2);
    drive(*sim, 0, sign_extend_wire, false);
    drive(*sim, 0, clk_wire, false);
    drive(*sim, 0, rst_wire, true);

    drive(*sim, 80, rst_wire, false);
    drive(*sim, 120, read_en_wire, true);

    for (uint32_t word = 0; word < 32; ++word) {
        const size_t base_time = 300 + static_cast<size_t>(word) * 420;
        drive32(*sim, base_time, addr_wire, word * 4U);
        drive32(*sim, base_time, write_data_wire, memoryWordPattern(word));
        drive2(*sim, base_time, size_wire, 2);
        drive(*sim, base_time, write_en_wire, true);
        drive(*sim, base_time + 120, clk_wire, true);
        drive(*sim, base_time + 180, clk_wire, false);
        drive(*sim, base_time + 190, write_en_wire, false);
        drive32(*sim, base_time + 210, addr_wire, word * 4U);
    }

    // WRITE_EN=0 must not modify a selected word.
    drive32(*sim, 14000, addr_wire, 17U * 4U);
    drive32(*sim, 14000, write_data_wire, 0xdeadbeefU);
    drive2(*sim, 14000, size_wire, 2);
    drive(*sim, 14000, write_en_wire, false);
    drive(*sim, 14140, clk_wire, true);
    drive(*sim, 14200, clk_wire, false);
    drive32(*sim, 14240, addr_wire, 17U * 4U);

    // Size, alignment, and range checks mirror Memory4x32 but use the 128-byte window.
    drive32(*sim, 14600, addr_wire, 0U);
    drive2(*sim, 14600, size_wire, 0);
    drive(*sim, 14600, read_en_wire, true);

    drive32(*sim, 14760, addr_wire, 0U);
    drive2(*sim, 14760, size_wire, 1);

    drive32(*sim, 14920, addr_wire, 2U);
    drive2(*sim, 14920, size_wire, 2);

    drive32(*sim, 15080, addr_wire, 128U);
    drive2(*sim, 15080, size_wire, 2);

    drive(*sim, 15300, read_en_wire, false);
    drive(*sim, 15300, write_en_wire, false);
    drive32(*sim, 15300, addr_wire, 128U);
    drive2(*sim, 15300, size_wire, 2);

    // A faulted write is blocked.
    drive32(*sim, 15500, addr_wire, 2U);
    drive32(*sim, 15500, write_data_wire, 0xffffffffU);
    drive2(*sim, 15500, size_wire, 2);
    drive(*sim, 15500, write_en_wire, true);
    drive(*sim, 15640, clk_wire, true);
    drive(*sim, 15700, clk_wire, false);
    drive(*sim, 15710, write_en_wire, false);
    drive(*sim, 15800, read_en_wire, true);
    drive32(*sim, 15800, addr_wire, 0U);

    // Reset clears all 32 words.
    drive(*sim, 16050, rst_wire, true);
    drive32(*sim, 16120, addr_wire, 31U * 4U);
    drive2(*sim, 16120, size_wire, 2);
}

void Memory32x32Test::verifyResults() {
    auto read_data_wire = std::dynamic_pointer_cast<Wire<32>>(builder->getWireDynamic("READ_DATA_OUT"));
    auto ready_wire = builder->getWire("READY_OUT");
    auto fault_wire = builder->getWire("FAULT_OUT");
    assert(read_data_wire && "Memory32x32Test requires 32-bit READ_DATA_OUT wire");
    assert(ready_wire && "Memory32x32Test requires READY_OUT wire");
    assert(fault_wire && "Memory32x32Test requires FAULT_OUT wire");

    expectWireAt(*sim, ready_wire, {160, LogicValue::HIGH, "READY is asserted"}, "Memory32x32Test");
    expectWireAt(*sim, fault_wire, {220, LogicValue::LOW, "valid reset read has no fault"}, "Memory32x32Test");
    expectRegisterAt(*sim, read_data_wire, {220, 0x00000000U, "reset clears word 0"}, "Memory32x32Test");

    for (uint32_t word = 0; word < 32; ++word) {
        const size_t base_time = 300 + static_cast<size_t>(word) * 420;
        const std::string label = "word " + std::to_string(word) + " write/read";
        expectRegisterAt(
            *sim,
            read_data_wire,
            {base_time + 330, memoryWordPattern(word), label},
            "Memory32x32Test");
    }

    expectRegisterAt(*sim, read_data_wire, {14360, memoryWordPattern(17), "WRITE_EN=0 preserves word 17"}, "Memory32x32Test");
    expectRegisterAt(*sim, read_data_wire, {15940, memoryWordPattern(0), "misaligned write does not alter word 0"}, "Memory32x32Test");
    expectRegisterAt(*sim, read_data_wire, {16300, 0x00000000U, "reset clears word 31"}, "Memory32x32Test");

    const ExpectedValue expected_faults[] = {
        {14700, LogicValue::HIGH, "byte size is unsupported in the structural slice"},
        {14860, LogicValue::HIGH, "halfword size is unsupported in the structural slice"},
        {15020, LogicValue::HIGH, "word access must be 4-byte aligned"},
        {15220, LogicValue::HIGH, "address above the 128-byte window faults"},
        {15420, LogicValue::LOW, "no access suppresses fault reporting"},
        {15640, LogicValue::HIGH, "misaligned write reports fault"},
        {15940, LogicValue::LOW, "valid read after fault clears FAULT"},
    };

    for (const auto& expected : expected_faults) {
        expectWireAt(*sim, fault_wire, expected, "Memory32x32Test");
    }
}

size_t Memory32x32Test::getRunDuration() const {
    return 16400;
}

std::vector<SimulationTest::SimulationCheckpoint> Memory32x32Test::getCheckpoints() const {
    std::vector<SimulationCheckpoint> checkpoints{
        {220, "Reset read", "Word 0 reads as 0x00000000 and FAULT=0", 0},
    };

    for (uint32_t word = 0; word < 32; ++word) {
        const size_t base_time = 300 + static_cast<size_t>(word) * 420;
        checkpoints.push_back({
            base_time + 330,
            "Write word " + std::to_string(word),
            "Aligned SW stores and reads back word " + std::to_string(word),
            static_cast<size_t>(word + 1),
        });
    }

    checkpoints.push_back({14360, "Write disabled", "WRITE_EN=0 holds word 17", 33});
    checkpoints.push_back({14700, "Unsupported byte", "SIZE=00 reports FAULT", 34});
    checkpoints.push_back({14860, "Unsupported halfword", "SIZE=01 reports FAULT", 35});
    checkpoints.push_back({15020, "Misaligned word", "ADDR[1:0]!=00 reports FAULT", 36});
    checkpoints.push_back({15220, "Out of range", "ADDR[31:7]!=0 reports FAULT", 37});
    checkpoints.push_back({15420, "No access", "READ_EN=0 and WRITE_EN=0 suppress FAULT", 38});
    checkpoints.push_back({15940, "Faulted write blocked", "A misaligned write does not change word 0", 39});
    checkpoints.push_back({16300, "Reset clear", "RST clears the stored words", 40});
    return checkpoints;
}
