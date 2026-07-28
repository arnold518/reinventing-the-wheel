#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily AddSub32;
}

class AddSub32 : public IOComponent {
public:
    explicit AddSub32(std::string name);
    static constexpr const char* TypeName = "AddSub32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
