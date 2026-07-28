#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily Comparator32;
}

class Comparator32 : public IOComponent {
public:
    explicit Comparator32(std::string name);
    static constexpr const char* TypeName = "Comparator32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
