cmake_minimum_required(VERSION 3.15)
project(CircuitSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
# Add pybind11 from the 'extern' subdirectory.
add_subdirectory(extern/pybind11)
# Find Python for the module target.
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# --- Source File Definitions ---
# Define a variable for the core C++ logic shared by both targets.
set(CORE_LOGIC_SOURCES
    src/basic/Pin.cpp
    src/basic/Wire.cpp
    src/components/Component.cpp
    src/components/IOComponent.cpp
    src/components/BasicComponent.cpp
    src/components/ComponentBuilder.cpp
    src/modules/Gate.cpp
    src/modules/DFlipFlop.cpp
    src/modules/ClockGenerator.cpp
    src/simulator/Event.cpp
    src/simulator/Simulator.cpp
)

# Define a variable for the Python binding source files.
# set(BINDING_SOURCES
#     src/bindings/export.cpp
#     src/bindings/ComponentBinding.cpp
#     src/bindings/IOComponentBinding.cpp
#     src/bindings/BasicComponentBinding.cpp
#     # Add other binding files here...
# )


# ===================================================================
# Target 1: The Python Module (`circuit_backend`)
# ===================================================================

# This is the correct way to create a Python module.
# It automatically handles naming, linking, and Python-specific compiler flags.
# pybind11_add_module(circuit_backend
#     ${CORE_LOGIC_SOURCES}
#     ${BINDING_SOURCES}
# )

# Tell the module where to find your local headers.
# target_include_directories(circuit_backend PRIVATE
#     ${PROJECT_SOURCE_DIR}/include
# )


# ===================================================================
# Target 2: The C++ Executable (`sim`)
# ===================================================================

# Create the standard C++ executable.
add_executable(sim
    ${CORE_LOGIC_SOURCES}
    src/main.cpp
)

# Tell the executable where to find your local headers.
target_include_directories(sim PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)