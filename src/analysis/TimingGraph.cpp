#include "TimingGraph.hpp"
#include "TimingCircuit.hpp"
#include "basic/PinBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "modules/utility/Rewire.hpp"
#include "../modules/rv32i/RegisterFile32x32Behavioral.hpp"
#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <typeinfo>
#include <unordered_set>

namespace circuit::analysis::detail::timing {
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
    const GraphRequest& request_;
    Graph& graph_;
    std::vector<std::string>& diagnostics_;
    std::shared_ptr<IOComponent> root_io_;
    bool root_is_leaf_ = false, failed_ = false;

    std::vector<Leaf> leaves_;
    std::unordered_map<const PinBase*, std::shared_ptr<PinBase>> driver_;
    // Pins the request describes although nothing in the circuit drives them.
    std::vector<std::shared_ptr<PinBase>> externals_;
    std::unordered_set<const PinBase*> clock_roots_, clock_sinks_;

    void fail(const std::string& message) {
        failed_ = true;
        if (diagnostics_.size() < 64) diagnostics_.push_back(message);
    }
    bool rootPin(const PinBase& pin) const { return pin.getOwner() == root_; }
    void reserve(const std::shared_ptr<PinBase>& pin) {
        if (!graph_.first_bit.emplace(pin.get(), static_cast<int>(graph_.nodes.size())).second) return;
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            graph_.nodes.emplace_back();
            graph_.nodes.back().pin = pin.get();
            graph_.nodes.back().bit = bit;
        }
    }
    Node& at(const std::shared_ptr<PinBase>& pin, size_t bit) {
        return graph_.nodes[graph_.first_bit.at(pin.get()) + bit];
    }
    int index(const std::shared_ptr<PinBase>& pin, size_t bit) const {
        return graph_.first_bit.at(pin.get()) + static_cast<int>(bit);
    }
    bool described(const PinBase* pin) const {
        return request_.fixed.contains(pin) || request_.final_value.contains(pin) || request_.arrival.contains(pin);
    }

    void collect() {
        collectTimingLeaves(root_, request_.contracts, 256, leaves_, {
            nullptr,
            [&](const Component& part) { fail(part.getID() + ": no timing contract for " + part.getTypeName()); }});
        root_is_leaf_ = leaves_.size() == 1 && leaves_.front().component == root_;
    }

    // Which pin drives each reading pin: a leaf output, a root input, or a
    // pin the request describes.
    void findDrivers() {
        std::unordered_set<const PinBase*> sources;
        if (!root_is_leaf_) for (const auto& [name, pin] : root_io_->getAllInputPins()) sources.insert(pin.get());
        for (const auto& leaf : leaves_)
            for (const auto& [name, pin] : leaf.component->getAllOutputPins()) sources.insert(pin.get());
        const auto find = [&](const std::shared_ptr<PinBase>& reader) {
            const auto source = drivingPin(reader, sources);
            if (source == reader && !described(reader.get())) return; // nothing drives it
            if (!sources.contains(source.get())) {
                if (!described(source.get())) return;
                if (!graph_.first_bit.contains(source.get())) { reserve(source); externals_.push_back(source); }
            }
            if (source == reader) return; // a described leaf input is its own source
            if (source->getWidth() != reader->getWidth()) throw std::runtime_error("Width mismatch at " + reader->getID());
            driver_.emplace(reader.get(), source);
        };
        if (root_is_leaf_) return;
        for (const auto& [name, pin] : root_io_->getAllOutputPins()) find(pin);
        for (const auto& leaf : leaves_)
            for (const auto& [name, pin] : leaf.component->getAllInputPins()) find(pin);
    }

    void makeSource(const std::shared_ptr<PinBase>& pin, Tick arrival, bool variable) {
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            auto& node = at(pin, bit);
            node.kind = Kind::Source;
            node.delay = arrival;
            if (variable && request_.binary_sources) {
                node.var = static_cast<int>(graph_.variables.size());
                graph_.variables.emplace_back(pin.get(), bit);
                graph_.variable_node.push_back(index(pin, bit));
            }
        }
    }
    void setValue(const std::shared_ptr<PinBase>& pin, uint64_t value) {
        if (pin->getWidth() < 64 && value >= (uint64_t{1} << pin->getWidth()))
            throw std::runtime_error("Value exceeds pin width: " + pin->getID());
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
            at(pin, bit).constant = static_cast<int8_t>((value >> bit) & 1);
    }
    void makeConstant(const std::shared_ptr<PinBase>& pin, uint64_t value) {
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) at(pin, bit).kind = Kind::Stable;
        setValue(pin, value);
    }
    // A root input, or any other pin the request describes.
    void makeExternal(const std::shared_ptr<PinBase>& pin) {
        if (clock_roots_.contains(pin.get())) return; // not data
        if (const auto fixed = request_.fixed.find(pin.get()); fixed != request_.fixed.end()) {
            makeConstant(pin, fixed->second);
            return;
        }
        const auto arrival = request_.arrival.find(pin.get());
        const auto final_value = request_.final_value.find(pin.get());
        makeSource(pin, arrival != request_.arrival.end() ? arrival->second : request_.default_arrival,
                   final_value == request_.final_value.end());
        if (final_value != request_.final_value.end()) setValue(pin, final_value->second);
    }

    Tick delayOf(const IOComponent& c) const {
        return static_cast<Tick>(static_cast<const BasicComponent&>(c).getDelay());
    }

    // The compact FF endpoint, when its built-in contract applies.
    bool makeEndpoint(const Leaf& leaf) {
        if (!leaf.builtin_rules) return false;
        const auto endpoint = std::dynamic_pointer_cast<BehavioralMemoryEndpoint>(leaf.component);
        if (!endpoint) return false;
        const auto& c = *leaf.component;
        Part part;
        part.endpoint = endpoint.get();
        part.delay = delayOf(c);
        const auto in = [&](const char* name) { return c.getInputPinDynamic(name); };
        part.req_valid = index(in("REQ_VALID"), 0);
        part.rsp_ready = index(in("RSP_READY"), 0);
        part.op = index(in("REQ_OP"), 0);
        part.address = index(in("REQ_ADDR"), 0);
        part.address_bits = static_cast<unsigned>(in("REQ_ADDR")->getWidth());
        part.mask = index(in("REQ_WRITE_MASK"), 0);
        part.mask_bits = static_cast<unsigned>(in("REQ_WRITE_MASK")->getWidth());
        if (in("REQ_OP")->getWidth() != 3) throw std::runtime_error(c.getID() + ": unexpected endpoint pins");
        part.bytes_per_word = endpoint->memoryStorage().wordBits() / 8;
        part.capacity_bytes = endpoint->memoryStorage().capacityBytes();
        const std::pair<const char*, PartOutput> outputs[] = {
            {"REQ_READY", Ready}, {"RSP_VALID", Valid}, {"RSP_READ_DATA", Data}, {"RSP_STATUS", Status}};
        for (const auto& [name, which] : outputs) {
            const auto pin = c.getOutputPinDynamic(name);
            part.outputs[which] = pin.get();
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                auto& node = at(pin, bit);
                node.kind = Kind::Part;
                node.owner = static_cast<int>(graph_.parts.size());
                node.output = which;
            }
        }
        graph_.parts.push_back(part);
        return true;
    }

    void makeLeaf(const Leaf& leaf) {
        const auto& c = *leaf.component;
        const auto& m = leaf.contract;
        const auto own = [&](const std::string& name, PinType type) {
            auto pin = type == PinType::INPUT ? c.getInputPinDynamic(name) : c.getOutputPinDynamic(name);
            if (!pin) throw std::runtime_error(c.getID() + ": contract names missing pin " + name);
            return pin;
        };
        const auto otherEdge = [&](ClockEdge edge, const char* what) {
            if (edge == request_.phase) return false;
            if (request_.other_edge_is_error) fail(c.getID() + ": falling-edge " + what + " is unsupported");
            return true;
        };
        std::set<std::string> classified{m.clock_pin};
        for (const auto& arc : m.arcs) classified.insert(arc.input);
        for (const auto& capture : m.captures) classified.insert(capture.pin);
        for (const auto& [name, value] : m.fixed_inputs) classified.insert(name);
        for (const auto& name : m.unused_inputs) classified.insert(name);

        for (const auto& [name, pin] : c.getAllInputPins()) {
            if (!classified.contains(name)) fail(pin->getID() + ": input absent from timing contract");
            if (name == m.clock_pin) continue;
            const auto source = driver_.find(pin.get());
            if (source == driver_.end()) {
                // The root's own input, a described pin, or undriven (never
                // changes, value unknown).
                if (leaf.component == root_ || described(pin.get())) makeExternal(pin);
                continue;
            }
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                auto& node = at(pin, bit);
                node.kind = Kind::Buf;
                node.a = index(source->second, bit);
            }
        }
        for (const auto& capture : m.captures) {
            if (otherEdge(capture.edge, "capture")) continue;
            const auto pin = own(capture.pin, PinType::INPUT);
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
                graph_.groups.front().endpoints.emplace_back(index(pin, bit), static_cast<Tick>(capture.setup));
        }
        for (const auto& [name, value] : m.fixed_inputs) {
            const auto pin = own(name, PinType::INPUT);
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
                graph_.required.emplace_back(index(pin, bit), static_cast<int8_t>((value >> bit) & 1), pin->getID());
        }

        if (leaf.builtin_rules) {
            if (const auto kind = gateKind(c)) {
                auto& out = at(own("OUT", PinType::OUTPUT), 0);
                out.kind = *kind;
                out.delay = delayOf(c);
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
                    node.constant = 2;
                    node.a = index(own(map.src_wire, PinType::INPUT), map.src_bit);
                    node.delay = delayOf(c);
                }
                return;
            }
            if (isBitSplitter(c)) {
                const auto in = own("IN", PinType::INPUT);
                for (unsigned bit = 0; bit < in->getWidth(); ++bit) {
                    auto& node = at(own("OUT_" + std::to_string(bit), PinType::OUTPUT), 0);
                    node.kind = Kind::Buf; node.a = index(in, bit); node.delay = delayOf(c);
                }
                return;
            }
            if (isBitJoiner(c)) {
                const auto out = own("OUT", PinType::OUTPUT);
                for (unsigned bit = 0; bit < out->getWidth(); ++bit) {
                    auto& node = at(out, bit);
                    node.kind = Kind::Buf; node.delay = delayOf(c);
                    node.a = index(own("IN_" + std::to_string(bit), PinType::INPUT), 0);
                }
                return;
            }
            if (request_.phase == ClockEdge::Rising && request_.launches && makeEndpoint(leaf)) return;
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
            hub.binary_with_inputs = leaf.builtin_rules && typeid(c) == typeid(RegisterFile32x32Behavioral);
            bool launched = false;
            for (const auto& launch : m.launches) if (launch.pin == name) {
                launched = true;
                if (otherEdge(launch.edge, "launch") || !request_.launches) continue;
                hub.launch = std::max(hub.launch, static_cast<Tick>(launch.delay.maximum));
            }
            for (const auto& arc : m.arcs) if (arc.output == name) {
                const auto in = own(arc.input, PinType::INPUT);
                for (unsigned bit = 0; bit < in->getWidth(); ++bit)
                    hub.inputs.emplace_back(index(in, bit), static_cast<Tick>(arc.delay.maximum));
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
                node.owner = static_cast<int>(graph_.hubs.size()) - 1;
            }
        }
    }

    // Inputs first wherever there is no loop; the members of loops follow.
    void sort() {
        const int count = static_cast<int>(graph_.nodes.size());
        std::vector<int> waiting(count);
        graph_.readers.assign(count, {});
        for (int n = 0; n < count; ++n)
            graph_.inputsOf(n, [&](int in) { ++waiting[n]; graph_.readers[in].push_back(n); });
        for (int n = 0; n < count; ++n) if (!waiting[n]) graph_.order.push_back(n);
        for (size_t next = 0; next < graph_.order.size(); ++next)
            for (const int reader : graph_.readers[graph_.order[next]]) if (!--waiting[reader]) graph_.order.push_back(reader);
        graph_.has_loop = static_cast<int>(graph_.order.size()) != count;
        for (int n = 0; n < count; ++n) if (waiting[n] > 0) graph_.order.push_back(n);
    }

    // One ideal clock, taken straight from a root input.
    void checkClocks() {
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
            if (source == driver_.end() || !rootPin(*source->second)) {
                if (request_.check_clocks) fail(pin->getID() + ": derived, gated or undriven clock is unsupported");
            } else clock_roots_.insert(source->second.get());
        }
        if (!request_.check_clocks) return;
        for (const auto& [sink, source] : driver_)
            if (clock_roots_.contains(source.get()) && !clock_sinks_.contains(sink))
                fail(source->getID() + ": clock used as data");
    }

public:
    Builder(const std::shared_ptr<Component>& root, const GraphRequest& request,
            Graph& graph, std::vector<std::string>& diagnostics)
        : root_(root), request_(request), graph_(graph), diagnostics_(diagnostics) {}

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
            checkClocks();
            if (failed_) return false;

            graph_.groups.push_back({"(captures)", {}});
            for (const auto& pin : externals_) makeExternal(pin);
            if (!root_is_leaf_) {
                for (const auto& [name, pin] : root_io_->getAllInputPins()) makeExternal(pin);
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
            if (failed_) return false;
            sort();
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

bool buildGraph(const std::shared_ptr<Component>& root, const GraphRequest& request,
                Graph& graph, std::vector<std::string>& diagnostics) {
    return Builder(root, request, graph, diagnostics).build();
}

} // namespace circuit::analysis::detail::timing
