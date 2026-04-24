#pragma once

#include "components/BasicComponent.hpp"

class ShiftLeftLogical8 : public BasicComponent {
public:
    ShiftLeftLogical8(std::string name);
    static constexpr const char* TypeName = "ShiftLeftLogical8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class ShiftRightLogical8 : public BasicComponent {
public:
    ShiftRightLogical8(std::string name);
    static constexpr const char* TypeName = "ShiftRightLogical8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class ShiftRightArithmetic8 : public BasicComponent {
public:
    ShiftRightArithmetic8(std::string name);
    static constexpr const char* TypeName = "ShiftRightArithmetic8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
