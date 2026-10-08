#pragma once

#include "analysis/CircuitTimingAnalyzer.hpp"
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

class Component;
class PinBase;
class BehavioralMemoryEndpoint;

// A circuit as the timing analysis sees it: one node per pin bit, each
// defined by the bits it is computed from. Loops are allowed. Built once from
// the component tree; the engine only reads it.
namespace circuit::analysis::detail::timing {

using Tick = long long;
// "Has always held / never changes during operation". Smaller than every
// tick, so max() ignores it and min() prefers it.
inline constexpr Tick NEVER = -1;
// "No bound established."
inline constexpr Tick OPEN = std::numeric_limits<Tick>::max();
// Sets of the four logic values, as bits.
inline constexpr uint8_t LOW = 1, HIGH = 2, X = 4, Z = 8, BINARY = 3, ANY = 15;

enum class Kind : uint8_t { Stable, Source, Buf, Not, And, Nand, Or, Nor, Xor, Hub, Part };

struct Node {
    Kind kind = Kind::Stable;
    int8_t constant = 2;   // Stable/Source: final value 0 or 1 when known, else 2
    int a = -1, b = -1;
    Tick delay = 0;        // Source: tick of the last transition, or NEVER
    int var = -1;          // Source: case variable (final value is 0 or 1)
    int owner = -1;        // Hub: hub index. Part: part index.
    uint8_t output = 0;    // Part: which output (PartOutput)
    const PinBase* pin = nullptr;
    unsigned bit = 0;
};
// An opaque contract output: latest of its launch and its timed input arcs.
struct Hub {
    std::vector<std::pair<int, Tick>> inputs;
    Tick launch = NEVER;
    // Binary whenever every arc input is (a register file read, under a binary start).
    bool binary_with_inputs = false;
};
// The compact FF memory endpoint. Its outputs depend on whether it is empty
// or holding a response, so the engine evaluates it by its own rule.
enum PartOutput : uint8_t { Ready, Valid, Data, Status };
struct Part {
    const BehavioralMemoryEndpoint* endpoint = nullptr;
    const PinBase* outputs[4] = {};
    Tick delay = 1;
    int req_valid = -1, rsp_ready = -1, op = -1, address = -1, mask = -1; // first bit of each input
    unsigned address_bits = 0, mask_bits = 0;
    size_t bytes_per_word = 0, capacity_bytes = 0;
};
// The bits whose latest change is reported together.
struct Group {
    std::string name;
    std::vector<std::pair<int, Tick>> endpoints; // node, added setup
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Hub> hubs;
    std::vector<Part> parts;
    std::vector<Group> groups;                                  // "(captures)" first, then root outputs
    std::vector<std::pair<const PinBase*, unsigned>> variables; // case variable -> source bit
    std::vector<int> variable_node;
    std::vector<int> order;               // inputs first where there is no loop
    std::vector<std::vector<int>> readers;
    bool has_loop = false;
    std::unordered_map<const PinBase*, int> first_bit;          // pin -> its first node
    // Bits a contract needs held at a value (for example RST = 0).
    std::vector<std::tuple<int, int8_t, std::string>> required;

    template<class Visit> void inputsOf(int n, Visit&& visit) const {
        const auto& node = nodes[n];
        if (node.kind == Kind::Hub) { for (const auto& [in, d] : hubs[node.owner].inputs) visit(in); return; }
        if (node.kind == Kind::Part) {
            const auto& part = parts[node.owner];
            const auto bits = [&](int first, unsigned count) { for (unsigned i = 0; i < count; ++i) visit(first + int(i)); };
            if (node.output == Ready) visit(part.rsp_ready);
            else if (node.output == Valid) visit(part.req_valid);
            else if (node.output == Data) { visit(part.req_valid); bits(part.op, 3); bits(part.address, part.address_bits); }
            else { bits(part.op, 3); bits(part.address, part.address_bits); bits(part.mask, part.mask_bits); }
            return;
        }
        if (node.a >= 0) visit(node.a);
        if (node.b >= 0) visit(node.b);
    }
    std::string label(int n) const;
};

// How the sources of one analysis behave. Pins are root inputs or pins that
// nothing drives.
struct GraphRequest {
    TimingContracts contracts;
    bool binary_sources = false;   // changing sources end at 0 or 1 and become case variables
    bool launches = true;          // clocked outputs launch; false: state holds
    ClockEdge phase = ClockEdge::Rising; // launches and captures on the other edge are held and skipped
    bool other_edge_is_error = true;
    bool check_clocks = true;      // one clock, straight from a root input
    Tick default_arrival = 0;      // root inputs not named below; NEVER: they hold
    std::unordered_map<const PinBase*, uint64_t> fixed;        // never changes, known value
    std::unordered_map<const PinBase*, uint64_t> final_value;  // changes, ends at this value
    std::unordered_map<const PinBase*, Tick> arrival;          // tick of the last transition
};

// False: the circuit is rejected, with the reasons in `diagnostics`.
bool buildGraph(const std::shared_ptr<Component>& root, const GraphRequest& request,
                Graph& graph, std::vector<std::string>& diagnostics);

} // namespace circuit::analysis::detail::timing
