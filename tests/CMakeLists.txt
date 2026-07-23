add_executable(SimulationTestRunner SimulationTestRunner.cpp)
target_link_libraries(SimulationTestRunner PRIVATE SimulatorLib)

set(SIMULATION_TESTS
    FullCircuitTest
    WireTemplateTest
    SimulatorAdvanceAndRecordTest
    SimulatorUnwiredOutputPinHistoryTest
    BuildProfileTest
    ComponentCatalogSelectionTest
    BuiltinComponentCatalogInventoryTest
    RecursiveBuildProfileTest
    MemoryBitEquivalenceTest
    Register32EquivalenceTest
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
    MemoryBitBehavioralContractTest
    Memory64Kx32Test
    MemoryBitStructuralContractTest
    Register32StructuralContractTest
    Register32CellArrayContractTest
    Register32BehavioralContractTest
    RegisterFile4x32Test
    RegisterFile32x32StructuralContractTest
    RegisterFile32x32BehavioralContractTest
    RegisterFile32x32BehavioralUnknownPolicyTest
    Memory4x32Test
    Memory32x32Test
    RewireUnpackTest
    RewirePackTest
    RewireSliceTest
    RewireZeroExtendTest
    RewireSignExtendTest
    RewireUnmappedHighTest
    RewireWidth5Test
    RewireUnmappedUnknownTest
    RewireValidationTest
    BitSplitter8Test
    BitJoiner8Test
    BitSplitter16Test
    BitJoiner32Test
    BitJoiner8UnknownTest
    ConstantValue1HighTest
    ConstantValue1LowTest
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
    ALU32StructuralContractTest
    ALU32BehavioralContractTest
    ALU32LowerLevelSliceTest
    RV32IControlFlowStructuralContractTest
    RV32IControlFlowBehavioralContractTest
    RV32IDecodeControlStructuralContractTest
    RV32IDecodeControlBehavioralContractTest
    RV32IExecutionStatusStructuralContractTest
    RV32IExecutionStatusBehavioralContractTest
    RV32IControlFlowUnitEquivalenceTest
    RV32IDecodeControlUnitEquivalenceTest
    RV32IRegisterFileEquivalenceTest
    ALU32EquivalenceTest
    RV32IExecutionControlStatusUnitEquivalenceTest
    RV32IBitPatternMatcherTest
    RV32IDecoderTest
    RV32IControlTest
    RV32IProgramLoaderTest
    RV32IInstructionOracleTest
    RV32IInstructionLockstepHarnessTest
    RV32IInstructionLockstepMismatchDetectionTest
    RV32ISingleCycleSystemSmokeTest
    RV32ISingleCycleSystemContractTest
    RV32ISingleCycleSystemProgram1Test
    RV32ISingleCycleSystemProgram2Test
    RV32ISingleCycleSystemProgram3Test
    RV32ISingleCycleSystemProgram4Test
    RV32ISingleCycleSystemProgram5Test
    RV32ISingleCycleSystemProgram6Test
    RV32ISingleCycleSystemProgram7Test
    RV32ISingleCycleSystemProgram8Test
    RV32ISingleCycleSystemProgram9Test
    RV32ISingleCycleSystemProgram10Test
    RV32ISingleCycleSystemProgram11Test
    RV32ISingleCycleSystemProgram12Test
    RV32ISingleCycleSystemProgram13Test
    RV32ISingleCycleSystemProgram14Test
    RV32ISingleCycleSystemProgram15Test
    RV32ISingleCycleSystemProgram16Test
    RV32IBalancedSystemProgram1Test
    RV32IBalancedSystemProgram2Test
    RV32IBalancedSystemProgram3Test
    RV32IBalancedSystemProgram4Test
    RV32IBalancedSystemProgram5Test
    RV32IBalancedSystemProgram6Test
    RV32IBalancedSystemProgram7Test
    RV32IBalancedSystemProgram8Test
    RV32IBalancedSystemProgram9Test
    RV32IBalancedSystemProgram10Test
    RV32IBalancedSystemProgram11Test
    RV32IBalancedSystemProgram12Test
    RV32IBalancedSystemProgram13Test
    RV32IBalancedSystemProgram14Test
    RV32IBalancedSystemProgram15Test
    RV32IBalancedSystemProgram16Test
    RV32IReferenceSystemContractTest
    RV32IReferenceSystemProgram1Test
    RV32IReferenceSystemProgram2Test
    RV32IReferenceSystemProgram3Test
    RV32IReferenceSystemProgram4Test
    RV32IReferenceSystemProgram5Test
    RV32IReferenceSystemProgram6Test
    RV32IReferenceSystemProgram7Test
    RV32IReferenceSystemProgram8Test
    RV32IReferenceSystemProgram9Test
    RV32IReferenceSystemProgram10Test
    RV32IReferenceSystemProgram11Test
    RV32IReferenceSystemProgram12Test
    RV32IReferenceSystemProgram13Test
    RV32IReferenceSystemProgram14Test
    RV32IReferenceSystemProgram15Test
    RV32IReferenceSystemProgram16Test
)

foreach(test_name ${SIMULATION_TESTS})
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})

    set(test_labels simulation)
    if(test_name MATCHES "Profile|Catalog|Balanced")
        list(APPEND test_labels profile)
    endif()
    if(test_name MATCHES "Equivalence")
        list(APPEND test_labels equivalence structural behavioral)
    endif()
    if(test_name MATCHES "Behavioral")
        list(APPEND test_labels behavioral)
    elseif(test_name MATCHES "Reference")
        list(APPEND test_labels reference)
    elseif(test_name MATCHES "Structural|CellArray|LowerLevel")
        list(APPEND test_labels structural)
    else()
        list(APPEND test_labels contract)
    endif()
    if(test_name MATCHES "RV32I|ALU32")
        list(APPEND test_labels rv32i)
    endif()
    if(test_name MATCHES "Balanced")
        list(APPEND test_labels mixed)
    endif()
    if(test_name MATCHES "System")
        list(APPEND test_labels system)
    endif()
    if(test_name MATCHES "WireTemplate|Simulator|BuildProfile|ComponentCatalog|Inventory|RecursiveBuild")
        list(APPEND test_labels framework)
    endif()
    if(test_name MATCHES "RV32I(SingleCycle|Balanced)SystemProgram|ALU32Equivalence")
        list(APPEND test_labels slow)
    endif()
    set_tests_properties(${test_name} PROPERTIES LABELS "${test_labels}")
endforeach()

add_test(
    NAME VisualizerModuleBindingsTest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/VisualizerModuleBindingsTest.py
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
