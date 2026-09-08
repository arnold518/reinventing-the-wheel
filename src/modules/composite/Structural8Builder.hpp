#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <array>
#include <map>
#include <stdexcept>

namespace circuit::detail {
// Construction-only helper: accumulate one fan-out wire per source pin.
// This does not evaluate logic or introduce a behavioral component.
class StructuralBuilder {
public:
    using Signal = std::shared_ptr<PinBase>;
    using Bits = std::array<Signal, 8>;
    StructuralBuilder(IOComponent& root, ComponentBuilder& builder)
        : root_(root), builder_(builder) {}

    Signal input(const std::string& name) { return root_.getInputPinDynamic(name); }
    Signal output(const std::string& name) { return root_.getOutputPinDynamic(name); }
    void connect(const Signal& source, const Signal& sink) {
        if (!source || !sink || source->getWidth() != sink->getWidth())
            throw std::logic_error("Invalid structural ALU connection");
        const auto key = source->getOwner().get() == &root_
            ? "INPUT/" + source->getName()
            : "CHILD/" + source->getOwner()->getName() + "/" + source->getName();
        auto& net = nets_[key];
        net.first = source;
        net.second.push_back(sink);
    }
    template<class T, class... Args>
    std::shared_ptr<T> add(const std::string& name, Args&&... args) {
        return builder_.addNewComponent<T>(name, std::forward<Args>(args)...);
    }
    std::shared_ptr<IOComponent> add(const ComponentFamily& family, const std::string& name) {
        return builder_.add(family, name);
    }
    template<size_t Width>
    Signal constant(const std::string& name, uint64_t value) {
        return add<ConstantValue<Width>>(name, value)->template getOutputPin<Width>("OUT");
    }
    template<size_t Width>
    std::array<Signal, Width> split(const std::string& name, const Signal& source) {
        auto c = add<BitSplitter<Width>>(name);
        connect(source, c->template getInputPin<Width>("IN"));
        std::array<Signal, Width> bits;
        for (size_t i = 0; i < Width; ++i) bits[i] = c->getOutputPin("OUT_" + std::to_string(i));
        return bits;
    }
    template<size_t Width>
    Signal join(const std::string& name, const std::array<Signal, Width>& bits) {
        auto c = add<BitJoiner<Width>>(name);
        for (size_t i = 0; i < Width; ++i) connect(bits[i], c->getInputPin("IN_" + std::to_string(i)));
        return c->template getOutputPin<Width>("OUT");
    }
    template<class Gate>
    Signal binary(const std::string& name, const Signal& a, const Signal& b) {
        auto c = add<Gate>(name);
        connect(a, c->getInputPin("A")); connect(b, c->getInputPin("B"));
        return c->getOutputPin("OUT");
    }
    Signal invert(const std::string& name, const Signal& a) {
        auto c = add<NOTGate>(name); connect(a, c->getInputPin("IN"));
        return c->getOutputPin("OUT");
    }
    Signal mux(const std::string& name, const Signal& a, const Signal& b, const Signal& sel) {
        auto c = add<Mux2to1>(name);
        connect(a, c->getInputPin("A")); connect(b, c->getInputPin("B")); connect(sel, c->getInputPin("SEL"));
        return c->getOutputPin("OUT");
    }
    Signal any(const std::string& name, std::vector<Signal> signals) {
        if (signals.empty()) return constant<1>(name + "_ZERO", 0);
        for (size_t i = 1; i < signals.size(); ++i)
            signals[0] = binary<ORGate>(name + "_" + std::to_string(i), signals[0], signals[i]);
        return signals[0];
    }
    Signal selectBit8(const std::string& name, const std::array<Signal, 8>& inputs, const Signal& op) {
        auto c = add<Mux8to1>(name);
        connect(op, c->getInputPin<3>("SEL"));
        for (size_t i = 0; i < 8; ++i) connect(inputs[i], c->getInputPin("IN" + std::to_string(i)));
        return c->getOutputPin("OUT");
    }
    Signal selectBus8(const std::string& name, const std::array<Signal, 8>& inputs, const Signal& op) {
        auto c = add<Mux8to1_8bit>(name);
        connect(op, c->getInputPin<3>("SEL"));
        for (size_t i = 0; i < 8; ++i) connect(inputs[i], c->getInputPin<8>("IN" + std::to_string(i)));
        return c->getOutputPin<8>("OUT");
    }
    Signal maskBus(const std::string& name, const Signal& bus, const Signal& enable) {
        auto bits = split<8>(name + "_SPLIT", bus);
        for (size_t i = 0; i < 8; ++i)
            bits[i] = binary<ANDGate>(name + "_BIT_" + std::to_string(i), bits[i], enable);
        return join<8>(name + "_JOIN", bits);
    }
    void finish() {
        // Lexical source order keeps wire ordering identical across native/WASM
        // builds and repeated constructions, independent of allocator addresses.
        for (const auto& [key, net] : nets_) {
            const auto& [source, sinks] = net;
            builder_.addNewWireDynamic(
                source->getOwner()->getName() + "__" + source->getName() + "_internal",
                source->getWidth(), source, sinks);
        }
    }
private:
    IOComponent& root_;
    ComponentBuilder& builder_;
    std::map<std::string, std::pair<Signal, std::vector<Signal>>> nets_;
};
// Compatibility name for the original eight-bit unit builders. split/join,
// connect and gate construction are width-independent.
using Structural8Builder = StructuralBuilder;
} // namespace circuit::detail
