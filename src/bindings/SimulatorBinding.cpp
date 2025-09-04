#include "Bindings.hpp"
#include "simulator/Simulator.hpp"

void bindSimulator(py::module_& m) {
    // A minimal binding just to make the type known to Pybind11.
    py::class_<Simulator, std::shared_ptr<Simulator>>(m, "Simulator", py::module_local(false));
}