#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
// The general requester (TAG, OP, WRITE_DATA and WRITE_MASK inputs present)
// and the load/store lane formatter with its WORD_ADDR output.
extern const ComponentFamily RV32IMemoryRequest;
extern const ComponentFamily RV32IMemoryLanes;
}

namespace rv32i {
// Requester payload pins; CLK/RST are shared with the CPU, not delayed outputs.
void memoryRequestPins(IOComponent*, const std::string& prefix);
}

// CPU-owned, one-outstanding universal requester. No device controller, relay,
// response buffer, or legacy READ_EN/SIZE interface is present here.
class RV32IMemoryRequest : public IOComponent {
public:
    // overlapped (§ 6.3, fetch only): with inputs NEXT_ADDR and ADVANCE, the
    // next fetch starts in the cycle the current answer is taken, from the
    // PC the core will hold after that edge. A wrong guess is discarded by
    // the usual address check. Off by default.
    explicit RV32IMemoryRequest(std::string name, bool read_only = false,
                                bool address_is_tag = false, bool overlapped = false);
    static constexpr const char* TypeName = "RV32IMemoryRequest";
    const char* getTypeName() const override {
        return overlapped_ ? "RV32IOverlappedFetchRequest" : read_only_ && address_is_tag_ ? "RV32IFetchRequest" : TypeName;
    }
    void buildInternals(ComponentBuilder&) override;
private:
    void buildOverlapped(ComponentBuilder&);
    bool read_only_;
    bool address_is_tag_;
    bool overlapped_ = false;
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
