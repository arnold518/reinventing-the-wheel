#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
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

    NetId equal5(
        const std::string& prefix,
        const std::array<NetId, 5>& left,
        const std::array<NetId, 5>& right) {
        std::vector<NetId> equal_bits;
        equal_bits.reserve(5);
        for (size_t bit = 0; bit < 5; ++bit) {
            const auto xor_net = [&] {
                const auto name =
                    prefix + "_XOR_" + std::to_string(bit);
                builder_.addNewComponent<XORGate>(name);
                sink(
                    left[bit],
                    builder_.getInputPin<XORGate>(name, "A"));
                sink(
                    right[bit],
                    builder_.getInputPin<XORGate>(name, "B"));
                return source(
                    name + "_OUT",
                    builder_.getOutputPin<XORGate>(name, "OUT"));
            }();
            equal_bits.push_back(logicalNot(
                prefix + "_XNOR_" + std::to_string(bit),
                xor_net));
        }
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
    }

protected:
    ComponentBuilder& builder_;

private:
    std::vector<LogicNet> nets_;
};
} // namespace rv32i::detail
