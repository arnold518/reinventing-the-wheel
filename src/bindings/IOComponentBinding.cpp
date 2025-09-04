#include "Bindings.hpp"
#include <pybind11/stl.h>           // For std::map conversion
#include "components/IOComponent.hpp"

class PyIOComponent : public IOComponent {
public:
    using IOComponent::IOComponent; // Inherit constructors

    // Provide the override for the pure virtual method.
    void initPins(std::shared_ptr<IOComponent> self_ptr) override {
        PYBIND11_OVERRIDE_PURE(void, IOComponent, initPins, self_ptr);
    }
};

void bindIOComponent(py::module_& m) {
    py::class_<IOComponent, PyIOComponent, Component, std::shared_ptr<IOComponent>>(m, "IOComponent", "An abstract component with input and output pins.", py::module_local(false))
        
        // --- Essential Getters for Structure and State ---
        
        .def("get_delay", &IOComponent::getDelay,
            "Gets the propagation delay of the component.")
        
        .def("get_input_pins", &IOComponent::getInputPins, py::return_value_policy::reference_internal,
            "Returns a map of the component's input pins.")
            
        .def("get_output_pins", &IOComponent::getOutputPins, py::return_value_policy::reference_internal,
            "Returns a map of the component's output pins.")
            
        .def("get_input_value", &IOComponent::getInputValue, py::arg("pin_name"),
            "Gets the current logic value of a specific input pin.")

        // --- Type Information ---

        .def("get_type_name", &IOComponent::getTypeName,
            "Returns the dynamic type name of the component instance.");
}