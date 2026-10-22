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

class RegisterFileWidth32Count32Test : public circuit::test::ComponentScenarioTest {
public:
    RegisterFileWidth32Count32Test();
};
