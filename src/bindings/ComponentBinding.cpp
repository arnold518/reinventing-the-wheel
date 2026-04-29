#include "Bindings.hpp"
#include <pybind11/stl.h>          // For std::vector conversion
#include "components/Component.hpp" // The class we are binding
#include "basic/Wire.hpp"
#include "basic/WireBase.hpp"

void bindComponent(py::module_& m) {
    py::class_<Component, std::shared_ptr<Component>>(m, "Component", "The base class for all circuit elements.", py::module_local(false))
        
        // --- Essential Getters for Structure Discovery ---

        .def("get_name", &Component::getName, 
            "Returns the name of the component.")
        
        .def("get_id", &Component::getID, 
            "Returns a unique identifier for the component.")

        .def("get_parent", &Component::getParent, 
            "Returns the parent component, or None if it's a root component.")

        .def("get_children", &Component::getChildren, py::return_value_policy::reference_internal,
            "Returns a list of the child components.")

        .def("get_wires", &Component::getAllWires, py::return_value_policy::reference_internal,
            "Returns a list of wires connected within this component's scope.")

        .def("get_all_wires", &Component::getAllWires, py::return_value_policy::reference_internal,
            "Returns a list of all wires, including multi-bit wires, connected within this component's scope.")

        // --- Type Information ---

        .def("get_type_name", &Component::getTypeName,
            "Returns the dynamic type name of the component instance.");
}
