add_executable(MemoryProvisioningStartupTest cpp/MemoryProvisioningStartupTest.cpp)
target_link_libraries(MemoryProvisioningStartupTest PRIVATE SimulatorLib)
add_test(NAME MemoryProvisioningStartupTest COMMAND MemoryProvisioningStartupTest)
set_tests_properties(MemoryProvisioningStartupTest PROPERTIES LABELS "memory;compatibility;phase6")
add_executable(GateListObservationTest cpp/GateListObservationTest.cpp)
target_link_libraries(GateListObservationTest PRIVATE SimulatorLib)
add_test(NAME GateListObservationTest COMMAND GateListObservationTest)
set_tests_properties(GateListObservationTest PROPERTIES LABELS "analysis;unit;rv32im;phase6")
add_executable(SimulationTestRunner cpp/SimulationTestRunner.cpp)
add_executable(RV32IMTimingTest cpp/RV32IMTimingTest.cpp)
target_link_libraries(RV32IMTimingTest PRIVATE SimulatorLib)
add_test(NAME RV32IMTimingTest COMMAND RV32IMTimingTest)
set_tests_properties(RV32IMTimingTest PROPERTIES LABELS "rv32im;timing;analysis;phase3" TIMEOUT 600)
add_executable(ComputerSystemTimingTest cpp/ComputerSystemTimingTest.cpp)
target_link_libraries(ComputerSystemTimingTest PRIVATE SimulatorLib)
add_test(NAME ComputerSystemTimingTest COMMAND ComputerSystemTimingTest)
set_tests_properties(ComputerSystemTimingTest PROPERTIES LABELS "system;timing;analysis" TIMEOUT 900)
add_executable(MemoryArchitectureCleanupTest cpp/MemoryArchitectureCleanupTest.cpp)
target_link_libraries(MemoryArchitectureCleanupTest PRIVATE SimulatorLib)
add_test(NAME MemoryArchitectureCleanupTest COMMAND MemoryArchitectureCleanupTest)
set_tests_properties(MemoryArchitectureCleanupTest PROPERTIES LABELS "memory;integration;cleanup" TIMEOUT 60)
add_executable(SafeClockTimingTest cpp/SafeClockTimingTest.cpp)
# A sample of the stored delay tables, recomputed from the gates. The full
# check is `part_delay_tables --check` (about ten minutes).
add_test(NAME PartDelayTablesTest COMMAND part_delay_tables --check
    word.parallel-prefix-adder.32 rv32i.alu.add-sub logic.mux.32.32 logic.decoder.5.full
    rv32i.alu.shifter word.zero-detect.32 rv32i.decode-control rv32i.pipeline.forwarding
    rv32i.pipeline.hazard-detection word.alu.8 rv32i.prediction.predictor rv32i.memory-lanes)
set_tests_properties(PartDelayTablesTest PROPERTIES LABELS "timing;analysis" TIMEOUT 600)
add_test(NAME PartDelayTablesCompleteTest COMMAND part_delay_tables --verify
    ${CMAKE_SOURCE_DIR}/src/components/selection/ProvenPartDelays.generated.inc)
set_tests_properties(PartDelayTablesCompleteTest PROPERTIES LABELS "timing;analysis;slow" TIMEOUT 3600)
# The same for each realistic timing profile (components/TimingProfile.hpp).
foreach(timing_profile embedded desktop)
    add_test(NAME PartDelayTablesComplete_${timing_profile}_Test COMMAND part_delay_tables --profile=${timing_profile} --verify
        ${CMAKE_SOURCE_DIR}/src/components/selection/ProvenPartDelays.${timing_profile}.generated.inc)
    set_tests_properties(PartDelayTablesComplete_${timing_profile}_Test PROPERTIES LABELS "timing;analysis;slow" TIMEOUT 3600)
endforeach()
# The documentation's timing numbers (profiles, clocks, access latency) (tools/timing/DocumentationTimingData.cpp):
# measured again in every profile, and the compact endpoints against the gates.
add_test(NAME DocumentationTimingDataTest COMMAND documentation_timing_data --check
    ${CMAKE_SOURCE_DIR}/tools/timing/documentation-timing.generated.json)
set_tests_properties(DocumentationTimingDataTest PROPERTIES LABELS "timing;memory;documentation" TIMEOUT 1800)
add_test(NAME MemoryLatencyGatesTest COMMAND documentation_timing_data --compare-gates)
set_tests_properties(MemoryLatencyGatesTest PROPERTIES LABELS "timing;memory" TIMEOUT 1800)
add_executable(SRAMSubarrayFormulaTest cpp/SRAMSubarrayFormulaTest.cpp)
target_link_libraries(SRAMSubarrayFormulaTest PRIVATE SimulatorLib)
add_test(NAME SRAMSubarrayFormulaTest COMMAND SRAMSubarrayFormulaTest)
set_tests_properties(SRAMSubarrayFormulaTest PROPERTIES LABELS "timing;memory")

add_executable(DelayRuleTest cpp/DelayRuleTest.cpp)
target_link_libraries(DelayRuleTest PRIVATE SimulatorLib)
add_test(NAME DelayRuleTest COMMAND DelayRuleTest)
add_test(NAME DelayRuleSelectionTest COMMAND ${CMAKE_COMMAND}
    -DEXECUTABLE=$<TARGET_FILE:DelayRuleTest>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/checks/DelayRuleSelectionTest.cmake)
set_tests_properties(DelayRuleSelectionTest PROPERTIES LABELS "timing;analysis;infrastructure" TIMEOUT 30)
set_tests_properties(DelayRuleTest PROPERTIES LABELS "timing;analysis;slow" TIMEOUT 5400)
# The same comparison with X shown while an output may still move (an option
# of the engine, off by default): a compact output must not look settled
# before its gates, pulses included.
add_test(NAME DelayRuleStrictSettlingTest COMMAND DelayRuleTest --strict-settling)
set_tests_properties(DelayRuleStrictSettlingTest PROPERTIES LABELS "timing;analysis;slow" TIMEOUT 5400)
add_executable(SRAMCellTimingTest cpp/SRAMCellTimingTest.cpp)
target_link_libraries(SRAMCellTimingTest PRIVATE SimulatorLib)
add_test(NAME SRAMCellTimingTest COMMAND SRAMCellTimingTest)
set_tests_properties(SRAMCellTimingTest PROPERTIES LABELS "timing;analysis;memory" TIMEOUT 300)
add_executable(DFlipFlopTimingTest cpp/DFlipFlopTimingTest.cpp)
target_link_libraries(DFlipFlopTimingTest PRIVATE SimulatorLib)
add_test(NAME DFlipFlopTimingTest COMMAND DFlipFlopTimingTest)
set_tests_properties(DFlipFlopTimingTest PROPERTIES LABELS "timing;analysis" TIMEOUT 120)
add_executable(SettlingBoundTest cpp/SettlingBoundTest.cpp)
target_link_libraries(SettlingBoundTest PRIVATE SimulatorLib)
add_test(NAME SettlingBoundTest COMMAND SettlingBoundTest)
set_tests_properties(SettlingBoundTest PROPERTIES LABELS "timing;analysis" TIMEOUT 600)
add_executable(ArithmeticTimingOptimizationTest cpp/ArithmeticTimingOptimizationTest.cpp)
add_executable(RV32ILogicOptimizationTest cpp/RV32ILogicOptimizationTest.cpp)
target_link_libraries(RV32ILogicOptimizationTest PRIVATE SimulatorLib)
add_test(NAME RV32ILogicOptimizationTest COMMAND RV32ILogicOptimizationTest)
set_tests_properties(RV32ILogicOptimizationTest PROPERTIES LABELS "rv32i;timing" TIMEOUT 120)
target_link_libraries(ArithmeticTimingOptimizationTest PRIVATE SimulatorLib)
add_test(NAME ArithmeticTimingOptimizationTest COMMAND ArithmeticTimingOptimizationTest)
set_tests_properties(ArithmeticTimingOptimizationTest PROPERTIES LABELS "arithmetic;timing" TIMEOUT 120)
target_link_libraries(SafeClockTimingTest PRIVATE SimulatorLib)
add_test(NAME SafeClockTimingTest COMMAND SafeClockTimingTest)
set_tests_properties(SafeClockTimingTest PROPERTIES LABELS "timing;analysis" TIMEOUT 60)
add_executable(MemoryEndpointSafeClockTest cpp/MemoryEndpointSafeClockTest.cpp)
target_link_libraries(MemoryEndpointSafeClockTest PRIVATE SimulatorLib)
add_test(NAME MemoryEndpointSafeClockTest COMMAND MemoryEndpointSafeClockTest)
set_tests_properties(MemoryEndpointSafeClockTest PROPERTIES LABELS "memory;timing;analysis" TIMEOUT 60)
add_executable(MemoryStateTimingTest cpp/MemoryStateTimingTest.cpp)
target_link_libraries(MemoryStateTimingTest PRIVATE SimulatorLib)
add_test(NAME MemoryStateTimingTest COMMAND MemoryStateTimingTest)
set_tests_properties(MemoryStateTimingTest PROPERTIES LABELS "memory;timing;analysis" TIMEOUT 60)
add_executable(MemoryFeedbackTimingTest cpp/MemoryFeedbackTimingTest.cpp)
target_link_libraries(MemoryFeedbackTimingTest PRIVATE SimulatorLib)
add_test(NAME MemoryFeedbackTimingTest COMMAND MemoryFeedbackTimingTest)
set_tests_properties(MemoryFeedbackTimingTest PROPERTIES LABELS "memory;timing;analysis" TIMEOUT 60)
add_executable(BinaryStartupTimingTest cpp/BinaryStartupTimingTest.cpp)
target_link_libraries(BinaryStartupTimingTest PRIVATE SimulatorLib)
add_test(NAME BinaryStartupTimingTest COMMAND BinaryStartupTimingTest)
set_tests_properties(BinaryStartupTimingTest PROPERTIES LABELS "memory;timing;analysis" TIMEOUT 60)
add_executable(RV32IExperimentTest cpp/RV32IExperimentTest.cpp)
add_executable(BenchmarkModelTest cpp/BenchmarkModelTest.cpp)
target_link_libraries(BenchmarkModelTest PRIVATE SimulatorLib)
add_test(NAME BenchmarkModelTest COMMAND BenchmarkModelTest)
set_tests_properties(BenchmarkModelTest PROPERTIES LABELS "benchmark;rv32i;memory" TIMEOUT 60)
if(TARGET system_header)
    add_test(NAME GuestHeaderTest COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/checks/GuestHeaderTest.py $<TARGET_FILE:system_header>)
    set_tests_properties(GuestHeaderTest PROPERTIES LABELS "system;device" TIMEOUT 120)
endif()
add_test(NAME BenchmarkCampaignCLITest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/BenchmarkCampaignCLITest.py ${PROJECT_BINARY_DIR})
set_tests_properties(BenchmarkCampaignCLITest PROPERTIES LABELS "benchmark;rv32i;memory" TIMEOUT 150)
add_test(NAME FullEmbenchCampaignTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/FullEmbenchCampaignTest.py ${PROJECT_BINARY_DIR})
set_tests_properties(FullEmbenchCampaignTest PROPERTIES LABELS "benchmark;rv32i" TIMEOUT 180)
add_test(NAME BackgroundCampaignResumeTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/BackgroundCampaignResumeTest.py)
set_tests_properties(BackgroundCampaignResumeTest PROPERTIES LABELS "benchmark;recovery" TIMEOUT 30)
add_test(NAME CleanupValidationRunnerTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/CleanupValidationRunnerTest.py)
set_tests_properties(CleanupValidationRunnerTest PROPERTIES LABELS "cleanup;infrastructure" TIMEOUT 30)
add_test(NAME WebTestRunnerTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/WebTestRunnerTest.py)
set_tests_properties(WebTestRunnerTest PROPERTIES LABELS "cleanup;infrastructure;visualizer" TIMEOUT 30)
add_test(NAME VisualizerScenarioCatalogTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerScenarioCatalogTest.py)
set_tests_properties(VisualizerScenarioCatalogTest PROPERTIES LABELS "visualizer;framework;cleanup" TIMEOUT 240 RUN_SERIAL TRUE)
add_test(NAME VisualizerLegacySmokeTest COMMAND ${Python3_EXECUTABLE}
    -c "import runpy; runpy.run_path('${PROJECT_SOURCE_DIR}/visualizer/smoke_test.py', run_name='__main__')")
set_tests_properties(VisualizerLegacySmokeTest PROPERTIES LABELS "visualizer;bindings;cleanup" TIMEOUT 30)
add_test(NAME RV32IMiddleBenchmarkTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IMiddleBenchmarkTest.py --build-dir ${PROJECT_BINARY_DIR})
set_tests_properties(RV32IMiddleBenchmarkTest PROPERTIES LABELS "benchmark;rv32i;equivalence" TIMEOUT 120 RUN_SERIAL TRUE)
add_executable(RV32IBenchmarkTest cpp/RV32IBenchmarkTest.cpp)
target_link_libraries(RV32IBenchmarkTest PRIVATE SimulatorLib)
add_test(NAME RV32IBenchmarkTest COMMAND RV32IBenchmarkTest)
set_tests_properties(RV32IBenchmarkTest PROPERTIES LABELS "benchmark;rv32i;experiment" TIMEOUT 60)
add_test(NAME RV32IBenchmarkCLITest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IBenchmarkCLITest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32IBenchmarkCLITest PROPERTIES LABELS "benchmark;rv32i;experiment" TIMEOUT 360)
add_test(NAME RV32IDifferentialTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IDifferentialTest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32IDifferentialTest PROPERTIES LABELS "benchmark;rv32i;experiment;equivalence" TIMEOUT 600 RUN_SERIAL TRUE)
target_link_libraries(RV32IExperimentTest PRIVATE SimulatorLib)
add_test(NAME RV32IExperimentTest COMMAND RV32IExperimentTest)
set_tests_properties(RV32IExperimentTest PROPERTIES LABELS "benchmark;rv32i;experiment;timing" TIMEOUT 60)
add_test(NAME RV32IExperimentCLITest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IExperimentCLITest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32IExperimentCLITest PROPERTIES LABELS "benchmark;rv32i;experiment;timing" TIMEOUT 240)
add_test(NAME RV32IProfileSweepTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IProfileSweepTest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32IProfileSweepTest PROPERTIES LABELS "rv32i;experiment;timing;profiles" TIMEOUT 1800)
add_test(NAME RV32ITopologyTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32ITopologyTest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32ITopologyTest PROPERTIES LABELS "benchmark;rv32i;experiment;cache;memory" TIMEOUT 1200)
add_test(NAME RV32IBinaryClockCLITest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IBinaryClockCLITest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32IBinaryClockCLITest PROPERTIES LABELS "benchmark;rv32i;experiment;timing" TIMEOUT 300)
add_executable(MemoryInspectorTest cpp/MemoryInspectorTest.cpp)
target_link_libraries(MemoryInspectorTest PRIVATE SimulatorLib)
add_test(NAME MemoryInspectorTest COMMAND MemoryInspectorTest)
set_tests_properties(MemoryInspectorTest PROPERTIES LABELS "memory;visualizer" TIMEOUT 30)
target_link_libraries(SimulationTestRunner PRIVATE SimulatorLib)
add_test(NAME DocumentationDiagramsTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/DocumentationDiagramsTest.py)
set_tests_properties(DocumentationDiagramsTest PROPERTIES LABELS "documentation;diagrams" TIMEOUT 30)
add_test(NAME RV32IBenchmarkDocumentationTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IBenchmarkDocumentationTest.py $<TARGET_FILE:rv32i_experiment>)
set_tests_properties(RV32IBenchmarkDocumentationTest PROPERTIES LABELS "documentation;rv32i;benchmark" TIMEOUT 30)
add_executable(RegisterFFIntegrationTest cpp/RegisterFFIntegrationTest.cpp)
target_link_libraries(RegisterFFIntegrationTest PRIVATE SimulatorLib)
add_test(NAME RegisterFFIntegrationTest COMMAND RegisterFFIntegrationTest)
set_tests_properties(RegisterFFIntegrationTest PROPERTIES LABELS "memory;register;rv32i;integration" TIMEOUT 180)
add_test(NAME RegisterDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RegisterDocumentationTest.py ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RegisterDocumentationTest PROPERTIES LABELS "memory;register;documentation")
add_test(NAME RegisterLayoutGenerationTest COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate-register-layouts.py --check)
set_tests_properties(RegisterLayoutGenerationTest PROPERTIES LABELS "memory;register;layout;visualizer" TIMEOUT 30)
add_executable(RegisterFileFFIntegrationTest cpp/RegisterFileFFIntegrationTest.cpp)
target_link_libraries(RegisterFileFFIntegrationTest PRIVATE SimulatorLib)
add_test(NAME RegisterFileFFIntegrationTest COMMAND RegisterFileFFIntegrationTest)
set_tests_properties(RegisterFileFFIntegrationTest PROPERTIES LABELS "memory;register-file;integration" TIMEOUT 180)
add_test(NAME RegisterFileDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RegisterFileDocumentationTest.py ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RegisterFileDocumentationTest PROPERTIES LABELS "memory;register-file;documentation")
add_test(NAME RV32IBuildingBlocksDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IBuildingBlocksDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RV32IBuildingBlocksDocumentationTest PROPERTIES LABELS "rv32i;documentation")
add_test(NAME RV32ISingleCycleDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32ISingleCycleDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RV32ISingleCycleDocumentationTest PROPERTIES LABELS "rv32i;documentation" TIMEOUT 240)
add_executable(RV32IMemoryMigrationTest cpp/RV32IMemoryMigrationTest.cpp)
add_executable(RV32IBaselineExamplesTest cpp/RV32IBaselineExamplesTest.cpp)
target_link_libraries(RV32IBaselineExamplesTest PRIVATE SimulatorLib)
add_test(NAME RV32IBaselineExamplesTest COMMAND RV32IBaselineExamplesTest)
set_tests_properties(RV32IBaselineExamplesTest PROPERTIES LABELS "rv32i;documentation;baseline")
add_executable(RV32IAdvancedExamplesTest cpp/RV32IAdvancedExamplesTest.cpp)
target_link_libraries(RV32IAdvancedExamplesTest PRIVATE SimulatorLib)
add_test(NAME RV32IAdvancedExamplesTest COMMAND RV32IAdvancedExamplesTest)
set_tests_properties(RV32IAdvancedExamplesTest PROPERTIES LABELS "rv32i;documentation;prediction")
add_test(NAME RV32IBaselineDocumentationTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IBaselineDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(RV32IBaselineDocumentationTest PROPERTIES LABELS "rv32i;documentation;baseline")
add_test(NAME RV32IAdvancedDocumentationTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IAdvancedDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(RV32IAdvancedDocumentationTest PROPERTIES LABELS "rv32i;documentation;prediction" TIMEOUT 120)
target_link_libraries(RV32IMemoryMigrationTest PRIVATE SimulatorLib)
add_test(NAME RV32IMemoryMigrationTest COMMAND RV32IMemoryMigrationTest)
set_tests_properties(RV32IMemoryMigrationTest PROPERTIES LABELS "rv32i;memory;integration" TIMEOUT 600)
add_test(NAME RV32IMemoryLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IMemoryLayoutTest.py)
set_tests_properties(RV32IMemoryLayoutTest PROPERTIES LABELS "rv32i;memory;visualizer")
add_test(NAME RV32ISingleCycleLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32ISingleCycleLayoutTest.py)
set_tests_properties(RV32ISingleCycleLayoutTest PROPERTIES LABELS "rv32i;visualizer;layout")
add_test(NAME RV32IBuildingBlockLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IBuildingBlockLayoutTest.py)
set_tests_properties(RV32IBuildingBlockLayoutTest PROPERTIES LABELS "rv32i;visualizer" TIMEOUT 120)
add_executable(MemoryStorageBenchmark cpp/MemoryStorageBenchmark.cpp)
target_link_libraries(MemoryStorageBenchmark PRIVATE SimulatorLib)
add_executable(MemoryEndpointBenchmark cpp/MemoryEndpointBenchmark.cpp)
target_link_libraries(MemoryEndpointBenchmark PRIVATE SimulatorLib)
add_executable(FFMemoryBenchmark cpp/FFMemoryBenchmark.cpp)
target_link_libraries(FFMemoryBenchmark PRIVATE SimulatorLib)
add_executable(CachePerformanceBenchmark cpp/CachePerformanceBenchmark.cpp)
target_link_libraries(CachePerformanceBenchmark PRIVATE SimulatorLib)
add_executable(SRAMBenchmark cpp/SRAMBenchmark.cpp)
target_link_libraries(SRAMBenchmark PRIVATE SimulatorLib)

add_test(NAME SRAMBindingsTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/SRAMBindingsTest.py)
set_tests_properties(SRAMBindingsTest PROPERTIES ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2" LABELS "memory;sram;bindings")
add_test(NAME SRAMLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/SRAMLayoutTest.py)
set_tests_properties(SRAMLayoutTest PROPERTIES LABELS "memory;sram;visualizer")
add_test(NAME MemoryDeviceLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/MemoryDeviceLayoutTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(MemoryDeviceLayoutTest PROPERTIES LABELS "memory;new-memory;visualizer;bindings")
add_test(NAME MemoryEndpointIntegrationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/MemoryEndpointIntegrationTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(MemoryEndpointIntegrationTest PROPERTIES LABELS "memory;memory-endpoint;visualizer;bindings;documentation")
add_test(NAME MemoryInspectorIntegrationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/MemoryInspectorIntegrationTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(MemoryInspectorIntegrationTest PROPERTIES LABELS "memory;visualizer;bindings" TIMEOUT 180)
add_test(NAME MemoryDeviceDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/MemoryDeviceDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER})
set_tests_properties(MemoryDeviceDocumentationTest PROPERTIES LABELS "memory;new-memory;documentation")
add_test(NAME CacheDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/CacheDocumentationTest.py ${CMAKE_CXX_COMPILER})
set_tests_properties(CacheDocumentationTest PROPERTIES LABELS "memory;cache;documentation")
add_test(NAME CacheLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/CacheLayoutTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(CacheLayoutTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "memory;cache;visualizer")

add_executable(
    VisualizerSessionProbe
    cpp/VisualizerSessionProbe.cpp
)
target_link_libraries(VisualizerSessionProbe PRIVATE SimulatorLib)
target_compile_definitions(VisualizerSessionProbe PRIVATE CIRCUITSIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_test(
    NAME RV32IExternalBenchmarkRunnerSmoke
    COMMAND
        $<TARGET_FILE:rv32i_benchmark_elf>
        ${PROJECT_SOURCE_DIR}/tests/fixtures/rv32i/external-smoke/self-check.elf
        0x3ffc0
        1000
        both
)
set_tests_properties(
    RV32IExternalBenchmarkRunnerSmoke
    PROPERTIES LABELS "simulation;integration;rv32i;benchmark"
)

set(SIMULATION_TESTS
    FullCircuitTest
    WireTemplateTest
    SimulatorAdvanceAndRecordTest
    SimulatorUnwiredOutputPinHistoryTest
    SimulatorDrainUntilIdleTest
    SettlingAnalysisTest
    BuildProfileTest
    ComponentCatalogSelectionTest
    BuiltinComponentCatalogInventoryTest
    RecursiveBuildProfileTest
    ComponentTestModelTest
    NOTGateTest
    ANDGateTest
    ORGateTest
    XORGateTest
    NANDGateTest
    NORGateTest
    SRLatchTest
    GatedDLatchTest
    DFlipFlopTest
    ClockGeneratorTest
    FlipFlopCellTest
    MemoryStorageTest
    MemoryTransactionPortTest
    MemoryResponseBufferTest
    RegisterWidthsTest
    ClockedStateBitTest
    MemoryControllerTest
    MemoryArbiterTest
    TwoPortMemoryArbiterTest
    FFAlwaysReadyControllerTest
    MemoryEndpointFFTest
    MemoryEndpointSRAMTest
    MemoryEndpointDRAMTest
    MemoryEndpointROMTest
    MemoryEndpointFlashTest
    ScalableMemoryEndpointTest
    NativeMemoryDeviceTest
    NativeMemoryDeviceContractTest
    MemoryEndpointContractTest
    MemoryEndpointTimingTest
    CacheReadTest
    CacheControllerLockstepTest
    CacheComponentScenarioTest
    CacheDirectMappedScenarioTest
    CacheScalableScenarioTest
    CacheTwoLevelScenarioTest
    FFMemoryTest
    FFWordTest
    FFWordContractTest
    FFStorageBankTest
    FFStorageBankContractTest
    FFMemoryFidelityTest
    SRAMCellTest
    SRAMRowTest
    SRAMBitlinePairTest
    SRAMRowDecoderTest
    SRAMPrechargeTest
    SRAMWriteDriverTest
    SRAMSenseAmplifierTest
    SRAMColumnSelectorTest
    SRAMAccessControlTest
    SRAMOutputBufferTest
    SRAMStorageBankTest
    SRAMTest
    SRAMSubarrayTest
    SRAMClockedTest
    SRAMClockedSubarrayTest
    SRAMContractTest
    SRAMTimingTest
    MatrixRowDecoderTest
    MatrixColumnMuxTest
    MatrixWriteDistributorTest
    StaticReadColumnTest
    MemoryOutputBufferTest
    ROMCellTest
    ROMRowTest
    ROMAddressControlTest
    ROMStorageBankTest
    ROMTest
    DRAMCellTest
    DRAMRowTest
    DRAMSenseColumnTest
    DRAMStorageBankTest
    DRAMCommandControlTest
    DRAMTest
    NORFlashCellTest
    NORFlashRowTest
    NORFlashStorageBankTest
    NORFlashCommandControlTest
    NORFlashTest
    MemoryHelperContractTest
    MemoryDeviceContractTest
    MemoryDeviceTimingTest
    FFMemoryContractTest
    RegisterTest
    RegisterFileWidth32Count32Test
    RewireTest
    RewireValidationTest
    BitSplitter1Test
    BitSplitter2Test
    BitSplitter3Test
    BitSplitter4Test
    BitSplitter5Test
    BitSplitter8Test
    BitSplitter16Test
    BitSplitter32Test
    BitJoiner1Test
    BitJoiner2Test
    BitJoiner3Test
    BitJoiner4Test
    BitJoiner5Test
    BitJoiner8Test
    BitJoiner16Test
    BitJoiner32Test
    ConstantValue1Test
    ConstantValue2Test
    ConstantValue4Test
    ConstantValue8Test
    ConstantValue32Test
    HalfAdderTest
    FullAdderTest
    Mux2Width1Test
    Mux4Width1Test
    Mux8Width1Test
    Mux32Width1Test
    Mux2Width4Test
    Mux2Width32Test
    Mux4Width32Test
    Mux8Width8Test
    Mux8Width32Test
    Mux32Width32Test
    DecoderWidth5Test
    DecoderWidth5ZeroDisabledTest
    RippleCarryAdderWidth8Test
    ZeroDetectWidth8Test
    AddSubWidth8Test
    ArithmeticUnitWidth8Test
    LogicUnitWidth8Test
    BarrelShifterWidth8Test
    CompareSelectWidth8Test
    ALUFlagUnitWidth8Test
    ALUContractTest
    ALUFidelityContractTest
    ALUControlDecoderTest
    ALUResultSelectorWidth8Test
    ALUHierarchyTest
    ParallelPrefixAdderWidth32Test
    ZeroDetectWidth32Test
    RV32IMemoryLanesTest
    RV32IMemoryRequestTest
    RV32IALUControlTest
    RV32IALUAddSubTest
    RV32IALULogicTest
    RV32IALUShifterTest
    RV32IALUCompareTest
    RV32IALUResultSelectTest
    RV32IALUTest
    RV32IALUSliceTest
    RV32IDecodeControlUnitTest
    RV32IExecutionControlStatusUnitTest
    RV32IIFIDPipelineRegisterTest
    RV32IIDEXPipelineRegisterTest
    RV32IEXMEMPipelineRegisterTest
    RV32IMEMWBPipelineRegisterTest
    RV32IForwardingUnitTest
    RV32IHazardDetectionUnitTest
    RV32IPipelineControlSweepTest
    WordGateSweepTest
    CompactFormEquivalenceTest
    GateListCompactEquivalenceTest
    RV32IMemoryAlignmentUnitTest
    RV32IPipelineRetirementUnitTest
    RV32IPipelineCoordinatorTest
    RV32IFetchStageTest
    RV32IDecodeStageTest
    RV32IExecuteStageTest
    RV32IMemoryStageTest
    RV32IPipelineExecutionStatusTest
    RV32IWritebackStageTest
    RV32IFiveStageCoreTest
    RV32IPredictionCounterTest
    RV32IBranchPredecodeTest
    RV32IBranchPredictorTest
    RV32IBranchRecoveryTest
    RV32IPredictionPathTest
    RV32IBitPatternMatcherTest
    RV32IDecoderTest
    RV32IControlTest
    RV32IElfLoaderTest
    RV32IExternalValidationSmokeTest
    RV32IProgramLoaderTest
    RV32IInstructionOracleTest
    RV32IInstructionLockstepHarnessTest
    RV32IInstructionLockstepMismatchDetectionTest
    RV32ISingleCycleCoreTest
    RV32ISingleCycleFetchStageTest
    RV32ISingleCycleDecodeStageTest
    RV32ISingleCycleExecuteStageTest
    RV32ISingleCycleMemoryStageTest
    RV32ISingleCycleWritebackStageTest
    RV32ISingleCycleNextPCStageTest
    RV32ISingleCycleStageIntegrationTest
    RV32IBaselineFetchStageTest
    RV32IBaselineDecodeStageTest
    RV32IBaselineExecuteStageTest
    RV32IBaselineMemoryStageTest
    RV32IBaselineDependencyUnitTest
    RV32IBaselineControlTest
    RV32IBaselinePipelineCoreTest
    RV32IProfileToggleSweepTest
)

set(INFRASTRUCTURE_TESTS
    WireTemplateTest
    SimulatorAdvanceAndRecordTest
    SimulatorUnwiredOutputPinHistoryTest
    SimulatorDrainUntilIdleTest
    SettlingAnalysisTest
    BuildProfileTest
    ComponentCatalogSelectionTest
    BuiltinComponentCatalogInventoryTest
    RecursiveBuildProfileTest
    ComponentTestModelTest
)

set(SEMANTIC_TESTS
    RV32IALUSliceTest
    RewireValidationTest
    RV32IDecoderTest
    RV32IControlTest
    RV32IElfLoaderTest
    RV32IProgramLoaderTest
    RV32IInstructionOracleTest
    RV32IInstructionLockstepMismatchDetectionTest
)

set(INTEGRATION_TESTS
    FullCircuitTest
    RV32IInstructionLockstepHarnessTest
    RV32IExternalValidationSmokeTest
    RV32IProfileToggleSweepTest
)

set(SLOW_SIMULATION_TESTS
    RV32ISharedMemoryProgramTest
    RV32IMemoryArrangementBTest
    RV32IMemoryArrangementD0Test
    RV32IMemoryArrangementDIDTest
    RV32IALUTest
    FFMemoryContractTest
    RegisterFileWidth32Count32Test
    RV32ISingleCycleCoreTest
    RV32IExternalValidationSmokeTest
    RV32IProfileToggleSweepTest
)

foreach(test_name ${SIMULATION_TESTS})
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})

    set(test_labels simulation)
    if(test_name IN_LIST INFRASTRUCTURE_TESTS)
        list(APPEND test_labels infrastructure framework)
    elseif(test_name IN_LIST SEMANTIC_TESTS)
        list(APPEND test_labels semantic)
    elseif(test_name IN_LIST INTEGRATION_TESTS)
        list(APPEND test_labels integration)
    else()
        list(APPEND test_labels contract)
    endif()
    if(test_name MATCHES "RV32I|ALU<32>")
        list(APPEND test_labels rv32i)
    endif()
    if(test_name MATCHES "System")
        list(APPEND test_labels system)
    endif()
    if(test_name IN_LIST SLOW_SIMULATION_TESTS)
        list(APPEND test_labels slow)
    endif()
    set_tests_properties(${test_name} PROPERTIES LABELS "${test_labels}")
endforeach()

set(RV32I_CORRECTNESS_PROGRAM_CASE_COUNT 16)
set(RV32I_PROGRAM_CASE_COUNT 22)

foreach(program_number RANGE 1 ${RV32I_PROGRAM_CASE_COUNT})
    if(program_number LESS 10)
        set(program_suffix "0${program_number}")
    else()
        set(program_suffix "${program_number}")
    endif()
    set(program_test_name
        "RV32ISingleCycleSystemTest/program-${program_suffix}")
    add_test(
        NAME "${program_test_name}"
        COMMAND SimulationTestRunner "${program_test_name}"
    )
    set(program_test_labels simulation contract program rv32i system slow)
    if(program_number GREATER ${RV32I_CORRECTNESS_PROGRAM_CASE_COUNT})
        list(APPEND program_test_labels performance)
    endif()
    set_tests_properties(
        "${program_test_name}"
        PROPERTIES LABELS "${program_test_labels}"
    )
    list(APPEND SIMULATION_TESTS "${program_test_name}")
endforeach()

foreach(program_number RANGE 1 ${RV32I_PROGRAM_CASE_COUNT})
    if(program_number LESS 10)
        set(program_suffix "0${program_number}")
    else()
        set(program_suffix "${program_number}")
    endif()
    set(program_test_name
        "RV32IFiveStageCoreProgramTest/program-${program_suffix}")
    add_test(
        NAME "${program_test_name}"
        COMMAND SimulationTestRunner "${program_test_name}"
    )
    set(program_test_labels
        simulation integration program rv32i pipeline slow)
    if(program_number GREATER ${RV32I_CORRECTNESS_PROGRAM_CASE_COUNT})
        list(APPEND program_test_labels performance)
    endif()
    set_tests_properties(
        "${program_test_name}"
        PROPERTIES LABELS "${program_test_labels}"
    )
    list(APPEND SIMULATION_TESTS "${program_test_name}")
endforeach()

# Keep these program families in the same order as getTestRegistry().
foreach(program_number RANGE 1 ${RV32I_PROGRAM_CASE_COUNT})
    if(program_number LESS 10)
        set(program_suffix "0${program_number}")
    else()
        set(program_suffix "${program_number}")
    endif()
    set(program_test_name "RV32IBaselinePipelineProgramTest/program-${program_suffix}")
    add_test(NAME ${program_test_name} COMMAND SimulationTestRunner ${program_test_name})
    set(program_test_labels simulation integration program rv32i pipeline baseline slow)
    if(program_number GREATER ${RV32I_CORRECTNESS_PROGRAM_CASE_COUNT})
        list(APPEND program_test_labels performance)
    endif()
    set_tests_properties(${program_test_name} PROPERTIES
        LABELS "${program_test_labels}")
    list(APPEND SIMULATION_TESTS "${program_test_name}")
endforeach()
foreach(policy not-taken taken backward-taken one-bit two-bit)
    foreach(program RANGE 1 ${RV32I_PROGRAM_CASE_COUNT})
        if(program LESS 10)
            set(program_suffix "0${program}")
        else()
            set(program_suffix "${program}")
        endif()
        set(prediction_test "RV32IPredictionProgramTest/${policy}/program-${program_suffix}")
        add_test(NAME ${prediction_test} COMMAND SimulationTestRunner ${prediction_test})
        set(program_test_labels simulation integration program rv32i pipeline prediction slow)
        if(program GREATER ${RV32I_CORRECTNESS_PROGRAM_CASE_COUNT})
            list(APPEND program_test_labels performance)
        endif()
        set_tests_properties(${prediction_test} PROPERTIES
            LABELS "${program_test_labels}")
        list(APPEND SIMULATION_TESTS "${prediction_test}")
    endforeach()
endforeach()

# These arrangements are appended after the program families in TestRegistry.
foreach(test_name RV32ISharedMemoryProgramTest RV32IMemoryArrangementBTest RV32IMemoryArrangementD0Test RV32IMemoryArrangementDIDTest)
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;integration;program;rv32i;pipeline;cache;memory;slow")
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()

# Generic arithmetic ownership precedes the game/device append hook.
foreach(test_name ALUWidthContractTest ALUScenarioTest ALUWidth8ScenarioTest ALUWidth32ScenarioTest RippleCarryAdderWidth16Test ParallelPrefixAdderWidth16Test AdderWidthSweepTest RV32IALUSweepTest ANDGateWidth8Test)
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;alu;width")
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()

# Additive phase 0-3 checks and recorded aliases, in the append hook's order.
set(RV32IM_PHASE3_TESTS
    WidthComponentsTest
    MultiplyStepTest
    DivideStepTest
    MultiplierWidth32Test
    DividerWidth32Test
    MulDivUnitWidth32Test
    MulDivGateLevelTest
    TableOnlyDelayPublicationTest
    RV32IMSemanticsTest
    RV32IMDecodeControlTest
    RV32IMDecodeStageTest
    RV32IMExecuteStageTest
    RV32IMPipelineCoordinatorTest
    RV32IMFiveStageCoreTest
    RV32IMCompiledProgramTest
    ClockedCounterTest
    SynchronousFifoTest
    DeviceRegisterPortWidth32Test
    ConsoleDeviceTest
    TimerDeviceTest
    ButtonDeviceTest
    FramebufferDisplayTest
    DeviceInspectorTest
    MultiplyStepScenarioTest
    DivideStepScenarioTest
    MultiplierWidth32ScenarioTest
    DividerWidth32ScenarioTest
    MulDivUnitWidth32ScenarioTest
    RV32IMDecodeControlScenarioTest
    RV32IMDecodeStageScenarioTest
    RV32IMExecuteStageScenarioTest
    RV32IMPipelineCoordinatorScenarioTest
    RV32IMFiveStageCoreScenarioTest
    ClockedCounterScenarioTest
    SynchronousFifoScenarioTest
    DeviceRegisterPortWidth32ScenarioTest
    ConsoleDeviceScenarioTest
    TimerDeviceScenarioTest
    ButtonDeviceScenarioTest
    FramebufferDisplayScenarioTest
)
foreach(test_name ${RV32IM_PHASE3_TESTS})
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;rv32im;phase3")
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()
# Exhaustive 4-bit and boundary 8/16/32-bit gate-level M units take about 45 s.
set_tests_properties(MulDivGateLevelTest PROPERTIES LABELS "simulation;rv32im;phase3;slow" TIMEOUT 600)

foreach(test_name RV32IMArithmeticProgramTest RV32IMDependenciesProgramTest RV32IMDivideEdgesProgramTest
    RV32IMFibonacciProgramTest RV32IMGcdProgramTest)
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;rv32im;program;recorded-program")
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()

foreach(test_name SimulationTimeTest SimulationTimePlaybackTest)
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;time;phase4")
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()

foreach(test_name ComputerSystemTest
    ComputerSystemCachedFFTest ComputerSystemCachedDRAMTest
    ComputerSystemWideTimeTest ComputerSystemUnsignedTimeTest
    ComputerSystemTwoBusTest ComputerSystemRegisteredBridgeTest ComputerSystemPortCheckTest ComputerSystemLockstepTest
    RV32IMLargerDisplayTest RV32IMRecordedInputValidationTest RV32IMRecordedEvidenceBudgetTest ComputerSystemFaultTest
    # A long game's replay needs a save: GameReplayScenarioTest.py below runs it.
    GameReplayScenarioTest
    # A real program on each machine, and the short games (GuestScenarioTests).
    ComputerSystemScenarioTest ComputerSystemTwoBusScenarioTest ComputerSystemRegisteredBridgeScenarioTest
    ComputerSystemCheckedScenarioTest ComputerSystemLedsScenarioTest
    Game2048ScenarioTest GameSokobanScenarioTest GameSnakeScenarioTest GamePongScenarioTest
    GameTetrisScenarioTest GameSnakeLedsScenarioTest ComputerSystemStructureScenarioTest)
    list(APPEND SIMULATION_TESTS ${test_name})
    if(test_name STREQUAL "GameReplayScenarioTest")
        continue()
    endif()
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;rv32im;device;phase4;games" TIMEOUT 3600)
endforeach()

# The computer-system parts (§ 5), last in the registry.
foreach(test_name SystemDescriptionTest BusInterconnectTest LedPanelTest BusPortCheckerTest GuestHeaderContentTest
    BusRouterScenarioTest BusBridgeScenarioTest MemorySubsystemScenarioTest LedPanelScenarioTest
    BusInterconnectScenarioTest BusPortCheckerScenarioTest)
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;system")
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()

# The accelerator parts (§ 6), after the computer system in the registry.
foreach(test_name ArrayMultiplierTest ArrayMulDivUnitTest MacCellTest RequantizerTest SystolicArrayTest
    ArrayMultiplierWidth4ScenarioTest ArrayMulDivUnitWidth4ScenarioTest MacCellScenarioTest RequantizerScenarioTest SystolicArrayWidth2ScenarioTest
    MatrixUnitTest MatrixUnitScenarioTest PipelinedBusTest AcceleratorMachineTest AcceleratorMachineScenarioTest)
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS "simulation;accelerator" TIMEOUT 3600)
    list(APPEND SIMULATION_TESTS ${test_name})
endforeach()

add_test(
    NAME VisualizerModuleBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerModuleBindingsTest.py
)

add_test(NAME DeviceParametersBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/DeviceParametersBindingsTest.py)
set_tests_properties(DeviceParametersBindingsTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "visualizer;device;contract" TIMEOUT 120)

add_test(NAME FFMemoryBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/FFMemoryBindingsTest.py
        $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(FFMemoryBindingsTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "visualizer;memory;contract")

add_test(
    NAME VisualizerSessionContractTest
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerSessionContractTest.py
        $<TARGET_FILE:VisualizerSessionProbe>
)
add_test(NAME VisualizerPinIdentityTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerPinIdentityTest.py
        $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(VisualizerPinIdentityTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "visualizer;unit;rv32i")
add_test(NAME VisualizerAutomaticLayoutTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerAutomaticLayoutTest.py)
set_tests_properties(VisualizerAutomaticLayoutTest PROPERTIES LABELS "visualizer;layout")
add_test(
    NAME DocumentationExamplesTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/DocumentationExamplesTest.py
        $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER}
)
set_tests_properties(DocumentationExamplesTest PROPERTIES LABELS "documentation;visualizer")
add_test(NAME TimingTableDocumentationTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/TimingTableDocumentationTest.py)
set_tests_properties(TimingTableDocumentationTest PROPERTIES LABELS "documentation;timing;unit")
add_executable(RV32IReferenceProbe cpp/RV32IReferenceProbe.cpp)
target_link_libraries(RV32IReferenceProbe PRIVATE SimulatorLib)
add_test(
    NAME RV32IReferenceDocumentationTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IReferenceDocumentationTest.py
        $<TARGET_FILE:RV32IReferenceProbe>
        ${CMAKE_CXX_COMPILER} $<TARGET_FILE:SimulatorLib>
)
set_tests_properties(RV32IReferenceDocumentationTest PROPERTIES LABELS "documentation;rv32i")
set_tests_properties(
    VisualizerSessionContractTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system;wasm"
)

add_test(
    NAME VisualizerProfileStoreTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerProfileStoreTest.py
)
add_test(
    NAME VisualizerColorPaletteTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerColorPaletteTest.py
)
set_tests_properties(
    VisualizerColorPaletteTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;unit"
)
find_program(CIRCUITSIM_NODE_EXECUTABLE NAMES node nodejs)
if(CIRCUITSIM_NODE_EXECUTABLE)
    add_test(NAME TickRecordingTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/TickRecordingTest.mjs $<TARGET_FILE:SignalTraceProbe>)
    set_tests_properties(TickRecordingTest PROPERTIES LABELS "visualizer;device;recorded-games" TIMEOUT 90)
    add_test(NAME VisualizerAutomaticLayoutMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerAutomaticLayoutMigrationTest.mjs)
    set_tests_properties(VisualizerAutomaticLayoutMigrationTest PROPERTIES LABELS "visualizer;layout;unit")
    add_test(NAME VisualizerLayoutChangesTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerLayoutChangesTest.mjs)
    set_tests_properties(VisualizerLayoutChangesTest PROPERTIES LABELS "visualizer;layout;unit")
    add_test(NAME VisualizerTopologyIdentityTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerTopologyIdentityTest.js)
    set_tests_properties(VisualizerTopologyIdentityTest PROPERTIES LABELS "visualizer;unit")
    add_test(NAME VisualizerDeviceViewTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerDeviceViewTest.js)
    set_tests_properties(VisualizerDeviceViewTest PROPERTIES LABELS "visualizer;device;unit")
    add_test(NAME ExampleRowsIsolationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/ExampleRowsIsolationTest.mjs)
    set_tests_properties(ExampleRowsIsolationTest PROPERTIES LABELS "documentation;visualizer;unit" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    add_test(NAME VisualizerMemoryCellViewTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerMemoryCellViewTest.js)
    add_test(NAME VisualizerMemoryViewTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerMemoryViewTest.js)
    set_tests_properties(VisualizerMemoryViewTest PROPERTIES LABELS "memory;visualizer;unit")
    add_test(NAME RegisterFileProfileMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RegisterFileProfileMigrationTest.mjs)
    add_test(
        NAME VisualizerReferenceProfileTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerReferenceProfileTest.mjs
    )
    set_tests_properties(VisualizerReferenceProfileTest PROPERTIES LABELS "visualizer;documentation")
    add_test(
        NAME VisualizerCameraAnimationTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerCameraAnimationTest.js
    )
    set_tests_properties(
        VisualizerCameraAnimationTest
        PROPERTIES LABELS "visualizer;unit"
    )
    add_test(
        NAME VisualizerWireGeometryTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerWireGeometryTest.js
    )
    set_tests_properties(
        VisualizerWireGeometryTest
        PROPERTIES LABELS "visualizer;unit"
    )
    add_test(
        NAME VisualizerRewireGeometryTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerRewireGeometryTest.js
    )
    add_test(NAME SRAMLayoutMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/SRAMLayoutMigrationTest.mjs)
    add_test(NAME RV32ILayoutMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32ILayoutMigrationTest.mjs)
    set_tests_properties(RV32ILayoutMigrationTest PROPERTIES LABELS "rv32i;visualizer;unit")
    set_tests_properties(
        VisualizerRewireGeometryTest
        PROPERTIES LABELS "visualizer;unit"
    )
endif()
# The static (WebAssembly) site cannot compute a missing child placement:
# every child of every scenario must have one in layout.json.
add_test(
    NAME VisualizerStaticLayoutTest
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check-static-layouts.py
)
set_tests_properties(
    VisualizerStaticLayoutTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;layout"
        TIMEOUT 3600
)
add_test(
    NAME VisualizerTimingProfileTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerTimingProfileTest.py
)
set_tests_properties(
    VisualizerTimingProfileTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system;timing"
)
set_tests_properties(
    VisualizerProfileStoreTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system"
)
set_tests_properties(
    VisualizerModuleBindingsTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system"
)

add_test(
    NAME TestRegistryCoverageTest
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/checks/TestRegistryCoverageTest.py
        $<TARGET_FILE:SimulationTestRunner>
        ${SIMULATION_TESTS}
)
set_tests_properties(
    TestRegistryCoverageTest
    PROPERTIES LABELS "framework"
)

# All Python checks must import this build's backend, including tests that
# exercise source-side layout helpers. An isolated build must never silently
# validate the older module deployed to the running preview.
get_property(circuitsim_directory_tests DIRECTORY PROPERTY TESTS)
set_property(TEST ${circuitsim_directory_tests} APPEND PROPERTY ENVIRONMENT
    "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2")

# Native CLI for the recorded device/game runtime. The portable counterpart is
# circuitsim_recorded_guest in the Emscripten build.
add_executable(RecordedGuestProbe cpp/RecordedGuestProbe.cpp)
target_link_libraries(RecordedGuestProbe PRIVATE SimulatorLib)
add_executable(RecordedCircuitProbe cpp/RecordedCircuitProbe.cpp)
target_link_libraries(RecordedCircuitProbe PRIVATE SimulatorLib)
add_executable(LongGameplayProbe cpp/LongGameplayProbe.cpp)
find_package(ZLIB REQUIRED)
target_link_libraries(LongGameplayProbe PRIVATE SimulatorLib ZLIB::ZLIB)
# A saved machine resumes exactly: the resumed run's save at a later cycle
# equals the uninterrupted run's, byte for byte (circuit::state).
add_executable(MachineStateProbe cpp/MachineStateProbe.cpp)
target_link_libraries(MachineStateProbe PRIVATE SimulatorLib ZLIB::ZLIB)
add_test(NAME MachineStateResume_pong-short_Test COMMAND MachineStateProbe pong-short --save 2500 --compare 5000)
add_test(NAME MachineStateResume_smoke-long_Test COMMAND MachineStateProbe smoke-long --save 20000 --compare 22000)
add_test(NAME MachineStateResume_pong-short-compact_Test COMMAND MachineStateProbe pong-short --save 2500 --compare 5000 --fidelity compact)
# The recording viewer's stepping: a replay session stepped, viewed and
# jumped ends in the state of one uninterrupted run.
add_executable(GameSessionProbe cpp/GameSessionProbe.cpp)
target_link_libraries(GameSessionProbe PRIVATE SimulatorLib)
add_test(NAME GameReplayScenarioTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/GameReplayScenarioTest.py
    --probe $<TARGET_FILE:MachineStateProbe> --runner $<TARGET_FILE:SimulationTestRunner> --session $<TARGET_FILE:GameSessionProbe>)
set_tests_properties(GameReplayScenarioTest PROPERTIES LABELS "engine;rv32im;games;visualizer" TIMEOUT 900)
# The published long-game saves still open in this build (one per set).
add_test(NAME PublishedSavesTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/PublishedSavesTest.py
    --session $<TARGET_FILE:GameSessionProbe>)
set_tests_properties(PublishedSavesTest PROPERTIES LABELS "rv32im;games;published" TIMEOUT 1800)
set_tests_properties(MachineStateResume_pong-short_Test MachineStateResume_smoke-long_Test MachineStateResume_pong-short-compact_Test PROPERTIES
    LABELS "engine;rv32im;games;slow" TIMEOUT 1800 ENVIRONMENT "CIRCUITSIM_TIMING_PROFILE=embedded")
add_executable(SignalTraceProbe cpp/SignalTraceProbe.cpp)
target_link_libraries(SignalTraceProbe PRIVATE SimulatorLib ZLIB::ZLIB)
add_executable(GateInputCacheTest cpp/GateInputCacheTest.cpp)
target_link_libraries(GateInputCacheTest PRIVATE SimulatorLib)
add_test(NAME GateInputCacheTest COMMAND GateInputCacheTest)
set_tests_properties(GateInputCacheTest PROPERTIES LABELS "engine;unit;timing")

add_executable(SignalWidthTest cpp/SignalWidthTest.cpp)
target_link_libraries(SignalWidthTest PRIVATE SimulatorLib)
add_test(NAME SignalWidthTest COMMAND SignalWidthTest)
set_tests_properties(SignalWidthTest PROPERTIES LABELS "engine;unit")
add_executable(DelayTwinIncrementalTest cpp/DelayTwinIncrementalTest.cpp)
target_link_libraries(DelayTwinIncrementalTest PRIVATE SimulatorLib)
target_include_directories(DelayTwinIncrementalTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp ${PROJECT_SOURCE_DIR}/src/analysis)
add_test(NAME DelayTwinIncrementalTest COMMAND DelayTwinIncrementalTest)
set_tests_properties(DelayTwinIncrementalTest PROPERTIES LABELS "engine;unit;timing" TIMEOUT 600)
add_test(NAME RecordedTraceAssetsTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RecordedTraceAssetsTest.py)
set_tests_properties(RecordedTraceAssetsTest PROPERTIES LABELS "docs;recorded-games" SKIP_RETURN_CODE 77)
add_executable(FidelityPresetsTest cpp/FidelityPresetsTest.cpp)
target_link_libraries(FidelityPresetsTest PRIVATE SimulatorLib)
add_test(NAME FidelityPresetsTest COMMAND FidelityPresetsTest)
set_tests_properties(FidelityPresetsTest PROPERTIES LABELS "visualizer;profile")
add_test(NAME VisualizerFidelityProfileTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerFidelityProfileTest.py)
set_tests_properties(VisualizerFidelityProfileTest PROPERTIES LABELS "visualizer;profile"
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2")
if(CIRCUITSIM_NODE_EXECUTABLE)
    add_test(NAME RecordedCircuitTransportTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RecordedCircuitTransportTest.mjs)
    set_tests_properties(RecordedCircuitTransportTest PROPERTIES LABELS "visualizer;device;recorded-games")
endif()

# These C sources are valid C++ as well. Keep the project compiler languages
# unchanged; the strict C11 cross-check is run by the reference check helper.
set_source_files_properties(cpp/RV32IMGameRulesTest.c
    ${PROJECT_SOURCE_DIR}/tools/rv32im/guest/games.c PROPERTIES LANGUAGE CXX)
add_executable(RV32IMGameRulesTest cpp/RV32IMGameRulesTest.c
    ${PROJECT_SOURCE_DIR}/tools/rv32im/guest/games.c)
target_include_directories(RV32IMGameRulesTest PRIVATE ${PROJECT_SOURCE_DIR}/tools/rv32im/guest)
add_test(NAME RV32IMGameRulesTest COMMAND RV32IMGameRulesTest)
set_tests_properties(RV32IMGameRulesTest PROPERTIES LABELS "rv32im;games;phase5;unit")
# Short fixtures: the circuit run against the host-compiled guest.
foreach(guest bringup bringup-leds 2048-short sokoban-short snake-short pong-short tetris-short snake-leds-short)
    add_test(NAME RV32IMRecorded_${guest}_Test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IMGameReferenceTest.py
            --runner $<TARGET_FILE:RecordedGuestProbe> --guest ${guest})
    set_tests_properties(RV32IMRecorded_${guest}_Test PROPERTIES
        LABELS "rv32im;device;games;phase4;phase5;integration;slow" TIMEOUT 3600)
endforeach()
# Long fixtures: the host run against the computer player's prediction (their
# circuit runs take hours; see tools/rv32im/run-long-gameplays.py).
foreach(guest 2048-long sokoban-long snake-long pong-long tetris-long snake-leds-long)
    add_test(NAME RV32IMReference_${guest}_Test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/RV32IMGameReferenceTest.py
            --reference-only --guest ${guest})
    set_tests_properties(RV32IMReference_${guest}_Test PROPERTIES LABELS "rv32im;games;unit")
endforeach()

add_test(NAME DelayTableChunksTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/DelayTableChunksTest.py)
set_tests_properties(DelayTableChunksTest PROPERTIES LABELS "build;timing;unit")
add_executable(DelayTableSnapshot cpp/DelayTableSnapshot.cpp)
target_link_libraries(DelayTableSnapshot PRIVATE SimulatorLib)

add_test(NAME SimulationTimeBindingsTest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/checks/SimulationTimeBindingsTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(SimulationTimeBindingsTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_BINARY_DIR}:${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "simulation;time;visualizer;contract;phase4" TIMEOUT 180)

# Compile the freestanding library with its actual language for geometry checks.
enable_language(C)
add_executable(GuestDisplayGeometryTest cpp/GuestDisplayGeometryTest.c
    ${CMAKE_SOURCE_DIR}/tools/rv32im/guest/platform.c)
target_include_directories(GuestDisplayGeometryTest PRIVATE ${CMAKE_SOURCE_DIR}/tools/rv32im/guest)
set_target_properties(GuestDisplayGeometryTest PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_compile_definitions(GuestDisplayGeometryTest PRIVATE CIRCUITSIM_GUEST_REFERENCE)
target_compile_options(GuestDisplayGeometryTest PRIVATE -fno-builtin -Wall -Wextra -Werror)
foreach(geometry IN ITEMS "64;48;64" "320;200;320" "97;65;97" "97;65;101")
    string(REPLACE ";" "x" key "${geometry}")
    add_test(NAME GuestDisplayGeometryTest/${key} COMMAND GuestDisplayGeometryTest ${geometry})
    set_tests_properties(GuestDisplayGeometryTest/${key} PROPERTIES LABELS "rv32im;device;phase6")
endforeach()

if(CIRCUITSIM_NODE_EXECUTABLE)
    add_test(NAME VisualizerProfileLayoutsTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/checks/VisualizerProfileLayoutsTest.mjs)
    set_tests_properties(VisualizerProfileLayoutsTest PROPERTIES LABELS "visualizer;profile")
endif()
