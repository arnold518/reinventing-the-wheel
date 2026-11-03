cmake_minimum_required(VERSION 3.15)
project(CircuitSim LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CIRCUITSIM_BUILD_PYTHON "Build the native pybind11 visualizer backend" ON)
option(CIRCUITSIM_BUILD_TESTS "Build the native CTest suite" ON)
option(CIRCUITSIM_DEPLOY_PYTHON "Copy the native module to the frontend directories" ON)
option(CIRCUITSIM_SPLIT_DELAY_TABLES "Compile generated delay tables in smaller units (requires Python)" OFF)

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
    src/analysis/SafeClockTiming.cpp
    src/analysis/CircuitTimingAnalyzer.cpp
    src/analysis/TimingGraph.cpp
    src/analysis/DelayTwin.cpp
    src/analysis/GateListModel.cpp
    src/analysis/TimingEngine.cpp
    src/analysis/TimingCircuit.cpp
    src/analysis/MemoryTimingState.cpp
    src/analysis/RV32IExperiment.cpp
    src/analysis/RV32IBenchmarks.cpp
    src/analysis/RV32IWorkloads.cpp
    src/basic/Pin.cpp
    src/basic/Wire.cpp
    src/components/Component.cpp
    src/components/IOComponent.cpp
    src/components/BasicComponent.cpp
    src/components/GateListPartBehavioral.cpp
    src/components/TimingProfile.cpp
    src/components/ComponentBuilder.cpp
    src/components/selection/SelectionTypes.cpp
    src/components/selection/BuildManifest.cpp
    src/components/selection/BuildProfile.cpp
    src/components/selection/StandardProfiles.cpp
    src/components/selection/BuildContext.cpp
    src/components/selection/ComponentCatalog.cpp
    src/components/selection/GateListCompact.cpp
    src/components/selection/ProvenPartDelays.cpp
    src/components/selection/ProvenPartDelaysUnit.cpp
    src/components/selection/ProvenPartDelaysEmbedded.cpp
    src/components/selection/ProvenPartDelaysDesktop.cpp
    src/components/selection/BuiltinComponentCatalog.cpp
    src/modules/basic/Gate.cpp
    src/modules/basic/Mux.cpp
    src/modules/basic/WordGate.cpp
    src/modules/basic/Decoder.cpp
    src/modules/basic/Latch.cpp
    src/modules/basic/DFlipFlop.cpp
    src/modules/basic/ClockGenerator.cpp
    src/modules/memory/FlipFlopCellBehavioral.cpp
    src/modules/memory/MemoryStorage.cpp
    src/modules/memory/MemoryProtocol.cpp
    src/modules/memory/MemoryController.cpp
    src/modules/memory/MemoryTimingParts.cpp
    src/modules/memory/MemorySequencers.cpp
    src/modules/memory/NativeMemoryDevice.cpp
    src/modules/memory/BehavioralMemoryEndpoint.cpp
    src/modules/memory/MemoryEndpointEngine.cpp
    src/modules/memory/MemoryEndpointCatalog.cpp
    src/modules/memory/Cache.cpp
    src/modules/memory/TwoPortMemoryArbiter.cpp
    src/modules/memory/FFMemoryCatalog.cpp
    src/modules/memory/FFMemory.cpp
    src/modules/memory/MemoryArrayBlocks.cpp
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
    src/tests/TwoPortMemoryArbiterTests.cpp
    src/tests/CacheTests.cpp
    src/tests/FFMemoryTests.cpp
    src/modules/storage/RegisterBehavioral.cpp
    src/modules/memory/FlipFlopCell.cpp
    src/modules/storage/ClockedStateBit.cpp
    src/modules/storage/RegisterFile.cpp
    src/modules/storage/RegisterFileBehavioral.cpp
    src/modules/storage/Register.cpp
    src/modules/rv32i/RV32IReferenceCore.cpp
    src/modules/rv32i/RV32IDecodeControlBehavioral.cpp
    src/modules/rv32i/RV32IExecutionStatusBehavioral.cpp
    src/modules/rv32i/RV32IBitPatternMatcher.cpp
    src/modules/rv32i/RV32IDecodeControlUnit.cpp
    src/modules/rv32i/RV32IExecutionControlStatusUnit.cpp
    src/modules/rv32i/RV32IPipelineRegisters.cpp
    src/modules/rv32i/RV32IPipelineControl.cpp
    src/modules/rv32i/RV32IPipelineControlBehavioral.cpp
    src/modules/rv32i/RV32IForwardingUnit.cpp
    src/modules/rv32i/RV32IMemoryAlignmentUnit.cpp
    src/modules/rv32i/RV32IPipelineRetirementUnit.cpp
    src/modules/rv32i/RV32IPipelineCoordinator.cpp
    src/modules/rv32i/RV32IPrediction.cpp
    src/modules/rv32i/RV32IPipelineStages.cpp
    src/modules/rv32i/RV32IPipelineStagesBehavioral.cpp
    src/modules/rv32i/RV32IPipelinePacking.cpp
    src/modules/rv32i/RV32IFetchStage.cpp
    src/modules/rv32i/RV32IDecodeStage.cpp
    src/modules/rv32i/RV32IExecuteStage.cpp
    src/modules/rv32i/RV32IMemoryStage.cpp
    src/modules/rv32i/RV32IWritebackStage.cpp
    src/modules/rv32i/RV32IFiveStageCoreBehavioral.cpp
    src/modules/rv32i/RV32IFiveStageCore.cpp
    src/modules/rv32i/RV32IBuildProfiles.cpp
    src/modules/rv32i/RV32ISingleCycleCore.cpp
    src/modules/rv32i/RV32ISingleCycleStages.cpp
    src/modules/rv32i/RV32ISingleCycleStagesBehavioral.cpp
    src/modules/rv32i/RV32IBaselinePipeline.cpp
    src/modules/rv32i/RV32IBaselinePipelineCore.cpp
    src/modules/rv32i/RV32IBaselinePipelineBehavioral.cpp
    src/modules/rv32i/RV32ISingleCycleSystem.cpp
    src/modules/rv32i/RV32IMemoryRequest.cpp
    src/modules/rv32i/RV32IMemoryLanes.cpp
    src/modules/rv32i/RV32IMemorySystem.cpp
    src/modules/rv32i/RV32IReferenceSystem.cpp
    src/modules/composite/HalfAdder.cpp
    src/modules/composite/FullAdder.cpp
    src/modules/composite/ParallelPrefixAdder.cpp
    src/modules/composite/Incrementer.cpp
    src/modules/composite/WordControl.cpp
    src/modules/composite/WordZero.cpp
    src/modules/composite/WordBehavioral.cpp
    src/modules/composite/WordDatapath.cpp
    src/modules/composite/WordExtended.cpp
    src/modules/composite/RippleCarryAdder.cpp
    src/modules/rv32i/RV32IALU.cpp
    src/modules/rv32i/RV32IALUAddSub.cpp
    src/modules/rv32i/RV32IALULogic.cpp
    src/modules/rv32i/RV32IALUShifter.cpp
    src/modules/rv32i/RV32IALUCompare.cpp
    src/modules/rv32i/RV32IALUResultSelect.cpp
    src/modules/utility/Rewire.cpp
    src/modules/utility/BitAdapter.cpp
    src/modules/utility/Constant.cpp
    src/rv32im/RV32IMInstruction.cpp
    src/modules/rv32im/MulDiv.cpp
    src/modules/rv32im/MulDivBehavioral.cpp
    src/modules/rv32im/RV32IMCore.cpp
    src/modules/rv32im/RV32IMCoreBehavioral.cpp
    src/modules/rv32i/RV32IFiveStageBlueprint.cpp
    src/modules/rv32i/RV32IDecodeBlueprint.cpp
    src/modules/device/DeviceBus.cpp
    src/modules/device/DeviceState.cpp
    src/modules/device/DeviceDisplay.cpp
    src/tests/WordALUTests.cpp
    src/tests/SettledSweep.cpp
    src/tests/ArithmeticSweepTests.cpp
    src/tests/PipelineControlSweepTests.cpp
    src/tests/WordGateTests.cpp
    src/tests/CompactFormEquivalenceTests.cpp
    src/tests/SystemTests.cpp
    src/system/Json.cpp
    src/system/SystemDescription.cpp
    src/modules/device/RegisterFrontEnd.cpp
    src/modules/device/DeviceRegisters.cpp
    src/modules/device/LedPanel.cpp
    src/system/Bus.cpp
    src/system/Bridge.cpp
    src/system/BusPortChecker.cpp
    src/system/MemorySubsystem.cpp
    src/system/ComputerSystem.cpp
    src/system/GuestHeader.cpp
    src/tests/RV32IMPhase3Tests.cpp
    src/tests/RV32IMPhase3Scenarios.cpp
    src/tests/RV32IMProgramTests.cpp
    src/tests/SimulationTimeTests.cpp
    src/tests/RV32IMRecordedTests.cpp
    src/tests/GuestScenarioTests.cpp
    src/runtime/RecordedGuest.cpp
    src/components/selection/FidelityPresets.cpp
    src/runtime/RecordedGuestFixtures.cpp
    src/rv32i/RV32IDecoder.cpp
    src/rv32i/RV32IControl.cpp
    src/rv32i/RV32IElfImage.cpp
    src/rv32i/RV32IProgram.cpp
    src/rv32i/RV32IFunctionalMemory.cpp
    src/rv32i/RV32IInstructionOracle.cpp
    src/simulator/Event.cpp
    src/simulator/Simulator.cpp
    src/visualizer/DeviceInspector.cpp
    src/visualizer/VisualizerSession.cpp
    src/tests/FullCircuitTest.cpp
    src/tests/ComponentTestModel.cpp
    src/tests/CorePrimitiveTests.cpp
    src/tests/ComponentSelectionTests.cpp
    src/tests/UtilityComponentTests.cpp
    src/tests/ArithmeticLogicTests.cpp
    src/tests/ALUUnitTests.cpp
    src/tests/WordFidelityTests.cpp
    src/tests/ALUIntegrationTests.cpp
    src/tests/RV32IALUTests.cpp
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
    src/bindings/BasicModulesBinding.cpp
    src/bindings/StorageModulesBinding.cpp
    src/bindings/PipelineModulesBinding.cpp
    src/bindings/CpuModulesBinding.cpp
    src/bindings/ArithmeticModulesBinding.cpp
    src/bindings/UtilityModulesBinding.cpp
    src/bindings/SimulationTestBinding.cpp
    src/bindings/SimulatorBinding.cpp
    src/bindings/TestsBinding.cpp
    src/bindings/RecordedGuestBinding.cpp
)

# Optional compiler-memory reduction. Preserve the canonical generated tables
# and unsplit source path; only whole initializer entries are partitioned.
if(CIRCUITSIM_SPLIT_DELAY_TABLES)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(_delay_source "${PROJECT_SOURCE_DIR}/src/components/selection")
    set(_delay_output "${PROJECT_BINARY_DIR}/generated-part-delays")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/tools/build/split-delay-tables.py"
        "${_delay_source}/ProvenPartDelays.generated.inc"
        "${_delay_source}/ProvenPartDelays.embedded.generated.inc"
        "${_delay_source}/ProvenPartDelays.desktop.generated.inc")
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/build/split-delay-tables.py" --output "${_delay_output}"
        RESULT_VARIABLE _delay_result)
    if(NOT _delay_result EQUAL 0)
        message(FATAL_ERROR "Cannot partition the generated delay tables")
    endif()
    foreach(_profile Unit Embedded Desktop)
        list(REMOVE_ITEM CORE_LOGIC_SOURCES "src/components/selection/ProvenPartDelays${_profile}.cpp")
        list(APPEND CORE_LOGIC_SOURCES "${_delay_output}/provenPartDelays${_profile}.cpp")
        foreach(_chunk RANGE 0 7)
            list(APPEND CORE_LOGIC_SOURCES "${_delay_output}/provenPartDelays${_profile}Chunk${_chunk}.cpp")
        endforeach()
    endforeach()
endif()

# ===================================================================
# Target 1: The Core Logic Library (SimulatorLib)
# ===================================================================
# Define all core source files into a library. This is compiled once
# and then shared by the Python module, the C++ executable, and all tests.
# Built-in system descriptions (systems/*.json) are embedded as raw strings,
# so native, Python and WebAssembly builds read the same files.
file(GLOB CIRCUITSIM_SYSTEM_FILES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/systems/*.json)
list(SORT CIRCUITSIM_SYSTEM_FILES)
set(CIRCUITSIM_SYSTEMS_BODY "")
foreach(system_file ${CIRCUITSIM_SYSTEM_FILES})
    get_filename_component(system_name ${system_file} NAME_WE)
    file(READ ${system_file} system_text)
    string(APPEND CIRCUITSIM_SYSTEMS_BODY "        {\"${system_name}\", R\"circuitsim_json(${system_text})circuitsim_json\"},\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${system_file})
endforeach()
file(WRITE ${PROJECT_BINARY_DIR}/generated/BuiltinSystems.generated.hpp.in
"// Generated by CMake from systems/*.json. Do not edit by hand.
#pragma once
#include <string>
#include <utility>
#include <vector>
namespace circuit::system::generated {
inline const std::vector<std::pair<std::string, std::string>>& builtinSystems() {
    static const std::vector<std::pair<std::string, std::string>> systems{
${CIRCUITSIM_SYSTEMS_BODY}    };
    return systems;
}
}
")
configure_file(${PROJECT_BINARY_DIR}/generated/BuiltinSystems.generated.hpp.in
               ${PROJECT_BINARY_DIR}/generated/BuiltinSystems.generated.hpp COPYONLY)

add_library(SimulatorLib STATIC ${CORE_LOGIC_SOURCES})

# Make the include directory available to any target that links this library.
target_include_directories(SimulatorLib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(SimulatorLib PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/generated
)

# The event loop, wire fanout and primitive evaluation dominate CPU time. When
# the release flags use -O1 (kept for compiler memory on this host), these
# small files are compiled at -O2; builds with other levels keep theirs.
option(CIRCUITSIM_HOT_PATH_O2 "Compile the engine's hot files at -O2 when the release flags use -O1" ON)
if(CIRCUITSIM_HOT_PATH_O2 AND NOT EMSCRIPTEN AND CMAKE_CXX_FLAGS_RELEASE MATCHES "-O1")
    set_source_files_properties(
        src/simulator/Simulator.cpp
        src/simulator/Event.cpp
        src/basic/Pin.cpp
        src/basic/Wire.cpp
        src/components/BasicComponent.cpp
        src/components/IOComponent.cpp
        src/modules/basic/Gate.cpp
        src/modules/utility/Rewire.cpp
        src/analysis/DelayTwin.cpp
        TARGET_DIRECTORY SimulatorLib
        PROPERTIES COMPILE_OPTIONS "-O2")
endif()

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
    set(CIRCUITSIM_WASM_LINK_OPTIMIZATION "3" CACHE STRING
        "Emscripten final link optimization level (0, 1, 2, 3, s or z)")
    set_property(CACHE CIRCUITSIM_WASM_LINK_OPTIMIZATION PROPERTY STRINGS 0 1 2 3 s z)
    if(NOT CIRCUITSIM_WASM_LINK_OPTIMIZATION MATCHES "^[0123sz]$")
        message(FATAL_ERROR "Invalid CIRCUITSIM_WASM_LINK_OPTIMIZATION")
    endif()
    add_executable(circuitsim_memory_startup EXCLUDE_FROM_ALL tests/cpp/MemoryProvisioningStartupTest.cpp)
    target_link_libraries(circuitsim_memory_startup PRIVATE SimulatorLib)
    target_compile_options(circuitsim_memory_startup PRIVATE -fexceptions)
    target_link_options(circuitsim_memory_startup PRIVATE -fexceptions -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0 -sEXIT_RUNTIME=1 -sFILESYSTEM=0)
    set_target_properties(circuitsim_memory_startup PROPERTIES SUFFIX ".js")
    add_executable(circuitsim_system_timing EXCLUDE_FROM_ALL tests/cpp/ComputerSystemTimingTest.cpp)
    target_link_libraries(circuitsim_system_timing PRIVATE SimulatorLib)
    target_compile_options(circuitsim_system_timing PRIVATE -fexceptions)
    target_link_options(circuitsim_system_timing PRIVATE -fexceptions -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0 -sEXIT_RUNTIME=1 -sFILESYSTEM=0)
    set_target_properties(circuitsim_system_timing PROPERTIES SUFFIX ".js")
    add_executable(circuitsim_gate_list_test EXCLUDE_FROM_ALL tests/cpp/GateListObservationTest.cpp)
    target_link_libraries(circuitsim_gate_list_test PRIVATE SimulatorLib)
    target_compile_options(circuitsim_gate_list_test PRIVATE -fexceptions)
    target_link_options(circuitsim_gate_list_test PRIVATE -fexceptions -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0 -sEXIT_RUNTIME=1 -sFILESYSTEM=0)
    set_target_properties(circuitsim_gate_list_test PROPERTIES SUFFIX ".js")
    add_executable(circuitsim_delay_snapshot EXCLUDE_FROM_ALL tests/cpp/DelayTableSnapshot.cpp)
    target_link_libraries(circuitsim_delay_snapshot PRIVATE SimulatorLib)
    target_compile_options(circuitsim_delay_snapshot PRIVATE -fexceptions)
    target_link_options(circuitsim_delay_snapshot PRIVATE -fexceptions -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0 -sEXIT_RUNTIME=1 -sFILESYSTEM=0)
    set_target_properties(circuitsim_delay_snapshot PROPERTIES SUFFIX ".js")
    add_executable(circuitsim_recorded_guest EXCLUDE_FROM_ALL tests/cpp/RecordedGuestProbe.cpp)
    target_link_libraries(circuitsim_recorded_guest PRIVATE SimulatorLib)
    target_compile_options(circuitsim_recorded_guest PRIVATE -fexceptions)
    target_link_options(circuitsim_recorded_guest PRIVATE
        -fexceptions -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0
        -sEXIT_RUNTIME=1 -sFILESYSTEM=0)
    set_target_properties(circuitsim_recorded_guest PROPERTIES SUFFIX ".js")
    add_executable(circuitsim_verification EXCLUDE_FROM_ALL tests/cpp/SimulationTestRunner.cpp)
    target_link_libraries(circuitsim_verification PRIVATE SimulatorLib)
    target_compile_options(circuitsim_verification PRIVATE -fexceptions)
    target_link_options(circuitsim_verification PRIVATE
        -fexceptions -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=8388608 -sDISABLE_EXCEPTION_CATCHING=0
        -sEXIT_RUNTIME=1 -sFILESYSTEM=0)
    set_target_properties(circuitsim_verification PROPERTIES SUFFIX ".js")
    add_executable(circuitsim_wasm src/wasm/WasmBindings.cpp)
    target_link_libraries(circuitsim_wasm PRIVATE SimulatorLib)
    target_compile_options(SimulatorLib PRIVATE -fexceptions)
    target_compile_options(circuitsim_wasm PRIVATE -O3 -fexceptions)
    target_link_options(circuitsim_wasm PRIVATE
        --bind
        -O${CIRCUITSIM_WASM_LINK_OPTIMIZATION}
        -fexceptions
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createCircuitSimModule
        -sENVIRONMENT=web,worker,node
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=268435456
        -sMAXIMUM_MEMORY=2147483648
        -sEXCEPTION_STACK_TRACES=1
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
    add_executable(part_delay_tables tools/timing/PartDelayTables.cpp)
    target_link_libraries(part_delay_tables PRIVATE SimulatorLib)
    add_executable(system_header tools/system/SystemHeader.cpp)
    target_link_libraries(system_header PRIVATE SimulatorLib)
    add_executable(system_experiment tools/system/SystemExperiment.cpp)
    target_link_libraries(system_experiment PRIVATE SimulatorLib)
    add_executable(documentation_timing_data tools/timing/DocumentationTimingData.cpp)
    target_link_libraries(documentation_timing_data PRIVATE SimulatorLib)
    add_executable(delay_estimate_experiment tools/timing/DelayEstimateExperiment.cpp)
    target_link_libraries(delay_estimate_experiment PRIVATE SimulatorLib)
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
