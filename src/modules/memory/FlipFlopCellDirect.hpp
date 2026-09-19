#pragma once
// Private factory implementation; callers use families::FlipFlopCell.

#include "components/BasicComponent.hpp"

class FlipFlopCellDirect : public BasicComponent {
public:
    FlipFlopCellDirect(std::string name);
    static constexpr const char* TypeName = "FlipFlopCell";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    LogicValue stored_value;
    LogicValue previous_clk;
};
