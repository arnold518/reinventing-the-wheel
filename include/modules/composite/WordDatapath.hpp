#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <bit>

namespace circuit { class ComponentCatalog; }
namespace circuit::word {
namespace ExtendedOp {
inline constexpr uint8_t ADD=0x00, ADC=0x01, SUB=0x02, SBC=0x03;
inline constexpr uint8_t INC=0x04, DEC=0x05, NEG=0x06;
inline constexpr uint8_t AND=0x08, OR=0x09, XOR=0x0A, NOT=0x0B;
inline constexpr uint8_t NAND=0x0C, NOR=0x0D, XNOR=0x0E;
inline constexpr uint8_t SLL=0x10, SRL=0x11, SRA=0x12, ROL=0x13, ROR=0x14;
inline constexpr uint8_t EQ=0x15, SLTU=0x16, SLT=0x17;
inline constexpr uint8_t MINU=0x18, MAXU=0x19, MINS=0x1A, MAXS=0x1B;
inline constexpr uint8_t PASS_A=0x1C, PASS_B=0x1D, CLEAR=0x1E;
}

namespace IntegerOp {
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

enum class Operations { Extended, IntegerDatapath };
enum class AdderTopology { Ripple, ParallelPrefix };
struct ALUConfig {
    Operations operations = Operations::Extended;
    AdderTopology adder = AdderTopology::ParallelPrefix;
    bool full_flags = true;
};
// The ALU default uses parallel prefix; topology remains an explicit choice.
inline constexpr ALUConfig defaultALUConfig{Operations::Extended, AdderTopology::ParallelPrefix, true};
enum class Unit { ALU, AddSub, Arithmetic, Logic, Shift, Compare, Flags, Result, Adder, Zero };
// All widths and operation contracts use these structural recipes.
void build(Unit, IOComponent&, ComponentBuilder&, size_t width, ALUConfig = {});
const ComponentFamily& family(Unit, size_t width, ALUConfig = {});
void registerComponents(ComponentCatalog&);
const ComponentFamily& controlFamily(Operations);
void buildControl(IOComponent&,ComponentBuilder&,Operations);
class ControlDecoder : public IOComponent {
    Operations operations_;std::string type_;
public:ControlDecoder(std::string,Operations);
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&)override;
};
class WordComponent : public IOComponent {
    Unit unit_; size_t width_; ALUConfig config_; std::string type_;
public:
    WordComponent(std::string, Unit, size_t width, ALUConfig = {});
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
};
}

template<size_t W> class ALU : public circuit::word::WordComponent {
    static_assert(W == 8 || W == 16 || W == 32, "ALU supports 8, 16 and 32 bits");
public:
    explicit ALU(std::string name, circuit::word::ALUConfig config = circuit::word::defaultALUConfig)
        : WordComponent(std::move(name), circuit::word::Unit::ALU, W, config) {}
};
template<size_t W> class Adder : public circuit::word::WordComponent {
    static_assert(W == 8 || W == 16 || W == 32);
public:
    explicit Adder(std::string name, circuit::word::AdderTopology topology = circuit::word::AdderTopology::ParallelPrefix)
        : WordComponent(std::move(name), circuit::word::Unit::Adder, W, {circuit::word::Operations::Extended, topology}) {}
};
template<size_t W> class AddSub : public circuit::word::WordComponent {
    static_assert(W == 8 || W == 16 || W == 32);
public:
    AddSub(std::string name,circuit::word::ALUConfig config)
        : WordComponent(std::move(name),circuit::word::Unit::AddSub,W,config) {}
    explicit AddSub(std::string name, circuit::word::AdderTopology topology = circuit::word::AdderTopology::ParallelPrefix)
        : WordComponent(std::move(name), circuit::word::Unit::AddSub, W, {circuit::word::Operations::Extended, topology}) {}
};
template<size_t W> class ZeroDetect : public circuit::word::WordComponent {
    static_assert(W == 8 || W == 16 || W == 32);
public:
    explicit ZeroDetect(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Zero, W) {}
};

template<size_t W> class ArithmeticUnit : public circuit::word::WordComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit ArithmeticUnit(std::string n,circuit::word::ALUConfig config={})
    :WordComponent(std::move(n),circuit::word::Unit::Arithmetic,W,config){}
};

template<size_t W> class LogicUnit : public circuit::word::WordComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit LogicUnit(std::string n,circuit::word::ALUConfig config={})
    :WordComponent(std::move(n),circuit::word::Unit::Logic,W,config){}
};

template<size_t W> class BarrelShifter : public circuit::word::WordComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit BarrelShifter(std::string n,circuit::word::ALUConfig config={})
    :WordComponent(std::move(n),circuit::word::Unit::Shift,W,config){}
};

template<size_t W> class CompareSelect : public circuit::word::WordComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit CompareSelect(std::string n,circuit::word::ALUConfig config={})
    :WordComponent(std::move(n),circuit::word::Unit::Compare,W,config){}
};

template<size_t W> class ALUFlagUnit : public circuit::word::WordComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit ALUFlagUnit(std::string n,circuit::word::ALUConfig config={})
    :WordComponent(std::move(n),circuit::word::Unit::Flags,W,config){}
};

template<size_t W> class ALUResultSelector : public circuit::word::WordComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit ALUResultSelector(std::string n,circuit::word::ALUConfig config={})
    :WordComponent(std::move(n),circuit::word::Unit::Result,W,config){}
};

using ALUControlDecoder = circuit::word::ControlDecoder;

namespace circuit::families {
const ComponentFamily& alu(size_t width,word::ALUConfig config = word::defaultALUConfig);
const ComponentFamily& adder(size_t width,word::AdderTopology topology = word::AdderTopology::ParallelPrefix);
const ComponentFamily& addSub(size_t width,word::ALUConfig config = {});
const ComponentFamily& zeroDetect(size_t width);
}
