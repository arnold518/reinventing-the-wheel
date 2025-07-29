#include "bindings/IOComponentBinding.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Simulator.hpp"

// Trampoline class for IOComponent
// This allows Python classes to inherit from IOComponent and override virtual functions.
class PyIOComponent : public IOComponent {
public:
    // Inherit the constructors from IOComponent
    using IOComponent::IOComponent;

    // PYBIND11_OVERRIDE_PURE is used for pure virtual functions.
    // It redirects the call from C++ to the Python implementation.
    void initPins(std::shared_ptr<IOComponent> self_ptr) override {
        PYBIND11_OVERRIDE_PURE(
            void,           // Return type
            IOComponent,    // C++ base class
            initPins,       // Function name
            self_ptr        // Arguments
        );
    }
};

void bindIOComponent(py::module_& m) {
    // Note on IOComponent::create:
    // The static template function `IOComponent::create` cannot be bound directly to Python
    // as templates must be instantiated with specific types at compile time.
    // The factory logic should be handled in Python or by binding specific instantiations.

    py::class_<IOComponent, PyIOComponent, Component, std::shared_ptr<IOComponent>>(m, "IOComponent")
        .def(py::init<std::string, size_t>(), py::arg("name"), py::arg("delay_val") = 1)
        .def("get_delay", &IOComponent::getDelay)
        .def("get_input_pin", &IOComponent::getInputPin, py::arg("pin_name"))
        .def("get_output_pin", &IOComponent::getOutputPin, py::arg("pin_name"))
        .def("get_input_value", &IOComponent::getInputValue, py::arg("pin_name"))
        // The pure virtual function `initPins` is not bound here;
        // it is expected to be implemented by a derived Python class.
        ;
}