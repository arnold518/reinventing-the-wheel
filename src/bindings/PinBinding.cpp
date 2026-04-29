#include "Bindings.hpp"
#include "basic/PinBase.hpp"
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

    py::class_<PinBase, std::shared_ptr<PinBase>>(m, "PinBase", "Runtime-width pin interface.", py::module_local(false))
        .def("get_name", &PinBase::getName)
        .def("get_type", &PinBase::getType)
        .def("get_owner", &PinBase::getOwner,
            "Returns the Component that owns this pin.")
        .def("get_external_wire", &PinBase::getExternalWireBase,
            "Returns the external Wire connected to this pin, or None.")
        .def("get_internal_wire", &PinBase::getInternalWireBase,
            "Returns the internal Wire connected to this pin, or None.")
        .def("get_value", &PinBase::getValue)
        .def("get_width", &PinBase::getWidth)
        .def("get_bit", &PinBase::getBit, py::arg("index"))
        .def("get_value_as_uint64", &PinBase::getValueAsUInt64)
        .def("get_id", &PinBase::getID);

    py::class_<Pin<>, PinBase, std::shared_ptr<Pin<>>>(m, "Pin", "Represents an input or output point on a component.", py::module_local(false))
        
        // --- Essential Getters for Structure and State ---
        
        .def("get_name", &Pin<>::getName)
        
        .def("get_type", &Pin<>::getType)

        .def("get_owner", &Pin<>::getOwner,
            "Returns the Component that owns this pin.")

        .def("get_external_wire", &Pin<>::getExternalWire,
            "Returns the external Wire connected to this pin, or None.")
        
        .def("get_internal_wire", &Pin<>::getInternalWire,
            "Returns the internal Wire connected to this pin, or None.")
        
        .def("get_value", &Pin<>::getValue)

        .def("get_id", &Pin<>::getID);
}
