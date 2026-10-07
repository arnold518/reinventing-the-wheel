#pragma once

#include "components/IOComponent.hpp"

class Decoder5to32 : public IOComponent {
public:
    Decoder5to32(std::string name,bool zero_output_unused = false);
    static constexpr const char* TypeName = "Decoder5to32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
private:
    bool zero_output_unused_ = false;
};
