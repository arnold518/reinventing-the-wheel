#pragma once

#include "components/IOComponent.hpp"

class Adder32 : public IOComponent {
public:
    explicit Adder32(std::string name, bool parallel_prefix = true);
    static constexpr const char* TypeName = "Adder32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
private:
    bool parallel_prefix_;
};
