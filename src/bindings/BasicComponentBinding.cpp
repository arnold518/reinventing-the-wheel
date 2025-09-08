#include "Bindings.hpp"
#include "components/BasicComponent.hpp"
#include "simulator/Simulator.hpp" 

class PyBasicComponent : public BasicComponent {
public:
    using BasicComponent::BasicComponent; // Inherit constructors

    void initPins(std::shared_ptr<IOComponent> self_ptr) override {
        PYBIND11_OVERRIDE_PURE(void, BasicComponent, initPins, self_ptr);
    }

    void evaluate(size_t current_time, Simulator& simulator) override {
        PYBIND11_OVERRIDE_PURE(void, BasicComponent, evaluate, current_time, simulator);
    }
};

void bindBasicComponent(py::module_& m) {
    py::class_<BasicComponent, PyBasicComponent, IOComponent, std::shared_ptr<BasicComponent>>(m, "BasicComponent", "The abstract base for fundamental logic components.", py::module_local(false))
        
        // --- No new methods are bound here ---
        
        // --- Type Information ---

        .def("get_delay", &BasicComponent::getDelay,
            "Gets the propagation delay of the component.")
        
        .def("get_type_name", &BasicComponent::getTypeName,
            "Returns the dynamic type name of the component instance.");
}