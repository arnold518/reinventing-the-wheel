#include "ModuleBindingHelpers.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/basic/Decoder.hpp"

void bindBasicModules(py::module_& m) {
    // --- NOTGate Binding ---
    py::class_<NOTGate, BasicComponent, std::shared_ptr<NOTGate>>(m, "NOTGate", "A standard 1-input NOT gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<NOTGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &NOTGate::getName)
        .def("get_parent", &NOTGate::getParent)
        .def("get_children", &NOTGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NOTGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NOTGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- ANDGate Binding ---
    py::class_<ANDGate, BasicComponent, std::shared_ptr<ANDGate>>(m, "ANDGate", "A standard 2-input AND gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<ANDGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &ANDGate::getName)
        .def("get_parent", &ANDGate::getParent)
        .def("get_children", &ANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ANDGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ANDGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- NANDGate Binding ---
    py::class_<NANDGate, BasicComponent, std::shared_ptr<NANDGate>>(m, "NANDGate", "A standard 2-input NAND gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<NANDGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &NANDGate::getName)
        .def("get_parent", &NANDGate::getParent)
        .def("get_children", &NANDGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NANDGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NANDGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- ORGate Binding ---
    py::class_<ORGate, BasicComponent, std::shared_ptr<ORGate>>(m, "ORGate", "A standard 2-input OR gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<ORGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &ORGate::getName)
        .def("get_parent", &ORGate::getParent)
        .def("get_children", &ORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ORGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ORGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- NORGate Binding ---
    py::class_<NORGate, BasicComponent, std::shared_ptr<NORGate>>(m, "NORGate", "A standard 2-input NOR gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<NORGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &NORGate::getName)
        .def("get_parent", &NORGate::getParent)
        .def("get_children", &NORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &NORGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &NORGate::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- XORGate Binding ---
    py::class_<XORGate, BasicComponent, std::shared_ptr<XORGate>>(m, "XORGate", "A standard 2-input XOR gate.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<XORGate>(instance_name);
        }), py::arg("name"))
        .def("get_name", &XORGate::getName)
        .def("get_parent", &XORGate::getParent)
        .def("get_children", &XORGate::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &XORGate::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &XORGate::getAllOutputPins, py::return_value_policy::reference_internal);

    bindCompositeModule<SRLatch>(m, "SRLatch", "Structural active-low SR latch built from cross-coupled NAND gates.");
    bindCompositeModule<GatedDLatch>(m, "GatedDLatch", "Structural gated D latch with reset.");
    bindCompositeModule<DFlipFlop>(m, "DFlipFlop", "Structural rising-edge master-slave D flip-flop.");

    // --- ClockGenerator Binding ---
    py::class_<ClockGenerator, BasicComponent, std::shared_ptr<ClockGenerator>>(m, "ClockGenerator", "Generates a periodic clock signal.", py::module_local(false))
        .def(py::init([](const std::string& instance_name, SimulationTime half_period) {
            return Component::create<ClockGenerator>(instance_name, half_period);
        }), py::arg("name"), py::arg("half_period"))
        .def("get_name", &ClockGenerator::getName)
        .def("get_parent", &ClockGenerator::getParent)
        .def("get_children", &ClockGenerator::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &ClockGenerator::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &ClockGenerator::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- HalfAdder Binding ---
    py::class_<HalfAdder, IOComponent, std::shared_ptr<HalfAdder>>(m, "HalfAdder", "A standard 2-input half-adder.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<HalfAdder>(instance_name);
        }), py::arg("name"))
        .def("get_name", &HalfAdder::getName)
        .def("get_parent", &HalfAdder::getParent)
        .def("get_children", &HalfAdder::getChildren, py::return_value_policy::reference_internal);

    // --- FullAdder Binding ---
    py::class_<FullAdder, IOComponent, std::shared_ptr<FullAdder>>(m, "FullAdder", "A standard full-adder.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<FullAdder>(instance_name);
        }), py::arg("name"))
        .def("get_name", &FullAdder::getName)
        .def("get_parent", &FullAdder::getParent)
        .def("get_children", &FullAdder::getChildren, py::return_value_policy::reference_internal);


    bindCompositeModule<Mux2to1>(m, "Mux2to1", "2:1 one-bit multiplexer.");
    bindCompositeModule<Mux4to1>(m, "Mux4to1", "4:1 one-bit multiplexer.");
    bindCompositeModule<Mux8to1>(m, "Mux8to1", "8:1 one-bit multiplexer.");
    bindCompositeModule<Mux32to1>(m, "Mux32to1", "32:1 one-bit multiplexer.");
    bindCompositeModule<Mux2to1_4bit>(m, "Mux2to1_4bit", "2:1 4-bit multiplexer.");
    bindCompositeModule<Mux2to1_32bit>(m, "Mux2to1_32bit", "2:1 32-bit multiplexer.");
    bindCompositeModule<Mux4to1_32bit>(m, "Mux4to1_32bit", "4:1 32-bit multiplexer.");
    bindCompositeModule<Mux8to1_8bit>(m, "Mux8to1_8bit", "8:1 8-bit multiplexer.");
    bindCompositeModule<Mux8to1_32bit>(m, "Mux8to1_32bit", "8:1 32-bit multiplexer.");
    bindCompositeModule<Mux32to1_32bit>(m, "Mux32to1_32bit", "32:1 32-bit multiplexer.");
    bindCompositeModule<Decoder5to32>(m, "Decoder5to32", "5-bit enabled one-hot decoder.");


}
