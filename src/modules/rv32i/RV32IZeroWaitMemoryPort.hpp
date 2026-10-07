#pragma once
#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "../memory/MemoryHandshakeWiring.hpp"

namespace rv32i {
// Combinational port formatting for cores with an explicit zero-wait contract.
// No request capture, generated clock, or response storage exists here.
class ZeroWaitMemoryPort : public IOComponent {
public:
    explicit ZeroWaitMemoryPort(std::string name)
        : IOComponent(std::move(name),[](IOComponent* s){
            s->addPin<32>("WORD_ADDR",PinType::INPUT);s->addPin<32>("WRITE_DATA",PinType::INPUT);
            s->addPin<4>("WRITE_MASK",PinType::INPUT);
            for(auto p:{"READ_EN","WE"})s->addPin(p,PinType::INPUT);
            s->addPin<32>("READ_DATA",PinType::OUTPUT);s->addPin("FAULT",PinType::OUTPUT);
            memoryRequestPins(s,"");
        }) {}
    const char* getTypeName() const override{return "RV32IZeroWaitMemoryPort";}
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        StructuralWiring w(b);
        auto addr=b.addNewComponent<Rewire>("WORD_TO_BYTE",std::vector<Rewire::WireSpec>{{"IN",32}},
            std::vector<Rewire::WireSpec>{{"OUT",32}},identity_mapping("IN",0,30,"OUT",2),Rewire::UnmappedBitValue::LOW);
        w.connect(getInputPin<32>("WORD_ADDR"),addr->getInputPin<32>("IN"));
        w.connect(addr->getOutputPin<32>("OUT"),getOutputPin<32>("REQ_ADDR"));
        auto op=b.addNewComponent<BitJoiner<3>>("OPERATION");
        w.connect(getInputPin("WE"),op->getInputPin("IN_0"));
        auto low=w.constant("ZERO",false);
        w.connect(low,op->getInputPin("IN_1"));w.connect(low,op->getInputPin("IN_2"));
        w.connect(op->getOutputPin<3>("OUT"),getOutputPin<3>("REQ_OP"));
        auto issue=w.either("ISSUE",getInputPin("READ_EN"),getInputPin("WE"));
        w.connect(issue,getOutputPin("REQ_VALID"));
        w.connect(w.constant("RESPONSE_ACCEPTED",true),getOutputPin("RSP_READY"));
        for(auto p:{"WRITE_DATA","WRITE_MASK"})w.connect(bridge(b,w,p,getInputPinDynamic(p)),getOutputPinDynamic(std::string("REQ_")+p));
        w.connect(bridge(b,w,"READ_RESPONSE",getInputPin<32>("RSP_READ_DATA")),getOutputPin<32>("READ_DATA"));
        auto status=b.addNewComponent<BitSplitter<3>>("STATUS");
        w.connect(getInputPin<3>("RSP_STATUS"),status->getInputPin<3>("IN"));
        auto error=w.reduce("ERROR",{status->getOutputPin("OUT_0"),status->getOutputPin("OUT_1"),status->getOutputPin("OUT_2")},false);
        w.connect(w.both("FAULT",issue,error),getOutputPin("FAULT"));
        w.finish();
    }
};
inline void wireZeroWaitPort(ComponentBuilder& b,circuit::detail::StructuralWiring& w,
    IOComponent& core,const std::shared_ptr<IOComponent>& stage,const std::string& prefix,bool instruction) {
    auto port=b.addNewComponent<ZeroWaitMemoryPort>(prefix=="IMEM_"?"INSTRUCTION_PORT":"DATA_PORT");
    for(auto& [name,pin]:stage->getAllOutputPins())if(name.starts_with(prefix))w.connect(pin,port->getInputPinDynamic(name.substr(prefix.size())));
    for(auto& [name,pin]:stage->getAllInputPins())if(name.starts_with(prefix))w.connect(port->getOutputPinDynamic(name.substr(prefix.size())),pin);
    for(auto& [name,pin]:port->getAllInputPins())if(name.starts_with("RSP_")||name=="REQ_READY")w.connect(core.getInputPinDynamic(prefix+name),pin);
    for(auto& [name,pin]:port->getAllOutputPins())if(name.starts_with("REQ_")||name=="RSP_READY")w.connect(pin,core.getOutputPinDynamic(prefix+name));
    if(instruction) {
        w.connect(w.constant("INSTRUCTION_NO_WRITE",false),port->getInputPin("WE"));
        w.connect(b.addNewComponent<ConstantValue<32>>("INSTRUCTION_WRITE_ZERO",0)->getOutputPin<32>("OUT"),port->getInputPin<32>("WRITE_DATA"));
        w.connect(b.addNewComponent<ConstantValue<4>>("INSTRUCTION_MASK_ZERO",0)->getOutputPin<4>("OUT"),port->getInputPin<4>("WRITE_MASK"));
    }
}
}
