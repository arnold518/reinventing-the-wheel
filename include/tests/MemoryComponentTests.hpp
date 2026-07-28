#pragma once

#include "simulator/SimulationTest.hpp"
#include "tests/ComponentTestModel.hpp"

class MemoryBitTest : public circuit::test::ComponentScenarioTest {
public:
    MemoryBitTest();
};

class Memory64Kx32Test : public circuit::test::ComponentScenarioTest {
public:
    Memory64Kx32Test();
    bool run() override;
};

class Register32Test : public circuit::test::ComponentScenarioTest {
public:
    Register32Test();
};

class RegisterFile4x32Test : public circuit::test::ComponentScenarioTest {
public:
    RegisterFile4x32Test();
};

class RegisterFile32x32Test : public circuit::test::ComponentScenarioTest {
public:
    RegisterFile32x32Test();
};

class Memory4x32Test : public circuit::test::ComponentScenarioTest {
public:
    Memory4x32Test();
};

class Memory32x32Test : public circuit::test::ComponentScenarioTest {
public:
    Memory32x32Test();
};
