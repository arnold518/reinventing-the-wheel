#include "ModuleBindingHelpers.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"

void bindUtilityModules(py::module_& m) {
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
