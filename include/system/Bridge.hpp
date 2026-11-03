#pragma once
// The bus bridge: a target port (S_) on one bus and a master port (M_) on
// another, so buses chain into layers (a system bus and a peripheral bus).
// Contract `bus.bridge.<W>`, parameter registered=0|1.
//  - pass-through (0): wires only; a transfer crosses in the same cycle.
//  - registered (1): one register stage each way, as real bridges have. A
//    request is taken on S_, offered on M_ the next cycle; the response is
//    taken on M_ and offered on S_ the next cycle. One transfer at a time;
//    single-word operations only (a ReadBurst does not cross it).
#include "components/IOComponent.hpp"

namespace circuit { class ComponentCatalog; }

namespace circuit::system {

struct BridgeConfig {
    size_t width = 32;
    bool registered = false;
};

class Bridge : public IOComponent {
public:
    Bridge(std::string name, BridgeConfig);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    BridgeConfig config_;
    std::string type_;
};

std::string bridgeContractId(size_t width);
void bridgePins(IOComponent*, size_t width);
void registerBridges(ComponentCatalog&);

} // namespace circuit::system
