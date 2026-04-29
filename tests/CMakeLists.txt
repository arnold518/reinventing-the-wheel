add_executable(SimulationTestRunner SimulationTestRunner.cpp)
target_link_libraries(SimulationTestRunner PRIVATE SimulatorLib)

set(SIMULATION_TESTS
    FullCircuitTest
    WireTemplateTest
    GateTest
    DFlipFlopTest
    ClockGeneratorTest
    RewireTest
    BitAdapterTest
    ConstantValueTest
    HalfAdderTest
    FullAdderTest
    Logic8Test
    MuxTest
    Adder8Test
    Arithmetic8Test
    Comparator8Test
    Shifter8Test
    ZeroDetect8Test
    ALU8Test
)

foreach(test_name ${SIMULATION_TESTS})
    add_test(NAME ${test_name} COMMAND SimulationTestRunner ${test_name})
endforeach()
