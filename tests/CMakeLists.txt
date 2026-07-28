add_executable(SimulationTestRunner SimulationTestRunner.cpp)
target_link_libraries(SimulationTestRunner PRIVATE SimulatorLib)

set(SIMULATION_TESTS
    FullCircuitTest
    WireTemplateTest
    SimulatorAdvanceAndRecordTest
    SimulatorUnwiredOutputPinHistoryTest
    SimulatorDrainUntilIdleTest
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
    MemoryBitTest
    Memory64Kx32Test
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
    RV32IBitPatternMatcherTest
    RV32IDecoderTest
    RV32IControlTest
    RV32IProgramLoaderTest
    RV32IInstructionOracleTest
    RV32IInstructionLockstepHarnessTest
    RV32IInstructionLockstepMismatchDetectionTest
    RV32ISingleCycleCoreTest
    RV32IProfileToggleSweepTest
)

set(INFRASTRUCTURE_TESTS
    WireTemplateTest
    SimulatorAdvanceAndRecordTest
    SimulatorUnwiredOutputPinHistoryTest
    SimulatorDrainUntilIdleTest
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
    RV32IProgramLoaderTest
    RV32IInstructionOracleTest
    RV32IInstructionLockstepMismatchDetectionTest
)

set(INTEGRATION_TESTS
    FullCircuitTest
    RV32IInstructionLockstepHarnessTest
    RV32IProfileToggleSweepTest
)

set(SLOW_SIMULATION_TESTS
    ALU32Test
    Memory32x32Test
    RegisterFile32x32Test
    RV32ISingleCycleCoreTest
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

foreach(program_number RANGE 1 16)
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
    set_tests_properties(
        "${program_test_name}"
        PROPERTIES LABELS "simulation;contract;rv32i;system;slow"
    )
    list(APPEND SIMULATION_TESTS "${program_test_name}")
endforeach()

add_test(
    NAME VisualizerModuleBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerModuleBindingsTest.py
)

add_test(
    NAME VisualizerProfileStoreTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerProfileStoreTest.py
)
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
