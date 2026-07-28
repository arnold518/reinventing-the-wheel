#pragma once

#include "tests/ComponentTestModel.hpp"

class RV32IControlFlowUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IControlFlowUnitTest();
};

class RV32IDecodeControlUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IDecodeControlUnitTest();
};

class RV32IExecutionControlStatusUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IExecutionControlStatusUnitTest();
};
