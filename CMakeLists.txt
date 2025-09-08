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
    src/components/Component.cpp
    src/components/IOComponent.cpp
    src/components/BasicComponent.cpp
    src/components/ComponentBuilder.cpp
    src/modules/basic/Gate.cpp
    src/modules/basic/DFlipFlop.cpp
    src/modules/basic/ClockGenerator.cpp
    src/simulator/Event.cpp
    src/simulator/Simulator.cpp
    src/tests/FullCircuitTest.cpp
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
    src/bindings/tests/FullCircuitTestBinding.cpp
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