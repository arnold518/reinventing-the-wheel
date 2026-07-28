#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily ZeroDetect32;
}

class ZeroDetect32 : public IOComponent {
public:
    explicit ZeroDetect32(std::string name);
    static constexpr const char* TypeName = "ZeroDetect32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
