#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily RV32IControlFlow;
}

class RV32IControlFlowUnit : public IOComponent {
public:
    explicit RV32IControlFlowUnit(std::string name);
    static constexpr const char* TypeName = "RV32IControlFlowUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
