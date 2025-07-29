#include <pybind11/pybind11.h>

#include "bindings/ComponentBinding.hpp"
#include "bindings/IOComponentBinding.hpp"
#include "bindings/BasicComponentBinding.hpp"
// #include "bindings/GateBinding.hpp"
// Add other binding headers here as you create them...

namespace py = pybind11;

// This macro creates the Python module.
// The first argument, "circuit_backend", is the name you will use in Python.
PYBIND11_MODULE(circuit_backend, m) {
    m.doc() = "C++ backend for the Reinventing-the-Wheel circuit simulator";

    // Call the binding functions from other files to populate the module.
    // The order is important for inherited classes: base classes must be bound first.
    bindComponent(m);
    bindIOComponent(m);
    bindBasicComponent(m);
    // bind_gate(m);
    // Add other binding function calls here...
}