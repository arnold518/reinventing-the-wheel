#pragma once
#include "modules/composite/ALU8Families.hpp"

#include "components/IOComponent.hpp"
#include <cstdint>

// Contract v2 replaces the previous four-bit operation encoding.
namespace ALU8Op {
inline constexpr uint8_t ADD=0x00, ADC=0x01, SUB=0x02, SBC=0x03;
inline constexpr uint8_t INC=0x04, DEC=0x05, NEG=0x06;
inline constexpr uint8_t AND=0x08, OR=0x09, XOR=0x0A, NOT=0x0B;
inline constexpr uint8_t NAND=0x0C, NOR=0x0D, XNOR=0x0E;
inline constexpr uint8_t SLL=0x10, SRL=0x11, SRA=0x12, ROL=0x13, ROR=0x14;
inline constexpr uint8_t EQ=0x15, SLTU=0x16, SLT=0x17;
inline constexpr uint8_t MINU=0x18, MAXU=0x19, MINS=0x1A, MAXS=0x1B;
inline constexpr uint8_t PASS_A=0x1C, PASS_B=0x1D, CLEAR=0x1E;
}

class ALU8 : public IOComponent {
public:
    ALU8(std::string name);
    static constexpr const char* TypeName = "ALU8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
