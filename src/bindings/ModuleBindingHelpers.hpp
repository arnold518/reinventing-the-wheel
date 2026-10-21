#pragma once
#include "Bindings.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/BasicComponent.hpp"
#include <pybind11/stl.h>

// Private binding templates shared by the bounded compilation groups.
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

// Called in the original Python registration order by bindModules().
void bindBasicModules(py::module_& m);
void bindStorageModules(py::module_& m);
void bindPipelineModules(py::module_& m);
void bindCpuModules(py::module_& m);
void bindArithmeticModules(py::module_& m);
void bindUtilityModules(py::module_& m);
