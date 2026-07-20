#include "Bindings.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include <pybind11/stl.h>       // Required for std::vector conversion
#include "simulator/Simulator.hpp"
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

char logicToken(LogicValue value) {
    switch (value) {
        case LogicValue::LOW: return '0';
        case LogicValue::HIGH: return '1';
        case LogicValue::UNKNOWN: return 'X';
        case LogicValue::HIGH_Z: return 'Z';
        default: return 'X';
    }
}

std::string logicVectorValue(const std::vector<LogicValue>& values) {
    if (values.empty()) {
        return "X";
    }

    std::string result(values.size(), 'X');
    for (size_t index = 0; index < values.size(); ++index) {
        result[values.size() - index - 1] = logicToken(values[index]);
    }
    return result;
}

class VisualSignalSnapshot {
public:
    VisualSignalSnapshot(
        std::vector<std::shared_ptr<PinBase>> pins,
        std::vector<std::shared_ptr<WireBase>> wires)
        : pins_(std::move(pins)), wires_(std::move(wires)) {}

    std::pair<std::vector<std::string>, std::vector<std::string>> getValues() const {
        std::vector<std::string> pin_values;
        pin_values.reserve(pins_.size());
        for (const auto& pin : pins_) {
            pin_values.push_back(pin ? logicVectorValue(pin->getValueAsVector()) : "X");
        }

        std::vector<std::string> wire_values;
        wire_values.reserve(wires_.size());
        for (const auto& wire : wires_) {
            wire_values.push_back(wire ? logicVectorValue(wire->getValueVector()) : "X");
        }
        return {std::move(pin_values), std::move(wire_values)};
    }

private:
    std::vector<std::shared_ptr<PinBase>> pins_;
    std::vector<std::shared_ptr<WireBase>> wires_;
};

} // namespace

void bindSimulator(py::module_& m) {
    py::class_<VisualSignalSnapshot>(m, "VisualSignalSnapshot",
        "Collects pin and wire values in stable visualizer index order using one C++ call.")
        .def(py::init<
            std::vector<std::shared_ptr<PinBase>>,
            std::vector<std::shared_ptr<WireBase>>>(),
            py::arg("pins"), py::arg("wires"))
        .def("get_values", &VisualSignalSnapshot::getValues,
            py::call_guard<py::gil_scoped_release>(),
            "Returns exact pin and wire values as index-aligned strings.");

    py::class_<Simulator, std::shared_ptr<Simulator>>(m, "Simulator", "Manages the event-driven simulation and its history.", py::module_local(false))
        
        .def("run_and_record", &Simulator::runAndRecord, py::arg("max_time"),
            "Runs the simulation and records the full history of wire changes.")

        .def("advance_and_record", &Simulator::advanceAndRecord, py::arg("target_time"),
            "Continues the simulation up to an absolute target time without clearing future events.")
            
        .def("set_circuit_state_at_time", &Simulator::setCircuitStateAtTime, py::arg("target_time"),
            "Sets the state of all wires to match the recorded state at a specific time.")
            
        .def("get_unique_timestamps", &Simulator::getUniqueTimestamps,
            "Returns a sorted list of unique timestamps where state changes occurred.")
            
        .def("clear", &Simulator::clear,
            "Clears the event queue and the recorded history.")
            
        .def("get_current_time", &Simulator::getCurrentTime,
            "Returns the current simulation time as set by set_circuit_state_at_time.");
}
