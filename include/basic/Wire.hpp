#pragma once
#include "basic/WireBase.hpp"
#include "basic/PinBase.hpp"
#include "ForwardDeclarations.hpp"
#include <array>
#include <memory>
#include <vector>

template<size_t WIDTH>
class Wire : public WireBase, public std::enable_shared_from_this<Wire<WIDTH>> {
private:
    std::array<LogicValue, WIDTH> value;

public:
    Wire(std::string name);

    void setSourcePin(std::shared_ptr<Pin<WIDTH>> pin);
    void addSinkPin(std::shared_ptr<Pin<WIDTH>> pin);

    std::shared_ptr<Pin<WIDTH>> getSourcePin() const;
    std::vector<std::weak_ptr<Pin<WIDTH>>> getSinkPins() const;
    std::vector<std::shared_ptr<Pin<WIDTH>>> getSinkPinsForPython() const;

    size_t getWidth() const override;
    LogicValue getBit(size_t index) const override;
    void setBit(size_t index, LogicValue bit_value) override;
    uint64_t getValue() const override;
    std::vector<LogicValue> getValueVector() const override;
    void setValue(uint64_t new_value) override;
    void setValue(LogicValue new_value);
    void setValueVector(const std::vector<LogicValue>& values) override;
    LogicValue getSingleValue() const override;
    void setSingleValue(LogicValue new_value) override;

    void propagateChange(Simulator& simulator, size_t propagation_time);
};

#include "basic/Wire.tpp"
