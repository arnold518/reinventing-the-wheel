#include "modules/device/DeviceBus.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "components/ComponentBuilder.tpp"
#include "DeviceLogic.hpp"
#include "modules/memory/MemoryController.hpp"
#include "components/selection/FamilyFactories.hpp"
#include <bit>
using namespace circuit::device::detail;
namespace {
void pixelPins(IOComponent*p,size_t width=32) {
    if(width!=8 && width!=16 && width!=32)throw std::invalid_argument("Invalid pixel word width");
    p->addPinDynamic("WORD",PinType::INPUT,width);p->addPinDynamic("OFFSET",PinType::INPUT,std::max(size_t{1},size_t(std::countr_zero(width/8))));
    p->addPin<8>("INDEX",PinType::OUTPUT);p->addPin<32>("ARGB",PinType::OUTPUT);
}
void displayPins(IOComponent*p){
    circuit::memory::protocolPins(p,32);circuit::memory::protocolPins(p,32,"FB_");
    p->addPin<32>("FRAME_SEQUENCE",PinType::OUTPUT);p->addPin("PRESENT",PinType::OUTPUT);
}
}
namespace circuit::families {
const ComponentFamily FramebufferDisplay{"device.framebuffer","FramebufferDisplay",displayPins,
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::FramebufferDisplay>(c,n,
        circuit::device::detail::parameter(c,"width",4),circuit::device::detail::parameter(c,"height",4),circuit::device::detail::parameter(c,"storage_bytes",0));}};
}
namespace {void buildPixel(IOComponent& self,ComponentBuilder& b,size_t width){
    Logic l(b);auto word=l.split(self.getInputPinDynamic("WORD")),offset=l.split(self.getInputPinDynamic("OFFSET"));auto pixel=l.constant(8);
    for(size_t i=0;i<width/8;++i)pixel=l.mux({l.equal({offset,l.constant(offset.size(),i)}),pixel,V(word.begin()+8*i,word.begin()+8*(i+1))});
    V argb(32);const std::array<size_t,8> three{1,2,0,1,2,0,1,2};
    for(size_t i=0;i<8;++i){argb[i]=pixel[i%2];argb[i+8]=pixel[2+three[i]];argb[i+16]=pixel[5+three[i]];argb[i+24]=l.bit(true);}
    l.drive(pixel,self.getOutputPin<8>("INDEX"));l.drive(argb,self.getOutputPin<32>("ARGB"));l.finish();
}}

FramebufferDisplay::FramebufferDisplay(std::string n,size_t width,size_t height,size_t storage_bytes):IOComponent(std::move(n),displayPins),width_(width),height_(height),storage_bytes_(storage_bytes){
    circuit::device::FramebufferConfig{width,height,storage_bytes}.validate();
}
void FramebufferDisplay::buildInternals(ComponentBuilder&b){
    Logic l(b);auto port=devicePort(*this,b,l);
    circuit::memory::EndpointConfig cfg;cfg.depth_words=circuit::device::FramebufferConfig{width_,height_,storage_bytes_}.capacityBytes()/4;
    auto video=b.addByContract({cfg.depth_words>128?"memory.ff.scalable-endpoint.32":"memory.ff.endpoint.32",
        "PIXELS",circuit::memory::endpointParameters(cfg),{},{},{}});
    storage_=std::dynamic_pointer_cast<MemoryStateView>(video);
    for(const auto&[n,p]:video->getAllInputPins())l.wire(getInputPinDynamic("FB_"+n),p);
    for(const auto&[n,p]:video->getAllOutputPins())l.out(p,getOutputPinDynamic("FB_"+n));
    auto width=at(l,port,0),height=at(l,port,4),stride=at(l,port,8),format=at(l,port,12),sequence=at(l,port,16),present=at(l,port,20);
    auto readable=l.any({width,height,stride,format,sequence});auto mapped=l.lor({readable,present});auto wr=port->getOutputPin("IS_WRITE");
    auto status=l.mux({mapped,l.constant(3,2),l.mux({wr,l.constant(3),l.constant(3,3)})});
    status=l.mux({present,status,l.mux({wr,l.constant(3,4),l.constant(3)})});
    auto mask=l.any(l.split(port->getOutputPin<4>("WRITE_MASK")));
    status=l.mux({l.land({present,l.land({wr,l.land({mask,l.inv(validCommand(l,b,port))})})}),status,l.constant(3,6)});
    l.drive(status,port->getInputPin<3>("BACK_STATUS"));l.wire(l.bit(true),port->getInputPin("BACK_READY"));
    auto command=l.land({present,l.land({port->getOutputPin("WRITE_STROBE"),l.split(port->getOutputPin<32>("WRITE_DATA"))[0]})});
    auto seq=l.reg("FRAME_COUNTER",32,getInputPin("CLK"),getInputPin("RST"),command);
    l.drive(l.add({l.split(seq->getOutputPin<32>("Q")),l.constant(32,1),l.bit(false)}).first,seq->getInputPin<32>("D"));
    V data=l.constant(32);data=l.mux({width,data,l.constant(32,width_)});data=l.mux({height,data,l.constant(32,height_)});
    data=l.mux({stride,data,l.constant(32,width_)});data=l.mux({format,data,l.constant(32,1)});
    data=l.mux({sequence,data,l.split(seq->getOutputPin<32>("Q"))});l.drive(data,port->getInputPin<32>("BACK_READ_DATA"));
    l.out(seq->getOutputPin<32>("Q"),getOutputPin<32>("FRAME_SEQUENCE"));l.out(command,getOutputPin("PRESENT"));l.finish();
}
std::string FramebufferDisplay::componentLayoutVariant() const {
    const auto bytes=configuration().capacityBytes();
    if(width_==4&&height_==4&&bytes==16)return {};
    return "width="+std::to_string(width_)+",height="+std::to_string(height_)+",storage="+std::to_string(bytes);
}
const MemoryStorage& FramebufferDisplay::memoryStorage()const {return storage_->memoryStorage();}
void FramebufferDisplay::loadMemoryBytes(uint32_t a,const std::vector<uint8_t>&d){storage_->loadMemoryBytes(a,d);}
void FramebufferDisplay::initializeMemoryBytes(uint32_t a,const std::vector<uint8_t>&d){storage_->initializeMemoryBytes(a,d);}
void FramebufferDisplay::clearMemoryImage(){storage_->clearMemoryImage();}
void FramebufferDisplay::setMemoryHistoryRecordingEnabled(bool e){storage_->setMemoryHistoryRecordingEnabled(e);}

namespace circuit::device {
WordPixelSelector::WordPixelSelector(std::string n,size_t w):IOComponent(std::move(n),[w](IOComponent*p){pixelPins(p,w);}),width_(w),type_("IndexedPixelSelector<"+std::to_string(w)+">"){}
void WordPixelSelector::buildInternals(ComponentBuilder& b){buildPixel(*this,b,width_);}
const ComponentFamily& indexedPixelSelector(size_t w){
    static const ComponentFamily byte{"video.pixel-selector.8","IndexedPixelSelector<8>",[](IOComponent*p){pixelPins(p,8);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<WordPixelSelector>(c,n,8);}};
    static const ComponentFamily half{"video.pixel-selector.16","IndexedPixelSelector<16>",[](IOComponent*p){pixelPins(p,16);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<WordPixelSelector>(c,n,16);}};
    static const ComponentFamily word{"video.pixel-selector.32","IndexedPixelSelector<32>",[](IOComponent*p){pixelPins(p,32);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<WordPixelSelector>(c,n,32);}};
    if(w==8)return byte;if(w==16)return half;if(w==32)return word;throw std::invalid_argument("Invalid pixel word width");
}
void registerWordPixelSelectors(ComponentCatalog& c){for(size_t w:{8u,16u,32u})circuit::registration::registerStructural(c,indexedPixelSelector(w),w==32?"IndexedPixelSelectorWidth32Test":"WidthComponentsTest");}
}
