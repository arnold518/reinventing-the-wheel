#include "Bindings.hpp"
#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"

// It's good practice to bind related enums in the same file.
void bindPinTypeEnum(py::module_& m) {
    py::enum_<PinType>(m, "PinType", "The direction of a pin.")
        .value("INPUT", PinType::INPUT)
        .value("OUTPUT", PinType::OUTPUT)
        .export_values(); // Makes the enum members available in the module's namespace
}

void bindPin(py::module_& m) {
    // Bind the enum first so the Pin binding can use it.
    bindPinTypeEnum(m);

    py::class_<Pin, std::shared_ptr<Pin>>(m, "Pin", "Represents an input or output point on a component.")
        
        // --- Essential Getters for Structure and State ---
        
        .def("get_name", &Pin::getName)
        
        .def("get_type", &Pin::getType)

        .def("get_owner", &Pin::getOwner, 
            "Returns the Component that owns this pin.")

        .def("get_connected_wire", &Pin::getConnectedWire,
            "Returns the Wire connected to this pin, or None.")
        
        .def("get_value", &Pin::getValue)

        .def("get_id", &Pin::getID);
}