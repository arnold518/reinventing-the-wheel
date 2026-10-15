#include "components/selection/ProvenPartDelays.hpp"
#include <map>

namespace circuit {
namespace {
using Arc = PartDelays::Arc;
}

// The tables generated for one timing profile (see ProvenPartDelays.cpp).
const std::map<std::string, PartDelays>& provenPartDelaysDesktop() {
    static const std::map<std::string, PartDelays> generated = {
#include "ProvenPartDelays.desktop.generated.inc"
    };
    return generated;
}

} // namespace circuit
