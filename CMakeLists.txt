cmake_minimum_required(VERSION 3.15)
project(CircuitSim LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
add_subdirectory(extern/pybind11)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# --- Source File Definitions ---
set(CORE_LOGIC_SOURCES
    src/basic/Pin.cpp
    src/basic/Wire.cpp
    src/basic/TemplateInstantiations.cpp
    src/components/Component.cpp
    src/components/IOComponent.cpp
    src/components/BasicComponent.cpp
    src/components/ComponentBuilder.cpp
    src/components/WireBuilder.cpp
    src/modules/basic/Gate.cpp
    src/modules/basic/Logic8.cpp
    src/modules/basic/Mux.cpp
    src/modules/basic/DFlipFlop.cpp
    src/modules/basic/ClockGenerator.cpp
    src/modules/composite/HalfAdder.cpp
    src/modules/composite/FullAdder.cpp
    src/modules/composite/Adder8.cpp
    src/modules/composite/Adder32.cpp
    src/modules/composite/AddSub32.cpp
    src/modules/composite/Logic32.cpp
    src/modules/composite/ZeroDetect32.cpp
    src/modules/composite/Comparator32.cpp
    src/modules/composite/Shifter32.cpp
    src/modules/composite/ALU32.cpp
    src/modules/composite/Arithmetic8.cpp
    src/modules/composite/Comparator8.cpp
    src/modules/composite/Shifter8.cpp
    src/modules/composite/ZeroDetect8.cpp
    src/modules/composite/ALU8.cpp
    src/modules/utility/Rewire.cpp
    src/modules/utility/BitAdapter.cpp
    src/modules/utility/Constant.cpp
    src/simulator/Event.cpp
    src/simulator/Simulator.cpp
    src/simulator/TruthTableTest.cpp
    src/tests/FullCircuitTest.cpp
    src/tests/CorePrimitiveTests.cpp
    src/tests/UtilityComponentTests.cpp
    src/tests/ArithmeticLogicTests.cpp
    src/tests/RV32IALU32Tests.cpp
    src/tests/TestRegistry.cpp
)

set(BINDING_SOURCES
    src/bindings/export.cpp
    src/bindings/ComponentBinding.cpp
    src/bindings/IOComponentBinding.cpp
    src/bindings/BasicComponentBinding.cpp
    src/bindings/PinBinding.cpp
    src/bindings/WireBinding.cpp
    src/bindings/ModulesBinding.cpp
    src/bindings/SimulationTestBinding.cpp
    src/bindings/SimulatorBinding.cpp
    src/bindings/TestsBinding.cpp
)

# ===================================================================
# Target 1: The Core Logic Library (SimulatorLib)
# ===================================================================
# Define all core source files into a library. This is compiled once
# and then shared by the Python module, the C++ executable, and all tests.
add_library(SimulatorLib STATIC ${CORE_LOGIC_SOURCES})

# Make the include directory available to any target that links this library.
target_include_directories(SimulatorLib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# ===================================================================
# Target 2: The Python Module (`circuit_backend`)
# ===================================================================
# Create the Python module using all binding sources.
pybind11_add_module(circuit_backend
    ${BINDING_SOURCES}
)

# Link the Python module against our core logic library.
target_link_libraries(circuit_backend PRIVATE SimulatorLib)

# The include directories are inherited automatically from SimulatorLib.

# ===================================================================
# Automatic Python Module Deployment to Visualizer V2
# ===================================================================
# After building the Python module, automatically copy it to the visualizer-v2
# directory so it can be imported without manual copying.
add_custom_command(TARGET circuit_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:circuit_backend>
        ${PROJECT_SOURCE_DIR}/visualizer-v2/$<TARGET_FILE_NAME:circuit_backend>
    COMMENT "Copying circuit_backend module to visualizer-v2 directory..."
)

# ===================================================================
# Target 3: The C++ Executable (`sim`)
# ===================================================================
# This executable is for running manual simulations from main.cpp.
add_executable(sim src/main.cpp)

# Link the executable against our core logic library.
target_link_libraries(sim PRIVATE SimulatorLib)

# ===================================================================
# Target 4: CTest Integration
# ===================================================================
# Enable the CTest framework for this project.
enable_testing()

# Add the 'tests' subdirectory, which will contain test definitions.
add_subdirectory(tests)
