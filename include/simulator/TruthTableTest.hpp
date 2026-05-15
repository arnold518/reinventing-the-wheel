#pragma once

#include "basic/LogicValue.hpp"
#include "simulator/SimulationTest.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class PinValue {
private:
    bool multi_bit_;
    LogicValue logic_value_;
    uint64_t integer_value_;

public:
    PinValue() : multi_bit_(false), logic_value_(LogicValue::UNKNOWN), integer_value_(0) {}
    PinValue(LogicValue value) : multi_bit_(false), logic_value_(value), integer_value_(0) {}
    PinValue(uint64_t value) : multi_bit_(true), logic_value_(LogicValue::UNKNOWN), integer_value_(value) {}

    bool isMultiBit() const { return multi_bit_; }
    LogicValue asLogicValue() const { return logic_value_; }
    uint64_t asUInt64() const { return integer_value_; }
};

struct TruthRow {
    std::map<std::string, PinValue> inputs;
    std::map<std::string, PinValue> outputs;

    TruthRow(std::map<std::string, PinValue> ins, std::map<std::string, PinValue> outs)
        : inputs(std::move(ins)), outputs(std::move(outs)) {}
};

class TruthTableTest : public SimulationTest {
protected:
    std::vector<TruthRow> truth_table_;
    size_t time_step_ = 10;

    void setInitialState() override;
    void verifyResults() override;
    virtual std::vector<TruthRow> getTruthTable() const = 0;

public:
    void buildCircuit() override {}
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
};
