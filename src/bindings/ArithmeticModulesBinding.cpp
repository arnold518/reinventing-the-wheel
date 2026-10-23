#include "ModuleBindingHelpers.hpp"
#include "modules/composite/WordComponent.hpp"
#include "modules/composite/ALUControlDecoder.hpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <map>

namespace {
circuit::Fidelity fidelityOf(const std::string& fidelity) {
    if (fidelity == "structural") return circuit::Fidelity::Structural;
    if (fidelity == "behavioral") return circuit::Fidelity::Behavioral;
    throw std::invalid_argument("Invalid fidelity");
}
std::shared_ptr<IOComponent> createRoot(const circuit::ComponentFamily& f, const std::string& name, const std::string& fidelity) {
    const auto selected = fidelityOf(fidelity);
    if (!f.supports(selected)) throw std::invalid_argument("No requested fidelity for this component");
    return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(f.request(name),
        circuit::withExactFidelity(circuit::canonicalDefaultProfile(), name, selected)).root);
}
}

void bindArithmeticModules(py::module_& m) {
    m.def("create_word_component", [](const std::string& unit, const std::string& name, size_t width, const std::string& fidelity) {
        using circuit::word::Unit;
        static const std::map<std::string, Unit> units{{"alu", Unit::ALU}, {"add-sub", Unit::AddSub},
            {"arithmetic", Unit::Arithmetic}, {"logic", Unit::Logic}, {"shift", Unit::Shift}, {"compare", Unit::Compare},
            {"flags", Unit::Flags}, {"result", Unit::Result}, {"ripple-adder", Unit::RippleAdder},
            {"prefix-adder", Unit::PrefixAdder}, {"zero", Unit::Zero}};
        if (!units.contains(unit)) throw std::invalid_argument("Invalid word component");
        return createRoot(circuit::word::family(units.at(unit), width), name, fidelity);
    }, py::arg("unit"), py::arg("name"), py::arg("width"), py::arg("fidelity") = "structural",
       "Creates a Chapter 2 word component (W = 8, 16 or 32) at the requested fidelity.");
    m.def("create_alu_control", [](const std::string& name) {
        return std::dynamic_pointer_cast<IOComponent>(circuit::families::aluControlDecoder()
            .create(circuit::Fidelity::Structural, name, nullptr));
    }, py::arg("name"), "Creates the structural ALUControlDecoder.");
    m.def("create_rv32i_alu", [](const std::string& name, const std::string& fidelity) {
        return createRoot(circuit::families::rv32iALU(), name, fidelity);
    }, py::arg("name"), py::arg("fidelity") = "structural", "Creates the RV32I execute ALU.");
}
