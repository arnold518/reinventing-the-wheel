#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
namespace circuit {class ComponentCatalog;}
namespace circuit::families {const ComponentFamily& decoder(size_t address_bits,bool suppress_zero=false);}
namespace circuit::logic {
class WordDecoder : public IOComponent {
    size_t address_bits_;bool suppress_zero_;std::string type_;
public:
    WordDecoder(std::string,size_t address_bits,bool suppress_zero=false);
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&)override;
};
void registerDecoders(ComponentCatalog&);
}
template<size_t AddressBits>class Decoder : public circuit::logic::WordDecoder {
    static_assert(AddressBits>=1&&AddressBits<=5);
public:
    explicit Decoder(std::string name,bool suppress_zero=false):WordDecoder(std::move(name),AddressBits,suppress_zero){}
    static const circuit::ComponentFamily& family(){return circuit::families::decoder(AddressBits);}
};
