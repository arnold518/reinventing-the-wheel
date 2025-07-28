#include <pybind11/pybind11.h>
namespace py = pybind11;

class Simulator {
public:
    Simulator(int ticks) : ticks(ticks) {}

    std::string tick() {
        return "Tick " + std::to_string(ticks++);
    }

private:
    int ticks;
};

PYBIND11_MODULE(simulation, m) {
    py::class_<Simulator>(m, "Simulator")
        .def(py::init<int>())
        .def("tick", &Simulator::tick);
}
