#pragma once

#include "components/IOComponent.hpp"

class ZeroDetect8 : public IOComponent {
public:
    ZeroDetect8(std::string name);
    static constexpr const char* TypeName = "ZeroDetect8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
