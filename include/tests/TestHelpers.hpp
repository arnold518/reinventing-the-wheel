#pragma once

#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct TestValue {
    bool multi = false;
    LogicValue logic = LogicValue::UNKNOWN;
    uint64_t numeric = 0;

    TestValue() = default;
    TestValue(LogicValue value) : multi(false), logic(value), numeric(0) {}
    TestValue(uint64_t value) : multi(true), logic(LogicValue::UNKNOWN), numeric(value) {}
};

struct TestRow {
    std::map<std::string, TestValue> inputs;
    std::map<std::string, TestValue> outputs;
};

class StandaloneVerificationTest : public SimulationTest {
public:
    void setupCircuit() override {
        root = Component::create<Component>(getTestName());
        builder = std::make_unique<ComponentBuilder>(root);
        buildCircuit();
        setInitialState();
    }

    size_t getRunDuration() const override {
        return 0;
    }

protected:
    void buildCircuit() override {}
    void setInitialState() override {}
};

inline std::shared_ptr<Event> makeTestWireUpdate(size_t time, const std::shared_ptr<WireBase>& wire, const TestValue& value) {
    uint64_t numeric = value.multi ? value.numeric : static_cast<uint64_t>(value.logic == LogicValue::HIGH);
    switch (wire->getWidth()) {
        case 1:
            if (value.multi) return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), numeric);
            return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), value.logic);
        case 2: return std::make_shared<WireUpdateEvent<2>>(time, std::dynamic_pointer_cast<Wire<2>>(wire), numeric);
        case 3: return std::make_shared<WireUpdateEvent<3>>(time, std::dynamic_pointer_cast<Wire<3>>(wire), numeric);
        case 4: return std::make_shared<WireUpdateEvent<4>>(time, std::dynamic_pointer_cast<Wire<4>>(wire), numeric);
        case 8: return std::make_shared<WireUpdateEvent<8>>(time, std::dynamic_pointer_cast<Wire<8>>(wire), numeric);
        case 16: return std::make_shared<WireUpdateEvent<16>>(time, std::dynamic_pointer_cast<Wire<16>>(wire), numeric);
        case 32: return std::make_shared<WireUpdateEvent<32>>(time, std::dynamic_pointer_cast<Wire<32>>(wire), numeric);
        default: return nullptr;
    }
}

template<typename ComponentT, typename... Args>
bool runRows(const std::vector<TestRow>& rows, Args&&... args) {
    for (const auto& row : rows) {
        Simulator sim;
        auto root = Component::create<ComponentT>("ROOT", std::forward<Args>(args)...);
        auto io = std::dynamic_pointer_cast<IOComponent>(root);
        assert(io);
        ComponentBuilder builder(root);

        for (const auto& [pin_name, value] : row.inputs) {
            auto pin = io->getInputPinDynamic(pin_name);
            assert(pin && "Missing input pin");
            auto wire = builder.addNewWireDynamic("IN_" + pin_name, pin->getWidth(), nullptr, {pin});
            sim.scheduleEvent(makeTestWireUpdate(0, wire, value));
        }

        sim.runAndRecord(1000);

        for (const auto& [pin_name, expected] : row.outputs) {
            auto pin = io->getOutputPinDynamic(pin_name);
            assert(pin && "Missing output pin");
            if (expected.multi || pin->getWidth() > 1) {
                auto actual = pin->getValueAsUInt64();
                if (actual != expected.numeric) {
                    std::cerr << pin_name << " = " << actual << ", expected " << expected.numeric << std::endl;
                    return false;
                }
            } else {
                auto actual = pin->getValue();
                if (actual != expected.logic) {
                    std::cerr << pin_name << " = " << actual << ", expected " << expected.logic << std::endl;
                    return false;
                }
            }
        }
    }
    return true;
}

inline TestValue bits(uint64_t value) {
    return TestValue(value);
}

inline TestValue bit(bool value) {
    return TestValue(value ? LogicValue::HIGH : LogicValue::LOW);
}
