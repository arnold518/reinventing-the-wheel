#include "GateListModel.hpp"
#include "TimingGraph.hpp"
#include "components/IOComponent.hpp"
#include "basic/PinBase.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "modules/storage/ClockedStateBitBehavioral.hpp"
#include "modules/storage/RegisterBehavioral.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <typeinfo>
#include <unordered_map>

namespace circuit::analysis {
using namespace detail::timing;
namespace {
constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH, UNK = LogicValue::UNKNOWN;
bool binary(LogicValue v) { return v == L || v == H; }
LogicValue invert(LogicValue v) { return v == H ? L : v == L ? H : UNK; }
} // namespace

struct GateListProgram {
    enum class Op : uint8_t { Constant, Input, Stored, Buf, Not, And, Nand, Or, Nor, Xor, Leaf };
    struct Node { Op op; int a = -1, b = -1; LogicValue constant = UNK; };
    // A compact state bit (no enable) or register: its data, enable and
    // reset bits and the bits it holds (the nodes of its output).
    struct Store { int enable = -1, reset = -1; std::vector<int> d, q; };
    // A primitive that describes itself: every input pin, and each output
    // bit's node.
    struct Leaf {
        std::shared_ptr<const memory_device::Primitive> part;
        std::vector<std::pair<std::string, std::vector<int>>> inputs;
    };
    std::vector<Node> nodes;
    std::vector<int> rank, order;
    std::vector<std::vector<int>> readers;
    std::vector<std::pair<std::string, size_t>> inputs, outputs;
    std::vector<std::vector<int>> input_nodes, output_nodes;
    std::vector<uint8_t> clock_output;
    int clock_input = -1;
    std::vector<Store> stores;
    std::vector<Leaf> leaves;
    // Leaf output nodes: the leaf, the output pin and the bit.
    struct LeafBit { int leaf; std::string pin; unsigned bit; };
    std::unordered_map<int, LeafBit> leaf_bits;
};

std::shared_ptr<const GateListProgram> GateListModel::compile(const std::shared_ptr<Component>& root, std::string* why) {
    using Program = GateListProgram;
    using Op = Program::Op;
    const auto no = [&](const std::string& reason) -> std::shared_ptr<const GateListProgram> {
        if (why) *why = reason;
        return nullptr;
    };
    const auto part = std::dynamic_pointer_cast<IOComponent>(root);
    if (!part) return no("not a part with pins");
    GraphRequest request;
    Graph graph;
    std::vector<std::string> diagnostics;
    if (!buildGraph(root, request, graph, diagnostics)) return no(diagnostics.empty() ? "no timing graph" : diagnostics.front());
    if (graph.has_loop) return no("a loop");
    if (!graph.parts.empty()) return no("a memory endpoint inside");
    if (graph.self_timed) return no("a part that times itself");
    if (graph.clock_inputs.size() > 1) return no("more than one clock");

    auto program = std::make_shared<Program>();
    program->nodes.resize(graph.nodes.size());
    std::unordered_map<const Component*, size_t> store_of, leaf_of;
    const auto first = [&](const std::shared_ptr<PinBase>& pin) -> int {
        const auto found = graph.first_bit.find(pin.get());
        return found == graph.first_bit.end() ? -1 : found->second;
    };
    const auto bits = [&](const std::shared_ptr<PinBase>& pin) {
        std::vector<int> nodes(pin ? pin->getWidth() : 0, -1);
        if (const int at = pin ? first(pin) : -1; at >= 0)
            for (size_t bit = 0; bit < nodes.size(); ++bit) nodes[bit] = at + int(bit);
        return nodes;
    };
    for (size_t index = 0; index < graph.nodes.size(); ++index) {
        const auto& node = graph.nodes[index];
        auto& out = program->nodes[index];
        out.a = node.a;
        out.b = node.b;
        switch (node.kind) {
        case Kind::Stable:
            out.op = Op::Constant;
            out.constant = node.constant == 0 ? L : node.constant == 1 ? H : UNK;
            break;
        case Kind::Buf: out.op = Op::Buf; break;
        case Kind::Not: out.op = Op::Not; break;
        case Kind::And: out.op = Op::And; break;
        case Kind::Nand: out.op = Op::Nand; break;
        case Kind::Or: out.op = Op::Or; break;
        case Kind::Nor: out.op = Op::Nor; break;
        case Kind::Xor: out.op = Op::Xor; break;
        case Kind::Source: {
            const auto owner = node.pin ? node.pin->getOwner() : nullptr;
            if (owner.get() == root.get()) { out.op = Op::Input; break; }
            // Not a root input: the output of a state part.
            const auto io = std::dynamic_pointer_cast<IOComponent>(owner);
            const bool state_bit = owner && typeid(*owner) == typeid(ClockedStateBitBehavioral);
            const bool word = owner && typeid(*owner) == typeid(RegisterBehavioral);
            if (!io || (!state_bit && !word)) return no("a source that is neither an input nor compact state: " + (node.pin ? node.pin->getID() : std::string("?")));
            out.op = Op::Stored;
            if (store_of.emplace(owner.get(), program->stores.size()).second) {
                Program::Store store;
                store.d = bits(io->getInputPinDynamic("D"));
                store.q = bits(io->getOutputPinDynamic("Q"));
                if (const auto reset = bits(io->getInputPinDynamic("RST")); !reset.empty()) store.reset = reset[0];
                if (word) if (const auto enable = bits(io->getInputPinDynamic("WE")); !enable.empty()) store.enable = enable[0];
                for (const int q : store.q) if (q < 0) return no("a state output outside the graph: " + io->getID());
                program->stores.push_back(std::move(store));
            }
            break;
        }
        case Kind::Hub: {
            const auto owner = node.pin ? std::dynamic_pointer_cast<const memory_device::Primitive>(node.pin->getOwner()) : nullptr;
            if (!owner) return no("a leaf that is not a gate, a state part or a self-describing primitive: " + (node.pin ? node.pin->getID() : std::string("?")));
            out.op = Op::Leaf;
            auto [entry, added] = leaf_of.emplace(owner.get(), program->leaves.size());
            if (added) {
                Program::Leaf leaf;
                leaf.part = owner;
                memory_device::Primitive::Values values;
                for (const auto& [name, pin] : owner->getAllInputPins()) {
                    leaf.inputs.emplace_back(name, bits(pin));
                    values[name] = memory_device::Bits(pin->getWidth(), L);
                }
                memory_device::Primitive::Values result;
                memory_device::Primitive::Deciders deciders;
                if (!owner->settled(values, result, deciders)) return no("a primitive that does not describe itself: " + owner->getID());
                program->leaves.push_back(std::move(leaf));
            }
            program->leaf_bits[int(index)] = {int(entry->second), node.pin->getName(), node.bit};
            break;
        }
        default:
            return no("an unsupported node");
        }
    }
    for (const auto& [name, pin] : part->getAllInputPins()) {
        program->inputs.emplace_back(name, pin->getWidth());
        const bool clock = std::find(graph.clock_inputs.begin(), graph.clock_inputs.end(), name) != graph.clock_inputs.end();
        if (clock) {
            if (pin->getWidth() != 1) return no("a clock wider than one bit");
            program->clock_input = int(program->inputs.size()) - 1;
        }
        program->input_nodes.push_back(clock ? std::vector<int>(pin->getWidth(), -1) : bits(pin));
    }
    for (const auto& [name, pin] : part->getAllOutputPins()) {
        program->outputs.emplace_back(name, pin->getWidth());
        const bool clock = std::find(graph.clock_outputs.begin(), graph.clock_outputs.end(), name) != graph.clock_outputs.end();
        program->clock_output.push_back(clock);
        program->output_nodes.push_back(bits(pin));
        if (!clock) for (const int node : program->output_nodes.back()) if (node < 0) return no("an output outside the graph: " + name);
    }
    if (!program->stores.empty() && program->clock_input < 0) return no("state without a clock");
    program->order = graph.order;
    program->rank.assign(graph.nodes.size(), 0);
    for (size_t position = 0; position < graph.order.size(); ++position) program->rank[graph.order[position]] = int(position);
    program->readers = graph.readers;
    return program;
}

GateListModel::GateListModel(std::shared_ptr<const GateListProgram> program) : program_(std::move(program)) {
    const auto& p = *program_;
    value_.assign(p.nodes.size(), UNK);
    queued_.assign(p.nodes.size(), 0);
    leaf_pass_.assign(p.leaves.size(), 0);
    leaf_out_.resize(p.leaves.size());
    // Inputs and state start unknown, as the compact state parts do; every
    // gate is computed once from there.
    for (const int node : p.order) {
        const auto op = p.nodes[node].op;
        value_[node] = op == GateListProgram::Op::Constant ? p.nodes[node].constant
                     : op == GateListProgram::Op::Input || op == GateListProgram::Op::Stored ? UNK : compute(node);
    }
}

const std::vector<std::pair<std::string, size_t>>& GateListModel::inputs() const { return program_->inputs; }
const std::vector<std::pair<std::string, size_t>>& GateListModel::outputs() const { return program_->outputs; }
bool GateListModel::passesClock(size_t output) const { return program_->clock_output[output]; }
size_t GateListModel::nodeCount() const { return program_->nodes.size(); }

std::vector<LogicValue> GateListModel::output(size_t pin) const {
    const auto& nodes = program_->output_nodes[pin];
    std::vector<LogicValue> out(nodes.size(), UNK);
    for (size_t bit = 0; bit < nodes.size(); ++bit) if (nodes[bit] >= 0) out[bit] = value_[nodes[bit]];
    return out;
}

LogicValue GateListModel::compute(int node) {
    using Op = GateListProgram::Op;
    const auto& n = program_->nodes[node];
    const auto a = n.a >= 0 ? value_[n.a] : UNK, b = n.b >= 0 ? value_[n.b] : UNK;
    switch (n.op) {
    case Op::Constant: return n.constant;
    case Op::Input: case Op::Stored: return value_[node];
    case Op::Buf: return a;
    case Op::Not: return invert(a);
    case Op::And: case Op::Nand: {
        const auto out = (a == L || b == L) ? L : (a == H && b == H) ? H : UNK;
        return n.op == Op::And ? out : invert(out);
    }
    case Op::Or: case Op::Nor: {
        const auto out = (a == H || b == H) ? H : (a == L && b == L) ? L : UNK;
        return n.op == Op::Or ? out : invert(out);
    }
    case Op::Xor: return binary(a) && binary(b) ? (a == b ? L : H) : UNK;
    case Op::Leaf: {
        const auto& where = program_->leaf_bits.at(node);
        auto& results = leaf_out_[where.leaf];
        if (leaf_pass_[where.leaf] != pass_) {
            // Every input of a leaf comes before each of its outputs in the
            // graph's order, so they are final when the first output is.
            const auto& leaf = program_->leaves[where.leaf];
            memory_device::Primitive::Values in, out;
            memory_device::Primitive::Deciders deciders;
            for (const auto& [name, nodes] : leaf.inputs) {
                auto& bits = in[name];
                bits.resize(nodes.size(), UNK);
                for (size_t bit = 0; bit < nodes.size(); ++bit) if (nodes[bit] >= 0) bits[bit] = value_[nodes[bit]];
            }
            leaf.part->settled(in, out, deciders);
            results.assign(out.begin(), out.end());
            leaf_pass_[where.leaf] = pass_;
        }
        for (const auto& [name, bits] : results)
            if (name == where.pin) return where.bit < bits.size() ? bits[where.bit] : UNK;
        return UNK;
    }
    }
    return UNK;
}

void GateListModel::mark(int node) {
    for (const int reader : program_->readers[node]) {
        if (queued_[reader]) continue;
        queued_[reader] = 1;
        heap_.push_back(program_->rank[reader]);
        std::push_heap(heap_.begin(), heap_.end(), std::greater<int>());
    }
}

void GateListModel::set(int node, LogicValue value) {
    if (value_[node] == value) return;
    value_[node] = value;
    mark(node);
}

void GateListModel::propagate() {
    ++pass_;
    while (!heap_.empty()) {
        std::pop_heap(heap_.begin(), heap_.end(), std::greater<int>());
        const int node = program_->order[heap_.back()];
        heap_.pop_back();
        queued_[node] = 0;
        const auto value = compute(node);
        if (value != value_[node]) { value_[node] = value; mark(node); }
    }
}

void GateListModel::step(const std::vector<std::vector<LogicValue>>& inputs) {
    const auto& p = *program_;
    const auto clock = p.clock_input >= 0 ? inputs[p.clock_input][0] : UNK;
    const bool edge = previous_clock_ == L && clock == H;
    previous_clock_ = clock;
    // The state parts' rules, on the values that stood before these inputs:
    // a reset clears at once (an unknown one leaves unknown what is not 0),
    // a rising edge captures the data (with an enable, where it is 1).
    const auto apply_resets = [&] {
        bool changed = false;
        for (const auto& store : p.stores) {
            const auto reset = store.reset >= 0 ? value_[store.reset] : L;
            if (reset == L) continue;
            for (const int q : store.q) {
                const auto next = reset == H || value_[q] == L ? L : UNK;
                if (next != value_[q]) { set(q, next); changed = true; }
            }
        }
        return changed;
    };
    if (edge) {
        std::vector<std::pair<int, LogicValue>> captured;
        for (const auto& store : p.stores) {
            if (store.reset >= 0 && value_[store.reset] != L) continue;  // handled as a reset
            const auto enable = store.enable >= 0 ? value_[store.enable] : H;
            if (enable == L) continue;
            for (size_t bit = 0; bit < store.q.size(); ++bit) {
                const auto data = store.d[bit] >= 0 ? value_[store.d[bit]] : UNK;
                const auto next = enable == H ? (binary(data) ? data : UNK)
                                              : (value_[store.q[bit]] == L && data == L ? L : UNK);
                captured.emplace_back(store.q[bit], next);
            }
        }
        for (const auto& [q, next] : captured) set(q, next);
    }
    apply_resets();
    for (size_t pin = 0; pin < inputs.size(); ++pin) {
        const auto& nodes = p.input_nodes[pin];
        for (size_t bit = 0; bit < nodes.size() && bit < inputs[pin].size(); ++bit)
            if (nodes[bit] >= 0) set(nodes[bit], inputs[pin][bit]);
    }
    propagate();
    // A reset that rises with these inputs clears its state now.
    for (size_t round = 0; round < 8 && apply_resets(); ++round) propagate();
}

} // namespace circuit::analysis
