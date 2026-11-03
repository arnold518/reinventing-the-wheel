#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/WordGate.hpp"
#include "modules/utility/Rewire.hpp"
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rv32i::detail {
using NetId = size_t;

struct LogicNet {
    std::string name;
    std::shared_ptr<Pin<>> source;
    std::vector<std::shared_ptr<Pin<>>> sinks;
};

// Construction-only helper shared by the RV32I control units: collect each
// source's fan-out, then emit one wire per net in creation order.
class LogicNetwork {
public:
    explicit LogicNetwork(ComponentBuilder& builder)
        : builder_(builder) {}

    NetId source(std::string name, std::shared_ptr<Pin<>> pin) {
        nets_.push_back({std::move(name), std::move(pin), {}});
        return nets_.size() - 1;
    }

    void sink(NetId net, std::shared_ptr<Pin<>> pin) {
        nets_.at(net).sinks.push_back(std::move(pin));
    }

    NetId logicalNot(const std::string& name, NetId input) {
        builder_.addNewComponent<NOTGate>(name);
        sink(input, builder_.getInputPin<NOTGate>(name, "IN"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<NOTGate>(name, "OUT"));
    }

    NetId logicalAnd(
        const std::string& name,
        NetId left,
        NetId right) {
        builder_.addNewComponent<ANDGate>(name);
        sink(left, builder_.getInputPin<ANDGate>(name, "A"));
        sink(right, builder_.getInputPin<ANDGate>(name, "B"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<ANDGate>(name, "OUT"));
    }

    NetId logicalOr(
        const std::string& name,
        NetId left,
        NetId right) {
        builder_.addNewComponent<ORGate>(name);
        sink(left, builder_.getInputPin<ORGate>(name, "A"));
        sink(right, builder_.getInputPin<ORGate>(name, "B"));
        return source(
            name + "_OUT",
            builder_.getOutputPin<ORGate>(name, "OUT"));
    }

    NetId andAll(
        const std::string& prefix,
        const std::vector<NetId>& inputs) {
        return reduce(prefix, inputs, true);
    }

    NetId orAll(
        const std::string& prefix,
        const std::vector<NetId>& inputs) {
        return reduce(prefix, inputs, false);
    }

    NetId reduce(const std::string& prefix, std::vector<NetId> inputs, bool conjunction) {
        for (size_t level=0; inputs.size()>1; ++level) {
            std::vector<NetId> next;
            for (size_t i=0; i<inputs.size(); i+=2) {
                if (i+1==inputs.size()) next.push_back(inputs[i]);
                else {
                    auto name=prefix+"_"+std::to_string(level)+"_"+std::to_string(i/2);
                    next.push_back(conjunction ? logicalAnd(name,inputs[i],inputs[i+1])
                                               : logicalOr(name,inputs[i],inputs[i+1]));
                }
            }
            inputs=std::move(next);
        }
        return inputs.front();
    }

    // Five-bit equality: XORGate<5> and NOTGate<5> (an XNOR per bit, grouped
    // as words between zero-delay Rewires), then the same AND tree.
    NetId equal5(
        const std::string& prefix,
        const std::array<NetId, 5>& left,
        const std::array<NetId, 5>& right) {
        using circuit::word::GateKind;
        const auto join = [&](const std::string& name, const std::array<NetId, 5>& bits) {
            std::vector<Rewire::WireSpec> ins;
            std::vector<Rewire::BitMap> map;
            for (size_t bit = 0; bit < 5; ++bit) {
                ins.emplace_back("B" + std::to_string(bit), 1);
                map.emplace_back("B" + std::to_string(bit), 0, "OUT", bit);
            }
            auto r = builder_.addNewComponent<Rewire>(name, ins, std::vector<Rewire::WireSpec>{{"OUT", 5}}, map);
            for (size_t bit = 0; bit < 5; ++bit) sink(bits[bit], r->getInputPin("B" + std::to_string(bit)));
            return r;
        };
        auto a = join(prefix + "_A", left), b = join(prefix + "_B", right);
        auto differ = builder_.add(circuit::word::gate(GateKind::Xor, 5), prefix + "_XOR");
        auto same = builder_.add(circuit::word::gate(GateKind::Not, 5), prefix + "_XNOR");
        words_.push_back({prefix + "_A_WORD", a->getOutputPinDynamic("OUT"), differ->getInputPinDynamic("A")});
        words_.push_back({prefix + "_B_WORD", b->getOutputPinDynamic("OUT"), differ->getInputPinDynamic("B")});
        words_.push_back({prefix + "_DIFFER", differ->getOutputPinDynamic("OUT"), same->getInputPinDynamic("IN")});
        std::vector<Rewire::WireSpec> outs;
        std::vector<Rewire::BitMap> map;
        for (size_t bit = 0; bit < 5; ++bit) {
            outs.emplace_back("B" + std::to_string(bit), 1);
            map.emplace_back("IN", bit, "B" + std::to_string(bit), 0);
        }
        auto split = builder_.addNewComponent<Rewire>(prefix + "_EQUAL_BITS",
            std::vector<Rewire::WireSpec>{{"IN", 5}}, outs, map);
        words_.push_back({prefix + "_SAME", same->getOutputPinDynamic("OUT"), split->getInputPinDynamic("IN")});
        std::vector<NetId> equal_bits;
        for (size_t bit = 0; bit < 5; ++bit)
            equal_bits.push_back(source(prefix + "_EQUAL_" + std::to_string(bit), split->getOutputPin("B" + std::to_string(bit))));
        return andAll(prefix + "_ALL", equal_bits);
    }

    NetId nonZero5(
        const std::string& prefix,
        const std::array<NetId, 5>& value) {
        return orAll(
            prefix,
            std::vector<NetId>(value.begin(), value.end()));
    }

    void materialize() {
        for (auto& net : nets_) {
            builder_.addNewWire(
                net.name,
                net.source,
                net.sinks);
        }
        for (auto& word : words_)
            builder_.addNewWireDynamic(word.name, word.source->getWidth(), word.source, {word.sink});
    }

protected:
    ComponentBuilder& builder_;

private:
    struct WordNet { std::string name; std::shared_ptr<PinBase> source, sink; };
    std::vector<LogicNet> nets_;
    std::vector<WordNet> words_;
};
} // namespace rv32i::detail
