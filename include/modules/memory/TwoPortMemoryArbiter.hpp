#pragma once

#include "modules/memory/MemoryProtocol.hpp"
#include <string>

namespace circuit::memory {

template<size_t Width>
class TwoPortMemoryArbiter : public IOComponent {
public:
    explicit TwoPortMemoryArbiter(std::string name);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;

private:
    std::string type_;
};

} // namespace circuit::memory
