#pragma once
#include "tests/TestHelpers.hpp"

// Word gates (ANDGate<W> ... NOTGate<W>): four-state values against an
// independent bit rule on both forms, and per-bit output timing of the compact
// form against its gates in every timing profile.
class WordGateSweepTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "WordGateSweepTest"; }
protected:
    void verifyResults() override;
};

// The recorded example of the word gates (§ 1.3.7): ANDGate<8> on boundary,
// alternating and four-state words. Logical owner: WordGateSweepTest.
struct TestRegistryEntry;
void appendWordGateScenarios(std::vector<TestRegistryEntry>&);
