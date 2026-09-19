#pragma once
#include "components/IOComponent.hpp"

namespace rv32i {
// Requester payload pins; CLK/RST are shared with the CPU, not delayed outputs.
void memoryRequestPins(IOComponent*, const std::string& prefix);
}

// CPU-owned, one-outstanding universal requester. No device controller, relay,
// response buffer, or legacy READ_EN/SIZE interface is present here.
class RV32IMemoryRequest : public IOComponent {
public:
    explicit RV32IMemoryRequest(std::string name);
    static constexpr const char* TypeName = "RV32IMemoryRequest";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

// CPU load/store formatting; independent of request acceptance/completion.
class RV32IMemoryLanes : public IOComponent {
public:
    explicit RV32IMemoryLanes(std::string name);
    static constexpr const char* TypeName = "RV32IMemoryLanes";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};
