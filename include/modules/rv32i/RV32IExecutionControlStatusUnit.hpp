#pragma once

#include "components/IOComponent.hpp"

class RV32IExecutionControlStatusUnit : public IOComponent {
public:
    explicit RV32IExecutionControlStatusUnit(std::string name);
    static constexpr const char* TypeName = "RV32IExecutionControlStatusUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
