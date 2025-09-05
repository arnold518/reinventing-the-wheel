#include "Bindings.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// Include all necessary headers
#include "modules/Gate.hpp"
#include "modules/DFlipFlop.hpp"
#include "modules/ClockGenerator.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "components/Component.hpp"

namespace py = pybind11;

void bindModules(py::module_& m) {
    
    // --- ANDGate Binding ---
    py::class_<ANDGate, BasicComponent, std::shared_ptr<ANDGate>>(m, "ANDGate", "A standard 2-input AND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        // --- Expose required inherited methods ---
        .def("get_name", &ANDGate::getName)
        .def("get_parent", &ANDGate::getParent)
        .def("get_children", &ANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ANDGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ANDGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- NANDGate (example, if needed) ---
    py::class_<NANDGate, BasicComponent, std::shared_ptr<NANDGate>>(m, "NANDGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &NANDGate::getName)
        .def("get_parent", &NANDGate::getParent)
        .def("get_children", &NANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NANDGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NANDGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- DFlipFlop Binding ---
    py::class_<DFlipFlop, BasicComponent, std::shared_ptr<DFlipFlop>>(m, "DFlipFlop", "A rising-edge triggered D-type Flip-Flop.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &DFlipFlop::getName)
        .def("get_parent", &DFlipFlop::getParent)
        .def("get_children", &DFlipFlop::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &DFlipFlop::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &DFlipFlop::getOutputPins, py::return_value_policy::reference_internal);

    // --- ClockGenerator Binding ---
    py::class_<ClockGenerator, BasicComponent, std::shared_ptr<ClockGenerator>>(m, "ClockGenerator", "Generates a periodic clock signal.", py::module_local(false))
        .def(py::init<const std::string&, size_t>(), py::arg("name"), py::arg("half_period"))
        .def("get_name", &ClockGenerator::getName)
        .def("get_parent", &ClockGenerator::getParent)
        .def("get_children", &ClockGenerator::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ClockGenerator::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ClockGenerator::getOutputPins, py::return_value_policy::reference_internal);
}