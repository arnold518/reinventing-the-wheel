#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily RegisterFile32x32;
}

class RegisterFile32x32 : public IOComponent, public RegisterStateView {
public:
    RegisterFile32x32(std::string name);
    static constexpr const char* TypeName = "RegisterFile32x32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(
        size_t target_time) const override;
};
