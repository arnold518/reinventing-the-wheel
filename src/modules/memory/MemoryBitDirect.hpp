#pragma once
// Private factory implementation; callers use families::MemoryBit.

#include "components/BasicComponent.hpp"

class MemoryBitDirect : public BasicComponent {
public:
    MemoryBitDirect(std::string name);
    static constexpr const char* TypeName = "MemoryBit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    LogicValue stored_value;
    LogicValue previous_clk;
};
