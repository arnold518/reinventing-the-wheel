#pragma once
#include <string>

// Optional presentation metadata for parameterized components. An empty key
// preserves the default floorplan; a nonempty key names another configuration.
// It changes no pins, simulation state, fidelity or component identity.
class ComponentLayoutView {
public:
    virtual ~ComponentLayoutView() = default;
    virtual std::string componentLayoutVariant() const = 0;
};
