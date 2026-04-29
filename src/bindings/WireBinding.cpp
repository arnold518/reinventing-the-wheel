#include "Bindings.hpp"
#include <pybind11/stl.h>      // For std::vector conversion
#include "basic/WireBase.hpp"
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include "components/Component.hpp"


void bindWire(py::module_& m) {
    py::class_<WireBase, std::shared_ptr<WireBase>>(m, "WireBase", "Runtime-width wire interface.", py::module_local(false))
        .def("get_name", &WireBase::getName)
        .def("get_id", &WireBase::getID)
        .def("get_value", &WireBase::getSingleValue)
        .def("get_numeric_value", &WireBase::getValue)
        .def("get_width", &WireBase::getWidth)
        .def("get_bit", &WireBase::getBit, py::arg("index"))
        .def("get_owner", &WireBase::getOwner,
            "Returns the Component that owns this wire.")
        .def("get_source_pin", &WireBase::getSourcePinBase,
            "Returns the single Pin that is the source of the signal for this wire.")
        .def("get_sink_pins", &WireBase::getSinkPinsBaseForPython,
            "Returns a list of strong references to the sink pins.");

    py::class_<Wire<>, WireBase, std::shared_ptr<Wire<>>>(m, "Wire", "Represents a connection between a source pin and one or more sink pins.", py::module_local(false))

        // --- Essential Getters for Structure and State ---

        .def("get_name", &Wire<>::getName)
        
        .def("get_id", &Wire<>::getID)

        .def("get_value", &Wire<>::getSingleValue)

        .def("get_owner", &Wire<>::getOwner,
            "Returns the Component that owns this wire.")

        .def("get_source_pin", &Wire<>::getSourcePin,
            "Returns the single Pin that is the source of the signal for this wire.")

        // We bind getSinkPins, but the result will be a list of weak_ptr.
        // Python code will need to call .lock() on each element to get a shared_ptr.
        // .def("get_sink_pins", &Wire::getSinkPins, py::return_value_policy::reference_internal,
        //     "Returns a list of weak references to the sink pins.");
            
        .def("get_sink_pins", &Wire<>::getSinkPinsForPython,
            "Returns a list of strong references to the sink pins.");
}
