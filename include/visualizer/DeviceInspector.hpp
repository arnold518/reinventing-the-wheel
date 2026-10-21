#pragma once
#include "simulator/SimulationTime.hpp"
#include <cstddef>
#include <memory>
#include <string>
class Component;
class Simulator;
namespace circuit::visualizer {
std::string componentLayoutVariant(const std::shared_ptr<Component>&);
// Read-only device observations at recorded time. No input injection or events.
bool supportsDeviceView(const std::shared_ptr<Component>&);
std::string deviceStateJson(const std::shared_ptr<Component>&, const Simulator&,
                            size_t index, SimulationTime time);
}
