#include "tests/MemoryEndpointTests.hpp"
#include "modules/memory/TwoPortMemoryArbiter.hpp"
#include "tests/TestHelpers.hpp"
#include <map>
#include <stdexcept>

// Two-port arbitration at every width: grant, held responses, round robin,
// reset and burst grants of 1, 2 and 4 words. The recorded example is
// MemoryArbiterTest.
using circuit::memory::TwoPortMemoryArbiter;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<size_t Width>
class Rig {
public:
    explicit Rig(size_t block_words = 2) {
        root_ = Component::create<TwoPortMemoryArbiter<Width>>("ARBITER", block_words);
        ComponentBuilder builder(root_);
        for (const auto& [name, pin] : root_->getAllInputPins())
            inputs_[name] = builder.addNewWireDynamic("IN_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : root_->getAllOutputPins())
            builder.addNewWireDynamic("OUT_" + name, pin->getWidth(), pin, {});
        sim_.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root_, sim_);
        for (const auto& [name, wire] : inputs_)
            sim_.scheduleEvent(makeTestWireUpdate(0, wire, TestValue(uint64_t(name.ends_with("_RST")))));
        sim_.advanceAndRecord(now_ = 200);
        cycle();
        drive({{"I_RST", 0}, {"D_RST", 0}});
    }
    void drive(const std::map<std::string, uint64_t>& changes) {
        for (const auto& [name, value] : changes)
            sim_.scheduleEvent(makeTestWireUpdate(now_ + 1, inputs_.at(name), TestValue(value)));
        sim_.advanceAndRecord(now_ += 200);
    }
    void cycle() {
        drive({{"I_CLK", 0}, {"D_CLK", 0}});
        drive({{"I_CLK", 1}, {"D_CLK", 1}});
        drive({{"I_CLK", 0}, {"D_CLK", 0}});
    }
    uint64_t out(const std::string& name) const {
        return root_->getOutputPinDynamic(name)->getValueAsUInt64();
    }
private:
    std::shared_ptr<TwoPortMemoryArbiter<Width>> root_;
    std::map<std::string, std::shared_ptr<WireBase>> inputs_;
    Simulator sim_;
    size_t now_ = 0;
};

template<size_t Width>
void exercise() {
    const uint64_t data = Width == 8 ? 0x5a : Width == 16 ? 0x1234 : 0x12345678;
    Rig<Width> rig;
    rig.drive({{"I_REQ_VALID", 1}, {"I_REQ_OP", 0}, {"I_REQ_ADDR", 0},
               {"D_REQ_VALID", 1}, {"D_REQ_OP", 1}, {"D_REQ_ADDR", 4},
               {"D_REQ_WRITE_DATA", data},
               {"D_REQ_WRITE_MASK", (1u << (Width / 8)) - 1},
               {"MEM_REQ_READY", 0}});
    for (int i = 0; i < 3; ++i) {
        require(rig.out("MEM_REQ_VALID") == 1 && rig.out("MEM_REQ_ADDR") == 0 &&
                rig.out("I_REQ_READY") == 0 && rig.out("D_REQ_READY") == 0,
                "Blocked first grant changed");
        rig.cycle();
    }
    rig.drive({{"MEM_REQ_READY", 1}});
    require(rig.out("I_REQ_READY") == 1 && rig.out("D_REQ_READY") == 0,
            "Initial tie must grant I");
    rig.cycle();
    rig.drive({{"I_REQ_VALID", 0}, {"MEM_REQ_READY", 0}});
    require(rig.out("MEM_REQ_VALID") == 0, "Arbiter accepted another request while busy");
    // I's response leaves the bus into I's buffer although I is not ready.
    rig.drive({{"MEM_RSP_VALID", 1}, {"MEM_RSP_READ_DATA", 0xa5}, {"MEM_RSP_STATUS", 0}});
    require(rig.out("I_RSP_VALID") == 1 && rig.out("D_RSP_VALID") == 0 &&
            rig.out("I_RSP_READ_DATA") == 0xa5 && rig.out("MEM_RSP_READY") == 1,
            "Response not offered to I alone, or not taken into its buffer");
    rig.cycle();
    rig.drive({{"MEM_RSP_VALID", 0}});
    require(rig.out("I_RSP_VALID") == 1 && rig.out("I_RSP_READ_DATA") == 0xa5,
            "Held response lost");
    // The bus is free: D, still waiting, is offered next.
    require(rig.out("MEM_REQ_VALID") == 1 && rig.out("MEM_REQ_ADDR") == 4 &&
            rig.out("MEM_REQ_OP") == 1 && rig.out("MEM_REQ_WRITE_DATA") == data && rig.out("D_REQ_READY") == 0,
            "D not offered while I's response is held");
    rig.drive({{"MEM_REQ_READY", 1}});
    require(rig.out("D_REQ_READY") == 1 && rig.out("I_REQ_READY") == 0, "D not granted");
    rig.cycle();
    rig.drive({{"D_REQ_VALID", 0}, {"I_RSP_READY", 1}});
    rig.cycle();
    rig.drive({{"I_RSP_READY", 0}});
    require(rig.out("I_RSP_VALID") == 0, "I's held response not taken");
    rig.drive({{"MEM_RSP_VALID", 1}, {"MEM_RSP_READ_DATA", 0x55}, {"MEM_RSP_STATUS", 5}});
    require(rig.out("D_RSP_VALID") == 1 && rig.out("I_RSP_VALID") == 0 &&
            rig.out("D_RSP_STATUS") == 5, "D response misrouted");
    rig.drive({{"D_RSP_READY", 1}});
    rig.cycle();
    rig.drive({{"MEM_RSP_VALID", 0}, {"D_RSP_READY", 0}});
    require(rig.out("D_RSP_VALID") == 0, "D response not taken");
    // Round robin: after D, a tie goes to I.
    rig.drive({{"I_REQ_VALID", 1}, {"D_REQ_VALID", 1}});
    require(rig.out("MEM_REQ_ADDR") == 0 && rig.out("I_REQ_READY") == 1,
            "Round-robin priority did not return to I");
    rig.drive({{"D_REQ_VALID", 0}});
    rig.cycle();
    rig.drive({{"I_REQ_VALID", 0}, {"I_RST", 1}, {"D_RST", 1}});
    rig.cycle();
    require(rig.out("I_RSP_VALID") == 0 && rig.out("D_RSP_VALID") == 0 &&
            rig.out("MEM_RST") == 1, "Reset failed to drain owner state");
    rig.drive({{"I_RST", 0}, {"D_RST", 0}, {"MEM_RSP_VALID", 1}});
    require(rig.out("I_RSP_VALID") == 0 && rig.out("D_RSP_VALID") == 0,
            "Late lower response was delivered after reset");
}
// A granted burst read (op 4) keeps the arbiter until its last word: the
// other port waits, every word goes to the burst's owner.
template<size_t Width>
void exerciseBurst(size_t block_words) {
    Rig<Width> rig(block_words);
    rig.drive({{"D_REQ_VALID", 1}, {"D_REQ_OP", 4}, {"D_REQ_ADDR", 8}, {"MEM_REQ_READY", 1},
               {"D_RSP_READY", 1}, {"I_RSP_READY", 1}});
    require(rig.out("D_REQ_READY") == 1 && rig.out("MEM_REQ_OP") == 4, "Burst not offered");
    rig.cycle();
    rig.drive({{"D_REQ_VALID", 0}, {"I_REQ_VALID", 1}, {"I_REQ_OP", 0}, {"I_REQ_ADDR", 0}});
    for (size_t word = 0; word < block_words; ++word) {
        require(rig.out("MEM_REQ_VALID") == 0 && rig.out("I_REQ_READY") == 0, "Request granted during a burst");
        rig.drive({{"MEM_RSP_VALID", 1}, {"MEM_RSP_READ_DATA", 0x10 + word}});
        require(rig.out("D_RSP_VALID") == 1 && rig.out("I_RSP_VALID") == 0 &&
                rig.out("D_RSP_READ_DATA") == 0x10 + word, "Burst word not passed to its owner");
        rig.cycle();
        rig.drive({{"MEM_RSP_VALID", 0}});
    }
    require(rig.out("MEM_REQ_VALID") == 1 && rig.out("I_REQ_READY") == 1, "Arbiter not free after the burst's last word");
    rig.cycle();
    rig.drive({{"I_REQ_VALID", 0}, {"MEM_RSP_VALID", 1}, {"MEM_RSP_READ_DATA", 0x77}});
    require(rig.out("I_RSP_VALID") == 1 && rig.out("D_RSP_VALID") == 0, "Single read after a burst misrouted");
    rig.cycle();
    rig.drive({{"MEM_RSP_VALID", 0}});
    require(rig.out("MEM_REQ_VALID") == 0 && rig.out("I_RSP_VALID") == 0, "Single read did not end at its word");
}
} // namespace


void TwoPortMemoryArbiterTest::verifyResults() {
    exercise<8>();
    exercise<16>();
    exercise<32>();
    exerciseBurst<32>(4);
    exerciseBurst<16>(2);
    exerciseBurst<8>(2);
    exerciseBurst<32>(1);
}
