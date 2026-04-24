#include "Bindings.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "components/Component.hpp"

#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/ClockGenerator.hpp"

#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/Shifter8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/ALU8.hpp"

namespace py = pybind11;

template<typename T>
void bindBasicModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, BasicComponent, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getOutputPins, py::return_value_policy::reference_internal);
}

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
    py::class_<HalfAdder, BasicComponent, std::shared_ptr<HalfAdder>>(m, "HalfAdder", "A standard 2-input half-adder.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &HalfAdder::getName)
        .def("get_parent", &HalfAdder::getParent)
        .def("get_children", &HalfAdder::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &HalfAdder::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &HalfAdder::getOutputPins, py::return_value_policy::reference_internal);
    
    // --- FullAdder Binding ---
    py::class_<FullAdder, BasicComponent, std::shared_ptr<FullAdder>>(m, "FullAdder", "A standard full-adder.", py::module_local(false))
        .def(py::init<const std::string&>(), py::arg("name"))
        .def("get_name", &FullAdder::getName)
        .def("get_parent", &FullAdder::getParent)
        .def("get_children", &FullAdder::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &FullAdder::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &FullAdder::getOutputPins, py::return_value_policy::reference_internal);

    bindBasicModule<AND8>(m, "AND8", "8-bit bitwise AND.");
    bindBasicModule<OR8>(m, "OR8", "8-bit bitwise OR.");
    bindBasicModule<XOR8>(m, "XOR8", "8-bit bitwise XOR.");
    bindBasicModule<NOT8>(m, "NOT8", "8-bit bitwise NOT.");
    bindBasicModule<NAND8>(m, "NAND8", "8-bit bitwise NAND.");
    bindBasicModule<NOR8>(m, "NOR8", "8-bit bitwise NOR.");

    bindBasicModule<Mux2to1>(m, "Mux2to1", "2:1 one-bit multiplexer.");
    bindBasicModule<Mux4to1>(m, "Mux4to1", "4:1 one-bit multiplexer.");
    bindBasicModule<Mux8to1>(m, "Mux8to1", "8:1 one-bit multiplexer.");
    bindBasicModule<Mux16to1>(m, "Mux16to1", "16:1 one-bit multiplexer.");
    bindBasicModule<Mux2to1_8bit>(m, "Mux2to1_8bit", "2:1 8-bit multiplexer.");
    bindBasicModule<Mux4to1_8bit>(m, "Mux4to1_8bit", "4:1 8-bit multiplexer.");
    bindBasicModule<Mux8to1_8bit>(m, "Mux8to1_8bit", "8:1 8-bit multiplexer.");
    bindBasicModule<Mux16to1_8bit>(m, "Mux16to1_8bit", "16:1 8-bit multiplexer.");

    bindBasicModule<Adder8>(m, "Adder8", "8-bit adder.");
    bindBasicModule<TwosComplement8>(m, "TwosComplement8", "8-bit two's complement.");
    bindBasicModule<Subtractor8>(m, "Subtractor8", "8-bit subtractor.");
    bindBasicModule<SubtractorWithBorrow8>(m, "SubtractorWithBorrow8", "8-bit subtractor with borrow.");
    bindBasicModule<Incrementer8>(m, "Incrementer8", "8-bit incrementer.");
    bindBasicModule<Decrementer8>(m, "Decrementer8", "8-bit decrementer.");
    bindBasicModule<EqualityChecker8>(m, "EqualityChecker8", "8-bit equality checker.");
    bindBasicModule<Comparator8>(m, "Comparator8", "8-bit unsigned comparator.");
    bindBasicModule<SignedComparator8>(m, "SignedComparator8", "8-bit signed comparator.");
    bindBasicModule<ShiftLeftLogical8>(m, "ShiftLeftLogical8", "8-bit logical shift left.");
    bindBasicModule<ShiftRightLogical8>(m, "ShiftRightLogical8", "8-bit logical shift right.");
    bindBasicModule<ShiftRightArithmetic8>(m, "ShiftRightArithmetic8", "8-bit arithmetic shift right.");
    bindBasicModule<ZeroDetect8>(m, "ZeroDetect8", "8-bit zero detector.");
    bindBasicModule<ALU8>(m, "ALU8", "8-bit arithmetic logic unit.");
}
