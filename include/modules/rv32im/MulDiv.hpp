#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
namespace circuit { class ComponentCatalog; }
namespace circuit::families {
const ComponentFamily& multiplyStep(size_t width);
const ComponentFamily& divideStep(size_t width);
extern const ComponentFamily Multiplier32, Divider32, MulDivUnit32;
}
class MultiplyStep : public IOComponent {
    size_t width_;std::string type_;
public:
    MultiplyStep(std::string name,size_t width=4);
    const char* getTypeName() const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
class DivideStep : public IOComponent {
    size_t width_;std::string type_;
public:
    DivideStep(std::string name,size_t width=4);
    const char* getTypeName() const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
class Multiplier32 : public IOComponent {
public:
    explicit Multiplier32(std::string name);
    static constexpr const char* TypeName="Multiplier32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class Divider32 : public IOComponent {
public:
    explicit Divider32(std::string name);
    static constexpr const char* TypeName="Divider32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class MulDivUnit32 : public IOComponent {
public:
    explicit MulDivUnit32(std::string name);
    static constexpr const char* TypeName="MulDivUnit32";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
namespace rv32im { void registerArithmetic(circuit::ComponentCatalog&); }
