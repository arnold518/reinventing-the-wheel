#pragma once
// Private factory implementation; callers use families::ALU32.

#include "components/BasicComponent.hpp"

class ALU32Direct : public BasicComponent {
public:
    explicit ALU32Direct(std::string name,bool full_flags = true);
    static constexpr const char* TypeName = "ALU32";
    const char* getTypeName() const override { return full_flags_?TypeName:"ALU32Core"; }
    void evaluate(size_t current_time, Simulator& simulator) override;
private:
    bool full_flags_ = true;
};
