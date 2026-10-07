#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
// The supported widths match the simulator's dynamically wired pin widths.
const ComponentFamily& registerWord(size_t width);
}

template<size_t Width>
class Register : public IOComponent {
public:
    explicit Register(std::string name);
    inline static const std::string TypeName = "Register<" + std::to_string(Width) + ">";
    const char* getTypeName() const override { return TypeName.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
};
