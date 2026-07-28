#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily Shifter32;
}

class Shifter32 : public IOComponent {
public:
    explicit Shifter32(std::string name);
    static constexpr const char* TypeName = "Shifter32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
