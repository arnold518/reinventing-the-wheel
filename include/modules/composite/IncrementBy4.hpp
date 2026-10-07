#pragma once

#include "components/IOComponent.hpp"

// Structural PC incrementer. Bits 0:1 pass through; bits 2:31 form a
// parallel-prefix increment network without a general B operand or Cin.
class IncrementBy4 : public IOComponent {
public:
    explicit IncrementBy4(std::string name);
    static constexpr const char* TypeName = "IncrementBy4";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
