#pragma once

#include "modules/memory/MemoryProtocol.hpp"
#include <string>

namespace circuit::memory {

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

} // namespace circuit::memory
