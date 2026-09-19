#pragma once
#include "components/IOComponent.hpp"

// One outstanding aligned 32-bit transaction. See docs/general-memory-container.md.
// CLK/RST must also connect to the attached array. No storage is owned here.
void defineMemoryBusClientPins(IOComponent* self);
class MemoryBusAdapter32 : public IOComponent {
public:
    static constexpr const char* TypeName = "MemoryBusAdapter32";
    const char* getTypeName() const override { return TypeName; }
    explicit MemoryBusAdapter32(std::string name);
    void buildInternals(ComponentBuilder& builder) override;
};
