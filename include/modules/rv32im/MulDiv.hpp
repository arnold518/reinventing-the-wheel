#include "modules/composite/IterativeArithmetic.hpp"
#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
namespace circuit { class ComponentCatalog; }
namespace circuit::families {
const ComponentFamily& multiplyStep(size_t width);
const ComponentFamily& divideStep(size_t width);
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
namespace rv32im { void registerArithmetic(circuit::ComponentCatalog&); }
