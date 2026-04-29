#pragma once

#include "components/IOComponent.hpp"

class ShiftLeftLogical8 : public IOComponent {
public:
    ShiftLeftLogical8(std::string name);
    static constexpr const char* TypeName = "ShiftLeftLogical8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class ShiftRightLogical8 : public IOComponent {
public:
    ShiftRightLogical8(std::string name);
    static constexpr const char* TypeName = "ShiftRightLogical8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class ShiftRightArithmetic8 : public IOComponent {
public:
    ShiftRightArithmetic8(std::string name);
    static constexpr const char* TypeName = "ShiftRightArithmetic8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
