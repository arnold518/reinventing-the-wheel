#pragma once
// The bus (interconnect): connects any number of masters to target ports by
// an address map. Each master has its own request router (decode, error
// answers, one transaction at a time); a target port reached by two masters
// gets a turn-taker (the two-port memory arbiter); a port reached by one
// master is wired straight through. Built from a BusDescription.
#include "components/IOComponent.hpp"
#include "components/capabilities/ComponentLayoutView.hpp"
#include "components/capabilities/DeviceView.hpp"
#include "system/SystemDescription.hpp"
#include "modules/device/DeviceConfiguration.hpp"

namespace circuit { class ComponentCatalog; }

namespace circuit::system {

class Bus : public IOComponent, public ComponentLayoutView, public DeviceView {
public:
    Bus(std::string name, BusDescription description);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
    std::string componentLayoutVariant() const override { return variant_; }
    // Collapsed, a bus is drawn as its traffic: which master talks to which target.
    std::string deviceViewKind() const override { return "bus"; }
    const BusDescription& description() const { return description_; }
    // The masters that reach a target port, in master order.
    std::vector<const MasterDescription*> mastersOf(const std::string& port) const;
private:
    BusDescription description_;
    std::string type_, variant_;
};

// One master's request router: its routes (target pin prefix and window) and
// whether it fetches (reads only). Contract `bus.router.<W>`, parameters
// access=fetch|data and routes="PREFIX@0xBASE+0xBYTES[!],..." ("!" = the
// target sees the whole address).
struct RouterConfig {
    std::vector<device::AddressRegion> routes;
    bool fetch = false;
    size_t width = 32;
};
std::string routerContractId(size_t width);
IOComponent::PinInitFunction routerPinsFor(const RouterConfig&);
ParameterMap routerParameters(const RouterConfig&);
RouterConfig routerFromParameters(const ParameterMap&, size_t width);

// The bus's pins: CLK, RST; per master "<MASTER>_" + the responder side of the
// request/response port; per target port portPrefix(port) + the requester side.
void busPins(IOComponent*, const BusDescription&);
// Contract `bus.interconnect.<W>`, parameter `config`: the bus as JSON.
std::string busContractId(size_t width);
std::string busConfigJson(const BusDescription&);
BusDescription busFromConfigJson(const std::string&);
void registerBuses(ComponentCatalog&);

} // namespace circuit::system
