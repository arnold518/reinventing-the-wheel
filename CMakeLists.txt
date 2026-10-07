cmake_minimum_required(VERSION 3.15)
project(CircuitSim LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CIRCUITSIM_BUILD_PYTHON "Build the native pybind11 visualizer backend" ON)
option(CIRCUITSIM_BUILD_TESTS "Build the native CTest suite" ON)
option(CIRCUITSIM_DEPLOY_PYTHON "Copy the native module to the frontend directories" ON)

if(EMSCRIPTEN)
    set(CIRCUITSIM_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
    set(CIRCUITSIM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

# --- Dependencies ---
if(CIRCUITSIM_BUILD_PYTHON)
    add_subdirectory(extern/pybind11)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
elseif(CIRCUITSIM_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

# --- Source File Definitions ---
set(CORE_LOGIC_SOURCES
    src/benchmark/Benchmark.cpp
    src/analysis/CircuitTimingAnalyzer.cpp
    src/analysis/SafeClockTiming.cpp
    src/analysis/MemoryTimingState.cpp
    src/analysis/RV32IExperiment.cpp
    src/analysis/RV32IBenchmarks.cpp
    src/analysis/RV32IWorkloads.cpp
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
    src/modules/memory/FlipFlopCellDirect.cpp
    src/modules/memory/MemoryStorage.cpp
    src/modules/memory/MemoryProtocol.cpp
    src/modules/memory/MemoryController.cpp
    src/modules/memory/MemoryContainer.cpp
    src/modules/memory/BehavioralMemoryEndpoint.cpp
    src/modules/memory/MemoryEndpointEngine.cpp
    src/modules/memory/MemoryEndpointCatalog.cpp
    src/modules/memory/FFMemoryCatalog.cpp
    src/modules/memory/FFMemory.cpp
    src/modules/memory/SRAM.cpp
    src/modules/memory/DigitalMemory.cpp
    src/modules/memory/DRAM.cpp
    src/modules/memory/ROM.cpp
    src/modules/memory/NORFlash.cpp
    src/modules/memory/SRAMCatalog.cpp
    src/tests/SRAMTests.cpp
    src/tests/MemoryDeviceTests.cpp
    src/tests/MemoryHelperTests.cpp
    src/tests/MemoryEndpointTests.cpp
    src/tests/FFMemoryTests.cpp
    src/modules/memory/RegisterDirect.cpp
    src/modules/memory/RegisterFile32x32Direct.cpp
    src/modules/memory/FlipFlopCell.cpp
    src/modules/memory/Register.cpp
    src/modules/memory/RegisterFile32x32.cpp
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
    src/modules/rv32i/RV32IPipelineControlDirect.cpp
    src/modules/rv32i/RV32IForwardingUnit.cpp
    src/modules/rv32i/RV32IMemoryAlignmentUnit.cpp
    src/modules/rv32i/RV32IPipelineRetirementUnit.cpp
    src/modules/rv32i/RV32IPipelineCoordinator.cpp
    src/modules/rv32i/RV32IPrediction.cpp
    src/modules/rv32i/RV32IPipelineStages.cpp
    src/modules/rv32i/RV32IPipelineStagesDirect.cpp
    src/modules/rv32i/RV32IPipelinePacking.cpp
    src/modules/rv32i/RV32IFetchStage.cpp
    src/modules/rv32i/RV32IDecodeStage.cpp
    src/modules/rv32i/RV32IExecuteStage.cpp
    src/modules/rv32i/RV32IMemoryStage.cpp
    src/modules/rv32i/RV32IWritebackStage.cpp
    src/modules/rv32i/RV32IFiveStageCoreDirect.cpp
    src/modules/rv32i/RV32IFiveStageCore.cpp
    src/modules/rv32i/RV32IBuildProfiles.cpp
    src/modules/rv32i/RV32ISingleCycleCore.cpp
    src/modules/rv32i/RV32ISingleCycleStages.cpp
    src/modules/rv32i/RV32ISingleCycleStagesDirect.cpp
    src/modules/rv32i/RV32IBaselinePipeline.cpp
    src/modules/rv32i/RV32IBaselinePipelineCore.cpp
    src/modules/rv32i/RV32IBaselinePipelineDirect.cpp
    src/modules/rv32i/RV32ISingleCycleSystem.cpp
    src/modules/rv32i/RV32IMemoryRequest.cpp
    src/modules/rv32i/RV32IMemoryLanes.cpp
    src/modules/rv32i/RV32IMemorySystem.cpp
    src/modules/rv32i/RV32IReferenceSystem.cpp
    src/modules/composite/HalfAdder.cpp
    src/modules/composite/FullAdder.cpp
    src/modules/composite/Adder8.cpp
    src/modules/composite/Adder32.cpp
    src/modules/composite/IncrementBy4.cpp
    src/modules/composite/AddSub32.cpp
    src/modules/composite/Logic32.cpp
    src/modules/composite/ZeroDetect32.cpp
    src/modules/composite/Shifter32.cpp
    src/modules/composite/ALU32.cpp
    src/modules/composite/ALU32Direct.cpp
    src/modules/composite/Arithmetic8.cpp
    src/modules/composite/Comparator8.cpp
    src/modules/composite/ZeroDetect8.cpp
    src/modules/composite/ALU8.cpp
    src/modules/composite/ALU8Behavioral.cpp
    src/modules/composite/ALU8ControlDecoder.cpp
    src/modules/composite/ALU8ResultSelector.cpp
    src/modules/composite/ALU32ControlDecoder.cpp
    src/modules/composite/ALU32CompareUnit.cpp
    src/modules/composite/ALU32ResultSelector.cpp
    src/modules/composite/ALU32FlagUnit.cpp
    src/modules/composite/AddSub8.cpp
    src/modules/composite/ArithmeticUnit8.cpp
    src/modules/composite/LogicUnit8.cpp
    src/modules/composite/BarrelShifter8.cpp
    src/modules/composite/CompareSelect8.cpp
    src/modules/composite/ALU8FlagUnit.cpp
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
    src/visualizer/VisualizerSession.cpp
    src/tests/FullCircuitTest.cpp
    src/tests/ComponentTestModel.cpp
    src/tests/CorePrimitiveTests.cpp
    src/tests/ComponentSelectionTests.cpp
    src/tests/UtilityComponentTests.cpp
    src/tests/ArithmeticLogicTests.cpp
    src/tests/ALU8Tests.cpp
    src/tests/ALU8BehavioralTests.cpp
    src/tests/ALUIntegrationTests.cpp
    src/tests/ALU32LowerLevelSliceTests.cpp
    src/tests/RV32IBlockStandaloneTests.cpp
    src/tests/RV32IMemoryAccessTests.cpp
    src/tests/RV32IDecoderTests.cpp
    src/tests/RV32IControlTests.cpp
    src/tests/RV32IElfTests.cpp
    src/tests/RV32IExternalValidationTests.cpp
    src/tests/RV32IProgramTests.cpp
    src/tests/RV32IInstructionOracleTests.cpp
    src/tests/RV32IInstructionLockstepTests.cpp
    src/tests/RV32IProgramRoot.cpp
    src/tests/RV32IFiveStageTests.cpp
    src/tests/RV32IPredictionTests.cpp
    src/tests/RV32IPipelineBlockTests.cpp
    src/tests/RV32ISingleCycleTests.cpp
    src/tests/RV32ISingleCycleStageTests.cpp
    src/tests/RV32IBaselinePipelineTests.cpp
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
if(CIRCUITSIM_BUILD_PYTHON)
    pybind11_add_module(circuit_backend
        ${BINDING_SOURCES}
    )

    # Link the Python module against our core logic library.
    target_link_libraries(circuit_backend PRIVATE SimulatorLib)

    # The include directories are inherited automatically from SimulatorLib.

    # Keep both frontends on the same pin-identity and binding contract.
    if(CIRCUITSIM_DEPLOY_PYTHON)
    add_custom_command(TARGET circuit_backend POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:circuit_backend>
            ${PROJECT_SOURCE_DIR}/visualizer-v2/$<TARGET_FILE_NAME:circuit_backend>
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:circuit_backend>
            ${PROJECT_SOURCE_DIR}/visualizer/$<TARGET_FILE_NAME:circuit_backend>
        COMMENT "Copying circuit_backend module to both visualizer directories..."
    )
    endif()
endif()

# ===================================================================
# Target 2b: Browser WebAssembly adapter
# ===================================================================
if(EMSCRIPTEN)
    add_executable(circuitsim_wasm src/wasm/WasmBindings.cpp)
    target_link_libraries(circuitsim_wasm PRIVATE SimulatorLib)
    target_compile_options(SimulatorLib PRIVATE -fexceptions)
    target_compile_options(circuitsim_wasm PRIVATE -O3 -fexceptions)
    target_link_options(circuitsim_wasm PRIVATE
        --bind
        -O3
        -fexceptions
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createCircuitSimModule
        -sENVIRONMENT=web,worker,node
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=268435456
        -sMAXIMUM_MEMORY=2147483648
        -sSTACK_SIZE=8388608
        -sDISABLE_EXCEPTION_CATCHING=0
        -sNO_EXIT_RUNTIME=1
        -sFILESYSTEM=0
    )
    set_target_properties(circuitsim_wasm PROPERTIES SUFFIX ".js")

    set(CIRCUITSIM_PAGES_DIR "${CMAKE_BINARY_DIR}/pages")
    set(CIRCUITSIM_PAGES_SIMULATOR_DIR "${CIRCUITSIM_PAGES_DIR}/simulator")
    add_custom_command(TARGET circuitsim_wasm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CIRCUITSIM_PAGES_SIMULATOR_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CIRCUITSIM_PAGES_SIMULATOR_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/visualizer-v2/static"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_SOURCE_DIR}/visualizer-v2/layout.json"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}/layout.json"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_SOURCE_DIR}/visualizer-v2/profiles.defaults.json"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}/profiles.json"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_SOURCE_DIR}/visualizer-v2/palette.json"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}/palette.json"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_SOURCE_DIR}/visualizer-v2/static/runtime-config.pages.js"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}/runtime-config.js"
        COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:circuitsim_wasm>"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}/circuitsim_wasm.js"
        COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE_DIR:circuitsim_wasm>/circuitsim_wasm.wasm"
            "${CIRCUITSIM_PAGES_SIMULATOR_DIR}/circuitsim_wasm.wasm"
        COMMENT "Packaging the static WebAssembly visualizer..."
    )
endif()

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

if(NOT EMSCRIPTEN)
    add_executable(rv32i_experiment tools/rv32i-performance/RV32IExperimentRunner.cpp)
    target_link_libraries(rv32i_experiment PRIVATE SimulatorLib)
    add_executable(rv32i_cycle_estimate tools/rv32i-benchmarks/RV32ICycleEstimate.cpp)
    target_link_libraries(rv32i_cycle_estimate PRIVATE SimulatorLib)
endif()

# ===================================================================
# Target 4: CTest Integration
# ===================================================================
# Enable the CTest framework for this project.
if(CIRCUITSIM_BUILD_TESTS)
    enable_testing()

    # Add the 'tests' subdirectory, which will contain test definitions.
    add_subdirectory(tests)
endif()
