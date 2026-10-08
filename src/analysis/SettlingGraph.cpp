#include "SettlingGraph.hpp"
#include "TimingCircuit.hpp"
#include "basic/PinBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Rewire.hpp"
#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

namespace circuit::analysis::detail::settling {
namespace {

using Leaf = TimingLeaf;

std::optional<Kind> gateKind(const IOComponent& c) {
    if (typeid(c) == typeid(NOTGate)) return Kind::Not;
    if (typeid(c) == typeid(ANDGate)) return Kind::And;
    if (typeid(c) == typeid(NANDGate)) return Kind::Nand;
    if (typeid(c) == typeid(ORGate)) return Kind::Or;
    if (typeid(c) == typeid(NORGate)) return Kind::Nor;
    if (typeid(c) == typeid(XORGate)) return Kind::Xor;
    return std::nullopt;
}

class Builder {
    const std::shared_ptr<Component>& root_;
    const SettlingOptions& options_;
    Graph& graph_;
    std::vector<std::string>& diagnostics_;
    std::shared_ptr<IOComponent> root_io_;
    bool root_is_leaf_ = false, failed_ = false;

    std::vector<Leaf> leaves_;
    std::unordered_map<const PinBase*, int> base_;
    std::unordered_map<const PinBase*, std::shared_ptr<PinBase>> driver_;
    std::unordered_set<const PinBase*> clock_roots_, clock_sinks_;

    void fail(const std::string& message) {
        failed_ = true;
        if (diagnostics_.size() < 64) diagnostics_.push_back(message);
    }
    bool rootPin(const PinBase& pin) const { return pin.getOwner() == root_; }
    int reserve(const std::shared_ptr<PinBase>& pin) {
        const auto [it, added] = base_.emplace(pin.get(), static_cast<int>(graph_.nodes.size()));
        if (added) for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            graph_.nodes.emplace_back();
            graph_.nodes.back().pin = pin.get();
            graph_.nodes.back().bit = bit;
        }
        return it->second;
    }
    Node& at(const std::shared_ptr<PinBase>& pin, size_t bit) { return graph_.nodes[base_.at(pin.get()) + bit]; }
    int index(const std::shared_ptr<PinBase>& pin, size_t bit) const {
        return base_.at(pin.get()) + static_cast<int>(bit);
    }

    void collect() {
        collectTimingLeaves(root_, options_.contracts, 256, leaves_, {
            nullptr,
            [&](const Component& part) { fail(part.getID() + ": no timing contract for " + part.getTypeName()); }});
        root_is_leaf_ = leaves_.size() == 1 && leaves_.front().component == root_;
    }

    // Which pin drives each reading pin: a leaf output or a root input.
    void findDrivers() {
        std::unordered_set<const PinBase*> sources;
        if (!root_is_leaf_) for (const auto& [name, pin] : root_io_->getAllInputPins()) sources.insert(pin.get());
        for (const auto& leaf : leaves_)
            for (const auto& [name, pin] : leaf.component->getAllOutputPins()) sources.insert(pin.get());
        const auto find = [&](const std::shared_ptr<PinBase>& reader) {
            const auto source = drivingPin(reader, sources);
            if (source == reader || !sources.contains(source.get())) return; // nothing drives it
            if (source->getWidth() != reader->getWidth()) throw std::runtime_error("Width mismatch at " + reader->getID());
            driver_.emplace(reader.get(), source);
        };
        if (root_is_leaf_) return;
        for (const auto& [name, pin] : root_io_->getAllOutputPins()) find(pin);
        for (const auto& leaf : leaves_)
            for (const auto& [name, pin] : leaf.component->getAllInputPins()) find(pin);
    }

    void makeSource(const std::shared_ptr<PinBase>& pin, long long arrival, bool splittable) {
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            auto& node = at(pin, bit);
            node.kind = Kind::Source;
            node.delay = arrival;
            if (splittable && options_.binary_sources) {
                node.var = static_cast<int>(graph_.variables.size());
                graph_.variables.emplace_back(pin.get(), bit);
                graph_.variable_node.push_back(index(pin, bit));
            }
        }
    }
    void makeConstant(const std::shared_ptr<PinBase>& pin, uint64_t value) {
        if (pin->getWidth() < 64 && value >= (uint64_t{1} << pin->getWidth()))
            throw std::runtime_error("Value exceeds pin width: " + pin->getID());
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            auto& node = at(pin, bit);
            node.kind = Kind::Stable;
            node.constant = static_cast<int8_t>((value >> bit) & 1);
        }
    }
    void makeRootInput(const std::shared_ptr<PinBase>& pin) {
        if (clock_roots_.contains(pin.get())) return; // not data
        const auto name = pin->getName();
        if (const auto fixed = options_.fixed_inputs.find(name); fixed != options_.fixed_inputs.end()) {
            makeConstant(pin, fixed->second);
            return;
        }
        long long arrival = 0;
        if (const auto it = options_.input_arrival.find(name); it != options_.input_arrival.end())
            arrival = static_cast<long long>(it->second);
        const auto final_value = options_.final_inputs.find(name);
        makeSource(pin, options_.sources == SettlingSources::StateOnly ? STABLE : arrival,
                   final_value == options_.final_inputs.end());
        if (final_value == options_.final_inputs.end()) return;
        if (pin->getWidth() < 64 && final_value->second >= (uint64_t{1} << pin->getWidth()))
            throw std::runtime_error("Value exceeds pin width: " + pin->getID());
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
            at(pin, bit).constant = static_cast<int8_t>((final_value->second >> bit) & 1);
    }

    void makeLeaf(const Leaf& leaf) {
        const auto& c = *leaf.component;
        const auto& m = leaf.contract;
        const auto own = [&](const std::string& name, PinType type) {
            auto pin = type == PinType::INPUT ? c.getInputPinDynamic(name) : c.getOutputPinDynamic(name);
            if (!pin) throw std::runtime_error(c.getID() + ": contract names missing pin " + name);
            return pin;
        };
        std::set<std::string> classified{m.clock_pin};
        for (const auto& arc : m.arcs) classified.insert(arc.input);
        for (const auto& capture : m.captures) {
            if (capture.edge != ClockEdge::Rising) fail(c.getID() + ": falling-edge capture is unsupported");
            classified.insert(capture.pin);
        }
        for (const auto& [name, value] : m.fixed_inputs) classified.insert(name);
        for (const auto& name : m.unused_inputs) classified.insert(name);

        for (const auto& [name, pin] : c.getAllInputPins()) {
            if (!classified.contains(name)) fail(pin->getID() + ": input absent from timing contract");
            if (name == m.clock_pin) continue;
            if (leaf.component == root_) { makeRootInput(pin); continue; }
            const auto source = driver_.find(pin.get());
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                auto& node = at(pin, bit);
                if (source == driver_.end()) continue; // undriven: never changes, value unknown
                node.kind = Kind::Buf;
                node.a = index(source->second, bit);
            }
        }
        for (const auto& capture : m.captures) {
            const auto pin = own(capture.pin, PinType::INPUT);
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
                graph_.groups.front().endpoints.emplace_back(index(pin, bit), static_cast<long long>(capture.setup));
        }
        for (const auto& [name, value] : m.fixed_inputs) {
            const auto pin = own(name, PinType::INPUT);
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
                graph_.required.emplace_back(index(pin, bit), static_cast<int8_t>((value >> bit) & 1), pin->getID());
        }

        const auto delay = [&]() {
            return static_cast<long long>(static_cast<const BasicComponent&>(c).getDelay());
        };
        if (leaf.builtin_rules) {
            if (const auto kind = gateKind(c)) {
                auto& out = at(own("OUT", PinType::OUTPUT), 0);
                out.kind = *kind;
                out.delay = delay();
                if (*kind == Kind::Not) out.a = index(own("IN", PinType::INPUT), 0);
                else { out.a = index(own("A", PinType::INPUT), 0); out.b = index(own("B", PinType::INPUT), 0); }
                return;
            }
            if (typeid(c) == typeid(Rewire)) {
                const auto& rewire = static_cast<const Rewire&>(c);
                for (const auto& [name, pin] : c.getAllOutputPins())
                    for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                        auto& node = at(pin, bit);
                        node.kind = Kind::Stable;
                        if (rewire.getUnmappedDefault() == Rewire::UnmappedBitValue::LOW) node.constant = 0;
                        if (rewire.getUnmappedDefault() == Rewire::UnmappedBitValue::HIGH) node.constant = 1;
                    }
                for (const auto& map : rewire.getBitMappings()) {
                    auto& node = at(own(map.dst_wire, PinType::OUTPUT), map.dst_bit);
                    node.kind = Kind::Buf;
                    node.constant = UNKNOWN;
                    node.a = index(own(map.src_wire, PinType::INPUT), map.src_bit);
                    node.delay = delay();
                }
                return;
            }
            if (isBitSplitter(c)) {
                const auto in = own("IN", PinType::INPUT);
                for (unsigned bit = 0; bit < in->getWidth(); ++bit) {
                    auto& node = at(own("OUT_" + std::to_string(bit), PinType::OUTPUT), 0);
                    node.kind = Kind::Buf; node.a = index(in, bit); node.delay = delay();
                }
                return;
            }
            if (isBitJoiner(c)) {
                const auto out = own("OUT", PinType::OUTPUT);
                for (unsigned bit = 0; bit < out->getWidth(); ++bit) {
                    auto& node = at(out, bit);
                    node.kind = Kind::Buf; node.delay = delay();
                    node.a = index(own("IN_" + std::to_string(bit), PinType::INPUT), 0);
                }
                return;
            }
        }
        // Any other contract is opaque: timed arcs and launches, no values.
        for (const auto& [name, pin] : c.getAllOutputPins()) {
            if (const auto constant = m.constant_outputs.find(name); constant != m.constant_outputs.end()) {
                makeConstant(pin, constant->second);
                continue;
            }
            if (std::find(m.stable_outputs.begin(), m.stable_outputs.end(), name) != m.stable_outputs.end())
                continue;
            Hub hub;
            bool launched = false;
            for (const auto& launch : m.launches) if (launch.pin == name) {
                if (launch.edge != ClockEdge::Rising) fail(c.getID() + ": falling-edge launch is unsupported");
                launched = true;
                if (options_.sources != SettlingSources::InputsOnly)
                    hub.launch = std::max(hub.launch, static_cast<long long>(launch.delay.maximum));
            }
            for (const auto& arc : m.arcs) if (arc.output == name) {
                const auto in = own(arc.input, PinType::INPUT);
                for (unsigned bit = 0; bit < in->getWidth(); ++bit)
                    hub.inputs.emplace_back(index(in, bit), static_cast<long long>(arc.delay.maximum));
            }
            if (!launched && hub.inputs.empty()) {
                fail(pin->getID() + ": output absent from timing contract");
                continue;
            }
            if (hub.inputs.empty()) { makeSource(pin, hub.launch, true); continue; }
            graph_.hubs.push_back(std::move(hub));
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                auto& node = at(pin, bit);
                node.kind = Kind::Hub;
                node.hub = static_cast<int>(graph_.hubs.size()) - 1;
            }
        }
    }

    bool sort() {
        std::vector<int> waiting(graph_.nodes.size());
        std::vector<std::vector<int>> readers(graph_.nodes.size());
        for (int n = 0; n < static_cast<int>(graph_.nodes.size()); ++n)
            graph_.inputsOf(n, [&](int in) { ++waiting[n]; readers[in].push_back(n); });
        for (int n = 0; n < static_cast<int>(graph_.nodes.size()); ++n) if (!waiting[n]) graph_.order.push_back(n);
        for (size_t next = 0; next < graph_.order.size(); ++next)
            for (const int reader : readers[graph_.order[next]]) if (!--waiting[reader]) graph_.order.push_back(reader);
        if (graph_.order.size() == graph_.nodes.size()) return true;
        fail("Combinational dependency cycle: no finite settling bound");
        for (int n = 0, shown = 0; n < static_cast<int>(graph_.nodes.size()) && shown < 8; ++n) if (waiting[n]) {
            diagnostics_.push_back("cycle or downstream: " + graph_.label(n));
            ++shown;
        }
        return false;
    }


public:
    Builder(const std::shared_ptr<Component>& root, const SettlingOptions& options,
            Graph& graph, std::vector<std::string>& diagnostics)
        : root_(root), options_(options), graph_(graph), diagnostics_(diagnostics) {}

    bool build() {
        try {
            root_io_ = std::dynamic_pointer_cast<IOComponent>(root_);
            if (!root_io_) throw std::runtime_error("Root has no pins");
            collect();
            if (failed_) return false;

            for (const auto* pins : {&root_io_->getAllInputPins(), &root_io_->getAllOutputPins()})
                for (const auto& [name, pin] : *pins) reserve(pin);
            for (const auto& leaf : leaves_)
                for (const auto* pins : {&leaf.component->getAllInputPins(), &leaf.component->getAllOutputPins()})
                    for (const auto& [name, pin] : *pins) reserve(pin);

            findDrivers();

            // One ideal clock, taken straight from a root input.
            for (const auto& leaf : leaves_) {
                if (leaf.contract.clock_pin.empty()) {
                    if (!leaf.contract.captures.empty() || !leaf.contract.launches.empty())
                        fail(leaf.component->getID() + ": clocked contract lacks a clock pin");
                    continue;
                }
                const auto pin = leaf.component->getInputPinDynamic(leaf.contract.clock_pin);
                if (!pin) throw std::runtime_error(leaf.component->getID() + ": contract names a missing clock pin");
                clock_sinks_.insert(pin.get());
                if (leaf.component == root_) { clock_roots_.insert(pin.get()); continue; }
                const auto source = driver_.find(pin.get());
                if (source == driver_.end() || !rootPin(*source->second))
                    fail(pin->getID() + ": derived, gated or undriven clock is unsupported");
                else clock_roots_.insert(source->second.get());
            }
            for (const auto& [sink, source] : driver_)
                if (clock_roots_.contains(source.get()) && !clock_sinks_.contains(sink))
                    fail(source->getID() + ": clock used as data");
            for (const auto& [name, value] : options_.fixed_inputs)
                if (!root_io_->getInputPinDynamic(name)) fail("fixed_inputs names no root input: " + name);
            for (const auto& [name, value] : options_.final_inputs)
                if (!root_io_->getInputPinDynamic(name) || options_.fixed_inputs.contains(name))
                    fail("final_inputs must name a root input that is not fixed: " + name);
            for (const auto& [name, tick] : options_.input_arrival)
                if (!root_io_->getInputPinDynamic(name)) fail("input_arrival names no root input: " + name);
            if (failed_) return false;

            graph_.groups.push_back({"(captures)", {}});
            if (!root_is_leaf_) {
                for (const auto& [name, pin] : root_io_->getAllInputPins()) makeRootInput(pin);
                for (const auto& [name, pin] : root_io_->getAllOutputPins()) {
                    const auto source = driver_.find(pin.get());
                    if (source == driver_.end()) continue;
                    for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                        auto& node = at(pin, bit);
                        node.kind = Kind::Buf;
                        node.a = index(source->second, bit);
                    }
                }
            }
            for (const auto& leaf : leaves_) makeLeaf(leaf);
            for (const auto& [name, pin] : root_io_->getAllOutputPins()) {
                Group group{name, {}};
                for (unsigned bit = 0; bit < pin->getWidth(); ++bit) group.endpoints.emplace_back(index(pin, bit), 0);
                graph_.groups.push_back(std::move(group));
            }
            if (failed_ || !sort()) return false;

            graph_.binary.assign(graph_.nodes.size(), 0);
            for (const int n : graph_.order) {
                const auto& x = graph_.nodes[n];
                switch (x.kind) {
                case Kind::Stable: graph_.binary[n] = x.constant != UNKNOWN; break;
                case Kind::Source: graph_.binary[n] = x.var >= 0 || x.constant != UNKNOWN; break;
                case Kind::Buf: case Kind::Not: graph_.binary[n] = graph_.binary[x.a]; break;
                case Kind::Hub: break;
                default: graph_.binary[n] = graph_.binary[x.a] && graph_.binary[x.b]; break;
                }
            }
            return true;
        } catch (const std::exception& error) {
            fail(error.what());
            return false;
        }
    }
};

} // namespace

std::string Graph::label(int n) const {
    return nodes[n].pin->getID() + "[" + std::to_string(nodes[n].bit) + "]";
}

bool buildGraph(const std::shared_ptr<Component>& root, const SettlingOptions& options,
                Graph& graph, std::vector<std::string>& diagnostics) {
    return Builder(root, options, graph, diagnostics).build();
}

} // namespace circuit::analysis::detail::settling
