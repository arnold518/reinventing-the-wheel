#include "Bindings.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cstdint>
#include <utility>

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
#include "modules/utility/Rewire.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"

namespace py = pybind11;

template<typename T>
void bindBasicModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, BasicComponent, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<T>(instance_name);
        }), py::arg("name"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal);
}

template<typename T>
void bindConstantModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, BasicComponent, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init([](const std::string& instance_name, uint64_t constant_value) {
            return Component::create<T>(instance_name, constant_value);
        }), py::arg("name"), py::arg("constant_value"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal);
}

template<typename T>
void bindIOModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, IOComponent, std::shared_ptr<T>>(m, name, description, py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<T>(instance_name);
        }), py::arg("name"))
        .def("get_name", &T::getName)
        .def("get_parent", &T::getParent)
        .def("get_children", &T::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &T::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal);
}

void bindModules(py::module_& m) {

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

    // --- DFlipFlop Binding ---
    py::class_<DFlipFlop, BasicComponent, std::shared_ptr<DFlipFlop>>(m, "DFlipFlop", "A rising-edge triggered D-type Flip-Flop.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<DFlipFlop>(instance_name);
        }), py::arg("name"))
        .def("get_name", &DFlipFlop::getName)
        .def("get_parent", &DFlipFlop::getParent)
        .def("get_children", &DFlipFlop::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &DFlipFlop::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &DFlipFlop::getAllOutputPins, py::return_value_policy::reference_internal);

    // --- ClockGenerator Binding ---
    py::class_<ClockGenerator, BasicComponent, std::shared_ptr<ClockGenerator>>(m, "ClockGenerator", "Generates a periodic clock signal.", py::module_local(false))
        .def(py::init([](const std::string& instance_name, size_t half_period) {
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
        .def("get_children", &HalfAdder::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &HalfAdder::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &HalfAdder::getOutputPins, py::return_value_policy::reference_internal);
    
    // --- FullAdder Binding ---
    py::class_<FullAdder, IOComponent, std::shared_ptr<FullAdder>>(m, "FullAdder", "A standard full-adder.", py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<FullAdder>(instance_name);
        }), py::arg("name"))
        .def("get_name", &FullAdder::getName)
        .def("get_parent", &FullAdder::getParent)
        .def("get_children", &FullAdder::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &FullAdder::getInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &FullAdder::getOutputPins, py::return_value_policy::reference_internal);

    bindIOModule<AND8>(m, "AND8", "8-bit bitwise AND.");
    bindIOModule<OR8>(m, "OR8", "8-bit bitwise OR.");
    bindIOModule<XOR8>(m, "XOR8", "8-bit bitwise XOR.");
    bindIOModule<NOT8>(m, "NOT8", "8-bit bitwise NOT.");
    bindIOModule<NAND8>(m, "NAND8", "8-bit bitwise NAND.");
    bindIOModule<NOR8>(m, "NOR8", "8-bit bitwise NOR.");

    bindIOModule<Mux2to1>(m, "Mux2to1", "2:1 one-bit multiplexer.");
    bindIOModule<Mux4to1>(m, "Mux4to1", "4:1 one-bit multiplexer.");
    bindIOModule<Mux8to1>(m, "Mux8to1", "8:1 one-bit multiplexer.");
    bindIOModule<Mux16to1>(m, "Mux16to1", "16:1 one-bit multiplexer.");
    bindIOModule<Mux2to1_8bit>(m, "Mux2to1_8bit", "2:1 8-bit multiplexer.");
    bindIOModule<Mux4to1_8bit>(m, "Mux4to1_8bit", "4:1 8-bit multiplexer.");
    bindIOModule<Mux8to1_8bit>(m, "Mux8to1_8bit", "8:1 8-bit multiplexer.");
    bindIOModule<Mux16to1_8bit>(m, "Mux16to1_8bit", "16:1 8-bit multiplexer.");

    bindIOModule<Adder8>(m, "Adder8", "8-bit adder.");
    bindIOModule<TwosComplement8>(m, "TwosComplement8", "8-bit two's complement.");
    bindIOModule<Subtractor8>(m, "Subtractor8", "8-bit subtractor.");
    bindIOModule<SubtractorWithBorrow8>(m, "SubtractorWithBorrow8", "8-bit subtractor with borrow.");
    bindIOModule<Incrementer8>(m, "Incrementer8", "8-bit incrementer.");
    bindIOModule<Decrementer8>(m, "Decrementer8", "8-bit decrementer.");
    bindIOModule<EqualityChecker8>(m, "EqualityChecker8", "8-bit equality checker.");
    bindIOModule<Comparator8>(m, "Comparator8", "8-bit unsigned comparator.");
    bindIOModule<SignedComparator8>(m, "SignedComparator8", "8-bit signed comparator.");
    bindIOModule<ShiftLeftLogical8>(m, "ShiftLeftLogical8", "8-bit logical shift left.");
    bindIOModule<ShiftRightLogical8>(m, "ShiftRightLogical8", "8-bit logical shift right.");
    bindIOModule<ShiftRightArithmetic8>(m, "ShiftRightArithmetic8", "8-bit arithmetic shift right.");
    bindIOModule<ZeroDetect8>(m, "ZeroDetect8", "8-bit zero detector.");
    bindIOModule<ALU8>(m, "ALU8", "8-bit arithmetic logic unit.");

    py::enum_<Rewire::UnmappedBitValue>(m, "UnmappedBitValue")
        .value("UNKNOWN", Rewire::UnmappedBitValue::UNKNOWN)
        .value("LOW", Rewire::UnmappedBitValue::LOW)
        .value("HIGH", Rewire::UnmappedBitValue::HIGH);

    py::class_<Rewire::WireSpec>(m, "RewireWireSpec")
        .def(py::init<std::string, size_t>(), py::arg("name"), py::arg("width") = 1)
        .def_readwrite("name", &Rewire::WireSpec::name)
        .def_readwrite("width", &Rewire::WireSpec::width);

    py::class_<Rewire::BitMap>(m, "RewireBitMap")
        .def(py::init<std::string, size_t, std::string, size_t>(),
             py::arg("src_wire"), py::arg("src_bit"), py::arg("dst_wire"), py::arg("dst_bit"))
        .def_readwrite("src_wire", &Rewire::BitMap::src_wire)
        .def_readwrite("src_bit", &Rewire::BitMap::src_bit)
        .def_readwrite("dst_wire", &Rewire::BitMap::dst_wire)
        .def_readwrite("dst_bit", &Rewire::BitMap::dst_bit);

    py::class_<Rewire, BasicComponent, std::shared_ptr<Rewire>>(m, "Rewire", "Generic bit-level routing adapter.", py::module_local(false))
        .def(py::init([](std::string instance_name,
                         std::vector<Rewire::WireSpec> inputs,
                         std::vector<Rewire::WireSpec> outputs,
                         std::vector<Rewire::BitMap> mappings,
                         Rewire::UnmappedBitValue unmapped) {
            return Component::create<Rewire>(
                std::move(instance_name),
                std::move(inputs),
                std::move(outputs),
                std::move(mappings),
                unmapped);
        }),
             py::arg("name"), py::arg("inputs"), py::arg("outputs"), py::arg("mappings"),
             py::arg("unmapped") = Rewire::UnmappedBitValue::UNKNOWN)
        .def("get_name", &Rewire::getName)
        .def("get_parent", &Rewire::getParent)
        .def("get_children", &Rewire::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &Rewire::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &Rewire::getAllOutputPins, py::return_value_policy::reference_internal)
        .def("validate_mappings", &Rewire::validateMappings)
        .def("get_total_input_bits", &Rewire::getTotalInputBits)
        .def("get_total_output_bits", &Rewire::getTotalOutputBits);

    m.def("identity_mapping", &identity_mapping, py::arg("src_wire"), py::arg("src_offset"),
          py::arg("count"), py::arg("dst_wire"), py::arg("dst_offset") = 0);
    m.def("unpack_mapping", &unpack_mapping, py::arg("src_bus"), py::arg("width"), py::arg("dst_wires"));
    m.def("pack_mapping", &pack_mapping, py::arg("src_wires"), py::arg("dst_bus"));
    m.def("slice_mapping", &slice_mapping, py::arg("src_wire"), py::arg("start_bit"),
          py::arg("length"), py::arg("dst_wire"));
    m.def("sign_extend_mapping", &sign_extend_mapping, py::arg("src_wire"), py::arg("src_width"),
          py::arg("dst_wire"), py::arg("dst_width"));

    bindBasicModule<BitSplitter<1>>(m, "BitSplitter1", "Split one 1-bit input into one single-bit output.");
    bindBasicModule<BitJoiner<1>>(m, "BitJoiner1", "Join one single-bit input into one 1-bit output.");
    bindBasicModule<BitSplitter<2>>(m, "BitSplitter2", "Split one 2-bit input into two single-bit outputs.");
    bindBasicModule<BitJoiner<2>>(m, "BitJoiner2", "Join two single-bit inputs into one 2-bit output.");
    bindBasicModule<BitSplitter<3>>(m, "BitSplitter3", "Split one 3-bit input into three single-bit outputs.");
    bindBasicModule<BitJoiner<3>>(m, "BitJoiner3", "Join three single-bit inputs into one 3-bit output.");
    bindBasicModule<BitSplitter<4>>(m, "BitSplitter4", "Split one 4-bit input into four single-bit outputs.");
    bindBasicModule<BitJoiner<4>>(m, "BitJoiner4", "Join four single-bit inputs into one 4-bit output.");
    bindBasicModule<BitSplitter<8>>(m, "BitSplitter8", "Split one 8-bit input into eight single-bit outputs.");
    bindBasicModule<BitJoiner<8>>(m, "BitJoiner8", "Join eight single-bit inputs into one 8-bit output.");
    bindBasicModule<BitSplitter<16>>(m, "BitSplitter16", "Split one 16-bit input into sixteen single-bit outputs.");
    bindBasicModule<BitJoiner<16>>(m, "BitJoiner16", "Join sixteen single-bit inputs into one 16-bit output.");
    bindBasicModule<BitSplitter<32>>(m, "BitSplitter32", "Split one 32-bit input into thirty-two single-bit outputs.");
    bindBasicModule<BitJoiner<32>>(m, "BitJoiner32", "Join thirty-two single-bit inputs into one 32-bit output.");
    bindConstantModule<ConstantValue<1, 1>>(m, "ConstantValue1High", "Single-bit constant with a single-bit trigger.");
    bindConstantModule<ConstantValue<1, 8>>(m, "ConstantValue1From8Trigger", "Single-bit constant with an 8-bit trigger.");
    bindConstantModule<ConstantValue<8, 8>>(m, "ConstantValue8", "8-bit constant with an 8-bit trigger.");
}
