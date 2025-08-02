#include "Bindings.hpp"
#include "components/BasicComponent.hpp"

// We need the full definition of Simulator to create the trampoline for 'evaluate'.
#include "simulator/Simulator.hpp" 

// The trampoline is still required because the class is abstract.
// It must override ALL pure virtual functions from its entire inheritance hierarchy.
class PyBasicComponent : public BasicComponent {
public:
    using BasicComponent::BasicComponent; // Inherit constructors

    // Override from IOComponent's hierarchy
    void initPins(std::shared_ptr<IOComponent> self_ptr) override {
        PYBIND11_OVERRIDE_PURE(void, BasicComponent, initPins, self_ptr);
    }

    // Override from BasicComponent itself
    void evaluate(size_t current_time, Simulator& simulator) override {
        PYBIND11_OVERRIDE_PURE(void, BasicComponent, evaluate, current_time, simulator);
    }
};

void bindBasicComponent(py::module_& m) {
    // Note that the body of this binding is very sparse.
    py::class_<BasicComponent, PyBasicComponent, IOComponent, std::shared_ptr<BasicComponent>>(m, "BasicComponent", "The abstract base for fundamental logic components.")
        
        // --- No new methods are bound here ---
        // All necessary getters are inherited from Component and IOComponent.
        
        // --- Type Information ---
        .def_property_readonly_static("type_name", [](py::object /* self */) { 
            return BasicComponent::TypeName; 
        }, "Returns the static type name of the class.");
}