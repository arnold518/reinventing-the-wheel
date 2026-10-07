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
    explicit RV32IMemoryRequest(std::string name, bool read_only = false,
                                bool address_is_tag = false);
    static constexpr const char* TypeName = "RV32IMemoryRequest";
    const char* getTypeName() const override {
        return read_only_ && address_is_tag_ ? "RV32IFetchRequest" : TypeName;
    }
    void buildInternals(ComponentBuilder&) override;
private:
    bool read_only_;
    bool address_is_tag_;
};

// CPU load/store formatting; independent of request acceptance/completion.
class RV32IMemoryLanes : public IOComponent {
public:
    explicit RV32IMemoryLanes(std::string name,bool with_word_address = true);
    static constexpr const char* TypeName = "RV32IMemoryLanes";
    const char* getTypeName() const override { return with_word_address_?TypeName:"RV32IMemoryLanesDataOnly"; }
    void buildInternals(ComponentBuilder&) override;
private:
    bool with_word_address_ = true;
};
