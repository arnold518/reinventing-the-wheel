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

class RV32IControlFlowStructuralContractTest : public RV32IControlFlowUnitStandaloneTestBase {
public:
    RV32IControlFlowStructuralContractTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class RV32IControlFlowBehavioralContractTest : public RV32IControlFlowUnitStandaloneTestBase {
public:
    RV32IControlFlowBehavioralContractTest();

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

class RV32IDecodeControlStructuralContractTest : public RV32IDecodeControlUnitStandaloneTestBase {
public:
    RV32IDecodeControlStructuralContractTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class RV32IDecodeControlBehavioralContractTest : public RV32IDecodeControlUnitStandaloneTestBase {
public:
    RV32IDecodeControlBehavioralContractTest();

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

class RV32IExecutionStatusStructuralContractTest
    : public RV32IExecutionControlStatusUnitStandaloneTestBase {
public:
    RV32IExecutionStatusStructuralContractTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};

class RV32IExecutionStatusBehavioralContractTest
    : public RV32IExecutionControlStatusUnitStandaloneTestBase {
public:
    RV32IExecutionStatusBehavioralContractTest();

protected:
    std::shared_ptr<Component> createRootComponent() const override;
};
