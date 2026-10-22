#include "components/capabilities/RegisterStateView.hpp"
#include "modules/composite/WordDatapath.hpp"
#include "Bindings.hpp"
#include "visualizer/DeviceInspector.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <algorithm>
#include "simulator/Simulator.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include <pybind11/stl.h>
void bindRecordedGuest(py::module_& m) {
    m.def("create_device_component", [](const std::string& id,const std::string& name,
            const circuit::ParameterMap& parameters,const std::string& fidelity) {
        if(!id.starts_with("device.")&&!id.starts_with("bus.word.address-router.")&&!id.starts_with("bus.word.error-target.")&&!id.starts_with("bus.word.instruction-guard.")&&!id.starts_with("video.pixel-selector."))
            throw std::invalid_argument("Expected a device, device bus or pixel-selector contract");
        if(fidelity!="structural"&&fidelity!="behavioral")throw std::invalid_argument("Invalid device fidelity");
        const auto selected=fidelity=="structural"?circuit::Fidelity::Structural:circuit::Fidelity::Behavioral;
        const auto& catalog=circuit::builtinComponentCatalog();
        const auto implementations=catalog.implementationsFor(id);
        if(std::none_of(implementations.begin(),implementations.end(),[&](const auto& impl){return impl.fidelity==selected&&impl.supports(parameters);}))
            throw std::invalid_argument("Unsupported device contract, parameters or fidelity");
        return std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({id,name,parameters,{},{},{}},
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(),name,selected)).root);
    },py::arg("contract"),py::arg("name"),py::arg("parameters")=circuit::ParameterMap{},py::arg("fidelity")="structural");
    m.def("create_alu",[](const std::string& name,size_t width,const std::string& operations,
                          const std::string& adder,bool full_flags,const std::string& fidelity){
        using namespace circuit::word;
        if(operations!="extended"&&operations!="integer")throw std::invalid_argument("Invalid ALU operation contract");
        if(adder!="ripple"&&adder!="prefix")throw std::invalid_argument("Invalid ALU adder topology");
        if(fidelity!="structural"&&fidelity!="behavioral")throw std::invalid_argument("Invalid ALU fidelity");
        const auto& f=circuit::families::alu(width,{operations=="extended"?Operations::Extended:Operations::IntegerDatapath,
            adder=="ripple"?AdderTopology::Ripple:AdderTopology::ParallelPrefix,full_flags});
        const auto selected=fidelity=="structural"?circuit::Fidelity::Structural:circuit::Fidelity::Behavioral;
        if(!f.supports(selected))throw std::invalid_argument("This ALU configuration has no requested fidelity");
        return std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(f.request(name),
            circuit::withExactFidelity(circuit::canonicalDefaultProfile(),name,selected)).root);
    },py::arg("name"),py::arg("width"),py::arg("operations")="extended",py::arg("adder")="prefix",py::arg("full_flags")=true,py::arg("fidelity")="structural");
    m.def("supports_register_view",[](const std::shared_ptr<Component>& p){return bool(std::dynamic_pointer_cast<RegisterStateView>(p));});
    m.def("device_view_kind", &circuit::visualizer::deviceViewKind);
    m.def("component_layout_variant", &circuit::visualizer::componentLayoutVariant);
    m.def("device_state_json", &circuit::visualizer::deviceStateJson,
        py::arg("component"),py::arg("simulator"),py::arg("index"),py::arg("time"),
        py::arg("current")=false);
    m.def("recorded_guest_names", &circuit::runtime::recordedGuestNames);
    m.def("run_recorded_guest_json", [](const std::string& name,bool caches,bool dram,size_t max_cycles){
            return circuit::runtime::runRecordedGuestFixtureJson(name,caches,dram,max_cycles);
        },
        py::arg("name"), py::arg("caches")=false, py::arg("dram")=false,
        py::arg("max_cycles")=500000);
}
