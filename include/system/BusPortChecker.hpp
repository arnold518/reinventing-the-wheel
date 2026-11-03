#pragma once
// A passive observer on one bus link. It reads every signal of the link and
// raises a sticky flag when the link breaks the port rules (§ 5.2):
//  - REQUEST_CHANGED: a waiting request (VALID high, READY low at an edge)
//    was withdrawn or changed before it was taken;
//  - RESPONSE_CHANGED: the same for a waiting response;
//  - EXTRA_RESPONSE: a response was taken with no request outstanding.
// VIOLATION is the OR of the three. Reset clears them. It drives nothing on
// the link. Contract `bus.port-checker.<W>`.
#include "components/IOComponent.hpp"

namespace circuit { class ComponentCatalog; }

namespace circuit::system {

class BusPortChecker : public IOComponent {
public:
    BusPortChecker(std::string name, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    std::string type_;
};

void busPortCheckerPins(IOComponent*, size_t width);
std::string portCheckerContractId(size_t width);
void registerPortCheckers(ComponentCatalog&);

} // namespace circuit::system
