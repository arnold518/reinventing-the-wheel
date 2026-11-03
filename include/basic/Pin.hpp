#pragma once
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "ForwardDeclarations.hpp"
#include <memory>

// A typed handle on a PinBase of WIDTH bits. It holds no state of its own;
// the type lets typed lookups (getInputPin<W>) check the width once.
template<size_t WIDTH>
class Pin : public PinBase, public std::enable_shared_from_this<Pin<WIDTH>> {
    static_assert(WIDTH >= 1 && WIDTH <= PackedLogic::kMaxWidth, "Pin width must be 1 to 64 bits");

public:
    Pin(std::string name, PinType type, std::shared_ptr<Component> owner_comp)
        : PinBase(std::move(name), type, std::move(owner_comp), WIDTH) {}

    std::shared_ptr<Wire<WIDTH>> getExternalWire() const;
    std::shared_ptr<Wire<WIDTH>> getInternalWire() const;
    void connectExternal(const std::shared_ptr<Wire<WIDTH>>& wire) { connectExternalBase(wire); }
    void connectInternal(const std::shared_ptr<Wire<WIDTH>>& wire) { connectInternalBase(wire); }
};

#include "basic/Pin.tpp"
