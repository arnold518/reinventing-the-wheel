#pragma once

#include "components/Component.hpp"
#include "basic/Pin.hpp"
#include "basic/PinBase.hpp"
#include "basic/LogicValue.hpp"
#include "ForwardDeclarations.hpp"
#include <map>
#include <string>
#include <functional>
#include <cstdint>

class Simulator;

class IOComponent : public Component
{
public:
    using PinInitFunction = std::function<void(IOComponent*)>;

protected:
    std::map<std::string, std::shared_ptr<Pin<>>> _inputPins;
    std::map<std::string, std::shared_ptr<Pin<>>> _outputPins;
    std::map<std::string, std::shared_ptr<PinBase>> _allInputPins;
    std::map<std::string, std::shared_ptr<PinBase>> _allOutputPins;
    PinInitFunction _pin_initializer;

    template<size_t WIDTH = 1>
    std::shared_ptr<Pin<WIDTH>> _addPin(const std::string& pin_name, PinType type, std::shared_ptr<IOComponent> self_ptr);

public:
    IOComponent(std::string name, PinInitFunction initializer = nullptr);
    virtual ~IOComponent() = default;
    static constexpr const char* TypeName = "IOComponent";
    const char* getTypeName() const override { return TypeName; }

    virtual void initPins(std::shared_ptr<IOComponent> self_ptr);

    template<size_t WIDTH = 1>
    std::shared_ptr<Pin<WIDTH>> addPin(const std::string& pin_name, PinType type);

    template<size_t WIDTH = 1>
    std::shared_ptr<Pin<WIDTH>> getInputPin(const std::string& pin_name) const;

    template<size_t WIDTH = 1>
    std::shared_ptr<Pin<WIDTH>> getOutputPin(const std::string& pin_name) const;

    std::shared_ptr<PinBase> getInputPinDynamic(const std::string& pin_name) const;
    std::shared_ptr<PinBase> getOutputPinDynamic(const std::string& pin_name) const;
    LogicValue getInputValue(const std::string& pin_name) const;
    uint64_t getInputValueAsUInt64(const std::string& pin_name) const;
    std::string formatPins(int lvl) const;
    
    const std::map<std::string, std::shared_ptr<Pin<>>>& getInputPins() const;
    const std::map<std::string, std::shared_ptr<Pin<>>>& getOutputPins() const;
    const std::map<std::string, std::shared_ptr<PinBase>>& getAllInputPins() const;
    const std::map<std::string, std::shared_ptr<PinBase>>& getAllOutputPins() const;
};

template<size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> IOComponent::_addPin(const std::string& pin_name, PinType type, std::shared_ptr<IOComponent> self_ptr) {
    auto pin = std::make_shared<Pin<WIDTH>>(pin_name, type, self_ptr);
    if (type == PinType::INPUT) {
        _allInputPins[pin_name] = pin;
        if constexpr (WIDTH == 1) {
            _inputPins[pin_name] = pin;
        }
    } else {
        _allOutputPins[pin_name] = pin;
        if constexpr (WIDTH == 1) {
            _outputPins[pin_name] = pin;
        }
    }
    return pin;
}

template<size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> IOComponent::addPin(const std::string& pin_name, PinType type) {
    auto self_ptr = std::static_pointer_cast<IOComponent>(shared_from_this());
    return this->_addPin<WIDTH>(pin_name, type, self_ptr);
}

template<size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> IOComponent::getInputPin(const std::string& pin_name) const {
    return std::dynamic_pointer_cast<Pin<WIDTH>>(getInputPinDynamic(pin_name));
}

template<size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> IOComponent::getOutputPin(const std::string& pin_name) const {
    return std::dynamic_pointer_cast<Pin<WIDTH>>(getOutputPinDynamic(pin_name));
}
