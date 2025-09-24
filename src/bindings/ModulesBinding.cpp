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
#include "modules/composite/FullAdder.hpp"

namespace py = pybind11;

void bindModules(py::module_& m) {

    // --- NOTGate Binding ---
    py::class_<NOTGate, BasicComponent, std::shared_ptr<NOTGate>>(m, "NOTGate", "A standard 1-input AND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &NOTGate::getName)
        .def("get_parent", &NOTGate::getParent)
        .def("get_children", &NOTGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NOTGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NOTGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- ANDGate Binding ---
    py::class_<ANDGate, BasicComponent, std::shared_ptr<ANDGate>>(m, "ANDGate", "A standard 2-input AND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &ANDGate::getName)
        .def("get_parent", &ANDGate::getParent)
        .def("get_children", &ANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ANDGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ANDGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- NANDGate Binding ---
    py::class_<NANDGate, BasicComponent, std::shared_ptr<NANDGate>>(m, "NANDGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &NANDGate::getName)
        .def("get_parent", &NANDGate::getParent)
        .def("get_children", &NANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NANDGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NANDGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- ORGate Binding ---
    py::class_<ORGate, BasicComponent, std::shared_ptr<ORGate>>(m, "ORGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &ORGate::getName)
        .def("get_parent", &ORGate::getParent)
        .def("get_children", &ORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ORGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ORGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- NORGate Binding ---
    py::class_<NORGate, BasicComponent, std::shared_ptr<NORGate>>(m, "NORGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &NORGate::getName)
        .def("get_parent", &NORGate::getParent)
        .def("get_children", &NORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NORGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NORGate::getOutputPins, py::return_value_policy::reference_internal);

    // --- XORGate Binding ---
    py::class_<XORGate, BasicComponent, std::shared_ptr<XORGate>>(m, "XORGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &XORGate::getName)
        .def("get_parent", &XORGate::getParent)
        .def("get_children", &XORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &XORGate::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &XORGate::getOutputPins, py::return_value_policy::reference_internal);

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

    // --- HalfAdder Binding ---
    py::class_<HalfAdder, IOComponent, std::shared_ptr<HalfAdder>>(m, "HalfAdder", "A standard 2-input half-adder.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &HalfAdder::getName)
        .def("get_parent", &HalfAdder::getParent)
        .def("get_children", &HalfAdder::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &HalfAdder::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &HalfAdder::getOutputPins, py::return_value_policy::reference_internal);
    
    // --- FullAdder Binding ---
    py::class_<FullAdder, IOComponent, std::shared_ptr<FullAdder>>(m, "FullAdder", "A standard full-adder.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &FullAdder::getName)
        .def("get_parent", &FullAdder::getParent)
        .def("get_children", &FullAdder::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &FullAdder::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &FullAdder::getOutputPins, py::return_value_policy::reference_internal);
}