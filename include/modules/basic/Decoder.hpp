#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily Decoder5to32;
// The register file's write decoder: output 0 is tied low, because x0 has
// no storage to write.
extern const ComponentFamily Decoder5to32X0Unused;
}

class Decoder5to32 : public IOComponent {
public:
    Decoder5to32(std::string name,bool zero_output_unused = false);
    static constexpr const char* TypeName = "Decoder5to32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
private:
    bool zero_output_unused_ = false;
};
