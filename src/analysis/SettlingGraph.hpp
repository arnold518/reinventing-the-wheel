#pragma once

#include "analysis/SettlingBound.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class Component;
class PinBase;

// A circuit as the settling analysis sees it: one node per pin bit, each
// defined by the bits it is computed from. Built once from the component
// tree; the search in SettlingBound.cpp only reads it.
namespace circuit::analysis::detail::settling {

// "Never changes during operation". Smaller than every tick, so max() ignores
// it and min() prefers it.
inline constexpr long long STABLE = -1;
inline constexpr int8_t UNKNOWN = 2;

enum class Kind : uint8_t { Stable, Source, Buf, Not, And, Nand, Or, Nor, Xor, Hub };

struct Node {
    Kind kind = Kind::Stable;
    int8_t constant = UNKNOWN; // known final value, if any
    int a = -1, b = -1;
    long long delay = 0;       // Source: tick of the last transition, or STABLE
    int var = -1;              // Source: case-split variable
    int hub = -1;
    const PinBase* pin = nullptr;
    unsigned bit = 0;
};
// An opaque contract output: latest of its launch and its timed input arcs.
struct Hub {
    std::vector<std::pair<int, long long>> inputs;
    long long launch = STABLE;
};
// The bits whose latest change is reported together.
struct Group {
    std::string name;
    std::vector<std::pair<int, long long>> endpoints; // node, added setup
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Hub> hubs;
    std::vector<Group> groups;                                  // "(captures)" first, then root outputs
    std::vector<std::pair<const PinBase*, unsigned>> variables; // case variable -> source bit
    std::vector<int> variable_node;
    std::vector<int> order;                                     // every node after its inputs
    std::vector<uint8_t> binary;                                // final value is certainly 0 or 1
    // Bits a contract needs held at a value (for example RST = 0).
    std::vector<std::tuple<int, int8_t, std::string>> required;

    template<class Visit> void inputsOf(int n, Visit&& visit) const {
        const auto& node = nodes[n];
        if (node.kind == Kind::Hub) { for (const auto& [in, d] : hubs[node.hub].inputs) visit(in); return; }
        if (node.a >= 0) visit(node.a);
        if (node.b >= 0) visit(node.b);
    }
    std::string label(int n) const;
};

// False: the circuit is rejected, with the reasons in `diagnostics`.
bool buildGraph(const std::shared_ptr<Component>& root, const SettlingOptions& options,
                Graph& graph, std::vector<std::string>& diagnostics);

} // namespace circuit::analysis::detail::settling
