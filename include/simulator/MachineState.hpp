#pragma once

#include "simulator/Simulator.hpp"
#include "simulator/StateArchive.hpp"
#include <memory>
#include <string>
#include <vector>

class Component;

// The complete state of one simulated tree at a moment: the simulator's time,
// event queue and counters, every wire and pin value, and each evaluated
// part's timing and own state. Restored into a fresh build of the same tree
// (same build profile, same added wires), the run continues exactly as the
// saved one would have: the same events in the same order.
//
// Components, pins and wires are numbered in a fixed order: pre-order over
// the children, each component's own wires, then its input and output pins in
// name order with their wires. The save records the counts and a hash of the
// names, types and widths, and a restore refuses a different tree.
namespace circuit::state {

void saveMachine(const std::shared_ptr<Component>& root, const Simulator& sim, Writer& out);
void restoreMachine(const std::shared_ptr<Component>& root, Simulator& sim, Reader& in);
// Names of evaluated parts in the tree that have not declared their state
// (BasicComponent::StateSupport::Unknown). Saving fails while any remain.
std::vector<std::string> undeclaredStateTypes(const std::shared_ptr<Component>& root);
// Only the wire and pin values and the simulator's time, applied to the
// machine the save was taken from and still in that state inside: viewing its
// history moved the values, nothing else. Queue, parts and history stay.
void restoreValues(const std::shared_ptr<Component>& root, Simulator& sim, Reader& in);

} // namespace circuit::state
