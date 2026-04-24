#pragma once

#include "components/BasicComponent.hpp"

class ZeroDetect8 : public BasicComponent {
public:
    ZeroDetect8(std::string name);
    static constexpr const char* TypeName = "ZeroDetect8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
