#pragma once

#include "simulator/SimulationTest.hpp"
#include "basic/LogicValue.hpp"
#include <vector>
#include <map>
#include <string>

/**
 * @brief A row in a truth table, specifying inputs and expected outputs.
 */
struct TruthRow {
    std::map<std::string, LogicValue> inputs;
    std::map<std::string, LogicValue> outputs;

    /**
     * @brief Construct a truth row with separate input and output maps.
     */
    TruthRow(std::map<std::string, LogicValue> ins, std::map<std::string, LogicValue> outs)
        : inputs(std::move(ins)), outputs(std::move(outs)) {}
};

/**
 * @brief Base class for simulation tests that verify combinational circuits
 *        using truth tables.
 *
 * IMPORTANT: This test requires the root component to be an IOComponent.
 * The test accesses input and output pins directly from the root component.
 *
 * This class automatically:
 * - Creates input wires for all inputs in the truth table
 * - Schedules WireUpdateEvents for each row at evenly-spaced times
 * - Verifies the final output state matches the last truth table row
 *
 * Subclasses only need to:
 * 1. Implement setupCircuit() to create the IOComponent under test
 * 2. Implement getTruthTable() to return the truth table
 * 3. Optionally set time_step_ to adjust timing between test cases (default: 10)
 *
 * Usage example:
 * @code
 * class HalfAdderTest : public TruthTableTest {
 * public:
 *     HalfAdderTest() {
 *         time_step_ = 15;  // Optional: customize timing
 *     }
 *
 *     void setupCircuit() override {
 *         root = Component::create<HalfAdder>("HA_ROOT");
 *         builder = std::make_unique<ComponentBuilder>(root);
 *         buildCircuit();
 *         setInitialState();
 *     }
 *
 *     void buildCircuit() override {}  // Empty for simple tests
 *
 *     std::vector<TruthRow> getTruthTable() const override {
 *         constexpr auto L = LogicValue::LOW;
 *         constexpr auto H = LogicValue::HIGH;
 *         return {
 *             {{ {"A", L}, {"B", L} }, { {"Sum", L}, {"Carry", L} }},
 *             {{ {"A", L}, {"B", H} }, { {"Sum", H}, {"Carry", L} }},
 *             {{ {"A", H}, {"B", L} }, { {"Sum", H}, {"Carry", L} }},
 *             {{ {"A", H}, {"B", H} }, { {"Sum", L}, {"Carry", H} }},
 *         };
 *     }
 * };
 * @endcode
 */
class TruthTableTest : public SimulationTest {
protected:
    std::vector<TruthRow> truth_table_;
    /**
     * @brief Time units between test cases (default: 10).
     *
     * Subclasses can modify this in their constructor to adjust timing.
     * Increase this value for circuits with longer propagation delays.
     */
    size_t time_step_ = 10;

    /**
     * @brief Set the initial state by scheduling events from the truth table.
     *
     * This method is called by setupCircuit() and should not normally be
     * overridden unless you need custom event scheduling.
     */
    void setInitialState() override;

    /**
     * @brief Verify that the final outputs match the last truth table row.
     *
     * This checks the output pin values at the end of simulation against
     * the expected values in the last row of the truth table.
     */
    void verifyResults() override;

    /**
     * @brief Subclasses must implement this to provide the truth table.
     * @return Vector of truth table rows
     */
    virtual std::vector<TruthRow> getTruthTable() const = 0;

public:
    /**
     * @brief Default implementation of buildCircuit() - empty for most truth table tests.
     *
     * Override this only if you need to add test harness components beyond
     * the component under test itself.
     */
    void buildCircuit() override {}

    /**
     * @brief Calculate the simulation duration based on truth table size.
     */
    size_t getRunDuration() const override;
};
