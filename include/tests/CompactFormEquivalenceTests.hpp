#pragma once
#include "tests/TestHelpers.hpp"

// Compact forms opened at more widths (2026-10-28 behavioral gap review): the
// Chapter 2 units at 16 and 32 bits, ZeroDetect<16>, and the word muxes at 1,
// 4, 8 and 16 bits, and the six RV32I ALU parts. Each family is built at gate level and compact, driven
// with the same binary and dense four-state vectors, and every output must
// agree. The gate-level forms are checked against independent references by
// their own contract tests (ALUWidthContractTest, the Mux tests).
class CompactFormEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "CompactFormEquivalenceTest"; }
protected:
    void verifyResults() override;
};

// Every GateListCompact (components/selection/GateListCompact.hpp) beside the
// gate-level form of its contract, on binary, dense four-state and
// single-unknown vectors: every output must agree.
class GateListCompactEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "GateListCompactEquivalenceTest"; }
protected:
    void verifyResults() override;
};
