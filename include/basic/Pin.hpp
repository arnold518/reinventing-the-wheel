#pragma once
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "ForwardDeclarations.hpp"
#include <array>
#include <memory>

template<size_t WIDTH>
class Pin : public PinBase, public std::enable_shared_from_this<Pin<WIDTH>> {
private:
    std::array<LogicValue, WIDTH> value;

public:
    Pin(std::string name, PinType type, std::shared_ptr<Component> owner_comp);

    std::shared_ptr<Wire<WIDTH>> getExternalWire() const;
    std::shared_ptr<Wire<WIDTH>> getInternalWire() const;
    void connectExternal(const std::shared_ptr<Wire<WIDTH>>& wire);
    void connectInternal(const std::shared_ptr<Wire<WIDTH>>& wire);

    size_t getWidth() const override;
    LogicValue getBit(size_t index) const override;
    void setBit(size_t index, LogicValue bit_value) override;
    uint64_t getValueAsUInt64() const override;
    std::vector<LogicValue> getValueAsVector() const override;
    void setValueFromUInt64(uint64_t new_value) override;
    void setValueFromVector(const std::vector<LogicValue>& values) override;
};

#include "basic/Pin.tpp"
