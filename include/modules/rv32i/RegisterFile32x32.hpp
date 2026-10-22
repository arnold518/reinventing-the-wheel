#pragma once
#include "modules/storage/RegisterFile.hpp"

// RV32I fixes both XLEN and the architectural register count at 32. This
// explicit ISA component shares the configurable register-file gate recipe.
class RegisterFile32x32 final : public circuit::storage::WordRegisterFile {
public:
    explicit RegisterFile32x32(std::string name):WordRegisterFile(std::move(name),32,32,{true}){}
    static constexpr const char* TypeName="RegisterFile32x32";
    const char* getTypeName()const override{return TypeName;}
    static const circuit::ComponentFamily& family(){return circuit::storage::registerFile(32,32);}
};
namespace circuit::families {
extern const ComponentFamily RegisterFile32x32;
}
