#include "bindings/BasicComponentBinding.hpp"
#include "components/BasicComponent.hpp"
#include "simulator/Simulator.hpp"

// Trampoline class for BasicComponent
// It must provide overrides for ALL pure virtual functions in its hierarchy.
class PyBasicComponent : public BasicComponent {
public:
    using BasicComponent::BasicComponent;

    // Override `initPins` from the IOComponent base class
    void initPins(std::shared_ptr<IOComponent> self_ptr) override {
        PYBIND11_OVERRIDE_PURE(
            void,               // Return type
            BasicComponent,     // C++ base class
            initPins,           // Function name
            self_ptr            // Arguments
        );
    }

    // Override `evaluate` from the BasicComponent class
    void evaluate(size_t current_time, Simulator& simulator) override {
        PYBIND11_OVERRIDE_PURE(
            void,               // Return type
            BasicComponent,     // C++ base class
            evaluate,           // Function name
            current_time,       // Argument 1
            simulator           // Argument 2
        );
    }
};


void bindBasicComponent(py::module_& m) {
    py::class_<BasicComponent, PyBasicComponent, IOComponent, std::shared_ptr<BasicComponent>>(m, "BasicComponent")
        .def(py::init<std::string, size_t>(), py::arg("name"), py::arg("delay_val") = 1)
        // `evaluate` and `initPins` are not bound directly. They are meant
        // to be overridden in Python classes that inherit from this one.
        ;
}