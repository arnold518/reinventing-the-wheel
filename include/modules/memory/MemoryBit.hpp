#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily MemoryBit;
}

class MemoryBit : public IOComponent {
public:
    MemoryBit(std::string name);
    static constexpr const char* TypeName = "MemoryBit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
