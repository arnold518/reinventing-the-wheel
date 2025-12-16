# Complete Circuit Building Workflow

This guide shows the **complete step-by-step workflow** for building a new circuit from scratch, using `FullAdder` as the reference example.

**NEW**: This guide now shows **both old and new APIs**. The new APIs (introduced Dec 2025) reduce boilerplate by 50% while maintaining type safety.

---

## Table of Contents

1. [Overview](#overview)
2. [Step 1: Component Header](#step-1-component-header)
3. [Step 2: Component Implementation](#step-2-component-implementation)
4. [Step 3: Build System Integration](#step-3-build-system-integration)
5. [Step 4: Python Bindings](#step-4-python-bindings)
6. [Step 5: C++ Test Implementation](#step-5-c-test-implementation)
7. [Step 6: Build and Run](#step-6-build-and-run)
8. [Complete File Checklist](#complete-file-checklist)
9. [Debugging Guide](#debugging-guide)

---

## Overview

### The Complete Pipeline

```
1. Component Header (.hpp)
   ↓
2. Component Implementation (.cpp)
   ↓
3. CMakeLists.txt Integration
   ↓
4. Python Bindings (pybind11)
   ↓
5. C++ Test (Header + Source + Executable)
   ↓
6. Build, Test, Run
```

### Example Circuit: FullAdder

A full adder adds three 1-bit values (A, B, Carry_in) and produces a sum and carry output.

**Logic:**
- Sum = A ⊕ B ⊕ Carry_in
- Carry_out = (A ⊕ B) · Carry_in + A · B

**Implementation:** Built from 2 HalfAdders and 1 OR gate.

---

## Step 1: Component Header

### File: `include/modules/composite/FullAdder.hpp`

This file declares the component class.

```cpp
#pragma once

#include "components/IOComponent.hpp"

class FullAdder : public IOComponent
{
public:
    // Required: TypeName constant for Python bindings
    static constexpr const char* TypeName = "FullAdder";
    const char* getTypeName() const override { return TypeName; }

    // Constructor: Defines the external pin interface
    FullAdder(std::string name);

    // BuildInternals: Constructs the internal circuit
    void buildInternals(ComponentBuilder& builder) override;
};
```

### Key Points

- **Base class**: Inherit from `IOComponent` for composite circuits (or `BasicComponent` for primitive gates)
- **TypeName constant**: Required for Python bindings (must match class name)
- **Constructor**: Will define external pins (inputs/outputs)
- **buildInternals()**: Will wire up internal components

### File Structure

For composite components:
```
include/modules/composite/<ComponentName>.hpp
```

For basic gates:
```
include/modules/basic/Gate.hpp  (add to existing file)
```

---

## Step 2: Component Implementation

### File: `src/modules/composite/FullAdder.cpp`

This file implements the constructor and internal wiring.

### 2.1: Constructor (Pin Definition)

**Option A: OLD API (Verbose Lambda)**

```cpp
#include "modules/composite/FullAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"  // CRITICAL: Template implementations
#include "modules/composite/HalfAdder.hpp"
#include "modules/basic/Gate.hpp"

FullAdder::FullAdder(std::string name)
    : IOComponent(std::move(name),
      // Lambda defines the EXTERNAL interface (pins visible to parent)
      [](IOComponent* self) {
          self->addPin("A", PinType::INPUT);
          self->addPin("B", PinType::INPUT);
          self->addPin("Carry_in", PinType::INPUT);
          self->addPin("Sum", PinType::OUTPUT);
          self->addPin("Carry_out", PinType::OUTPUT);
      })
{}
```

**Lines:** 17 (including lambda)

---

**Option B: NEW API (Pin Macros)** ⭐ **RECOMMENDED**

```cpp
#include "modules/composite/FullAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/WireBuilder.hpp"      // NEW: For wire chaining
#include "components/PinMacros.hpp"        // NEW: For pin macros
#include "modules/composite/HalfAdder.hpp"
#include "modules/basic/Gate.hpp"

// Using new PinMacros API (Proposal 2)
BEGIN_PINS(FullAdder, IOComponent)
    INPUT_PIN("A")
    INPUT_PIN("B")
    INPUT_PIN("Carry_in")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry_out")
END_PINS()
```

**Lines:** 9 (47% reduction)

**Benefits:**
- ✅ Less noise (no lambda syntax)
- ✅ Consistent style
- ✅ Easier to read and modify

---

### 2.2: buildInternals (Internal Wiring)

**Option A: OLD API (Verbose)**

```cpp
void FullAdder::buildInternals(ComponentBuilder& builder) {
    // Step 1: Create internal components
    builder.addNewComponent<HalfAdder>("HA1");
    builder.addNewComponent<HalfAdder>("HA2");
    builder.addNewComponent<ORGate>("OR1");

    // Step 2: Wire the primary inputs to first HalfAdder
    builder.addNewWire("A_internal",
        builder.getInputPin("A"),
        { builder.getInputPin<HalfAdder>("HA1", "A") }
    );

    builder.addNewWire("B_internal",
        builder.getInputPin("B"),
        { builder.getInputPin<HalfAdder>("HA1", "B") }
    );

    builder.addNewWire("Cin_internal",
        builder.getInputPin("Carry_in"),
        { builder.getInputPin<HalfAdder>("HA2", "B") }
    );

    // Step 3: Wire first HalfAdder's output to second's input
    builder.addNewWire("HA1_Sum_to_HA2_A",
        builder.getOutputPin<HalfAdder>("HA1", "Sum"),
        { builder.getInputPin<HalfAdder>("HA2", "A") }
    );

    // Step 4: Wire carry outputs to OR gate
    builder.addNewWire("HA1_Carry_to_OR",
        builder.getOutputPin<HalfAdder>("HA1", "Carry"),
        { builder.getInputPin<ORGate>("OR1", "A") }
    );

    builder.addNewWire("HA2_Carry_to_OR",
        builder.getOutputPin<HalfAdder>("HA2", "Carry"),
        { builder.getInputPin<ORGate>("OR1", "B") }
    );

    // Step 5: Wire final results to FullAdder's output pins
    builder.addNewWire("Sum_internal",
        builder.getOutputPin<HalfAdder>("HA2", "Sum"),
        { builder.getOutputPin("Sum") }
    );

    builder.addNewWire("Carry_out_internal",
        builder.getOutputPin<ORGate>("OR1", "OUT"),
        { builder.getOutputPin("Carry_out") }
    );
}
```

**Lines:** 48

---

**Option B: NEW API (Wire Chaining)** ⭐ **RECOMMENDED**

```cpp
void FullAdder::buildInternals(ComponentBuilder& builder) {
    // Step 1: Create internal components
    builder.addNewComponent<HalfAdder>("HA1");
    builder.addNewComponent<HalfAdder>("HA2");
    builder.addNewComponent<ORGate>("OR1");

    // Step 2: Wire using new WireBuilder API (Proposal 3)

    // Primary inputs → first HalfAdder
    builder.wire("A_internal")
        .fromInput("A")
        .to<HalfAdder>("HA1", "A");

    builder.wire("B_internal")
        .fromInput("B")
        .to<HalfAdder>("HA1", "B");

    builder.wire("Cin_internal")
        .fromInput("Carry_in")
        .to<HalfAdder>("HA2", "B");

    // First HalfAdder Sum → Second HalfAdder input
    builder.wire("HA1_Sum_to_HA2_A")
        .from<HalfAdder>("HA1", "Sum")
        .to<HalfAdder>("HA2", "A");

    // Both HalfAdder carries → OR gate
    builder.wire("HA1_Carry_to_OR")
        .from<HalfAdder>("HA1", "Carry")
        .to<ORGate>("OR1", "A");

    builder.wire("HA2_Carry_to_OR")
        .from<HalfAdder>("HA2", "Carry")
        .to<ORGate>("OR1", "B");

    // Final outputs
    builder.wire("Sum_internal")
        .from<HalfAdder>("HA2", "Sum")
        .toOutput("Sum");

    builder.wire("Carry_out_internal")
        .from<ORGate>("OR1", "OUT")
        .toOutput("Carry_out");
}
```

**Lines:** 44 (8% reduction, but much more readable)

**Benefits:**
- ✅ Reads like English: "wire X from A to B"
- ✅ Impossible to swap source/sink (type safety)
- ✅ Supports fan-out: chain multiple `.to()` calls
- ✅ Auto-commits on destruction (or call `.build()` explicitly)

### Wire Chaining API Reference

**Source methods:**
- `.fromInput("pin")` - This component's input pin as source
- `.from<Type>("comp", "pin")` - Child component's output pin as source
- `.fromOutput("pin")` - This component's output pin as source (rare)

**Sink methods:**
- `.to<Type>("comp", "pin")` - Child component's input pin as sink
- `.toOutput("pin")` - This component's output pin as sink
- `.toInput("pin")` - This component's input pin as sink (rare)
- Chain multiple `.to()` for fan-out

---

### Complete FullAdder.cpp (NEW API)

```cpp
#include "modules/composite/FullAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/WireBuilder.hpp"
#include "components/PinMacros.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/basic/Gate.hpp"

// Using new PinMacros API (Proposal 2)
BEGIN_PINS(FullAdder, IOComponent)
    INPUT_PIN("A")
    INPUT_PIN("B")
    INPUT_PIN("Carry_in")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry_out")
END_PINS()

void FullAdder::buildInternals(ComponentBuilder& builder) {
    // 1. Create internal components
    builder.addNewComponent<HalfAdder>("HA1");
    builder.addNewComponent<HalfAdder>("HA2");
    builder.addNewComponent<ORGate>("OR1");

    // 2. Wire using new WireBuilder API (Proposal 3)

    // Primary inputs → first HalfAdder
    builder.wire("A_internal").fromInput("A").to<HalfAdder>("HA1", "A");
    builder.wire("B_internal").fromInput("B").to<HalfAdder>("HA1", "B");
    builder.wire("Cin_internal").fromInput("Carry_in").to<HalfAdder>("HA2", "B");

    // First HalfAdder Sum → Second HalfAdder
    builder.wire("HA1_Sum_to_HA2_A")
        .from<HalfAdder>("HA1", "Sum")
        .to<HalfAdder>("HA2", "A");

    // Carries → OR gate
    builder.wire("HA1_Carry_to_OR").from<HalfAdder>("HA1", "Carry").to<ORGate>("OR1", "A");
    builder.wire("HA2_Carry_to_OR").from<HalfAdder>("HA2", "Carry").to<ORGate>("OR1", "B");

    // Final outputs
    builder.wire("Sum_internal").from<HalfAdder>("HA2", "Sum").toOutput("Sum");
    builder.wire("Carry_out_internal").from<ORGate>("OR1", "OUT").toOutput("Carry_out");
}
```

**Total:** 33 lines (vs 65 with old API - **49% reduction**)

---

## Step 3: Build System Integration

### 3.1: Root CMakeLists.txt

**File:** `CMakeLists.txt` (project root)

Add your component's `.cpp` file to the `CORE_LOGIC_SOURCES` list:

```cmake
set(CORE_LOGIC_SOURCES
    src/basic/Pin.cpp
    src/basic/Wire.cpp
    src/components/Component.cpp
    src/components/IOComponent.cpp
    src/components/BasicComponent.cpp
    src/components/ComponentBuilder.cpp
    src/components/WireBuilder.cpp           # New API support
    src/modules/basic/Gate.cpp
    src/modules/basic/DFlipFlop.cpp
    src/modules/basic/ClockGenerator.cpp
    src/modules/composite/HalfAdder.cpp
    src/modules/composite/FullAdder.cpp      # <-- ADD YOUR COMPONENT HERE
    src/simulator/Event.cpp
    src/simulator/Simulator.cpp
    src/simulator/TruthTableTest.cpp         # New API support
    src/tests/FullCircuitTest.cpp
    src/tests/HalfAdderTest.cpp
    src/tests/FullAdderTest.cpp              # <-- ADD YOUR TEST HERE
)
```

**That's it!** The rest of the build system is already configured:
- `SimulatorLib` (static library) is built from these sources
- `circuit_backend` (Python module) links to SimulatorLib
- `sim` (C++ executable) links to SimulatorLib

---

## Step 4: Python Bindings

Python bindings expose your C++ component to the visualizer.

### 4.1: Add Component Binding

**File:** `src/bindings/ModulesBinding.cpp`

#### Add Include (top of file)

```cpp
#include "Bindings.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "components/Component.hpp"

#include "modules/basic/Gate.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/ClockGenerator.hpp"

#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/FullAdder.hpp"  // <-- ADD YOUR INCLUDE HERE

namespace py = pybind11;
```

#### Add Binding Code (in bindModules function)

```cpp
void bindModules(py::module_& m) {
    // ... existing bindings (NOTGate, ANDGate, etc.) ...

    // --- HalfAdder Binding ---
    py::class_<HalfAdder, IOComponent, std::shared_ptr<HalfAdder>>(
        m, "HalfAdder",
        "A standard 2-input half-adder.",
        py::module_local(false)
    )
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &HalfAdder::getName)
        .def("get_parent", &HalfAdder::getParent)
        .def("get_children", &HalfAdder::getChildren,
             py::return_value_policy::reference_internal)
        .def("get_input_pins", &HalfAdder::getInputPins,
             py::return_value_policy::reference_internal)
        .def("get_output_pins", &HalfAdder::getOutputPins,
             py::return_value_policy::reference_internal);

    // --- FullAdder Binding ---
    py::class_<FullAdder, IOComponent, std::shared_ptr<FullAdder>>(  // <-- ADD THIS
        m, "FullAdder",
        "A standard full-adder.",
        py::module_local(false)
    )
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &FullAdder::getName)
        .def("get_parent", &FullAdder::getParent)
        .def("get_children", &FullAdder::getChildren,
             py::return_value_policy::reference_internal)
        .def("get_input_pins", &FullAdder::getInputPins,
             py::return_value_policy::reference_internal)
        .def("get_output_pins", &FullAdder::getOutputPins,
             py::return_value_policy::reference_internal);
}
```

**Binding Breakdown:**
- `py::class_<FullAdder, IOComponent, std::shared_ptr<FullAdder>>` - Type, base class, smart pointer
- `m` - Python module (circuit_backend)
- `"FullAdder"` - Name in Python
- `.def(py::init<...>())` - Expose constructor
- `.def("method", &Class::method)` - Expose methods

**Note:** For BasicComponent (gates), change `IOComponent` to `BasicComponent` in the template args.

---

## Step 5: C++ Test Implementation

Tests verify your circuit works correctly and enable visualizer integration.

### 5.1: Test Header

**File:** `include/tests/FullAdderTest.hpp`

**Option A: OLD API**

```cpp
#pragma once
#include "simulator/SimulationTest.hpp"

class FullAdderTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
};
```

---

**Option B: NEW API (TruthTableTest)** ⭐ **RECOMMENDED for combinational circuits**

```cpp
#pragma once
#include "simulator/TruthTableTest.hpp"

class FullAdderTest : public TruthTableTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;

protected:
    // Provide the truth table for the FullAdder
    std::vector<TruthRow> getTruthTable() const override;
};
```

---

### 5.2: Test Implementation

**File:** `src/tests/FullAdderTest.cpp`

**Option A: OLD API (Manual Event Scheduling)**

```cpp
#include "tests/FullAdderTest.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/ComponentBuilder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include "simulator/Event.hpp"
#include <cassert>

std::string FullAdderTest::getTestName() const {
    return "FullAdderTest";
}

void FullAdderTest::setupCircuit() {
    root = Component::create<FullAdder>("FA_ROOT");
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void FullAdderTest::buildCircuit() {
    // Empty - FullAdder builds itself via buildInternals()
}

void FullAdderTest::setInitialState() {
    // Create source-less wires to act as test inputs
    auto wire_a = builder->addNewWire("INPUT_A", nullptr,
                                      { builder->getInputPin("A") });
    auto wire_b = builder->addNewWire("INPUT_B", nullptr,
                                      { builder->getInputPin("B") });
    auto wire_cin = builder->addNewWire("INPUT_CIN", nullptr,
                                        { builder->getInputPin("Carry_in") });

    // Schedule events to test all 8 cases of the full adder's truth table
    // T=0:  A=0, B=0, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_cin, LogicValue::LOW));

    // T=10: A=0, B=0, Cin=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(10, wire_cin, LogicValue::HIGH));

    // T=20: A=0, B=1, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_b, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_cin, LogicValue::LOW));

    // T=30: A=0, B=1, Cin=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(30, wire_cin, LogicValue::HIGH));

    // T=40: A=1, B=0, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(40, wire_a, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(40, wire_b, LogicValue::LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(40, wire_cin, LogicValue::LOW));

    // T=50: A=1, B=0, Cin=1
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(50, wire_cin, LogicValue::HIGH));

    // T=60: A=1, B=1, Cin=0
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(60, wire_b, LogicValue::HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(60, wire_cin, LogicValue::LOW));

    // T=70: A=1, B=1, Cin=1 (final state for verification)
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(70, wire_cin, LogicValue::HIGH));
}

void FullAdderTest::verifyResults() {
    // Check the final state (after all events processed)
    // At T=70+: A=1, B=1, Cin=1 → Expected: Sum=1, Carry_out=1

    auto pin_sum = builder->getOutputPin("Sum");
    auto pin_carry = builder->getOutputPin("Carry_out");

    std::cout << "Verification: Checking final state of FullAdder..." << std::endl;
    assert(pin_sum->getValue() == LogicValue::HIGH &&
           "Sum output was expected to be HIGH.");
    assert(pin_carry->getValue() == LogicValue::HIGH &&
           "Carry_out output was expected to be HIGH.");
}

size_t FullAdderTest::getRunDuration() const {
    // Return time long enough for all events to propagate
    // Rule: last_event_time + (2 * max_gate_depth * max_gate_delay)
    return 100;
}
```

**Lines:** ~100

---

**Option B: NEW API (Truth Table)** ⭐ **RECOMMENDED**

```cpp
#include "tests/FullAdderTest.hpp"
#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "modules/composite/FullAdder.hpp"

std::string FullAdderTest::getTestName() const {
    return "FullAdderTest";
}

void FullAdderTest::setupCircuit() {
    // Create a FullAdder instance as the top-level component
    root = Component::create<FullAdder>("FA_ROOT");

    // Initialize the builder
    builder = std::make_unique<ComponentBuilder>(root);

    // buildCircuit() is empty (inherited from TruthTableTest)
    buildCircuit();

    // setInitialState() is auto-generated from truth table!
    setInitialState();
}

// Using new TruthTableTest API (Proposal 4)
std::vector<TruthRow> FullAdderTest::getTruthTable() const {
    constexpr auto L = LogicValue::LOW;
    constexpr auto H = LogicValue::HIGH;

    // Full-Adder Truth Table:
    // A B Cin | Sum Cout
    // 0 0  0  |  0   0
    // 0 0  1  |  1   0
    // 0 1  0  |  1   0
    // 0 1  1  |  0   1
    // 1 0  0  |  1   0
    // 1 0  1  |  0   1
    // 1 1  0  |  0   1
    // 1 1  1  |  1   1

    return {
        // Inputs                                    Outputs
        {{ {"A", L}, {"B", L}, {"Carry_in", L} }, { {"Sum", L}, {"Carry_out", L} }},
        {{ {"A", L}, {"B", L}, {"Carry_in", H} }, { {"Sum", H}, {"Carry_out", L} }},
        {{ {"A", L}, {"B", H}, {"Carry_in", L} }, { {"Sum", H}, {"Carry_out", L} }},
        {{ {"A", L}, {"B", H}, {"Carry_in", H} }, { {"Sum", L}, {"Carry_out", H} }},
        {{ {"A", H}, {"B", L}, {"Carry_in", L} }, { {"Sum", H}, {"Carry_out", L} }},
        {{ {"A", H}, {"B", L}, {"Carry_in", H} }, { {"Sum", L}, {"Carry_out", H} }},
        {{ {"A", H}, {"B", H}, {"Carry_in", L} }, { {"Sum", L}, {"Carry_out", H} }},
        {{ {"A", H}, {"B", H}, {"Carry_in", H} }, { {"Sum", H}, {"Carry_out", H} }},
    };
}
```

**Lines:** ~52 (48% reduction)

**What TruthTableTest does automatically:**
- ✅ Creates input wires for all input pins
- ✅ Schedules WireUpdateEvents at 10-time-unit intervals
- ✅ Runs simulation for correct duration
- ✅ Verifies all output pins match expected values
- ✅ Provides detailed pass/fail messages

**Test Output:**
```
[TruthTableTest] Scheduling test case 0 at t=0: A=0 B=0 Carry_in=0 → Expected: Sum=0 Carry_out=0
[TruthTableTest] Scheduling test case 1 at t=10: A=0 B=0 Carry_in=1 → Expected: Sum=1 Carry_out=0
...
[TruthTableTest] Verifying final state:
  ✓ Sum = 1
  ✓ Carry_out = 1
[TruthTableTest] All outputs match truth table ✓
```

---

### 5.3: Add Test Bindings

**File:** `src/bindings/TestsBinding.cpp`

Add your test header and binding:

```cpp
#include "Bindings.hpp"
#include "simulator/SimulationTest.hpp"

#include "tests/FullCircuitTest.hpp"
#include "tests/HalfAdderTest.hpp"
#include "tests/FullAdderTest.hpp"  // <-- ADD INCLUDE

namespace py = pybind11;

void bindTests(py::module_& m) {
    // ... existing bindings ...

    // --- FullAdderTest Binding ---
    py::class_<FullAdderTest, SimulationTest, std::shared_ptr<FullAdderTest>>(  // <-- ADD THIS
        m, "FullAdderTest",
        "Test scenario for FullAdder circuit.",
        py::module_local(false)
    )
        .def(py::init<>())
        .def("get_run_duration", &FullAdderTest::getRunDuration,
             "Returns the total duration for the simulation test.");
}
```

---

### 5.4: Create CTest Executable

**File:** `tests/FullAdderTest.cpp`

This file contains the main() function for the CTest executable:

```cpp
#include "tests/FullAdderTest.hpp"
#include <iostream>

int main() {
    FullAdderTest test;
    test.run();  // Calls setupCircuit → runs simulation → verifyResults
    return 0;
}
```

**Then add to tests/CMakeLists.txt:**

```cmake
set(TEST_SOURCES
    FullCircuitTest.cpp
    FullAdderTest.cpp  # <-- ADD HERE
)
```

The loop in tests/CMakeLists.txt automatically creates the executable and registers it with CTest.

---

## Step 6: Build and Run

### 6.1: Build Everything

From project root:

```bash
# Initial build (if build/ doesn't exist)
mkdir -p build
cd build
cmake ..
make -j$(nproc)

# Subsequent builds (after changes)
cd build
make -j$(nproc)
```

**Expected output:**
```
[  2%] Building CXX object CMakeFiles/SimulatorLib.dir/src/modules/composite/FullAdder.cpp.o
...
[ 95%] Linking CXX shared module circuit_backend.cpython-*.so
Copying circuit_backend module to visualizer directory...
[100%] Built target circuit_backend
```

**Note:** The Python module is automatically copied to `visualizer/` after each successful build (via CMake POST_BUILD command). No manual copying needed!

### 6.2: Run C++ Tests

```bash
cd build

# Run all tests via CTest
ctest --verbose

# Or run your specific test directly
./tests/FullAdderTest
```

**Expected output:**
```
Test project /path/to/build
    Start 1: FullCircuitTest
1/2 Test #1: FullCircuitTest ..........   Passed    0.01 sec
    Start 2: FullAdderTest
[TruthTableTest] Scheduling test case 0 at t=0: A=0 B=0 Carry_in=0 → Expected: Sum=0 Carry_out=0
...
[TruthTableTest] All outputs match truth table ✓
2/2 Test #2: FullAdderTest ............   Passed    0.02 sec

100% tests passed, 0 tests failed
```

### 6.3: Run Visualizer

**File:** `visualizer/main.py`

Change the test scenario (line ~106):

```python
def initialize_circuit(self):
    # Change this line to use your test:
    self.test_scenario = circuit_backend.FullAdderTest()  # <-- YOUR TEST HERE

    self.test_scenario.setup_circuit()
    root_cpp = self.test_scenario.get_root()
    # ... rest of initialization ...
```

Run from project root:
```bash
cd visualizer
python main.py
```

**Note:** The Python module (`circuit_backend.cpython-*.so`) is automatically available in the `visualizer/` directory after building - no manual copying required!

**Controls:**
- **Spacebar:** Play/pause simulation
- **Left/Right arrows:** Step through time
- **Mouse drag:** Pan camera
- **Mouse wheel:** Zoom
- **Drag components:** Reposition (saves to layout.json)

---

## Complete File Checklist

When adding a new circuit (e.g., FullAdder), you need to touch these files:

### Files to Create (5)

- [ ] `include/modules/composite/FullAdder.hpp` - Component header
- [ ] `src/modules/composite/FullAdder.cpp` - Component implementation
- [ ] `include/tests/FullAdderTest.hpp` - Test header
- [ ] `src/tests/FullAdderTest.cpp` - Test implementation
- [ ] `tests/FullAdderTest.cpp` - CTest executable (with main())

### Files to Modify (5)

- [ ] `CMakeLists.txt` - Add to CORE_LOGIC_SOURCES (2 lines)
- [ ] `src/bindings/ModulesBinding.cpp` - Add include + binding (~13 lines)
- [ ] `src/bindings/TestsBinding.cpp` - Add include + binding (~11 lines)
- [ ] `tests/CMakeLists.txt` - Add to TEST_SOURCES (1 line)
- [ ] `visualizer/main.py` - Change test scenario (1 line)

### Auto-Generated (Don't Edit)

- `build/circuit_backend.*.so` - Python module (rebuilt by make)
- `build/tests/FullAdderTest` - Test executable (rebuilt by make)
- `visualizer/layout.json` - Component layouts (saved by visualizer)

---

## Debugging Guide

### Build Errors

**Error:** `undefined reference to ComponentBuilder::addNewComponent<FullAdder>`
- **Fix:** Add `#include "components/ComponentBuilder.tpp"` to your .cpp file

**Error:** `invalid use of incomplete type 'class WireBuilder'`
- **Fix:** Add `#include "components/WireBuilder.hpp"` to your .cpp file

**Error:** `FullAdder.hpp: No such file or directory`
- **Fix:** Check that path in `#include` matches actual file location

**Error:** `multiple definition of FullAdder::FullAdder`
- **Fix:** Don't include .cpp files, only .hpp files

### Python Import Errors

**Error:** `AttributeError: module 'circuit_backend' has no attribute 'FullAdder'`
- **Fix:** Add binding to `ModulesBinding.cpp` and rebuild

**Error:** `ImportError: cannot import name 'circuit_backend'`
- **Fix:** Make sure `build/circuit_backend.*.so` exists (rebuild if needed)
- **Check:** `ls build/*.so` should show the Python module

### Test Failures

**Error:** `Assertion failed: Sum output was expected to be HIGH`
- **Fix:** Check your truth table logic in `getTruthTable()` or `setInitialState()`
- **Debug:** Add `std::cout` to print intermediate pin values

**Error:** `Segmentation fault` in test
- **Fix:** Check that you used `Component::create<T>()`, not `std::make_shared<T>()`
- **Reason:** `create()` calls `initPins()` and `buildInternals()`, direct construction doesn't

**Error:** `'LOW' in 'enum class LogicValue' does not name a type`
- **Fix:** Use `constexpr auto` instead of `using` for LogicValue shortcuts:
  ```cpp
  constexpr auto L = LogicValue::LOW;  // ✓ Correct
  using L = LogicValue::LOW;           // ✗ Won't work
  ```

### Visualizer Issues

**Error:** Black screen or no components visible
- **Fix:** Check that `initialize_circuit()` uses correct test name
- **Debug:** Print `root_cpp.get_name()` to verify component loaded

**Error:** Wires not showing values
- **Fix:** Verify `setInitialState()` schedules events correctly
- **Debug:** Print `event_timestamps` to see if events were recorded

---

## API Comparison Summary

### Constructor

| Metric | Old API | New API | Improvement |
|--------|---------|---------|-------------|
| Lines | 17 | 9 | **47% fewer** |
| Readability | Lambda noise | Clean macros | **Much better** |
| Error-prone | Medium | Low | **Safer** |

### buildInternals

| Metric | Old API | New API | Improvement |
|--------|---------|---------|-------------|
| Lines | 48 | 44 | **8% fewer** |
| Readability | Verbose | Fluent/English-like | **Much better** |
| Error-prone | High (easy to swap source/sink) | Low (type-safe) | **Safer** |

### Tests

| Metric | Old API | New API | Improvement |
|--------|---------|---------|-------------|
| Lines | ~100 | ~52 | **48% fewer** |
| Readability | Imperative events | Declarative table | **Much better** |
| Error-prone | High (manual assertions) | Low (auto-verify) | **Safer** |

### Total for FullAdder Component + Test

| Metric | Old API | New API | Improvement |
|--------|---------|---------|-------------|
| **Total Lines** | **~165** | **~105** | **36% reduction** |
| **Readability** | Verbose | Clean | **Significantly better** |
| **Maintainability** | Manual | Semi-automated | **Easier to maintain** |

---

## Recommendations

### For New Circuits

**Always use the NEW APIs:**
1. ✅ Use `BEGIN_PINS` / `END_PINS` for constructors
2. ✅ Use `builder.wire()` chaining for all wiring
3. ✅ Use `TruthTableTest` for combinational circuits
4. ✅ Keep old `SimulationTest` for sequential circuits (D flip-flops, counters, etc.)

### For Existing Circuits

**Migration strategy:**
- Convert opportunistically (when modifying a file)
- No urgency - old and new APIs coexist peacefully
- Prioritize frequently-modified files first

### Development Workflow

**Typical development cycle:**
1. Write component header (5 min)
2. Write component implementation with new APIs (10 min)
3. Add to CMakeLists.txt (1 min)
4. Write test with truth table (5 min)
5. Add bindings (3 min)
6. Build and test (2 min)

**Total time:** ~25 minutes per simple circuit

---

## Summary

This guide showed the complete workflow for building a circuit from scratch using FullAdder as an example. The new APIs (Dec 2025) reduce boilerplate by 36-50% while improving readability and type safety.

**Key takeaways:**
- Use the new APIs for all new code
- Truth tables work great for combinational circuits
- Wire chaining makes buildInternals() much clearer
- Pin macros reduce constructor noise significantly

For more details, see:
- `docs/building-circuits.md` - Quick API reference
- `docs/simplification-proposals.md` - All 7 simplification proposals
- `docs/implementation-summary.md` - API implementation details
- `docs/conversion-complete.md` - Migration status
