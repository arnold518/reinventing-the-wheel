#include "Bindings.hpp"
#include <pybind11/stl.h>       // Required for std::vector conversion
#include "simulator/Simulator.hpp"

namespace py = pybind11;

void bindSimulator(py::module_& m) {
    py::class_<Simulator, std::shared_ptr<Simulator>>(m, "Simulator", "Manages the event-driven simulation and its history.", py::module_local(false))
        
        .def("run_and_record", &Simulator::runAndRecord, py::arg("max_time"),
            "Runs the simulation and records the full history of wire changes.")

        .def("advance_and_record", &Simulator::advanceAndRecord, py::arg("target_time"),
            "Continues the simulation up to an absolute target time without clearing future events.")
            
        .def("set_circuit_state_at_time", &Simulator::setCircuitStateAtTime, py::arg("target_time"),
            "Sets the state of all wires to match the recorded state at a specific time.")
            
        .def("get_unique_timestamps", &Simulator::getUniqueTimestamps,
            "Returns a sorted list of unique timestamps where state changes occurred.")
            
        .def("clear", &Simulator::clear,
            "Clears the event queue and the recorded history.")
            
        .def("get_current_time", &Simulator::getCurrentTime,
            "Returns the current simulation time as set by set_circuit_state_at_time.");
}
