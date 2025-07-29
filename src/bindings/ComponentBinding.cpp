#include "bindings/ComponentBinding.hpp"

#include <pybind11/stl.h>
#include "components/Component.hpp"

void bindComponent(py::module_& m) {
    py::class_<Component, std::shared_ptr<Component>>(m, "Component")
        .def(py::init<std::string>(), py::arg("name"))
        .def("get_name", &Component::getName)
        .def("get_id", &Component::getID)
        .def("get_parent", &Component::getParent)
        .def("get_children", &Component::getChildren, py::return_value_policy::reference_internal)
        .def("add_child", &Component::addChild, py::arg("child"));
}