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
    // The second template argument, `PyIOComponent`, is the trampoline class.
    // The third, `Component`, is the C++ parent class.
    py::class_<IOComponent, PyIOComponent, Component, std::shared_ptr<IOComponent>>(m, "IOComponent", "An abstract component with input and output pins.")
        
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

        .def_property_readonly_static("type_name", [](py::object /* self */) { 
            return IOComponent::TypeName; 
        }, "Returns the static type name of the class.");
}