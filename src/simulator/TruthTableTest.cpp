#include "simulator/TruthTableTest.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <iostream>
#include <cassert>

void TruthTableTest::setInitialState() {
    // Verify that root is an IOComponent
    auto io_root = std::dynamic_pointer_cast<IOComponent>(root);
    if (!io_root) {
        std::cerr << "[TruthTableTest] ERROR: Root component must be an IOComponent!" << std::endl;
        std::cerr << "                 TruthTableTest requires direct access to input/output pins." << std::endl;
        assert(false && "TruthTableTest requires IOComponent as root");
        return;
    }

    // Get the truth table from the subclass
    truth_table_ = getTruthTable();

    if (truth_table_.empty()) {
        std::cerr << "[TruthTableTest] Warning: Truth table is empty!" << std::endl;
        return;
    }

    // Create input wires for all unique input pins in the truth table
    // We collect all input pin names from the first row
    std::map<std::string, std::shared_ptr<Wire>> input_wires;

    if (!truth_table_.empty()) {
        for (const auto& [pin_name, _] : truth_table_[0].inputs) {
            auto wire = builder->addNewWire(
                "INPUT_" + pin_name,
                nullptr,  // Source-less wire
                { builder->getInputPin(pin_name) }
            );
            input_wires[pin_name] = wire;
        }
    }

    // Schedule events for each truth table row
    for (size_t i = 0; i < truth_table_.size(); i++) {
        size_t time = i * time_step_;

        std::cout << "[TruthTableTest] Scheduling test case " << i << " at t=" << time << ": ";

        for (const auto& [pin_name, value] : truth_table_[i].inputs) {
            std::cout << pin_name << "=" << (int)value << " ";

            auto wire_it = input_wires.find(pin_name);
            if (wire_it == input_wires.end()) {
                std::cerr << "Error: No wire found for input pin '" << pin_name << "'" << std::endl;
                continue;
            }

            sim->scheduleEvent(std::make_shared<WireUpdateEvent>(
                time,
                wire_it->second,
                value
            ));
        }

        std::cout << "→ Expected: ";
        for (const auto& [pin_name, value] : truth_table_[i].outputs) {
            std::cout << pin_name << "=" << (int)value << " ";
        }
        std::cout << std::endl;
    }
}

void TruthTableTest::verifyResults() {
    if (truth_table_.empty()) {
        std::cerr << "Error: Cannot verify results - truth table is empty!" << std::endl;
        return;
    }

    // Verify the final state matches the last row of the truth table
    const auto& last_row = truth_table_.back();

    std::cout << "[TruthTableTest] Verifying final state:" << std::endl;

    bool all_passed = true;

    for (const auto& [pin_name, expected_value] : last_row.outputs) {
        auto pin = builder->getOutputPin(pin_name);
        if (!pin) {
            std::cerr << "  ✗ Output pin '" << pin_name << "' not found!" << std::endl;
            all_passed = false;
            continue;
        }

        LogicValue actual_value = pin->getValue();

        if (actual_value != expected_value) {
            std::cerr << "  ✗ " << pin_name << " = " << (int)actual_value
                      << ", expected " << (int)expected_value << std::endl;
            all_passed = false;
        } else {
            std::cout << "  ✓ " << pin_name << " = " << (int)actual_value << std::endl;
        }
    }

    if (all_passed) {
        std::cout << "[TruthTableTest] All outputs match truth table ✓" << std::endl;
    } else {
        std::cerr << "[TruthTableTest] Some outputs do NOT match truth table ✗" << std::endl;
        assert(false && "Truth table verification failed");
    }
}

size_t TruthTableTest::getRunDuration() const {
    if (truth_table_.empty()) {
        return 100;  // Default fallback
    }

    // Run duration = (number of test cases) * time_step + buffer for propagation
    // Buffer is 2x time_step to allow for deep circuits
    return truth_table_.size() * time_step_ + (2 * time_step_);
}
