#pragma once

#include "tests/ComponentTestModel.hpp"

class RV32IIFIDPipelineRegisterTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IIFIDPipelineRegisterTest();
};

class RV32IIDEXPipelineRegisterTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IIDEXPipelineRegisterTest();
};

class RV32IEXMEMPipelineRegisterTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IEXMEMPipelineRegisterTest();
};

class RV32IMEMWBPipelineRegisterTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IMEMWBPipelineRegisterTest();
};

class RV32IForwardingUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IForwardingUnitTest();
};

class RV32IHazardDetectionUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IHazardDetectionUnitTest();
};

class RV32IPipelineControlFlowUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IPipelineControlFlowUnitTest();
};

class RV32IMemoryAlignmentUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IMemoryAlignmentUnitTest();
};

class RV32IPipelineRetirementUnitTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IPipelineRetirementUnitTest();
};

class RV32IPipelineCoordinatorTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IPipelineCoordinatorTest();
};

class RV32IFetchStageTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IFetchStageTest();
};

class RV32IDecodeStageTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IDecodeStageTest();
};

class RV32IExecuteStageTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IExecuteStageTest();
};

class RV32IMemoryStageTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IMemoryStageTest();
};

class RV32IWritebackStageTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IWritebackStageTest();
};

class RV32IFiveStageCoreTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IFiveStageCoreTest();
    bool run() override;
};
