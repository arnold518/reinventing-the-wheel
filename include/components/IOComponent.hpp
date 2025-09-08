#pragma once

#include "components/Component.hpp"
#include "basic/Pin.hpp"
#include "basic/LogicValue.hpp"
#include "ForwardDeclarations.hpp"
#include <map>
#include <string>
#include <functional>

class Simulator;

class IOComponent : public Component
{
public:
    using PinInitFunction = std::function<void(IOComponent*)>;

protected:
    std::map<std::string, std::shared_ptr<Pin>> _inputPins;
    std::map<std::string, std::shared_ptr<Pin>> _outputPins;
    PinInitFunction _pin_initializer;

    std::shared_ptr<Pin> _addPin(const std::string& pin_name, PinType type, std::shared_ptr<IOComponent> self_ptr);

public:
    IOComponent(std::string name, PinInitFunction initializer = nullptr);
    virtual ~IOComponent() = default;
    static constexpr const char* TypeName = "IOComponent";
    const char* getTypeName() const override { return TypeName; }

    virtual void initPins(std::shared_ptr<IOComponent> self_ptr);

    std::shared_ptr<Pin> addPin(const std::string& pin_name, PinType type);

    std::shared_ptr<Pin> getInputPin(const std::string& pin_name) const;
    std::shared_ptr<Pin> getOutputPin(const std::string& pin_name) const;
    LogicValue getInputValue(const std::string& pin_name) const;
    std::string formatPins(int lvl) const;
    
    const std::map<std::string, std::shared_ptr<Pin>>& getInputPins() const;
    const std::map<std::string, std::shared_ptr<Pin>>& getOutputPins() const;
};