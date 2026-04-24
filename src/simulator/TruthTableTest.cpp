#include "simulator/TruthTableTest.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <cassert>
#include <iostream>

namespace {
std::shared_ptr<Event> makeWireUpdateEvent(size_t time, const std::shared_ptr<WireBase>& wire, const PinValue& value) {
    if (!wire) {
        return nullptr;
    }

    auto numeric_value = value.isMultiBit() ? value.asUInt64() : static_cast<uint64_t>(value.asLogicValue() == LogicValue::HIGH);
    switch (wire->getWidth()) {
        case 1:
            if (value.isMultiBit()) {
                return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), numeric_value);
            }
            return std::make_shared<WireUpdateEvent<1>>(time, std::dynamic_pointer_cast<Wire<1>>(wire), value.asLogicValue());
        case 2:
            return std::make_shared<WireUpdateEvent<2>>(time, std::dynamic_pointer_cast<Wire<2>>(wire), numeric_value);
        case 3:
            return std::make_shared<WireUpdateEvent<3>>(time, std::dynamic_pointer_cast<Wire<3>>(wire), numeric_value);
        case 4:
            return std::make_shared<WireUpdateEvent<4>>(time, std::dynamic_pointer_cast<Wire<4>>(wire), numeric_value);
        case 8:
            return std::make_shared<WireUpdateEvent<8>>(time, std::dynamic_pointer_cast<Wire<8>>(wire), numeric_value);
        case 16:
            return std::make_shared<WireUpdateEvent<16>>(time, std::dynamic_pointer_cast<Wire<16>>(wire), numeric_value);
        default:
            return nullptr;
    }
}
}

void TruthTableTest::setInitialState() {
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "TruthTableTest requires IOComponent as root");

    truth_table_ = getTruthTable();
    if (truth_table_.empty()) {
        return;
    }

    std::map<std::string, std::shared_ptr<WireBase>> input_wires;
    for (const auto& [pin_name, _] : truth_table_[0].inputs) {
        auto pin = io_root->getInputPinDynamic(pin_name);
        assert(pin && "Truth table references a missing input pin");

        input_wires[pin_name] = builder->addNewWireDynamic(
            "INPUT_" + pin_name,
            pin->getWidth(),
            nullptr,
            {pin});
    }

    for (size_t i = 0; i < truth_table_.size(); ++i) {
        size_t time = i * time_step_;
        for (const auto& [pin_name, value] : truth_table_[i].inputs) {
            auto wire_it = input_wires.find(pin_name);
            assert(wire_it != input_wires.end() && "Missing input wire");
            sim->scheduleEvent(makeWireUpdateEvent(time, wire_it->second, value));
        }
    }
}

void TruthTableTest::verifyResults() {
    assert(!truth_table_.empty() && "Cannot verify an empty truth table");
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    assert(io_root && "TruthTableTest requires IOComponent as root");

    const auto& last_row = truth_table_.back();
    for (const auto& [pin_name, expected] : last_row.outputs) {
        auto pin = io_root->getOutputPinDynamic(pin_name);
        assert(pin && "Truth table references a missing output pin");

        if (expected.isMultiBit() || pin->getWidth() > 1) {
            auto actual = pin->getValueAsUInt64();
            if (actual != expected.asUInt64()) {
                std::cerr << pin_name << " = " << actual << ", expected " << expected.asUInt64() << std::endl;
                assert(false && "Truth table multi-bit output mismatch");
            }
        } else {
            auto actual = pin->getValue();
            if (actual != expected.asLogicValue()) {
                std::cerr << pin_name << " = " << actual << ", expected " << expected.asLogicValue() << std::endl;
                assert(false && "Truth table output mismatch");
            }
        }
    }
}

size_t TruthTableTest::getRunDuration() const {
    if (truth_table_.empty()) {
        return 100;
    }
    return truth_table_.size() * time_step_ + (2 * time_step_);
}
