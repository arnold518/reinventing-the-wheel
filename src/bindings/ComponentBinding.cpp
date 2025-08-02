#include "Bindings.hpp"
#include <pybind11/stl.h>          // For std::vector conversion
#include "components/Component.hpp" // The class we are binding
#include "basic/Wire.hpp"

void bindComponent(py::module_& m) {
    py::class_<Component, std::shared_ptr<Component>>(m, "Component", "The base class for all circuit elements.")
        
        // --- Essential Getters for Structure Discovery ---

        .def("get_name", &Component::getName, 
            "Returns the name of the component.")
        
        .def("get_id", &Component::getID, 
            "Returns a unique identifier for the component.")

        .def("get_parent", &Component::getParent, 
            "Returns the parent component, or None if it's a root component.")

        .def("get_children", &Component::getChildren, py::return_value_policy::reference_internal,
            "Returns a list of the child components.")

        .def("get_wires", &Component::getWires, py::return_value_policy::reference_internal,
            "Returns a list of wires connected within this component's scope.")

        // --- Type Information ---

        .def_property_readonly_static("type_name", [](py::object /* self */) { 
            return Component::TypeName; 
        }, "Returns the static type name of the class.");
}