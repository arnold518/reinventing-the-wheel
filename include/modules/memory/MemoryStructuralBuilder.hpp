#pragma once

#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Constant.hpp"
#include <map>
#include <stdexcept>
#include <vector>

namespace circuit::memory_detail {

// Memory-side structural wiring: nets are ordered by the source pin's full ID
// and named net_<index>.
class StructuralWiring : public circuit::detail::StructuralNetBuilder {
public:
    using StructuralNetBuilder::StructuralNetBuilder;
    PinPtr constant(const std::string& name, bool value) {
        return builder_.addNewComponent<ConstantValue<1>>(name, value)->getOutputPin("OUT");
    }
    PinPtr both(const std::string& name, PinPtr a, PinPtr b) { return binary<ANDGate>(name, a, b); }
    PinPtr either(const std::string& name, PinPtr a, PinPtr b) { return binary<ORGate>(name, a, b); }

protected:
    std::string netKey(const PinPtr& source) const override { return source->getID(); }
    std::string wireName(const PinPtr&, size_t index) const override { return "net_" + std::to_string(index); }
    [[noreturn]] void throwInvalidConnection() const override {
        throw std::invalid_argument("Invalid structural memory connection");
    }
};
inline size_t arrayAddressBits(size_t depth) {
    // The narrowest supported array has at most 512 words. Validate before
    // shifts or constructing dynamic pin lists, even for standalone blocks.
    if (depth == 0 || depth > 512) throw std::invalid_argument("Invalid structural memory depth");
    size_t bits = 0;
    while ((size_t{1} << bits) < depth) ++bits;
    return bits;
}

} // namespace circuit::memory_detail
