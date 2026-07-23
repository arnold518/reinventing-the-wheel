#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <cstdint>

namespace circuit::families {
extern const ComponentFamily ALU32;
}

namespace ALU32Op {
constexpr uint8_t ADD = 0x00;
constexpr uint8_t SUB = 0x01;
constexpr uint8_t AND = 0x02;
constexpr uint8_t OR = 0x03;
constexpr uint8_t XOR = 0x04;
constexpr uint8_t SLL = 0x05;
constexpr uint8_t SRL = 0x06;
constexpr uint8_t SRA = 0x07;
constexpr uint8_t SLT = 0x08;
constexpr uint8_t SLTU = 0x09;
constexpr uint8_t PASS_A = 0x0A;
constexpr uint8_t PASS_B = 0x0B;
constexpr uint8_t ZERO = 0x0C;
}

class ALU32 : public IOComponent {
public:
    explicit ALU32(std::string name);
    static constexpr const char* TypeName = "ALU32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
