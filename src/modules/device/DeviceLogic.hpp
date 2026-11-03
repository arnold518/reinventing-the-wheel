#pragma once
#include "components/StructuralLogic.hpp"
#include "modules/device/DeviceBus.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "components/selection/BuildContext.hpp"
#include <charconv>
#include <bit>
#include <set>
namespace circuit::device::detail {
using circuit::structural::P;using circuit::structural::V;
// Device recipes use the same minimal adapters as every other recipe.
struct Logic : circuit::structural::Logic {
    explicit Logic(ComponentBuilder& builder) : circuit::structural::Logic(builder) {}
};
inline uint32_t parameter(const circuit::ParameterMap& parameters, const std::string& key, uint32_t fallback) {
    auto found = parameters.find(key);
    if (found == parameters.end()) return fallback;
    uint32_t value = 0;
    const auto& text = found->second;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("Invalid device parameter: " + key);
    return value;
}
inline uint32_t parameter(const std::shared_ptr<circuit::BuildContext>& context, const std::string& key, uint32_t fallback) {
    return context ? parameter(context->parameters(), key, fallback) : fallback;
}
inline bool supportsParameters(const circuit::ParameterMap& parameters, const std::set<std::string>& keys) {
    try {
        for (const auto& [key, value] : parameters) {
            if (!keys.contains(key)) return false;
            (void)parameter(parameters, key, 0);
        }
        return true;
    } catch (const std::exception&) { return false; }
}
inline bool supportsCounter(const circuit::ParameterMap& p) {
    return supportsParameters(p, {"divisor"}) && parameter(p, "divisor", 1) != 0;
}
inline bool supportsTimer(const circuit::ParameterMap& p) {
    return supportsParameters(p, {"divisor", "frequency_hz"}) && parameter(p, "divisor", 1) != 0;
}
inline bool supportsFifo(const circuit::ParameterMap& p) {
    return supportsParameters(p, {"depth"}) && validFifoDepth(parameter(p,"depth",4));
}
inline bool supportsQueuedDevice(const circuit::ParameterMap& p) {
    return supportsParameters(p, {"fifo_depth"}) && validFifoDepth(parameter(p,"fifo_depth",4));
}
inline bool supportsDisplay(const circuit::ParameterMap& p) {
    if (!supportsParameters(p, {"width", "height", "storage_bytes"})) return false;
    try {
        FramebufferConfig{parameter(p,"width",4),parameter(p,"height",4),parameter(p,"storage_bytes",0)}.validate();
        return true;
    } catch (const std::exception&) { return false; }
}
inline V pad(Logic&l,V v,size_t width=32){while(v.size()<width)v.push_back(l.bit(false));if(v.size()!=width)throw std::invalid_argument("Invalid padding");return v;}
inline std::shared_ptr<IOComponent> devicePart(IOComponent& root,ComponentBuilder& b,
    const circuit::ComponentFamily& family,const std::string& name,circuit::ParameterMap parameters={}){
    if(!b.getBuildContext())return b.add(family,name,std::move(parameters));
    auto request=family.request(name,std::move(parameters));
    if(root.getSelectedFidelity()=="behavioral")request.required_fidelity=circuit::Fidelity::Behavioral;
    return b.addByContract(std::move(request));
}
P knownWord(Logic&,ComponentBuilder&,P);
inline P at(Logic&l,const std::shared_ptr<IOComponent>& port,uint32_t offset) {
    return l.equal({l.split(port->getOutputPin<32>("ADDRESS")),l.constant(32,offset)});
}
inline std::shared_ptr<IOComponent> devicePort(IOComponent&root,ComponentBuilder&b,Logic&l) {
    auto p=devicePart(root,b,circuit::device::busFamily(circuit::device::BusUnit::RegisterPort,32),"PORT");
    for(auto n:{"CLK","RST","REQ_VALID","REQ_OP","REQ_ADDR","REQ_WRITE_DATA","REQ_WRITE_MASK","RSP_READY"})
        l.wire(root.getInputPinDynamic(n),p->getInputPinDynamic(n));
    for(auto n:{"REQ_READY","RSP_VALID","RSP_READ_DATA","RSP_STATUS"})
        l.wire(p->getOutputPinDynamic(n),root.getOutputPinDynamic(n));
    return p;
}
inline P validCommand(Logic&l,ComponentBuilder&b,const std::shared_ptr<IOComponent>&p) {
    auto data=l.split(p->getOutputPin<32>("WRITE_DATA"));
    auto reserved=l.any(V(data.begin()+1,data.end()));
    return l.land({knownWord(l,b,p->getOutputPin<32>("WRITE_DATA")),l.inv(reserved)});
}
}
