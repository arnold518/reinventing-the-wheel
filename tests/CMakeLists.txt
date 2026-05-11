add_executable(SimulationTestRunner SimulationTestRunner.cpp)
target_link_libraries(SimulationTestRunner PRIVATE SimulatorLib)

set(SIMULATION_TESTS
    FullCircuitTest
    WireTemplateTest
    NOTGateTest
    ANDGateTest
    ORGateTest
    XORGateTest
    NANDGateTest
    NORGateTest
    DFlipFlopTest
    ClockGeneratorTest
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
    ConstantValue1Low8TriggerTest
    ConstantValue8Test
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
    Mux2to1_8bitTest
    Mux4to1_8bitTest
    Mux8to1_8bitTest
    Mux16to1_8bitTest
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
    RV32IALU32Test
)

foreach(test_name ${SIMULATION_TESTS})
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
endforeach()
