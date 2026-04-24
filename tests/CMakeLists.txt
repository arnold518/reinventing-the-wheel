# ===================================================================
# Define all test source files in a single list.
# To add a new test, simply add its .cpp file here.
# ===================================================================
set(TEST_SOURCES
    FullCircuitTest.cpp
    FullAdderTest.cpp
    WireTemplateTest.cpp
    Adder8Test.cpp
    Logic8Test.cpp
    MuxTest.cpp
    Arithmetic8Test.cpp
    Comparator8Test.cpp
    Shifter8Test.cpp
    ZeroDetect8Test.cpp
    ALU8Test.cpp
)

# ===================================================================
# Loop over the source list and automatically create a test target
# for each file. This is the most maintainable approach.
# ===================================================================
foreach(test_source ${TEST_SOURCES})
    # 1. Get the base name of the file without the .cpp extension.
    #    For "TestFullCircuit.cpp", this will set test_name to "TestFullCircuit".
    get_filename_component(test_name ${test_source} NAME_WE)

    # 2. Create an executable with that name from the source file.
    add_executable(${test_name} ${test_source})

    # 3. Link the new test executable against our core logic library.
    target_link_libraries(${test_name} PRIVATE SimulatorLib)

    # 4. Register the executable with CTest. The test name will be the same
    #    as the executable name (e.g., "TestFullCircuit").
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
