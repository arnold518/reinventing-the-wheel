#include "modules/device/DeviceBus.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "components/ComponentBuilder.tpp"
#include "DeviceGroups.hpp"
#include "modules/device/RegisterTable.hpp"
#include "modules/memory/MemoryController.hpp"
#include "components/selection/FamilyFactories.hpp"
#include <bit>
using namespace circuit::device::detail;
namespace {
void displayPins(IOComponent*p){
    circuit::memory::protocolPins(p,32);circuit::memory::protocolPins(p,32,"FB_");
    p->addPin<32>("FRAME_SEQUENCE",PinType::OUTPUT);p->addPin("PRESENT",PinType::OUTPUT);
}
}
namespace circuit::families {
const ComponentFamily FramebufferDisplay{"device.framebuffer","FramebufferDisplay",displayPins,
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::FramebufferDisplay>(c,n,
        circuit::device::detail::parameter(c,"width",4),circuit::device::detail::parameter(c,"height",4),circuit::device::detail::parameter(c,"storage_bytes",0));},
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<DeviceBehavioral<::FramebufferDisplay>>(c,n,
        circuit::device::detail::parameter(c,"width",4),circuit::device::detail::parameter(c,"height",4),circuit::device::detail::parameter(c,"storage_bytes",0));}};
}
FramebufferDisplay::FramebufferDisplay(std::string n,size_t width,size_t height,size_t storage_bytes):IOComponent(std::move(n),displayPins),width_(width),height_(height),storage_bytes_(storage_bytes){
    circuit::device::FramebufferConfig{width,height,storage_bytes}.validate();
}
void FramebufferDisplay::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto port=devicePort(*this,b,l);
    circuit::memory::EndpointConfig cfg;cfg.depth_words=configuration().capacityBytes()/4;
    circuit::ComponentBuildRequest pixels{cfg.depth_words>128?"memory.ff.scalable-endpoint.32":"memory.ff.endpoint.32","PIXELS",circuit::memory::endpointParameters(cfg),{},{},{}};
    if(getSelectedFidelity()=="behavioral")pixels.required_fidelity=circuit::Fidelity::Behavioral;
    auto video=b.addByContract(std::move(pixels));
    storage_=std::dynamic_pointer_cast<MemoryStateView>(video);
    for(const auto& [name,pin]:video->getAllInputPins())l.wire(getInputPinDynamic("FB_"+name),pin);
    for(const auto& [name,pin]:video->getAllOutputPins())l.wire(pin,getOutputPinDynamic("FB_"+name));
    auto registers=circuit::device::buildRegisterFrontEnd(b,circuit::device::deviceRegisterTable("device.framebuffer",
        {{"width",std::to_string(width_)},{"height",std::to_string(height_)}}));
    frontEnd(l,port,registers);l.wire(l.bit(true),registers->getInputPin("READY"));
    auto frames=group(b,"FRAME_COUNTER","DisplayFrameCounter",{{"CLK",1},{"RST",1},{"PRESENT",1}},{{"FRAME_SEQUENCE",32}},
        [](IOComponent& self,ComponentBuilder& gates) {
            circuit::structural::Logic c(gates);
            auto seq=c.reg("SEQUENCE",32,input(self,"CLK"),input(self,"RST"),input(self,"PRESENT"));
            c.drive(c.add({c.split(seq->getOutputPin<32>("Q")),c.constant(32,1),c.bit(false)}).first,seq->getInputPin<32>("D"));
            c.wire(seq->getOutputPin<32>("Q"),output(self,"FRAME_SEQUENCE"));c.finish();
        });
    inputs(l,*this,frames,{"CLK","RST"});l.wire(registers->getOutputPin("PRESENT_WRITE"),frames->getInputPin("PRESENT"));
    l.wire(frames->getOutputPin<32>("FRAME_SEQUENCE"),registers->getInputPin<32>("FRAME_SEQUENCE_VALUE"));
    l.wire(registers->getOutputPin("PRESENT_WRITE"),getOutputPin("PRESENT"));l.wire(frames->getOutputPin<32>("FRAME_SEQUENCE"),getOutputPin<32>("FRAME_SEQUENCE"));l.finish();
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

