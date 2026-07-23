#include "Bindings.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Simulator.hpp"

class PyBasicComponent : public BasicComponent {
public:
    using BasicComponent::BasicComponent;

    void evaluate(size_t current_time, Simulator& simulator) override {
        PYBIND11_OVERRIDE_PURE(void, BasicComponent, evaluate, current_time, simulator);
    }
};

void bindBasicComponent(py::module_& m) {
    py::class_<BasicComponent, PyBasicComponent, IOComponent,
               std::shared_ptr<BasicComponent>>(
        m, "BasicComponent",
        "A directly evaluated component. Fidelity is supplied by build metadata.",
        py::module_local(false))
        .def("get_delay", &BasicComponent::getDelay,
            "Gets the propagation delay of the component.")
        .def("get_type_name", &BasicComponent::getTypeName,
            "Returns the dynamic type name of the component instance.");
}
