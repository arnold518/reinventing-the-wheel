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
    src/analysis/CircuitTimingAnalyzer.cpp
    src/basic/Pin.cpp
    src/basic/Wire.cpp
    src/basic/TemplateInstantiations.cpp
    src/components/Component.cpp
    src/components/IOComponent.cpp
    src/components/BasicComponent.cpp
    src/components/ComponentBuilder.cpp
    src/components/WireBuilder.cpp
    src/components/selection/SelectionTypes.cpp
    src/components/selection/BuildManifest.cpp
    src/components/selection/BuildProfile.cpp
    src/components/selection/StandardProfiles.cpp
    src/components/selection/BuildContext.cpp
    src/components/selection/ComponentCatalog.cpp
    src/components/selection/BuiltinComponentCatalog.cpp
    src/modules/basic/Gate.cpp
    src/modules/basic/Logic8.cpp
    src/modules/basic/Mux.cpp
    src/modules/basic/Decoder.cpp
    src/modules/basic/Latch.cpp
    src/modules/basic/DFlipFlop.cpp
    src/modules/basic/ClockGenerator.cpp
    src/modules/memory/MemoryBitDirect.cpp
    src/modules/memory/Memory64Kx32.cpp
    src/modules/memory/Register32Direct.cpp
    src/modules/memory/RegisterFile32x32Direct.cpp
    src/modules/memory/MemoryBit.cpp
    src/modules/memory/Register32.cpp
    src/modules/memory/RegisterFile4x32.cpp
    src/modules/memory/RegisterFile32x32.cpp
    src/modules/memory/Memory4x32.cpp
    src/modules/memory/Memory32x32.cpp
    src/modules/rv32i/RV32IReferenceCore.cpp
    src/modules/rv32i/RV32IControlFlowDirect.cpp
    src/modules/rv32i/RV32IDecodeControlDirect.cpp
    src/modules/rv32i/RV32IExecutionStatusDirect.cpp
    src/modules/rv32i/RV32IBitPatternMatcher.cpp
    src/modules/rv32i/RV32IControlFlowUnit.cpp
    src/modules/rv32i/RV32IDecodeControlUnit.cpp
    src/modules/rv32i/RV32IExecutionControlStatusUnit.cpp
    src/modules/rv32i/RV32IPipelineRegisters.cpp
    src/modules/rv32i/RV32IPipelineControl.cpp
    src/modules/rv32i/RV32IPipelineStages.cpp
    src/modules/rv32i/RV32IFiveStageCoreDirect.cpp
    src/modules/rv32i/RV32IFiveStageCore.cpp
    src/modules/rv32i/RV32IBuildProfiles.cpp
    src/modules/rv32i/RV32ISingleCycleCore.cpp
    src/modules/rv32i/RV32ISingleCycleSystem.cpp
    src/modules/rv32i/RV32IReferenceSystem.cpp
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
    src/modules/composite/ALU32Direct.cpp
    src/modules/composite/Arithmetic8.cpp
    src/modules/composite/Comparator8.cpp
    src/modules/composite/Shifter8.cpp
    src/modules/composite/ZeroDetect8.cpp
    src/modules/composite/ALU8.cpp
    src/modules/utility/Rewire.cpp
    src/modules/utility/BitAdapter.cpp
    src/modules/utility/Constant.cpp
    src/rv32i/RV32IDecoder.cpp
    src/rv32i/RV32IControl.cpp
    src/rv32i/RV32IElfImage.cpp
    src/rv32i/RV32IProgram.cpp
    src/rv32i/RV32IFunctionalMemory.cpp
    src/rv32i/RV32IInstructionOracle.cpp
    src/simulator/Event.cpp
    src/simulator/Simulator.cpp
    src/tests/FullCircuitTest.cpp
    src/tests/ComponentTestModel.cpp
    src/tests/CorePrimitiveTests.cpp
    src/tests/ComponentSelectionTests.cpp
    src/tests/UtilityComponentTests.cpp
    src/tests/ArithmeticLogicTests.cpp
    src/tests/ALU32LowerLevelSliceTests.cpp
    src/tests/RV32IBlockStandaloneTests.cpp
    src/tests/RV32IDecoderTests.cpp
    src/tests/RV32IControlTests.cpp
    src/tests/RV32IElfTests.cpp
    src/tests/RV32IExternalValidationTests.cpp
    src/tests/RV32IProgramTests.cpp
    src/tests/RV32IInstructionOracleTests.cpp
    src/tests/RV32IInstructionLockstepTests.cpp
    src/tests/RV32IProgramRoot.cpp
    src/tests/RV32IFiveStageTests.cpp
    src/tests/RV32IPipelineBlockTests.cpp
    src/tests/RV32ISingleCycleTests.cpp
    src/tests/RV32IProgramScenarios.cpp
    src/tests/MemoryComponentTests.cpp
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
target_include_directories(SimulatorLib PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

# Test implementations are compiled into SimulatorLib. Keep their assertions
# active even when the surrounding build type defines NDEBUG.
target_compile_options(SimulatorLib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-UNDEBUG>
)
target_compile_definitions(SimulatorLib PRIVATE
    CIRCUITSIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
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

# Generic runner for committed smoke fixtures and explicitly generated ACT4
# ELF files. It is not registered as a separate test per fidelity.
add_executable(
    rv32i_validate_elf
    tools/rv32i-validation/RV32IElfValidationRunner.cpp
)
target_link_libraries(rv32i_validate_elf PRIVATE SimulatorLib)

add_executable(
    rv32i_benchmark_elf
    tools/rv32i-benchmarks/RV32IBenchmarkRunner.cpp
)
target_link_libraries(rv32i_benchmark_elf PRIVATE SimulatorLib)

add_executable(
    rv32i_performance
    tools/rv32i-performance/RV32IPerformanceRunner.cpp
)
target_link_libraries(rv32i_performance PRIVATE SimulatorLib)

# ===================================================================
# Target 4: CTest Integration
# ===================================================================
# Enable the CTest framework for this project.
enable_testing()

# Add the 'tests' subdirectory, which will contain test definitions.
add_subdirectory(tests)
