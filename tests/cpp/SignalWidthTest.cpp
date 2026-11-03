// Pins and wires of any width from 1 to 64 bits: the packed four-state value,
// typed handles at widths outside the former fixed list, and a Rewire moving
// bits between a 64-bit, a 7-bit and a 13-bit wire.
#include "basic/SupportedWidths.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/utility/Rewire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using L = LogicValue;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

std::vector<L> randomBits(std::mt19937_64& random, size_t width) {
    std::vector<L> bits(width);
    for (auto& bit : bits) bit = static_cast<L>(random() % 4);
    return bits;
}

void packed() {
    std::mt19937_64 random(0x5167a1);
    for (size_t width : {1, 7, 33, 63, 64}) {
        for (int round = 0; round < 50; ++round) {
            const auto bits = randomBits(random, width);
            const auto value = PackedLogic::fromVector(bits, width);
            require(value.toVector(width) == bits, "Packed value does not round-trip");
            for (size_t i = 0; i < width; ++i) require(value.get(i) == bits[i], "Packed bit differs");
            require((value.value & ~PackedLogic::mask(width)) == 0 && (value.unknown & ~PackedLogic::mask(width)) == 0,
                    "Packed value has bits above its width");
            // Narrower drops high bits; wider pads with UNKNOWN.
            const auto narrow = value.resized(width, width / 2);
            for (size_t i = 0; i < width / 2; ++i) require(narrow.get(i) == bits[i], "Narrowed bit differs");
            if (width < 64) {
                const auto wide = value.resized(width, 64);
                for (size_t i = width; i < 64; ++i) require(wide.get(i) == L::UNKNOWN, "Widened bit is not UNKNOWN");
            }
        }
        // Short vectors pad with UNKNOWN, long ones are cut.
        const auto shorter = PackedLogic::fromVector({L::HIGH}, width);
        require(shorter.get(0) == L::HIGH, "Short vector lost its bit");
        for (size_t i = 1; i < width; ++i) require(shorter.get(i) == L::UNKNOWN, "Short vector not padded with UNKNOWN");
        require(PackedLogic::fromVector(std::vector<L>(width + 3, L::LOW), width) == PackedLogic::fromUInt64(0, width),
                "Long vector not cut to the width");
        require(PackedLogic::fromUInt64(~uint64_t{0}, width).highBits() == PackedLogic::mask(width), "Two-state bits differ");
    }
}

void dispatch() {
    const auto width = [](size_t w) {
        return circuit::dispatchWidth(w, []<size_t W>() -> size_t { return W; }, []() -> size_t { return 0; });
    };
    require(width(0) == 0 && width(65) == 0, "Width outside 1-64 dispatched");
    for (size_t w = 1; w <= 64; ++w) require(width(w) == w, "Width dispatched to the wrong instantiation");
}

void rewire() {
    using Spec = Rewire::WireSpec;
    using Map = Rewire::BitMap;
    std::vector<Map> map;
    for (size_t i = 0; i < 7; ++i) map.emplace_back("IN", i, "LOW7", i);
    for (size_t i = 0; i < 13; ++i) map.emplace_back("IN", 51 + i, "HIGH13", i);
    for (size_t i = 0; i < 64; ++i) map.emplace_back("IN", i, "ALL", 63 - i);
    auto part = Component::create<Rewire>("RW", std::vector<Spec>{{"IN", 64}},
                                          std::vector<Spec>{{"LOW7", 7}, {"HIGH13", 13}, {"ALL", 64}}, map);
    require(part->getInputPin<64>("IN") && !part->getInputPin<32>("IN"), "Typed lookup does not check the width");
    require(part->getOutputPin<7>("LOW7") && part->getOutputPin<13>("HIGH13"), "Pins outside the former width list missing");
    ComponentBuilder b(part);
    auto in = b.addNewWire<64>("IN", nullptr, {part->getInputPin<64>("IN")});
    auto low = b.addNewWire<7>("LOW7", part->getOutputPin<7>("LOW7"));
    auto high = b.addNewWire<13>("HIGH13", part->getOutputPin<13>("HIGH13"));
    auto all = b.addNewWire<64>("ALL", part->getOutputPin<64>("ALL"));
    Simulator sim;
    std::mt19937_64 random(0x7e5);
    SimulationTime time = 10;
    for (int round = 0; round < 20; ++round, time += 10) {
        const auto bits = randomBits(random, 64);
        sim.scheduleEvent(std::make_shared<WireUpdateEvent>(time, in, bits));
        sim.drainUntilIdle(time + 1, 100000);
        require(in->getValueVector() == bits, "64-bit wire did not take its value");
        for (size_t i = 0; i < 7; ++i) require(low->getBit(i) == bits[i], "7-bit wire bit differs");
        for (size_t i = 0; i < 13; ++i) require(high->getBit(i) == bits[51 + i], "13-bit wire bit differs");
        for (size_t i = 0; i < 64; ++i) require(all->getBit(i) == bits[63 - i], "Reversed 64-bit wire bit differs");
    }
}
} // namespace

int main() {
    try {
        packed();
        dispatch();
        rewire();
        std::cout << "SignalWidthTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SignalWidthTest failed: " << error.what() << '\n';
        return 1;
    }
}
