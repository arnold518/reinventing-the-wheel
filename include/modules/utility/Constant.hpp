#pragma once

#include "components/BasicComponent.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

template<size_t OUT_WIDTH, size_t TRIGGER_WIDTH = 8>
class ConstantValue : public BasicComponent {
public:
    ConstantValue(std::string name, uint64_t constant_value);
    static constexpr const char* TypeName = "ConstantValue";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    uint64_t value;
};

#include "modules/utility/Constant.tpp"
