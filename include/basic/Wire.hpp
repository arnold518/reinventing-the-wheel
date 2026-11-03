#pragma once

#include "simulator/SimulationTime.hpp"
#include "basic/WireBase.hpp"
#include "basic/PinBase.hpp"
#include "ForwardDeclarations.hpp"
#include <memory>
#include <vector>

// A typed handle on a WireBase of WIDTH bits. It holds no state of its own.
template<size_t WIDTH>
class Wire : public WireBase, public std::enable_shared_from_this<Wire<WIDTH>> {
    static_assert(WIDTH >= 1 && WIDTH <= PackedLogic::kMaxWidth, "Wire width must be 1 to 64 bits");

public:
    explicit Wire(std::string name) : WireBase(std::move(name), WIDTH) {}

    void setSourcePin(std::shared_ptr<Pin<WIDTH>> pin) { setSourcePinBase(std::move(pin)); }
    void addSinkPin(std::shared_ptr<Pin<WIDTH>> pin) { addSinkPinBase(std::move(pin)); }

    std::shared_ptr<Pin<WIDTH>> getSourcePin() const;
    std::vector<std::weak_ptr<Pin<WIDTH>>> getSinkPins() const;
    std::vector<std::shared_ptr<Pin<WIDTH>>> getSinkPinsForPython() const;

    using WireBase::setValue;
    void setValue(LogicValue new_value) { setSingleValue(new_value); }
};

#include "basic/Wire.tpp"
