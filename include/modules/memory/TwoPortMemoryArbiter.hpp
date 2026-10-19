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
    explicit TwoPortMemoryArbiter(std::string name, size_t block_words = 2);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;

private:
    size_t block_words_;
    std::string type_;
};

// Contracts memory.arbiter.<8|16|32> (structural only); parameter
// `block_words` (default 2).
void registerArbiters(ComponentCatalog&);

} // namespace circuit::memory
