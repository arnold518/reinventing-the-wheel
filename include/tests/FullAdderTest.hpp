#pragma once
#include "simulator/TruthTableTest.hpp"

class FullAdderTest : public TruthTableTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;

protected:
    // Provide the truth table for the FullAdder
    std::vector<TruthRow> getTruthTable() const override;
};
