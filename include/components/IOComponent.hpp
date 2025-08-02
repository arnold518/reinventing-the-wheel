#pragma once
#include "components/Component.hpp"
#include "basic/Pin.hpp"
#include "basic/LogicValue.hpp"
#include "ForwardDeclarations.hpp"
#include <map>
#include <string>
#include <typeinfo>
#include <iostream>

class Simulator;

class IOComponent : public Component
{
protected:
    std::map<std::string, std::shared_ptr<Pin>> _inputPins;
    std::map<std::string, std::shared_ptr<Pin>> _outputPins;
    size_t delay;

    std::shared_ptr<Pin> _addPin(const std::string& pin_name, PinType type, std::shared_ptr<IOComponent> self_ptr);

public:
    IOComponent(std::string name, size_t delay_val = 1);
    virtual ~IOComponent() = default;
    static constexpr const char* TypeName = "IOComponent";    
    
    template<typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args) {
        std::shared_ptr<T> obj = std::make_shared<T>(std::forward<Args>(args)...);
        
        if (auto io_obj = std::dynamic_pointer_cast<IOComponent>(obj)) {
            io_obj->initPins(io_obj);
        } else {
            std::cerr << "Error: Component '" << obj->getName() << "' (type: " << typeid(T).name() << ") created via IOComponent::create but not castable to IOComponent for pin initialization." << std::endl;
        }
        return obj;
    }

    virtual void initPins(std::shared_ptr<IOComponent> self_ptr) = 0;

    size_t getDelay() const { return delay; }
    std::shared_ptr<Pin> getInputPin(const std::string& pin_name) const;
    std::shared_ptr<Pin> getOutputPin(const std::string& pin_name) const;
    LogicValue getInputValue(const std::string& pin_name) const;
    std::string formatPins(int lvl) const;
    
    const std::map<std::string, std::shared_ptr<Pin>>& getInputPins() const { return _inputPins; }
    const std::map<std::string, std::shared_ptr<Pin>>& getOutputPins() const { return _outputPins; }

protected:
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name, LogicValue new_value, size_t current_sim_time);
};