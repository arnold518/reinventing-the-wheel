#pragma once
#include "simulator/TruthTableTest.hpp"

class HalfAdderTest : public TruthTableTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;

protected:
    // Provide the truth table for the HalfAdder
    std::vector<TruthRow> getTruthTable() const override;
};
