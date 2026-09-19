#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Constant.hpp"
#include <map>
#include <stdexcept>
#include <vector>

namespace circuit::memory_detail {

// Construction-only wiring helper. This neither evaluates nor stores signals.
// Each source has one wire in a hierarchy scope, so collect its fanout first.
class StructuralWiring {
public:
    using PinPtr = std::shared_ptr<PinBase>;
    explicit StructuralWiring(ComponentBuilder& builder) : builder_(builder) {}
    void connect(PinPtr source, PinPtr sink) {
        if (!source || !sink || source->getWidth() != sink->getWidth())
            throw std::invalid_argument("Invalid structural memory connection");
        auto& net = nets_[source->getID()];
        net.first = source;
        net.second.push_back(sink);
    }
    PinPtr constant(const std::string& name, bool value) {
        return builder_.addNewComponent<ConstantValue<1>>(name, value)->getOutputPin("OUT");
    }
    template<class Gate>
    PinPtr binary(const std::string& name, PinPtr a, PinPtr b) {
        auto gate = builder_.addNewComponent<Gate>(name);
        connect(a, gate->getInputPin("A"));
        connect(b, gate->getInputPin("B"));
        return gate->getOutputPin("OUT");
    }
    PinPtr both(const std::string& name, PinPtr a, PinPtr b) { return binary<ANDGate>(name, a, b); }
    PinPtr either(const std::string& name, PinPtr a, PinPtr b) { return binary<ORGate>(name, a, b); }
    PinPtr invert(const std::string& name, PinPtr input) {
        auto gate = builder_.addNewComponent<NOTGate>(name);
        connect(input, gate->getInputPin("IN"));
        return gate->getOutputPin("OUT");
    }
    PinPtr reduce(const std::string& name, std::vector<PinPtr> inputs, bool conjunction) {
        if (inputs.empty()) return constant(name + "_IDENTITY", conjunction);
        for (size_t level = 0; inputs.size() > 1; ++level) {
            std::vector<PinPtr> next;
            for (size_t i = 0; i < inputs.size(); i += 2) {
                if (i + 1 == inputs.size()) next.push_back(inputs[i]);
                else {
                    const auto node = name + "_" + std::to_string(level) + "_" + std::to_string(i / 2);
                    next.push_back(conjunction ? both(node, inputs[i], inputs[i + 1])
                                               : either(node, inputs[i], inputs[i + 1]));
                }
            }
            inputs = std::move(next);
        }
        return inputs.front();
    }
    void finish() {
        size_t index = 0;
        // IDs, not pointers, keep native and WASM net order deterministic.
        for (const auto& [id, net] : nets_)
            builder_.addNewWireDynamic("net_" + std::to_string(index++), net.first->getWidth(), net.first, net.second);
    }
private:
    ComponentBuilder& builder_;
    std::map<std::string, std::pair<PinPtr, std::vector<PinPtr>>> nets_;
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
