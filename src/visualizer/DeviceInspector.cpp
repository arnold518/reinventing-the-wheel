#include "visualizer/DeviceInspector.hpp"
#include "components/IOComponent.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "modules/device/DeviceComponents.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace circuit::visualizer {
namespace {
std::shared_ptr<IOComponent> child(const std::shared_ptr<Component>& root,const std::string& name) {
    for(const auto& c:root->getChildren())if(c->getName()==name)return std::dynamic_pointer_cast<IOComponent>(c);
    throw std::logic_error("Missing observed device child: "+name);
}
std::vector<LogicValue> signal(const std::shared_ptr<PinBase>& pin,const Simulator& sim,SimulationTime time) {
    if(!pin)throw std::logic_error("Missing observed device pin");
    const auto& history=sim.signalHistory(pin);
    const auto end=std::upper_bound(history.begin(),history.end(),time,
        [](SimulationTime t,const auto& item){return t<item.first;});
    return end==history.begin()?std::vector<LogicValue>(pin->getWidth(),LogicValue::UNKNOWN):std::prev(end)->second;
}
std::string quotedBits(const std::vector<LogicValue>& bits) {
    std::string result="\"";
    for(auto it=bits.rbegin();it!=bits.rend();++it)
        result+=*it==LogicValue::LOW?'0':*it==LogicValue::HIGH?'1':*it==LogicValue::HIGH_Z?'Z':'X';
    return result+'"';
}
std::string hex(const std::vector<uint8_t>& bytes){
    constexpr char digits[]="0123456789abcdef";std::string result;result.reserve(2*bytes.size());
    for(auto b:bytes){result+=digits[b>>4];result+=digits[b&15];}return result;
}
void appendPixels(std::vector<uint8_t>& pixels,std::vector<uint8_t>& known,const std::vector<LogicValue>& bits,size_t limit) {
    for(size_t i=0;i<bits.size()&&pixels.size()<limit;i+=8){
        uint8_t value=0,mask=0;
        for(size_t b=0;b<8;++b){auto v=bits.at(i+b);if(v==LogicValue::HIGH)value|=1u<<b;if(v==LogicValue::HIGH||v==LogicValue::LOW)mask|=1u<<b;}
        pixels.push_back(value);known.push_back(mask);
    }
}
std::shared_ptr<IOComponent> wordBank(const std::shared_ptr<Component>& node) {
    auto io=std::dynamic_pointer_cast<IOComponent>(node);
    if(io && io->getOutputPinDynamic("WORD_0"))return io;
    for(const auto& c:node->getChildren())if(auto bank=wordBank(c))return bank;
    return {};
}
}
std::string componentLayoutVariant(const std::shared_ptr<Component>& component) {
    if(auto view=std::dynamic_pointer_cast<ComponentLayoutView>(component))return view->componentLayoutVariant();
    return {};
}
std::string deviceViewKind(const std::shared_ptr<Component>& component) {
    if(auto view=std::dynamic_pointer_cast<DeviceView>(component))return view->deviceViewKind();
    // Compact gate-list parts and old recordings retain their public type.
    const std::string type=component->getTypeName();
    if(type=="FramebufferDisplay")return "display";
    if(type=="ButtonDevice")return "keyboard";
    if(type=="ConsoleDevice")return "console";
    if(type=="TimerDevice")return "timer";
    if(type=="SynchronousFifo<8>"||type=="SynchronousFifo<16>"||type=="SynchronousFifo<32>")return "queue";
    return {};
}
bool supportsDeviceView(const std::shared_ptr<Component>& component) {return !deviceViewKind(component).empty();}

std::string deviceStateJson(const std::shared_ptr<Component>& component,const Simulator& sim,size_t index,SimulationTime time,bool current) {
    if(!supportsDeviceView(component))throw std::invalid_argument("Unsupported device view");
    if(current && (time<sim.getCurrentTime() || (sim.nextEventTime() && *sim.nextEventTime()<=time)))
        throw std::invalid_argument("Current device observation requires a settled boundary");
    const auto io=std::dynamic_pointer_cast<IOComponent>(component);
    std::ostringstream out;out<<"{\"index\":"<<index<<",\"time\":";
    if(time>MAX_EXACT_JSON_TIME)out<<'"'<<time<<'"';else out<<time;
    // Instance names may be supplied by callers.
    const auto quote=[](const std::string& value){
        constexpr char digits[]="0123456789abcdef";std::string result="\"";
        for(unsigned char c:value){if(c=='"'||c=='\\'){result+='\\';result+=c;}else if(c<32){result+="\\u00";result+=digits[c>>4];result+=digits[c&15];}else result+=c;}
        return result+'"';
    };
    out<<",\"componentId\":"<<quote(component->getID())<<",\"type\":"<<quote(component->getTypeName());
    if(!current && !sim.isHistoryRecordingEnabled()){out<<",\"supported\":false,\"reason\":\"Device history is disabled\"}";return out.str();}
    const auto observed=[&](const std::shared_ptr<PinBase>& pin){
        if(!pin)throw std::logic_error("Missing observed device pin");
        return current?pin->getValueAsVector():signal(pin,sim,time);
    };
    const auto field=[&](const char* name,const std::shared_ptr<IOComponent>& part,const char* pin){
        out<<",\""<<name<<"\":"<<quotedBits(observed(part->getOutputPinDynamic(pin)));
    };
    if(deviceViewKind(component)=="display") {
        auto metadata=std::dynamic_pointer_cast<DeviceView>(component);
        const auto shape=metadata?metadata->deviceSurfaceShape():std::nullopt;
        if(!shape){out<<",\"supported\":false,\"reason\":\"Display geometry unavailable\"}";return out.str();}
        const auto geometry=*shape;
        if(!geometry.width || !geometry.height || geometry.width>circuit::device::MaxFramebufferDimension || geometry.height>circuit::device::MaxFramebufferDimension
            || geometry.stride<geometry.width || geometry.stride>circuit::device::MaxFramebufferBytes/geometry.height
            || (geometry.storage_word_bytes!=1 && geometry.storage_word_bytes!=2 && geometry.storage_word_bytes!=4))
            throw std::invalid_argument("Invalid display observation geometry");
        const auto count=geometry.stride*geometry.height;
        std::vector<uint8_t> pixels,known;pixels.reserve(count);known.reserve(count);
        const auto storage=child(component,"PIXELS");bool snapshot=false;
        if(auto view=std::dynamic_pointer_cast<MemoryStateView>(storage)){
            const MemoryStorage* memory=nullptr;
            try{memory=&view->memoryStorage();}catch(const std::logic_error&){} // fully expanded cells
            if(memory){
                if(current) {
                    for(size_t address=0;pixels.size()<count;address+=geometry.storage_word_bytes)
                        appendPixels(pixels,known,memory->readLogic(address,geometry.storage_word_bytes),count);
                } else {
                    if(!memory->isHistoryRecordingEnabled()){out<<",\"supported\":false,\"reason\":\"Pixel history is disabled\"}";return out.str();}
                    for(const auto& word:memory->getWordsAtTime(time,0,(count+geometry.storage_word_bytes-1)/geometry.storage_word_bytes))appendPixels(pixels,known,word.second,count);
                }
                snapshot=true;
            }
        }
        if(!snapshot){
            auto bank=wordBank(storage);if(!bank)throw std::logic_error("Display has no observable pixel words");
            for(size_t word=0;pixels.size()<count;++word)
                appendPixels(pixels,known,observed(bank->getOutputPinDynamic("WORD_"+std::to_string(word))),count);
        }
        out<<",\"kind\":\"display\",\"width\":"<<geometry.width<<",\"height\":"<<geometry.height
           <<",\"stride\":"<<geometry.stride<<",\"format\":\"rgb332\",\"pixels\":\""<<hex(pixels)<<"\",\"known\":\""<<hex(known)<<'"';
        field("sequence",io,"FRAME_SEQUENCE");field("present",io,"PRESENT");
    } else if(deviceViewKind(component)=="keyboard") {
        out<<",\"kind\":\"keyboard\",\"buttonCount\":"<<child(component,"LEVELS")->getOutputPinDynamic("Q")->getWidth();field("levels",child(component,"LEVELS"),"Q");
        const auto fifo=child(component,"EVENTS");field("count",fifo,"COUNT");field("head",fifo,"DATA");field("overflow",fifo,"OVERFLOW");
    } else if(deviceViewKind(component)=="console") {
        out<<",\"kind\":\"console\"";field("valid",io,"TX_VALID");field("data",io,"TX_DATA");field("count",child(component,"TX_FIFO"),"COUNT");
    } else if(deviceViewKind(component)=="timer") {
        out<<",\"kind\":\"timer\"";field("count",child(component,"COUNTER"),"COUNT");
    } else {
        out<<",\"kind\":\"queue\",\"dataWidth\":"<<io->getOutputPinDynamic("DATA")->getWidth();field("count",io,"COUNT");field("head",io,"DATA");field("full",io,"FULL");field("overflow",io,"OVERFLOW");
    }
    out<<",\"supported\":true}";return out.str();
}
}
