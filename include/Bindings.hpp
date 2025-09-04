#pragma once
#include "basic/LogicValue.hpp"
#include <pybind11/pybind11.h>

namespace py = pybind11;

inline void bindLogicValue(py::module_& m) {
    py::enum_<LogicValue>(m, "LogicValue", "Represents the four states of a logic signal.")
        .value("LOW", LogicValue::LOW)
        .value("HIGH", LogicValue::HIGH)
        .value("UNKNOWN", LogicValue::UNKNOWN)
        .value("HIGH_Z", LogicValue::HIGH_Z)
        .export_values()
        // Add a __str__ method to the enum that uses your C++ stream operator
        .def("__str__", [](LogicValue val) {
            std::stringstream ss;
            ss << val; // Uses your overloaded operator<<
            return ss.str();
        });
}

// --- Component Hierarchy Bindings ---
void bindComponent(py::module_ &m);
void bindIOComponent(py::module_ &m);
void bindBasicComponent(py::module_ &m);

// --- Modules Bindings ---
void bindModules(py::module_ &m);

// --- Core Type Bindings ---
void bindPin(py::module_ &m);
void bindWire(py::module_ &m);

// --- Simulator and Test Bindings ---
void bindSimulator(py::module_ &m);
void bindSimulationTest(py::module_ &m);

void bindFullCircuitTest(py::module_ &m);