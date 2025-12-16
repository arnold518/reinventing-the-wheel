# Building New Circuits - Quick Guide

## Two Types of Components

### 1. Basic Gates (Logic-Level)
Implements gate behavior via `evaluate()` method.

**Example: 2-input NAND gate**

```cpp
// include/modules/basic/MyGates.hpp
class NANDGate : public BasicComponent {
public:
    NANDGate(std::string name);
    void evaluate(size_t current_time, Simulator& simulator) override;
};

// src/modules/basic/MyGates.cpp
NANDGate::NANDGate(std::string name)
    : BasicComponent(std::move(name), 1,  // delay = 1 time unit
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("OUT", PinType::OUTPUT);
        })
{}

void NANDGate::evaluate(size_t current_time, Simulator& simulator) {
    LogicValue a = getInputValue("A");
    LogicValue b = getInputValue("B");
    LogicValue result = (a == HIGH && b == HIGH) ? LOW : HIGH;
    _updateOutputWire(simulator, "OUT", result, current_time);
}
```

### 2. Composite Circuits (Hierarchical)
Builds internal circuit from other components via `buildInternals()`.

**Example: HalfAdder (XOR + AND)**

```cpp
// include/modules/composite/HalfAdder.hpp
class HalfAdder : public IOComponent {
public:
    HalfAdder(std::string name);
    void buildInternals(ComponentBuilder& builder) override;
};

// src/modules/composite/HalfAdder.cpp
#include "components/ComponentBuilder.tpp"  // CRITICAL for templates

HalfAdder::HalfAdder(std::string name)
    : IOComponent(std::move(name),
        [](IOComponent* self) {
            self->addPin("A", PinType::INPUT);
            self->addPin("B", PinType::INPUT);
            self->addPin("Sum", PinType::OUTPUT);
            self->addPin("Carry", PinType::OUTPUT);
        })
{}

void HalfAdder::buildInternals(ComponentBuilder& builder) {
    // 1. Add internal components
    builder.addNewComponent<XORGate>("XOR1");
    builder.addNewComponent<ANDGate>("AND1");

    // 2. Wire inputs to internal gates
    builder.addNewWire("A_internal",
        builder.getInputPin("A"),  // Source: my input pin
        { builder.getInputPin<XORGate>("XOR1", "A"),
          builder.getInputPin<ANDGate>("AND1", "A") }  // Sinks: fan-out
    );
    builder.addNewWire("B_internal",
        builder.getInputPin("B"),
        { builder.getInputPin<XORGate>("XOR1", "B"),
          builder.getInputPin<ANDGate>("AND1", "B") }
    );

    // 3. Wire internal outputs to my outputs
    builder.addNewWire("Sum_internal",
        builder.getOutputPin<XORGate>("XOR1", "OUT"),
        { builder.getOutputPin("Sum") }
    );
    builder.addNewWire("Carry_internal",
        builder.getOutputPin<ANDGate>("AND1", "OUT"),
        { builder.getOutputPin("Carry") }
    );
}
```

## Writing Tests

**Pattern**: Make your circuit the root, test via direct pin access.

```cpp
// include/tests/HalfAdderTest.hpp
class HalfAdderTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
};

// src/tests/HalfAdderTest.cpp
void HalfAdderTest::setupCircuit() {
    root = Component::create<HalfAdder>("HA_ROOT");  // Circuit is root!
    builder = std::make_unique<ComponentBuilder>(root);
    buildCircuit();
    setInitialState();
}

void HalfAdderTest::buildCircuit() {
    // Empty - HalfAdder builds itself via buildInternals()
}

void HalfAdderTest::setInitialState() {
    // Create source-less wires to drive inputs
    auto wire_a = builder->addNewWire("INPUT_A", nullptr,
                                      { builder->getInputPin("A") });
    auto wire_b = builder->addNewWire("INPUT_B", nullptr,
                                      { builder->getInputPin("B") });

    // Schedule test vectors
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_a, LOW));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(0, wire_b, LOW));

    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(10, wire_b, HIGH));

    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_a, HIGH));
    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(20, wire_b, LOW));

    sim->scheduleEvent(std::make_shared<WireUpdateEvent>(30, wire_b, HIGH));
}

void HalfAdderTest::verifyResults() {
    // Check final state (A=1, B=1 → Sum=0, Carry=1)
    auto pin_sum = builder->getOutputPin("Sum");
    auto pin_carry = builder->getOutputPin("Carry");

    assert(pin_sum->getValue() == LOW);
    assert(pin_carry->getValue() == HIGH);
}

size_t HalfAdderTest::getRunDuration() const {
    return 50;  // Run until t=50 (last event at t=30 + propagation)
}

std::string HalfAdderTest::getTestName() const { return "HalfAdderTest"; }
```

## Build Integration

### 1. Add to CMakeLists.txt

```cmake
# For new composite circuit
set(COMPOSITE_SOURCES
    src/modules/composite/HalfAdder.cpp
    src/modules/composite/FullAdder.cpp
    src/modules/composite/MyNewCircuit.cpp  # <-- ADD HERE
)

# For new test
add_executable(MyCircuitTest tests/MyCircuitTest.cpp)
target_link_libraries(MyCircuitTest circuit_core)
add_test(NAME MyCircuitTest COMMAND MyCircuitTest)
```

### 2. Python Bindings (Optional)

```cpp
// src/bindings/ModulesBinding.cpp
void bindModules(py::module& m) {
    // ... existing bindings ...
    py::class_<MyNewCircuit, IOComponent, std::shared_ptr<MyNewCircuit>>(m, "MyNewCircuit")
        .def(py::init<std::string>());
}
```

### 3. Build & Test

```bash
cd build
cmake .. && make -j$(nproc)
./MyCircuitTest
ctest --verbose
```

---

## Critical Rules

1. **Always use `Component::create<T>()`** - Never `std::make_shared<T>()` directly
2. **Include `.tpp` in .cpp files** - Not in headers (circular dependency)
3. **Source-less wires for test inputs** - `addNewWire("name", nullptr, {sinks})`
4. **Wire sources are outputs, sinks are inputs** - Even for INPUT pins at boundaries

---

## Simplification Ideas

### Problem 1: Too Much Boilerplate

**Current**: Every component needs .hpp + .cpp + constructor + lambda

**Solution A: Macro-based declaration**
```cpp
DEFINE_BASIC_GATE(NAND, 2, 1) {  // 2 inputs, 1 output, auto-named A/B/OUT
    return !(input[0] && input[1]);
}
```

**Solution B: Pin specification in header**
```cpp
class NANDGate : public BasicComponent {
    PINS(INPUT("A"), INPUT("B"), OUTPUT("OUT"));
    DELAY(1);
    EVALUATE { return !(A && B); }  // Implicit pin access
};
```

### Problem 2: Wire Creation is Verbose

**Current**: 4 lines per wire with builder->getInputPin<Type>("instance", "pin")

**Solution: Chain API**
```cpp
builder.wire("A_internal")
    .from(myInputPin("A"))
    .to<XORGate>("XOR1", "A")
    .to<ANDGate>("AND1", "A");
```

### Problem 3: Testing is Repetitive

**Current**: Each test needs setupCircuit, buildCircuit, setInitialState, verify...

**Solution: Truth table-based testing**
```cpp
TEST_CIRCUIT(HalfAdder, {
    INPUT("A", "B"),
    OUTPUT("Sum", "Carry"),
    TRUTH_TABLE({
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {1, 0, 1, 0},
        {1, 1, 0, 1}
    })
});
```

Auto-generates test vectors and verification.

### Problem 4: Template Include Hell

**Current**: Must remember to include ComponentBuilder.tpp in every composite .cpp

**Solution: Forward declare + explicit instantiation**
```cpp
// ComponentBuilder.cpp - Pre-instantiate common types
template class ComponentBuilder::addNewComponent<XORGate>;
template class ComponentBuilder::addNewComponent<ANDGate>;
// ... etc
```

Now just include .hpp, linker finds instantiation.

### Problem 5: Pin Access Type Safety

**Current**: String-based pin names, runtime errors if typo

**Solution: Constexpr pin descriptors**
```cpp
class HalfAdder : public IOComponent {
    static constexpr auto PIN_A = InputPin("A");
    static constexpr auto PIN_SUM = OutputPin("Sum");
    // ...
};

builder.getInputPin<HalfAdder>("HA1", HalfAdder::PIN_A);  // Compile-time check
```

---

## Recommended Priorities

1. **Truth table testing** (biggest time saver for simple circuits)
2. **Wire chaining API** (readability + less typing)
3. **Pin specification macros** (reduces boilerplate by 50%)
4. **Explicit template instantiation** (solves .tpp include complexity)
5. **Constexpr pin names** (nice-to-have, catches typos early)

Start with #1 and #2 - they provide immediate productivity gains without major refactoring.
