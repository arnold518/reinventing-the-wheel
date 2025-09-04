#include "Bindings.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// Include the headers for the C++ classes we want to bind.
#include "modules/Gate.hpp"
#include "modules/DFlipFlop.hpp"
#include "modules/ClockGenerator.hpp"

// We must also include the header for the direct parent class
// so pybind11 can establish the inheritance relationship.
#include "components/BasicComponent.hpp"

// The ClockGenerator::startClock method requires the Simulator type.
#include "simulator/Simulator.hpp"

namespace py = pybind11;

// This function will be called from your main binding entry point (e.g., circuit_backend.cpp)
void bindModules(py::module_& m) {
    
    // --- Gate Bindings ---

    // Note: The body of the binding is minimal. Its main purpose is to register
    // the type 'ANDGate' and declare its inheritance from 'BasicComponent'.
    // This provides the necessary RTTI information for pybind11.
    py::class_<ANDGate, BasicComponent, std::shared_ptr<ANDGate>>(m, "ANDGate", "A standard 2-input AND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"), "Constructor for an ANDGate.");

    py::class_<NANDGate, BasicComponent, std::shared_ptr<NANDGate>>(m, "NANDGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"), "Constructor for a NANDGate.");


    // --- DFlipFlop Binding ---

    py::class_<DFlipFlop, BasicComponent, std::shared_ptr<DFlipFlop>>(m, "DFlipFlop", "A rising-edge triggered D-type Flip-Flop with an asynchronous reset.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"), "Constructor for a DFlipFlop.");


    // --- ClockGenerator Binding ---

    py::class_<ClockGenerator, BasicComponent, std::shared_ptr<ClockGenerator>>(m, "ClockGenerator", "Generates a periodic clock signal.", py::module_local(false))
        .def(py::init<const std::string&, size_t>(), py::arg("name"), py::arg("half_period"), "Constructor for a ClockGenerator.")
        
        // We also bind the 'startClock' method as it's unique to this component
        // and needed to initialize the simulation state from Python.
        .def("start_clock", &ClockGenerator::startClock, py::arg("simulator"), py::arg("start_time"),
            "Schedules the initial events to start the clock signal.");
}