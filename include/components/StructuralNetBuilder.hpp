#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/IOComponent.hpp"
#include "basic/PinBase.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "modules/basic/WordGate.hpp"
#include <algorithm>
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
    // A one-bit gate on every bit pair (NOT: on every bit of `a`). From two
    // bits on: one word gate between zero-delay Rewires `<name>_A`/`_B`
    // (inputs) and `<name>_BITS` (output), the same gates grouped as a word;
    // a single bit gets the plain gate named `name`.
    std::vector<PinPtr> gateBits(circuit::word::GateKind kind, const std::string& name,
                                 const std::vector<PinPtr>& a, const std::vector<PinPtr>& b = {}) {
        using circuit::word::GateKind;
        const bool unary = kind == GateKind::Not;
        if (!unary && a.size() != b.size()) throwInvalidConnection();
        const size_t n = a.size();
        // Wider than a word gate: one word gate per 32 bits, `<name>_<k>`.
        if (n > 32) {
            std::vector<PinPtr> out;
            for (size_t first = 0, chunk = 0; first < n; first += 32, ++chunk) {
                const size_t last = std::min(n, first + 32);
                const std::vector<PinPtr> part_a(a.begin() + first, a.begin() + last);
                const std::vector<PinPtr> part_b = unary ? std::vector<PinPtr>{} : std::vector<PinPtr>(b.begin() + first, b.begin() + last);
                const auto part = gateBits(kind, name + "_" + std::to_string(chunk), part_a, part_b);
                out.insert(out.end(), part.begin(), part.end());
            }
            return out;
        }
        if (n < 2) {
            std::vector<PinPtr> out;
            for (size_t i = 0; i < n; ++i) {
                switch (kind) {
                case GateKind::Not: out.push_back(invert(name, a[i])); break;
                case GateKind::And: out.push_back(binary<ANDGate>(name, a[i], b[i])); break;
                case GateKind::Nand: out.push_back(binary<NANDGate>(name, a[i], b[i])); break;
                case GateKind::Or: out.push_back(binary<ORGate>(name, a[i], b[i])); break;
                case GateKind::Nor: out.push_back(binary<NORGate>(name, a[i], b[i])); break;
                case GateKind::Xor: out.push_back(binary<XORGate>(name, a[i], b[i])); break;
                }
            }
            return out;
        }
        auto gate = builder_.add(circuit::word::gate(kind, n), name);
        connect(joinBits(name + (unary ? "_IN" : "_A"), a), gate->getInputPinDynamic(unary ? "IN" : "A"));
        if (!unary) connect(joinBits(name + "_B", b), gate->getInputPinDynamic("B"));
        return splitWord(name + "_BITS", gate->getOutputPinDynamic("OUT"));
    }
    std::vector<PinPtr> invertBits(const std::string& name, const std::vector<PinPtr>& bits) {
        return gateBits(circuit::word::GateKind::Not, name, bits);
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
    // Adapters (zero-delay Rewires) made through this builder. A split's bits
    // remember their word; a join remembers its bits. A join of bits that came
    // from splits reads those words directly, a split of a join returns the
    // join's bits, and finish() removes adapters that drive nothing, so a
    // split is never wired straight into a join (or a join into a split).
    std::vector<PinPtr> splitWord(const std::string& name, const PinPtr& word) {
        if (const auto found = bits_of_join_.find(word.get()); found != bits_of_join_.end()) return found->second;
        const auto width = word->getWidth();
        std::vector<Rewire::WireSpec> outs;
        std::vector<Rewire::BitMap> map;
        for (size_t i = 0; i < width; ++i) { outs.emplace_back("B" + std::to_string(i), 1); map.emplace_back("IN", i, "B" + std::to_string(i), 0); }
        auto split = builder_.addNewComponent<Rewire>(name, std::vector<Rewire::WireSpec>{{"IN", width}}, outs, map);
        connect(word, split->getInputPinDynamic("IN"));
        std::vector<PinPtr> bits;
        for (size_t i = 0; i < width; ++i) {
            bits.push_back(split->getOutputPinDynamic("B" + std::to_string(i)));
            bit_of_[bits.back().get()] = {word, i};
        }
        adapters_.push_back(split);
        return bits;
    }
    PinPtr joinBits(const std::string& name, const std::vector<PinPtr>& bits) {
        // Every bit of one word, in order: that word.
        if (!bits.empty()) {
            const auto first = bit_of_.find(bits.front().get());
            if (first != bit_of_.end() && first->second.word->getWidth() == bits.size()) {
                bool whole = true;
                for (size_t i = 0; i < bits.size() && whole; ++i) {
                    const auto at = bit_of_.find(bits[i].get());
                    whole = at != bit_of_.end() && at->second.word == first->second.word && at->second.bit == i;
                }
                if (whole) return first->second.word;
            }
        }
        // One input per source word (its split is skipped) or loose bit.
        std::vector<Rewire::WireSpec> ins;
        std::vector<Rewire::BitMap> map;
        std::vector<std::pair<std::string, PinPtr>> sources;
        std::map<const PinBase*, std::string> word_inputs;
        for (size_t i = 0; i < bits.size(); ++i) {
            if (const auto at = bit_of_.find(bits[i].get()); at != bit_of_.end()) {
                auto& input = word_inputs[at->second.word.get()];
                if (input.empty()) {
                    input = "W" + std::to_string(word_inputs.size() - 1);
                    ins.emplace_back(input, at->second.word->getWidth());
                    sources.emplace_back(input, at->second.word);
                }
                map.emplace_back(input, at->second.bit, "OUT", i);
            } else {
                const auto input = "B" + std::to_string(i);
                ins.emplace_back(input, 1);
                sources.emplace_back(input, bits[i]);
                map.emplace_back(input, 0, "OUT", i);
            }
        }
        auto join = builder_.addNewComponent<Rewire>(name, ins, std::vector<Rewire::WireSpec>{{"OUT", bits.size()}}, map);
        for (const auto& [input, source] : sources) connect(source, join->getInputPinDynamic(input));
        const auto out = join->getOutputPinDynamic("OUT");
        bits_of_join_[out.get()] = bits;
        adapters_.push_back(join);
        return out;
    }

    void finish() {
        pruneAdapters();
        size_t index = 0;
        for (const auto& [key, net] : nets_) {
            auto name = wireName(net.first, index++);
            while (builder_.getWireDynamic(name)) name = wireName(net.first, index++);
            builder_.addNewWireDynamic(std::move(name), net.first->getWidth(), net.first, net.second);
        }
        nets_.clear();
    }

protected:
    struct BitOf { PinPtr word; size_t bit; };
    std::map<const PinBase*, BitOf> bit_of_;
    std::map<const PinBase*, std::vector<PinPtr>> bits_of_join_;
    std::vector<std::shared_ptr<Component>> adapters_;

    bool drives(const PinPtr& pin) const {
        if (pin->getExternalWireBase()) return true;  // wired by another builder
        const auto found = nets_.find(netKey(pin));
        return found != nets_.end() && found->second.first == pin && !found->second.second.empty();
    }
    // Remove adapters whose outputs drive nothing, until none is left.
    void pruneAdapters() {
        for (bool changed = true; changed;) {
            changed = false;
            for (auto it = adapters_.begin(); it != adapters_.end();) {
                auto io = std::dynamic_pointer_cast<IOComponent>(*it);
                bool used = false;
                for (const auto& [name, pin] : io->getAllOutputPins()) used = used || drives(pin);
                if (used) { ++it; continue; }
                for (const auto& [name, pin] : io->getAllInputPins())
                    for (auto net = nets_.begin(); net != nets_.end();) {
                        auto& sinks = net->second.second;
                        sinks.erase(std::remove(sinks.begin(), sinks.end(), pin), sinks.end());
                        net = sinks.empty() ? nets_.erase(net) : std::next(net);
                    }
                builder_.removeComponent(*it);
                it = adapters_.erase(it);
                changed = true;
            }
        }
    }
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
