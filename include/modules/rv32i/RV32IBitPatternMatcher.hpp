#pragma once

#include "components/IOComponent.hpp"
#include <cstdint>

class RV32IBitPatternMatcher : public IOComponent {
public:
    RV32IBitPatternMatcher(std::string name, uint32_t mask, uint32_t value);
    static constexpr const char* TypeName = "RV32IBitPatternMatcher";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
    uint32_t mask() const { return mask_; }
    uint32_t value() const { return value_; }

private:
    uint32_t mask_;
    uint32_t value_;
};
