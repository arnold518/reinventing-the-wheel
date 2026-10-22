#include "ModuleBindingHelpers.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/storage/Register.hpp"
#include "modules/storage/RegisterFile.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"

void bindStorageModules(py::module_& m) {
    bindCompositeModule<RegisterFile32x32>(m,"RegisterFile32x32","RV32I architectural register file using the generic storage recipe.");
    bindCompositeModule<FlipFlopCell>(m, "FlipFlopCell", "Structural one-bit storage cell with write enable and reset.");
    bindCompositeModule<Register<1>>(m, "RegisterW1", "Whole-word 1-bit register built from flip-flop cells.");
    bindCompositeModule<Register<2>>(m, "RegisterW2", "Whole-word 2-bit register built from flip-flop cells.");
    bindCompositeModule<Register<3>>(m, "RegisterW3", "Whole-word 3-bit register built from flip-flop cells.");
    bindCompositeModule<Register<4>>(m, "RegisterW4", "Whole-word 4-bit register built from flip-flop cells.");
    bindCompositeModule<Register<5>>(m, "RegisterW5", "Whole-word 5-bit register built from flip-flop cells.");
    bindCompositeModule<Register<8>>(m, "RegisterW8", "Whole-word 8-bit register built from flip-flop cells.");
    bindCompositeModule<Register<16>>(m, "RegisterW16", "Whole-word 16-bit register built from flip-flop cells.");
    bindCompositeModule<Register<32>>(m, "RegisterW32", "Whole-word 32-bit register built from flip-flop cells.");
    m.def("create_register_file", [](const std::string& name,size_t width,size_t count,bool hardwire_zero){
        return std::dynamic_pointer_cast<IOComponent>(circuit::storage::registerFile(width,count,{hardwire_zero}).create(circuit::Fidelity::Structural,name,nullptr));
    },py::arg("name"),py::arg("width"),py::arg("count"),py::arg("hardwire_zero")=true);
}
