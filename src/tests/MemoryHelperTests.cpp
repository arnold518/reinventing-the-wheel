#include "tests/MemoryDeviceTests.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "modules/memory/MemoryArrayBlocks.hpp"
#include "modules/memory/SRAM.hpp"
#include <array>

using namespace circuit;
using namespace circuit::test;
namespace {
constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH,
               X = LogicValue::UNKNOWN, Z = LogicValue::HIGH_Z;
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
struct Probe {
    std::shared_ptr<IOComponent> io;
    Simulator sim;
    std::map<std::string, std::shared_ptr<WireBase>> wires;
    std::map<std::string, std::shared_ptr<WireBase>> outputs;
    explicit Probe(std::shared_ptr<IOComponent> component) : io(std::move(component)) {
        sim.setHistoryRecordingEnabled(false);
        ComponentBuilder b(io);
        for (const auto& [name, pin] : io->getAllInputPins())
            wires[name] = b.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : io->getAllOutputPins())
            outputs[name] = b.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
        SimulationTest::scheduleInitialEventsForTree(io, sim, 0);
    }
    void drive(size_t time, const NamedValues& values) {
        for (const auto& [name, value] : values)
            sim.scheduleEvent(makeTestWireUpdate(time, wires.at(name), value));
    }
    void step(const NamedValues& values) {
        auto time = sim.getCurrentTime() + 1;
        drive(time, values);
        check(sim.drainUntilIdle(time + 256, 500000).idle(), "Helper failed to settle");
    }
    LogicVector out(const std::string& pin) const {
        return outputs.at(pin)->getValueVector();
    }
};

void outputBuffers(size_t width) {
    for (const auto* family : {"memory.matrix.output-buffer.", "memory.sram.output-buffer."}) {
        auto built = builtinComponentCatalog().createRoot(
            {family + std::to_string(width), "DUT", {}, {}, {}, {}}, canonicalDefaultProfile());
        Probe probe(std::dynamic_pointer_cast<IOComponent>(built.root));
        check(probe.io->getChildren().empty(), "Output alias added a hierarchy level");
        check(probe.io->getTypeName() == std::string(family == std::string("memory.sram.output-buffer.")
                  ? "SRAMOutputBuffer<" : "MemoryOutputBuffer<") + std::to_string(width) + ">",
              "Output buffer display identity changed");
        auto previous = LogicVector(width, X);
        size_t time = 10;
        for (auto enable : {L, H, X, Z, H, L}) {
            for (size_t phase = 0; phase < 4; ++phase) {
                LogicVector data(width);
                const std::array<LogicValue, 4> values{L, H, X, Z};
                for (size_t bit = 0; bit < width; ++bit) data[bit] = values[(bit + phase) % 4];
                auto expected = enable == H ? data : LogicVector(width, enable == L ? Z : X);
                probe.drive(time, {{"DATA", data}, {"ENABLE", logicBit(enable)}});
                probe.sim.advanceAndRecord(time);
                check(probe.out("OUT") == previous, "Output buffer lost its one-tick delay");
                probe.sim.advanceAndRecord(time + 1);
                check(probe.out("OUT") == expected, "Output buffer four-state contract changed");
                previous = expected;
                time += 4;
            }
        }
    }
}

LogicValue equality(const LogicVector& address, size_t value, LogicValue enable) {
    if (enable == L) return L;
    bool uncertain = enable != H;
    for (size_t bit = 0; bit < address.size(); ++bit) {
        auto v = address[bit];
        if (v != L && v != H) uncertain = true;
        else if ((v == H) != bool(value & (size_t{1} << bit))) return L;
    }
    return uncertain ? X : H;
}
void decoders() {
    Probe sram(Component::create<SRAMRowDecoder>("SRAM", SRAMDeviceConfig{6, 2}));
    Probe matrix(Component::create<memory_device::MatrixRowDecoder>("MATRIX", 3));
    Probe ff(Component::create<MemoryAddressDecoder>("FF", 3, false));
    std::vector<LogicVector> addresses;
    for (auto a : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 0xffffffffu}) addresses.push_back(logicBits(32, a));
    for (size_t bit : {0u, 1u, 31u})
        for (auto v : {X, Z}) {
            auto address = logicBits(32, 0);
            address[bit] = v;
            addresses.push_back(address);
        }
    for (const auto& word : addresses) {
        LogicVector row(word.begin() + 1, word.end());
        row.push_back(L); // SRAM K=2: word-address bit zero is the column slot.
        ff.step({{"WORD_ADDR", row}});
        for (auto enable : {L, H, X, Z}) {
            sram.step({{"WORD_ADDR", word}, {"ENABLE", logicBit(enable)}});
            matrix.step({{"ROW_ADDR", row}, {"ENABLE", logicBit(enable)}});
            for (size_t r = 0; r < 3; ++r) {
                auto pin = "WL_" + std::to_string(r);
                auto expected = logicBit(equality(row, r, enable));
                check(sram.out(pin) == expected && matrix.out(pin) == expected,
                      "Row decoder address/enable semantics changed");
                check(ff.out("VALID_" + std::to_string(r)) == logicBit(equality(row, r, H)),
                      "FF decoder full-address validation changed");
            }
        }
    }
}

void selectors(size_t width) {
    // Exercise all supported SRAM column geometries, including the legacy K=64
    // contract (the shared post-sense matrix mux deliberately stops at K=16).
    for (size_t k : {1u, 2u, 4u, 16u, 64u}) {
        auto built = builtinComponentCatalog().createRoot(
            {"memory.sram.column-selector." + std::to_string(width), "DUT",
             {{"words_per_row", std::to_string(k)}}, {}, {}, {}}, canonicalDefaultProfile());
        Probe probe(std::dynamic_pointer_cast<IOComponent>(built.root));
        NamedValues inputs;
        for (size_t bit = 0; bit < width; ++bit) {
            auto n = std::to_string(bit);
            inputs["WRITE_BL_" + n] = logicBit(H);
            inputs["WRITE_BL_BAR_" + n] = logicBit(L);
            inputs["WRITE_DRIVE_" + n] = logicBit(H);
        }
        for (size_t c = 0; c < width * k; ++c) {
            auto n = std::to_string(c);
            bool value = ((c / width) ^ c) & 1;
            inputs["BL_" + n] = logicBit(value);
            inputs["BL_BAR_" + n] = logicBit(!value);
            inputs["VALID_" + n] = logicBit(H);
        }
        std::vector<size_t> slots;
        if (k <= 4) for (size_t slot = 0; slot < k; ++slot) slots.push_back(slot);
        else slots = {0, 1, k / 2, k - 1};
        for (size_t slot : slots) {
            inputs["WORD_ADDR"] = logicBits(32, k * 3 + slot); // high bits must not affect slot
            probe.step(inputs);
            for (size_t bit = 0; bit < width; ++bit) {
                auto n = std::to_string(bit);
                bool value = (slot ^ bit) & 1;
                check(probe.out("READ_BL_" + n) == logicBit(value) &&
                      probe.out("READ_BL_BAR_" + n) == logicBit(!value) &&
                      probe.out("READ_VALID_" + n) == logicBit(H), "SRAM mux selection changed");
            }
            for (size_t c = 0; c < width * k; ++c)
                check(probe.out("DRIVE_" + std::to_string(c)) == logicBit(c / width == slot),
                      "SRAM write distribution changed");
        }
        if (k > 1) {
            auto address = logicBits(32, 0); address[0] = X;
            probe.step({{"WORD_ADDR", address}});
            check(probe.out("READ_BL_0") == logicBit(X), "SRAM unknown slot was guessed");
            address[0] = Z;
            probe.step({{"WORD_ADDR", address}});
            check(probe.out("READ_BL_0") == logicBit(X), "SRAM released slot was guessed");
        }
    }
}

template<size_t W> void ffSelectors() {
    for (size_t depth : {1u, 3u, 4u, 5u}) {
        Probe probe(Component::create<MemoryReadSelector<W>>("FF_READ", depth));
        NamedValues inputs{{"READ_VALID", logicBit(H)}};
        const auto bits = memory_detail::arrayAddressBits(depth);
        for (size_t word = 0; word < depth; ++word)
            inputs["WORD_" + std::to_string(word)] = logicBits(W, word + 1);
        for (size_t address = 0; address < (size_t{1} << bits); ++address) {
            for (size_t bit = 0; bit < bits; ++bit)
                inputs["SELECT_" + std::to_string(bit)] = logicBit(bool(address & (size_t{1} << bit)));
            probe.step(inputs);
            check(probe.out("READ_DATA") == logicBits(W, address < depth ? address + 1 : 0),
                  "FF mux singleton/non-power-of-two padding changed");
        }
        if (bits) {
            probe.step({{"SELECT_0", logicBit(X)}, {"READ_VALID", logicBit(L)}});
            check(probe.out("READ_DATA") == logicBits(W, 0), "Disabled FF read leaked unknown data");
        }
    }
}
} // namespace

void MemoryHelperContractTest::verifyResults() {
    decoders();
    for (size_t width : {8, 16, 32}) {
        outputBuffers(width);
        selectors(width);
    }
    ffSelectors<8>(); ffSelectors<16>(); ffSelectors<32>();
}
