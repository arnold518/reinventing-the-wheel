#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily ClockedStateBit;
}

// One flip-flop that captures D on every rising clock edge. It has no
// write-enable: use FlipFlopCell or Register<W> when a value must be held.
class ClockedStateBit : public IOComponent {
public:
    explicit ClockedStateBit(std::string name);
    const char* getTypeName() const override { return "ClockedStateBit"; }
    void buildInternals(ComponentBuilder&) override;
};
