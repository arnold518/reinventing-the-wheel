#pragma once
// Private factory implementation; callers use families::ALU32.

#include "components/BasicComponent.hpp"

#include <cstdint>

// The ALU32 result for known binary operands. This is the single behavioral
// statement of the ALU's arithmetic; behavioral implementations of larger
// components that contain an ALU call it rather than restating the rule.
// (The instruction oracle keeps its own independent copy on purpose.)
uint32_t alu32Result(uint8_t op, uint32_t a, uint32_t b);

class ALU32Behavioral : public BasicComponent {
public:
    explicit ALU32Behavioral(std::string name,bool full_flags = true);
    static constexpr const char* TypeName = "ALU32";
    const char* getTypeName() const override { return full_flags_?TypeName:"ALU32Core"; }
    void evaluate(size_t current_time, Simulator& simulator) override;
private:
    bool full_flags_ = true;
};
