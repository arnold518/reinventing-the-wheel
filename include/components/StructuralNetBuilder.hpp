#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Constant.hpp"
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace circuit::detail {
// Construction-only base for structural blueprints: collect each source pin's
// fan-out, then emit one wire per source. This neither evaluates nor stores
// signals. A derived builder chooses how nets are ordered and named, so saved
// wire identities of existing circuits do not depend on this shared code.
class StructuralNetBuilder {
public:
    using PinPtr = std::shared_ptr<PinBase>;

    explicit StructuralNetBuilder(ComponentBuilder& builder) : builder_(builder) {}
    virtual ~StructuralNetBuilder() = default;

    void connect(const PinPtr& source, const PinPtr& sink) {
        if (!source || !sink || source->getWidth() != sink->getWidth())
            throwInvalidConnection();
        auto& net = nets_[netKey(source)];
        net.first = source;
        net.second.push_back(sink);
    }
    // One source driving several sinks.
    void connect(const PinPtr& source, std::initializer_list<PinPtr> sinks) {
        for (const auto& sink : sinks) connect(source, sink);
    }
    template<std::ranges::range Sinks>
    void connect(const PinPtr& source, const Sinks& sinks) {
        for (const auto& sink : sinks) connect(source, PinPtr(sink));
    }
    template<class Gate>
    PinPtr binary(const std::string& name, const PinPtr& a, const PinPtr& b) {
        auto gate = builder_.addNewComponent<Gate>(name);
        connect(a, gate->getInputPin("A"));
        connect(b, gate->getInputPin("B"));
        return gate->getOutputPin("OUT");
    }
    PinPtr invert(const std::string& name, const PinPtr& input) {
        auto gate = builder_.addNewComponent<NOTGate>(name);
        connect(input, gate->getInputPin("IN"));
        return gate->getOutputPin("OUT");
    }
    // Balanced AND/OR tree; an empty input list yields the identity constant.
    PinPtr reduce(const std::string& name, std::vector<PinPtr> inputs, bool conjunction) {
        if (inputs.empty())
            return builder_.addNewComponent<ConstantValue<1>>(name + "_IDENTITY", conjunction)->getOutputPin("OUT");
        for (size_t level = 0; inputs.size() > 1; ++level) {
            std::vector<PinPtr> next;
            for (size_t i = 0; i < inputs.size(); i += 2) {
                if (i + 1 == inputs.size()) next.push_back(inputs[i]);
                else {
                    const auto node = name + "_" + std::to_string(level) + "_" + std::to_string(i / 2);
                    next.push_back(conjunction ? binary<ANDGate>(node, inputs[i], inputs[i + 1])
                                               : binary<ORGate>(node, inputs[i], inputs[i + 1]));
                }
            }
            inputs = std::move(next);
        }
        return inputs.front();
    }
    // Keys, not pointers, keep net order deterministic across native and WASM.
    // Several builders may wire one component (a helper function has its own),
    // so an index whose name is already taken in this scope is skipped: wire
    // IDs must be unique within their owner.
    void finish() {
        size_t index = 0;
        for (const auto& [key, net] : nets_) {
            auto name = wireName(net.first, index++);
            while (builder_.getWireDynamic(name)) name = wireName(net.first, index++);
            builder_.addNewWireDynamic(std::move(name), net.first->getWidth(), net.first, net.second);
        }
        nets_.clear();
    }

protected:
    virtual std::string netKey(const PinPtr& source) const = 0;
    virtual std::string wireName(const PinPtr& source, size_t index) const = 0;
    [[noreturn]] virtual void throwInvalidConnection() const = 0;

    ComponentBuilder& builder_;

private:
    std::map<std::string, std::pair<PinPtr, std::vector<PinPtr>>> nets_;
};

// General structural wiring: nets are ordered by the source pin's full ID and
// named net_<index>.
class StructuralWiring : public StructuralNetBuilder {
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
        throw std::invalid_argument("Invalid structural connection");
    }
};
} // namespace circuit::detail
