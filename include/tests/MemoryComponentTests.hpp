#pragma once

#include "simulator/SimulationTest.hpp"
#include "tests/ComponentTestModel.hpp"

class FlipFlopCellTest : public circuit::test::ComponentScenarioTest {
public:
    FlipFlopCellTest();
};

class RegisterTest : public circuit::test::ComponentScenarioTest {
public:
    RegisterTest();
};

class RegisterFile32x32Test : public circuit::test::ComponentScenarioTest {
public:
    RegisterFile32x32Test();
};
