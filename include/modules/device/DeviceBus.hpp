#pragma once
#include <vector>
#include "modules/device/DeviceComponents.hpp"
namespace circuit::device {
// The register port is the one fixed bus unit; routers are built per bus
// from a description (system/Bus.hpp).
enum class BusUnit { RegisterPort };
class WordBusComponent : public IOComponent {
    BusUnit unit_; size_t width_; std::string type_;
public:
    WordBusComponent(std::string,BusUnit,size_t width);
    const char* getTypeName() const override {return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
const ComponentFamily& busFamily(BusUnit,size_t width);
// The request router of one bus master: decodes each request against the
// regions, answers errors itself, holds one transaction and routes it to the
// region's target port (pins prefixed with the region's prefix). A fetch
// router accepts reads only.
void addressRouterPins(IOComponent*,const std::vector<AddressRegion>&,size_t width);
// pipelined (§ 6.3): a new request is taken at the edge the previous answer is
// taken, not one edge later. Off by default: the chapter 5 router.
void buildAddressRouter(IOComponent&,ComponentBuilder&,const std::vector<AddressRegion>&,bool fetch,size_t width,bool pipelined=false);
void registerWordBusComponents(ComponentCatalog&);
}
template<size_t W> class DeviceRegisterPort : public circuit::device::WordBusComponent {
    static_assert(W==8 || W==16 || W==32);
public:explicit DeviceRegisterPort(std::string n):WordBusComponent(std::move(n),circuit::device::BusUnit::RegisterPort,W){}
};
