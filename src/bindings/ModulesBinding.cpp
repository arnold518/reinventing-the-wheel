#include "Bindings.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "components/BasicComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/ClockGenerator.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/memory/Register32BitCellArray.hpp"
#include "modules/memory/Memory4x32.hpp"
#include "modules/memory/Memory32x32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile4x32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IReferenceCore.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "modules/rv32i/RV32IReferenceSystem.hpp"

#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/Shifter8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "rv32i/RV32IDecoder.hpp"

namespace py = pybind11;

template<typename T, typename Base>
void bindModule(py::module_& m, const char* name, const char* description) {
    py::class_<T, Base, std::shared_ptr<T>>(m, name, description, py::module_local(false))
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
void bindBasicModule(py::module_& m, const char* name, const char* description) {
    bindModule<T, BasicComponent>(m, name, description);
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
        .def("get_output_pins", &T::getAllOutputPins, py::return_value_policy::reference_internal)
        .def("get_constant_value", &T::getConstantValue);
}

template<typename T>
void bindCompositeModule(py::module_& m, const char* name, const char* description) {
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
    m.def(
        "disassemble_rv32i_instruction",
        &rv32i::RV32IDecoder::disassemble,
        py::arg("raw"),
        py::arg("pc") = 0,
        "Returns a compact RV32I assembly string for a 32-bit instruction word.");
    m.def(
        "get_register_state_at_time",
        [](const std::shared_ptr<Component>& component, size_t target_time) {
            auto view = std::dynamic_pointer_cast<RegisterStateView>(component);
            if (!view) {
                throw std::invalid_argument(
                    "Component does not provide the register-state-view capability");
            }
            return view->getRegisterStateAtTime(target_time);
        },
        py::arg("component"),
        py::arg("time"),
        "Returns register state through the shared register-state-view capability.");

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

    bindCompositeModule<AND8>(m, "AND8", "8-bit bitwise AND.");
    bindCompositeModule<OR8>(m, "OR8", "8-bit bitwise OR.");
    bindCompositeModule<XOR8>(m, "XOR8", "8-bit bitwise XOR.");
    bindCompositeModule<NOT8>(m, "NOT8", "8-bit bitwise NOT.");
    bindCompositeModule<NAND8>(m, "NAND8", "8-bit bitwise NAND.");
    bindCompositeModule<NOR8>(m, "NOR8", "8-bit bitwise NOR.");

    bindCompositeModule<Mux2to1>(m, "Mux2to1", "2:1 one-bit multiplexer.");
    bindCompositeModule<Mux4to1>(m, "Mux4to1", "4:1 one-bit multiplexer.");
    bindCompositeModule<Mux8to1>(m, "Mux8to1", "8:1 one-bit multiplexer.");
    bindCompositeModule<Mux16to1>(m, "Mux16to1", "16:1 one-bit multiplexer.");
    bindCompositeModule<Mux32to1>(m, "Mux32to1", "32:1 one-bit multiplexer.");
    bindCompositeModule<Mux2to1_8bit>(m, "Mux2to1_8bit", "2:1 8-bit multiplexer.");
    bindCompositeModule<Mux2to1_4bit>(m, "Mux2to1_4bit", "2:1 4-bit multiplexer.");
    bindCompositeModule<Mux2to1_32bit>(m, "Mux2to1_32bit", "2:1 32-bit multiplexer.");
    bindCompositeModule<Mux4to1_8bit>(m, "Mux4to1_8bit", "4:1 8-bit multiplexer.");
    bindCompositeModule<Mux4to1_32bit>(m, "Mux4to1_32bit", "4:1 32-bit multiplexer.");
    bindCompositeModule<Mux8to1_8bit>(m, "Mux8to1_8bit", "8:1 8-bit multiplexer.");
    bindCompositeModule<Mux8to1_32bit>(m, "Mux8to1_32bit", "8:1 32-bit multiplexer.");
    bindCompositeModule<Mux16to1_8bit>(m, "Mux16to1_8bit", "16:1 8-bit multiplexer.");
    bindCompositeModule<Mux32to1_32bit>(m, "Mux32to1_32bit", "32:1 32-bit multiplexer.");
    bindCompositeModule<Decoder2to4>(m, "Decoder2to4", "2-bit enabled one-hot decoder.");
    bindCompositeModule<Decoder5to32>(m, "Decoder5to32", "5-bit enabled one-hot decoder.");

    py::class_<Memory64Kx32, BasicComponent, std::shared_ptr<Memory64Kx32>>(
        m,
        "Memory64Kx32",
        "Behavioral 64K-word 32-bit byte-addressed RV32I memory.",
        py::module_local(false))
        .def(py::init([](const std::string& instance_name) {
            return Component::create<Memory64Kx32>(instance_name);
        }), py::arg("name"))
        .def("get_name", &Memory64Kx32::getName)
        .def("get_parent", &Memory64Kx32::getParent)
        .def("get_children", &Memory64Kx32::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &Memory64Kx32::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &Memory64Kx32::getAllOutputPins, py::return_value_policy::reference_internal)
        .def_static("capacity_bytes", &Memory64Kx32::capacityBytes)
        .def_static("capacity_words", &Memory64Kx32::capacityWords)
        .def(
            "get_touched_words_at_time",
            &Memory64Kx32::getTouchedWordsAtTime,
            py::arg("time"),
            py::arg("max_words") = 64,
            "Returns 32-bit words touched since the effective reset at the requested simulation time.")
        .def(
            "get_touched_word_count_at_time",
            &Memory64Kx32::getTouchedWordCountAtTime,
            py::arg("time"),
            "Counts words touched since the effective reset at the requested simulation time.")
        .def(
            "get_occupied_words_at_time",
            &Memory64Kx32::getOccupiedWordsAtTime,
            py::arg("time"),
            py::arg("max_words") = 64,
            "Deprecated alias for get_touched_words_at_time.")
        .def(
            "get_occupied_word_count_at_time",
            &Memory64Kx32::getOccupiedWordCountAtTime,
            py::arg("time"),
            "Deprecated alias for get_touched_word_count_at_time.")
        .def(
            "get_words_at_time",
            &Memory64Kx32::getWordsAtTime,
            py::arg("time"),
            py::arg("base_address"),
            py::arg("word_count"),
            "Returns a word-aligned memory window at the requested simulation time.");
    bindCompositeModule<Register32BitCellArray>(m, "Register32BitCellArray", "Structural 32-bit register assembled from bit cells.");
    bindCompositeModule<MemoryBit>(m, "MemoryBit", "Structural one-bit storage cell with write enable and reset.");
    bindCompositeModule<Register32>(m, "Register32", "Structural 32-bit register built from MemoryBit cells.");
    bindCompositeModule<RegisterFile4x32>(m, "RegisterFile4x32", "Four-entry 32-bit register file built from behavioral register cells.");
    bindCompositeModule<RegisterFile32x32>(m, "RegisterFile32x32", "32-entry 32-bit register file built from behavioral register cells.");
    bindCompositeModule<Memory4x32>(m, "Memory4x32", "Four-word 32-bit memory slice with CPU-facing memory pins.");
    bindCompositeModule<Memory32x32>(m, "Memory32x32", "Thirty-two-word 32-bit memory slice with CPU-facing memory pins.");
    bindBasicModule<RV32IReferenceCore>(m, "RV32IReferenceCore", "Oracle-backed RV32I answer-sheet core with visible instruction/data memory bus pins.");
    bindCompositeModule<RV32IControlFlowUnit>(m, "RV32IControlFlowUnit", "Structural RV32I PC, branch, and jump block.");
    bindCompositeModule<RV32IDecodeControlUnit>(m, "RV32IDecodeControlUnit", "Structural RV32I field, immediate, and control decoder.");
    bindCompositeModule<RV32IExecutionControlStatusUnit>(m, "RV32IExecutionControlStatusUnit", "Structural RV32I commit permission, memory handshake, halt, and trap-state block.");
    py::class_<RV32IBitPatternMatcher, IOComponent, std::shared_ptr<RV32IBitPatternMatcher>>(
        m,
        "RV32IBitPatternMatcher",
        "Structural masked RV32I instruction-pattern matcher.",
        py::module_local(false))
        .def(py::init([](const std::string& instance_name, uint32_t mask, uint32_t value) {
            return Component::create<RV32IBitPatternMatcher>(instance_name, mask, value);
        }), py::arg("name"), py::arg("mask"), py::arg("value"))
        .def("get_name", &RV32IBitPatternMatcher::getName)
        .def("get_parent", &RV32IBitPatternMatcher::getParent)
        .def("get_children", &RV32IBitPatternMatcher::getChildren, py::return_value_policy::reference_internal)
        .def("get_input_pins", &RV32IBitPatternMatcher::getAllInputPins, py::return_value_policy::reference_internal)
        .def("get_output_pins", &RV32IBitPatternMatcher::getAllOutputPins, py::return_value_policy::reference_internal)
        .def_property_readonly("mask", &RV32IBitPatternMatcher::mask)
        .def_property_readonly("value", &RV32IBitPatternMatcher::value);
    bindCompositeModule<RV32ISingleCycleCore>(m, "RV32ISingleCycleCore", "Structural single-cycle RV32I core composed from the five educational blocks.");
    bindCompositeModule<RV32ISingleCycleSystem>(m, "RV32ISingleCycleSystem", "Structural RV32I core with separate behavioral instruction and data memories.");
    bindCompositeModule<RV32IReferenceSystem>(m, "RV32IReferenceSystem", "RV32I answer-sheet system containing the reference core and instruction/data memories.");

    bindCompositeModule<Adder8>(m, "Adder8", "8-bit adder.");
    bindCompositeModule<TwosComplement8>(m, "TwosComplement8", "8-bit two's complement.");
    bindCompositeModule<Subtractor8>(m, "Subtractor8", "8-bit subtractor.");
    bindCompositeModule<SubtractorWithBorrow8>(m, "SubtractorWithBorrow8", "8-bit subtractor with borrow.");
    bindCompositeModule<Incrementer8>(m, "Incrementer8", "8-bit incrementer.");
    bindCompositeModule<Decrementer8>(m, "Decrementer8", "8-bit decrementer.");
    bindCompositeModule<EqualityChecker8>(m, "EqualityChecker8", "8-bit equality checker.");
    bindCompositeModule<Comparator8>(m, "Comparator8", "8-bit unsigned comparator.");
    bindCompositeModule<SignedComparator8>(m, "SignedComparator8", "8-bit signed comparator.");
    bindCompositeModule<ShiftLeftLogical8>(m, "ShiftLeftLogical8", "8-bit logical shift left.");
    bindCompositeModule<ShiftRightLogical8>(m, "ShiftRightLogical8", "8-bit logical shift right.");
    bindCompositeModule<ShiftRightArithmetic8>(m, "ShiftRightArithmetic8", "8-bit arithmetic shift right.");
    bindCompositeModule<ZeroDetect8>(m, "ZeroDetect8", "8-bit zero detector.");
    bindCompositeModule<ALU8>(m, "ALU8", "8-bit arithmetic logic unit.");
    bindCompositeModule<Adder32>(m, "Adder32", "Structural 32-bit adder.");
    bindCompositeModule<AddSub32>(m, "AddSub32", "Structural 32-bit add/subtract unit.");
    bindCompositeModule<Logic32>(m, "Logic32", "Structural 32-bit bitwise logic unit.");
    bindCompositeModule<ZeroDetect32>(m, "ZeroDetect32", "Structural 32-bit zero detector.");
    bindCompositeModule<Comparator32>(m, "Comparator32", "Structural 32-bit comparator.");
    bindCompositeModule<Shifter32>(m, "Shifter32", "Structural 32-bit barrel shifter.");
    bindCompositeModule<ALU32>(m, "ALU32", "Structural RV32I-oriented 32-bit ALU.");

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
        .def("get_total_output_bits", &Rewire::getTotalOutputBits)
        .def("get_input_specs", &Rewire::getInputSpecs)
        .def("get_output_specs", &Rewire::getOutputSpecs)
        .def("get_bit_mappings", &Rewire::getBitMappings)
        .def("get_unmapped_default", &Rewire::getUnmappedDefault);

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
    bindBasicModule<BitSplitter<5>>(m, "BitSplitter5", "Split one 5-bit input into five single-bit outputs.");
    bindBasicModule<BitJoiner<5>>(m, "BitJoiner5", "Join five single-bit inputs into one 5-bit output.");
    bindBasicModule<BitSplitter<16>>(m, "BitSplitter16", "Split one 16-bit input into sixteen single-bit outputs.");
    bindBasicModule<BitJoiner<16>>(m, "BitJoiner16", "Join sixteen single-bit inputs into one 16-bit output.");
    bindBasicModule<BitSplitter<32>>(m, "BitSplitter32", "Split one 32-bit input into thirty-two single-bit outputs.");
    bindBasicModule<BitJoiner<32>>(m, "BitJoiner32", "Join thirty-two single-bit inputs into one 32-bit output.");
    bindConstantModule<ConstantValue<1>>(m, "ConstantValue1", "Single-bit constant source.");
    bindConstantModule<ConstantValue<2>>(m, "ConstantValue2", "2-bit constant source.");
    bindConstantModule<ConstantValue<4>>(m, "ConstantValue4", "4-bit constant source.");
    bindConstantModule<ConstantValue<8>>(m, "ConstantValue8", "8-bit constant source.");
    bindConstantModule<ConstantValue<32>>(m, "ConstantValue32", "32-bit constant source.");
}
