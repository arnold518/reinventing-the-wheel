#include "Bindings.hpp" // The master header with all function declarations

// The PYBIND11_MODULE macro is the entry point that Python calls when you run `import circuit_backend`.
// It must only appear once in your entire project.
PYBIND11_MODULE(circuit_backend, m) {
    m.doc() = "C++ backend for the Reinventing-the-Wheel circuit simulator";

    // --- Call Binding Functions in Logical Order ---
    // The order is important for classes that inherit from each other.
    // Base classes must be bound before their children.

    // 1. Bind fundamental enums and data types first.
    bindLogicValue(m);
    bindPin(m);
    bindWire(m);

    // 2. Bind the component class hierarchy.
    bindComponent(m);
    bindIOComponent(m);
    bindBasicComponent(m);

    // 3. Bind the simulator and test hierarchy.
    bindSimulator(m);
    bindSimulationTest(m);
    bindFullCircuitTest(m);
}