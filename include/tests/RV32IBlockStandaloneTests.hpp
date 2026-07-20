#pragma once

#include "simulator/SimulationTest.hpp"
#include <memory>
#include <string>

class RV32IControlFlowUnitStandaloneTestBase : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    explicit RV32IControlFlowUnitStandaloneTestBase(std::string test_name);
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
    virtual std::shared_ptr<Component> createRootComponent() const = 0;

private:
    std::string test_name_;
};

class RV32IControlFlowUnitTest : public RV32IControlFlowUnitStandaloneTestBase {
public:
    RV32IControlFlowUnitTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class BehavioralRV32IControlFlowUnitTest : public RV32IControlFlowUnitStandaloneTestBase {
public:
    BehavioralRV32IControlFlowUnitTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class RV32IDecodeControlUnitStandaloneTestBase : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    explicit RV32IDecodeControlUnitStandaloneTestBase(std::string test_name);
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
    virtual std::shared_ptr<Component> createRootComponent() const = 0;

private:
    std::string test_name_;
};

class RV32IDecodeControlUnitTest : public RV32IDecodeControlUnitStandaloneTestBase {
public:
    RV32IDecodeControlUnitTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class BehavioralRV32IDecodeControlUnitTest : public RV32IDecodeControlUnitStandaloneTestBase {
public:
    BehavioralRV32IDecodeControlUnitTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class RV32IExecutionControlStatusUnitStandaloneTestBase : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    explicit RV32IExecutionControlStatusUnitStandaloneTestBase(std::string test_name);
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
    virtual std::shared_ptr<Component> createRootComponent() const = 0;

private:
    std::string test_name_;
};

class RV32IExecutionControlStatusUnitTest
    : public RV32IExecutionControlStatusUnitStandaloneTestBase {
public:
    RV32IExecutionControlStatusUnitTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class BehavioralRV32IExecutionControlStatusUnitTest
    : public RV32IExecutionControlStatusUnitStandaloneTestBase {
public:
    BehavioralRV32IExecutionControlStatusUnitTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};
