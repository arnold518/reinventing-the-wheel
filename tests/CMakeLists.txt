add_executable(SimulationTestRunner SimulationTestRunner.cpp)
target_link_libraries(SimulationTestRunner PRIVATE SimulatorLib)
add_executable(Register32FFIntegrationTest Register32FFIntegrationTest.cpp)
target_link_libraries(Register32FFIntegrationTest PRIVATE SimulatorLib)
add_test(NAME Register32FFIntegrationTest COMMAND Register32FFIntegrationTest)
set_tests_properties(Register32FFIntegrationTest PROPERTIES LABELS "memory;register;rv32i;integration" TIMEOUT 180)
add_test(NAME Register32DocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Register32DocumentationTest.py ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(Register32DocumentationTest PROPERTIES LABELS "memory;register;documentation")
add_executable(RegisterFileFFIntegrationTest RegisterFileFFIntegrationTest.cpp)
target_link_libraries(RegisterFileFFIntegrationTest PRIVATE SimulatorLib)
add_test(NAME RegisterFileFFIntegrationTest COMMAND RegisterFileFFIntegrationTest)
set_tests_properties(RegisterFileFFIntegrationTest PROPERTIES LABELS "memory;register-file;integration" TIMEOUT 180)
add_test(NAME RegisterFileDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RegisterFileDocumentationTest.py ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RegisterFileDocumentationTest PROPERTIES LABELS "memory;register-file;documentation")
add_test(NAME RV32IBuildingBlocksDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32IBuildingBlocksDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RV32IBuildingBlocksDocumentationTest PROPERTIES LABELS "rv32i;documentation")
add_test(NAME RV32ISingleCycleDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32ISingleCycleDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER} ${PROJECT_BINARY_DIR}/libSimulatorLib.a)
set_tests_properties(RV32ISingleCycleDocumentationTest PROPERTIES LABELS "rv32i;documentation" TIMEOUT 240)
add_executable(RV32IMemoryMigrationTest RV32IMemoryMigrationTest.cpp)
target_link_libraries(RV32IMemoryMigrationTest PRIVATE SimulatorLib)
add_test(NAME RV32IMemoryMigrationTest COMMAND RV32IMemoryMigrationTest)
set_tests_properties(RV32IMemoryMigrationTest PROPERTIES LABELS "rv32i;memory;integration" TIMEOUT 180)
add_test(NAME RV32IMemoryLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32IMemoryLayoutTest.py)
set_tests_properties(RV32IMemoryLayoutTest PROPERTIES LABELS "rv32i;memory;visualizer")
add_test(NAME RV32ISingleCycleLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32ISingleCycleLayoutTest.py)
set_tests_properties(RV32ISingleCycleLayoutTest PROPERTIES LABELS "rv32i;visualizer;layout")
add_test(NAME RV32IBuildingBlockLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32IBuildingBlockLayoutTest.py)
set_tests_properties(RV32IBuildingBlockLayoutTest PROPERTIES LABELS "rv32i;visualizer" TIMEOUT 120)
add_executable(MemoryContainerBenchmark MemoryContainerBenchmark.cpp)
target_link_libraries(MemoryContainerBenchmark PRIVATE SimulatorLib)
add_executable(MemoryEndpointBenchmark MemoryEndpointBenchmark.cpp)
target_link_libraries(MemoryEndpointBenchmark PRIVATE SimulatorLib)
add_executable(FFMemoryBenchmark FFMemoryBenchmark.cpp)
target_link_libraries(FFMemoryBenchmark PRIVATE SimulatorLib)
add_executable(SRAMBenchmark SRAMBenchmark.cpp)
target_link_libraries(SRAMBenchmark PRIVATE SimulatorLib)

add_test(NAME SRAMBindingsTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SRAMBindingsTest.py)
set_tests_properties(SRAMBindingsTest PROPERTIES ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2" LABELS "memory;sram;bindings")
add_test(NAME SRAMLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SRAMLayoutTest.py)
set_tests_properties(SRAMLayoutTest PROPERTIES LABELS "memory;sram;visualizer")
add_test(NAME MemoryDeviceLayoutTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MemoryDeviceLayoutTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(MemoryDeviceLayoutTest PROPERTIES LABELS "memory;new-memory;visualizer;bindings")
add_test(NAME MemoryEndpointIntegrationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MemoryEndpointIntegrationTest.py $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(MemoryEndpointIntegrationTest PROPERTIES LABELS "memory;memory-endpoint;visualizer;bindings;documentation")
add_test(NAME MemoryDeviceDocumentationTest COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MemoryDeviceDocumentationTest.py $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER})
set_tests_properties(MemoryDeviceDocumentationTest PROPERTIES LABELS "memory;new-memory;documentation")

add_executable(
    VisualizerSessionProbe
    VisualizerSessionProbe.cpp
)
target_link_libraries(VisualizerSessionProbe PRIVATE SimulatorLib)

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
    CircuitTimingAnalyzerTest
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
    Memory64Kx32Test
    MemoryStorageTest
    MemoryBusAdapter32Test
    MemoryBusAdapterIntegrationTest
    MemoryTransactionPortTest
    MemoryControllerTest
    MemoryEndpointFFTest
    MemoryEndpointSRAMTest
    MemoryEndpointDRAMTest
    MemoryEndpointROMTest
    MemoryEndpointFlashTest
    MemoryEndpointRelayTest
    ScalableMemoryContainerTest
    MemoryEndpointContractTest
    MemoryEndpointTimingTest
    FFMemoryContainerTest
    FFWordTest
    FFWordContractTest
    FFStorageBankTest
    FFStorageBankContractTest
    FFMemoryFidelityTest
    SRAMContainerTest
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
    ROMContainerTest
    MemoryContainerContractTest
    Register32Test
    RegisterFile4x32Test
    RegisterFile32x32Test
    Memory4x32Test
    Memory32x32Test
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
    AND8Test
    OR8Test
    XOR8Test
    NOT8Test
    NAND8Test
    NOR8Test
    Mux2to1Test
    Mux4to1Test
    Mux8to1Test
    Mux16to1Test
    Mux32to1Test
    Mux2to1_8bitTest
    Mux2to1_4bitTest
    Mux2to1_32bitTest
    Mux4to1_8bitTest
    Mux4to1_32bitTest
    Mux8to1_8bitTest
    Mux8to1_32bitTest
    Mux16to1_8bitTest
    Mux32to1_32bitTest
    Decoder2to4Test
    Decoder5to32Test
    Adder8Test
    TwosComplement8Test
    Subtractor8Test
    SubtractorWithBorrow8Test
    Incrementer8Test
    Decrementer8Test
    EqualityChecker8Test
    Comparator8Test
    SignedComparator8Test
    ShiftLeftLogical8Test
    ShiftRightLogical8Test
    ShiftRightArithmetic8Test
    ZeroDetect8Test
    ALU8Test
    AddSub8Test
    ArithmeticUnit8Test
    LogicUnit8Test
    BarrelShifter8Test
    CompareSelect8Test
    ALU8FlagUnitTest
    ALU8ContractTest
    ALU8FidelityContractTest
    ALU8ControlDecoderTest
    ALU8ResultSelectorTest
    ALU32ControlDecoderTest
    ALU32CompareUnitTest
    ALU32ResultSelectorTest
    ALU32FlagUnitTest
    ALUHierarchyTest
    Adder32Test
    AddSub32Test
    Logic32Test
    ZeroDetect32Test
    Comparator32Test
    Shifter32Test
    ALU32Test
    ALU32RepresentativeSliceTest
    RV32IControlFlowUnitTest
    RV32IDecodeControlUnitTest
    RV32IExecutionControlStatusUnitTest
    RV32IIFIDPipelineRegisterTest
    RV32IIDEXPipelineRegisterTest
    RV32IEXMEMPipelineRegisterTest
    RV32IMEMWBPipelineRegisterTest
    RV32IForwardingUnitTest
    RV32IHazardDetectionUnitTest
    RV32IPipelineControlFlowUnitTest
    RV32IMemoryAlignmentUnitTest
    RV32IMemoryLanesTest
    RV32IMemoryRequestTest
    RV32IPipelineRetirementUnitTest
    RV32IPipelineCoordinatorTest
    RV32IFetchStageTest
    RV32IDecodeStageTest
    RV32IExecuteStageTest
    RV32IMemoryStageTest
    RV32IWritebackStageTest
    RV32IFiveStageCoreTest
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
    RV32IProfileToggleSweepTest
)

set(INFRASTRUCTURE_TESTS
    WireTemplateTest
    SimulatorAdvanceAndRecordTest
    SimulatorUnwiredOutputPinHistoryTest
    SimulatorDrainUntilIdleTest
    CircuitTimingAnalyzerTest
    BuildProfileTest
    ComponentCatalogSelectionTest
    BuiltinComponentCatalogInventoryTest
    RecursiveBuildProfileTest
    ComponentTestModelTest
)

set(SEMANTIC_TESTS
    ALU32RepresentativeSliceTest
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
    ALU32Test
    MemoryContainerContractTest
    Memory32x32Test
    RegisterFile32x32Test
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
    if(test_name MATCHES "RV32I|ALU32")
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

add_test(
    NAME VisualizerModuleBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerModuleBindingsTest.py
)

add_test(NAME MemoryContainerBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MemoryContainerBindingsTest.py
        $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(MemoryContainerBindingsTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "visualizer;memory;contract")

add_test(
    NAME VisualizerSessionContractTest
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerSessionContractTest.py
        $<TARGET_FILE:VisualizerSessionProbe>
)
add_test(NAME VisualizerPinIdentityTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerPinIdentityTest.py
        $<TARGET_FILE:VisualizerSessionProbe>)
set_tests_properties(VisualizerPinIdentityTest PROPERTIES
    ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2"
    LABELS "visualizer;unit;rv32i")
add_test(NAME VisualizerAutomaticLayoutTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerAutomaticLayoutTest.py)
set_tests_properties(VisualizerAutomaticLayoutTest PROPERTIES LABELS "visualizer;layout")
add_test(
    NAME DocumentationExamplesTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/DocumentationExamplesTest.py
        $<TARGET_FILE:VisualizerSessionProbe> ${CMAKE_CXX_COMPILER}
)
set_tests_properties(DocumentationExamplesTest PROPERTIES LABELS "documentation;visualizer")
add_executable(RV32IReferenceProbe RV32IReferenceProbe.cpp)
target_link_libraries(RV32IReferenceProbe PRIVATE SimulatorLib)
add_test(
    NAME RV32IReferenceDocumentationTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32IReferenceDocumentationTest.py
        $<TARGET_FILE:RV32IReferenceProbe>
        ${CMAKE_CXX_COMPILER} $<TARGET_FILE:SimulatorLib>
)
set_tests_properties(RV32IReferenceDocumentationTest PROPERTIES LABELS "documentation;rv32i")
set_tests_properties(
    VisualizerSessionContractTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system;wasm"
)

add_test(
    NAME VisualizerProfileStoreTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerProfileStoreTest.py
)
add_test(
    NAME VisualizerColorPaletteTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerColorPaletteTest.py
)
set_tests_properties(
    VisualizerColorPaletteTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;unit"
)
find_program(CIRCUITSIM_NODE_EXECUTABLE NAMES node nodejs)
if(CIRCUITSIM_NODE_EXECUTABLE)
    add_test(NAME VisualizerAutomaticLayoutMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerAutomaticLayoutMigrationTest.mjs)
    set_tests_properties(VisualizerAutomaticLayoutMigrationTest PROPERTIES LABELS "visualizer;layout;unit")
    add_test(NAME VisualizerTopologyIdentityTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerTopologyIdentityTest.js)
    set_tests_properties(VisualizerTopologyIdentityTest PROPERTIES LABELS "visualizer;unit")
    add_test(NAME VisualizerMemoryCellViewTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerMemoryCellViewTest.js)
    add_test(NAME RegisterFileProfileMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RegisterFileProfileMigrationTest.mjs)
    add_test(
        NAME VisualizerReferenceProfileTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerReferenceProfileTest.mjs
    )
    set_tests_properties(VisualizerReferenceProfileTest PROPERTIES LABELS "visualizer;documentation")
    add_test(
        NAME VisualizerCameraAnimationTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerCameraAnimationTest.js
    )
    set_tests_properties(
        VisualizerCameraAnimationTest
        PROPERTIES LABELS "visualizer;unit"
    )
    add_test(
        NAME VisualizerWireGeometryTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerWireGeometryTest.js
    )
    set_tests_properties(
        VisualizerWireGeometryTest
        PROPERTIES LABELS "visualizer;unit"
    )
    add_test(
        NAME VisualizerRewireGeometryTest
        COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerRewireGeometryTest.js
    )
    add_test(NAME SRAMLayoutMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SRAMLayoutMigrationTest.mjs)
    add_test(NAME RV32ILayoutMigrationTest COMMAND ${CIRCUITSIM_NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RV32ILayoutMigrationTest.mjs)
    set_tests_properties(RV32ILayoutMigrationTest PROPERTIES LABELS "rv32i;visualizer;unit")
    set_tests_properties(
        VisualizerRewireGeometryTest
        PROPERTIES LABELS "visualizer;unit"
    )
endif()
set_tests_properties(
    VisualizerProfileStoreTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system"
)
set_tests_properties(
    VisualizerModuleBindingsTest
    PROPERTIES
        ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/visualizer-v2"
        LABELS "visualizer;system"
)

add_test(
    NAME TestRegistryCoverageTest
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/TestRegistryCoverageTest.py
        $<TARGET_FILE:SimulationTestRunner>
        ${SIMULATION_TESTS}
)
set_tests_properties(
    TestRegistryCoverageTest
    PROPERTIES LABELS "framework"
)
