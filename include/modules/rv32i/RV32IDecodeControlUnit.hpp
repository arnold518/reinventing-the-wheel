#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily RV32IDecodeControl;
}

class RV32IDecodeControlUnit : public IOComponent {
public:
    explicit RV32IDecodeControlUnit(std::string name);
    static constexpr const char* TypeName = "RV32IDecodeControlUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
