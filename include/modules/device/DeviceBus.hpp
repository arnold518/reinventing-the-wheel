#pragma once
#include "modules/device/DeviceComponents.hpp"
namespace circuit::device {
enum class BusUnit { RegisterPort, ErrorTarget, Router, InstructionGuard };
class WordBusComponent : public IOComponent, public ComponentLayoutView {
    BusUnit unit_; size_t width_; AddressMap map_; std::string type_;
public:
    WordBusComponent(std::string,BusUnit,size_t width,AddressMap = {});
    const char* getTypeName() const override {return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
    std::string componentLayoutVariant() const override;
};
const ComponentFamily& busFamily(BusUnit,size_t width);
void registerWordBusComponents(ComponentCatalog&);
}
template<size_t W> class DeviceRegisterPort : public circuit::device::WordBusComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit DeviceRegisterPort(std::string n):WordBusComponent(std::move(n),circuit::device::BusUnit::RegisterPort,W){}
};
template<size_t W> class BusErrorTarget : public circuit::device::WordBusComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit BusErrorTarget(std::string n):WordBusComponent(std::move(n),circuit::device::BusUnit::ErrorTarget,W){}
};
template<size_t W> class AddressRouter : public circuit::device::WordBusComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit AddressRouter(std::string n,circuit::device::AddressMap map={}):WordBusComponent(std::move(n),circuit::device::BusUnit::Router,W,std::move(map)){}
};
template<size_t W> class InstructionAccessGuard : public circuit::device::WordBusComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit InstructionAccessGuard(std::string n):WordBusComponent(std::move(n),circuit::device::BusUnit::InstructionGuard,W){}
};

namespace circuit::device {
class WordPixelSelector : public IOComponent {
    size_t width_;std::string type_;
public:WordPixelSelector(std::string,size_t width);
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&)override;
};
const ComponentFamily& indexedPixelSelector(size_t width);
void registerWordPixelSelectors(ComponentCatalog&);
}
template<size_t W> class IndexedPixelSelector : public circuit::device::WordPixelSelector {
    static_assert(W==8 || W==16 || W==32);
public:explicit IndexedPixelSelector(std::string n):WordPixelSelector(std::move(n),W){}
};
