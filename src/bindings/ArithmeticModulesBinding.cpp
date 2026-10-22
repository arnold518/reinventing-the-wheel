#include "ModuleBindingHelpers.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <map>

void bindArithmeticModules(py::module_& m) {
    m.def("create_word_component", [](const std::string& unit,const std::string& name,size_t width,
            const std::string& operations,const std::string& adder,bool full_flags,const std::string& fidelity) {
        using namespace circuit::word;
        static const std::map<std::string,Unit> units{{"alu",Unit::ALU},{"adder",Unit::Adder},{"add-sub",Unit::AddSub},
            {"arithmetic",Unit::Arithmetic},{"logic",Unit::Logic},{"shift",Unit::Shift},{"compare",Unit::Compare},
            {"flags",Unit::Flags},{"result",Unit::Result},{"zero",Unit::Zero}};
        if(!units.contains(unit))throw std::invalid_argument("Invalid word component");
        if(operations!="extended"&&operations!="integer")throw std::invalid_argument("Invalid operation contract");
        if(adder!="ripple"&&adder!="prefix")throw std::invalid_argument("Invalid addition topology");
        if(fidelity!="structural"&&fidelity!="behavioral")throw std::invalid_argument("Invalid fidelity");
        const auto& f=family(units.at(unit),width,{operations=="extended"?Operations::Extended:Operations::IntegerDatapath,
            adder=="prefix"?AdderTopology::ParallelPrefix:AdderTopology::Ripple,full_flags});
        const auto selected=fidelity=="structural"?circuit::Fidelity::Structural:circuit::Fidelity::Behavioral;
        if(!f.supports(selected))throw std::invalid_argument("No requested fidelity for this configuration");
        return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(f.request(name),
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(),name,selected)).root);
    },py::arg("unit"),py::arg("name"),py::arg("width"),py::arg("operations")="extended",py::arg("adder")="prefix",
       py::arg("full_flags")=true,py::arg("fidelity")="structural");
    m.def("create_alu_control", [](const std::string& name,const std::string& operations) {
        using namespace circuit::word;
        if(operations!="extended"&&operations!="integer")throw std::invalid_argument("Invalid operation contract");
        return std::dynamic_pointer_cast<IOComponent>(controlFamily(operations=="extended"?Operations::Extended:Operations::IntegerDatapath)
            .create(circuit::Fidelity::Structural,name,nullptr));
    },py::arg("name"),py::arg("operations")="extended");
}
