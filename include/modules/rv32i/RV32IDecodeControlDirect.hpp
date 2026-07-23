#pragma once

#include "components/BasicComponent.hpp"

class RV32IDecodeControlDirect : public BasicComponent {
public:
    explicit RV32IDecodeControlDirect(std::string name);
    static constexpr const char* TypeName = "RV32IDecodeControlUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
