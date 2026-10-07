#pragma once

#include "components/IOComponent.hpp"

class EqualityChecker8 : public IOComponent {
public:
    EqualityChecker8(std::string name);
    static constexpr const char* TypeName = "EqualityChecker8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
