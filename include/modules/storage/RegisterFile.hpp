#pragma once
#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <bit>
namespace circuit {class ComponentCatalog;}
namespace circuit::storage {
struct RegisterFileConfig {bool hardwire_zero = true;};
class WordRegisterFile : public IOComponent, public RegisterStateView {
    size_t width_,count_; RegisterFileConfig config_;std::string type_;
public:
    WordRegisterFile(std::string,size_t width,size_t count,RegisterFileConfig = {});
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&)override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(SimulationTime)const override;
};
const ComponentFamily& registerFile(size_t width,size_t count,RegisterFileConfig = {});
void registerRegisterFiles(ComponentCatalog&);
}
template<size_t W,size_t Count> class RegisterFile : public circuit::storage::WordRegisterFile {
    static_assert(W==8 || W==16 || W==32);
    static_assert(Count>=2 && Count<=32 && std::has_single_bit(Count));
public:
    static const circuit::ComponentFamily& family(){return circuit::storage::registerFile(W,Count);}
    explicit RegisterFile(std::string n,circuit::storage::RegisterFileConfig config={})
    :WordRegisterFile(std::move(n),W,Count,config){}
};
