#pragma once

#include "modules/memory/MemoryProtocol.hpp"
#include <string>

namespace circuit::memory {

// Two requester ports (I_, D_) sharing one lower memory (MEM_). Both
// requesters run on one clock: the arbiter is clocked by I_CLK, and D_CLK
// must carry the same clock (it is not read).
template<size_t Width>
class TwoPortMemoryArbiter : public IOComponent {
public:
    // `block_words`: the words of a granted burst read (op 4), as the lower
    // memory returns them; the arbiter stays with that requester until the
    // last one.
    // `pipelined` (§ 6.3): an answer is passed on in the cycle its request is
    // accepted, and the next request is accepted at the edge the current one
    // completes. Off by default: the chapter 3–5 arbiter.
    explicit TwoPortMemoryArbiter(std::string name, size_t block_words = 2, bool pipelined = false);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;

private:
    void buildPipelined(ComponentBuilder&);
    size_t block_words_;
    bool pipelined_ = false;
    std::string type_;
};

// Contracts memory.arbiter.<8|16|32> (structural only); parameters
// `block_words` (default 2) and `pipelined` (0 or 1, default 0).
void registerArbiters(ComponentCatalog&);

} // namespace circuit::memory
