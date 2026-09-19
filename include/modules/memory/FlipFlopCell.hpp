#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily FlipFlopCell;
}

class FlipFlopCell : public IOComponent {
public:
    FlipFlopCell(std::string name);
    static constexpr const char* TypeName = "FlipFlopCell";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
