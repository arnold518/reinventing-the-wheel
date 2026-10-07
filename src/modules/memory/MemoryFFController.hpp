#pragma once
#include <bit>
#include "MemoryHandshakeWiring.hpp"

namespace circuit::memory_detail {
// X/Z detection is simulator metadata, not a synthesizable binary gate.
class RequestKnownBits : public memory_device::Primitive {
public:
    RequestKnownBits(std::string n,size_t width)
        : Primitive(std::move(n),"MemoryRequestKnownBits",[width](IOComponent* s){
            s->addPin<3>("OP",PinType::INPUT);s->addPin<32>("ADDR",PinType::INPUT);
            s->addPinDynamic("MASK",PinType::INPUT,width/8);
            s->addPin("FIELDS_KNOWN",PinType::OUTPUT);s->addPin("MASK_KNOWN",PinType::OUTPUT);
        }) {}
    void evaluate(size_t t,Simulator& sim) override {
        publish(sim,t,"FIELDS_KNOWN",memory_device::known(read("OP"))&&memory_device::known(read("ADDR"))?memory_device::H:memory_device::L);
        publish(sim,t,"MASK_KNOWN",memory_device::known(read("MASK"))?memory_device::H:memory_device::L);
    }
};
class FFRequestCheck : public IOComponent {
public:
    FFRequestCheck(std::string n,size_t width,size_t depth)
        : IOComponent(std::move(n),[width](IOComponent* s){
            s->addPin<3>("OP",PinType::INPUT);s->addPin<32>("ADDR",PinType::INPUT);
            s->addPinDynamic("MASK",PinType::INPUT,width/8);
            for(auto p:{"READ","WRITE"})s->addPin(p,PinType::OUTPUT);
            s->addPin<3>("STATUS",PinType::OUTPUT);s->addPin<32>("WORD_ADDR",PinType::OUTPUT);
        }),width_(width),depth_(depth),type_("MemoryFFRequestCheck<"+std::to_string(width)+","+std::to_string(depth)+">") {}
    const char* getTypeName() const override {return type_.c_str();}
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto op=memory_device::bitAdapter(b,"OP_BITS",3,true);
        auto addr=memory_device::bitAdapter(b,"ADDRESS_BITS",32,true);
        w.connect(getInputPin<3>("OP"),op->getInputPin<3>("IN"));
        w.connect(getInputPin<32>("ADDR"),addr->getInputPin<32>("IN"));
        auto known=b.addNewComponent<RequestKnownBits>("KNOWN_FIELDS",width_);
        for(auto p:{"OP","ADDR","MASK"})w.connect(getInputPinDynamic(p),known->getInputPinDynamic(p));
        auto o0=op->getOutputPin("OUT_0"),o1=op->getOutputPin("OUT_1"),o2=op->getOutputPin("OUT_2");
        auto n0=w.invert("NOT_OP0",o0),n1=w.invert("NOT_OP1",o1),n2=w.invert("NOT_OP2",o2);
        auto uses_mask=w.binary<XORGate>("WRITE_OR_PROGRAM",o0,o1);
        auto unknown_mask=w.both("UNKNOWN_MASK",uses_mask,w.invert("MASK_UNKNOWN",known->getOutputPin("MASK_KNOWN")));
        const size_t shift=width_==32?2:width_==16?1:0;
        std::vector<StructuralWiring::PinPtr> low;
        for(size_t i=0;i<shift;++i)low.push_back(addr->getOutputPin("OUT_"+std::to_string(i)));
        auto unaligned=w.reduce("UNALIGNED",low,false);
        const uint64_t capacity=uint64_t(depth_)*(width_/8);
        StructuralWiring::PinPtr ge;
        if (std::has_single_bit(capacity)) {
            // For a power-of-two byte capacity, only the high address bits
            // matter: no full-width magnitude comparator is needed.
            std::vector<StructuralWiring::PinPtr> high_bits;
            for (size_t i=std::countr_zero(capacity);i<32;++i)
                high_bits.push_back(addr->getOutputPin("OUT_"+std::to_string(i)));
            ge=w.reduce("OUT_OF_RANGE",high_bits,false);
        } else {
            // Combine equal/greater groups as a balanced unsigned comparison.
            // The high group decides unless it equals its constant slice.
            struct CompareGroup { StructuralWiring::PinPtr equal,greater; };
            std::vector<CompareGroup> groups;
            auto no_greater=w.constant("RANGE_FALSE",false);
            for (size_t i=0;i<32;++i) {
                auto bit=addr->getOutputPin("OUT_"+std::to_string(i));
                bool constant_bit=(capacity&(uint64_t{1}<<i))!=0;
                groups.push_back({constant_bit?bit:w.invert("RANGE_NOT_"+std::to_string(i),bit),
                                  constant_bit?no_greater:bit});
            }
            for (size_t stride=1;stride<32;stride*=2) {
                for (size_t low=0;low+stride<32;low+=stride*2) {
                    auto& lo=groups[low];
                    auto& hi=groups[low+stride];
                    lo.greater=w.either("RANGE_GREATER_"+std::to_string(stride)+"_"+std::to_string(low),
                        hi.greater,w.both("RANGE_EQUAL_GREATER_"+std::to_string(stride)+"_"+std::to_string(low),hi.equal,lo.greater));
                    lo.equal=w.both("RANGE_EQUAL_"+std::to_string(stride)+"_"+std::to_string(low),hi.equal,lo.equal);
                }
            }
            ge=w.either("OUT_OF_RANGE",groups.front().greater,groups.front().equal);
        }
        auto constant=[&](const std::string& name,unsigned v){return b.addNewComponent<ConstantValue<3>>(name,v)->getOutputPin<3>("OUT");};
        // Component creation mutates the hierarchy. Function argument evaluation
        // order differs between native GCC and WebAssembly Clang.
        auto ok=constant("OK",0);
        auto unsupported=constant("UNSUPPORTED",4);
        auto status=payloadMux(b,w,"PERMISSION",ok,unsupported,o1);
        status=payloadMux(b,w,"RANGE_STATUS",status,constant("RANGE_ERROR",2),ge);
        status=payloadMux(b,w,"ALIGN_STATUS",status,constant("ALIGN_ERROR",1),unaligned);
        status=payloadMux(b,w,"MASK_STATUS",status,constant("BAD_MASK",6),unknown_mask);
        status=payloadMux(b,w,"OPCODE_STATUS",status,constant("RESERVED",4),o2);
        status=payloadMux(b,w,"KNOWN_STATUS",constant("UNKNOWN_FIELDS",6),status,known->getOutputPin("FIELDS_KNOWN"));
        w.connect(status,getOutputPin<3>("STATUS"));
        auto legal=w.reduce("LEGAL",{known->getOutputPin("FIELDS_KNOWN"),n1,n2,
            w.invert("ALIGNED",unaligned),w.invert("IN_RANGE",ge)},true);
        w.connect(w.both("READ",legal,n0),getOutputPin("READ"));
        w.connect(w.reduce("WRITE",{legal,o0,known->getOutputPin("MASK_KNOWN")},true),getOutputPin("WRITE"));
        auto word=b.addNewComponent<Rewire>("BYTE_TO_WORD",std::vector<Rewire::WireSpec>{{"IN",32}},
            std::vector<Rewire::WireSpec>{{"OUT",32}},identity_mapping("IN",shift,32-shift,"OUT"),Rewire::UnmappedBitValue::LOW);
        w.connect(getInputPin<32>("ADDR"),word->getInputPin<32>("IN"));
        w.connect(word->getOutputPin<32>("OUT"),getOutputPin<32>("WORD_ADDR"));
        w.finish();
    }
private:size_t width_,depth_;std::string type_;
};
inline void buildFFController(ComponentBuilder& b,IOComponent& self,size_t width,size_t depth,bool always_ready) {
    StructuralWiring w(b);
    auto check=b.addNewComponent<FFRequestCheck>("REQUEST_CHECK",width,depth);
    for(auto p:{"OP","ADDR"}) w.connect(self.getInputPinDynamic(std::string("REQ_")+p),check->getInputPinDynamic(p));
    w.connect(self.getInputPinDynamic("REQ_WRITE_MASK"),check->getInputPinDynamic("MASK"));
    std::shared_ptr<PinBase> ready;
    std::shared_ptr<MemoryResponseBuffer> response;
    if (always_ready) {
        ready=w.invert("ACTIVE",self.getInputPin("RST"));
    } else {
        response=b.addNewComponent<MemoryResponseBuffer>("RESPONSE",width);
        for(auto p:{"CLK","RST"})w.connect(self.getInputPin(p),response->getInputPin(p));
        w.connect(self.getInputPin("RSP_READY"),response->getInputPin("OUT_READY"));
        ready=response->getOutputPin("IN_READY");
    }
    auto accept=w.both("ACCEPT",self.getInputPin("REQ_VALID"),ready);
    w.connect(ready,self.getOutputPin("REQ_READY"));
    if (response) w.connect(accept,response->getInputPin("IN_VALID"));
    else w.connect(accept,self.getOutputPin("RSP_VALID"));
    auto access=w.either("LEGAL_ACCESS",check->getOutputPin("READ"),check->getOutputPin("WRITE"));
    auto fault=w.both("DEVICE_FAULT",access,self.getInputPin("MEM_FAULT"));
    auto device_error=b.addNewComponent<ConstantValue<3>>("DEVICE_ERROR",5);
    auto status=payloadMux(b,w,"RESPONSE_STATUS",check->getOutputPin<3>("STATUS"),
        device_error->getOutputPin<3>("OUT"),fault);
    w.connect(status,response ? std::static_pointer_cast<PinBase>(response->getInputPin<3>("IN_STATUS"))
                              : std::static_pointer_cast<PinBase>(self.getOutputPin<3>("RSP_STATUS")));
    auto bits=memory_device::bitAdapter(b,"READ_ZERO",width,false);
    auto low=w.constant("LOW",false);
    for(size_t i=0;i<width;++i)w.connect(low,bits->getInputPin("IN_"+std::to_string(i)));
    auto read_ok=w.both("READ_OK",check->getOutputPin("READ"),w.invert("NO_DEVICE_FAULT",self.getInputPin("MEM_FAULT")));
    auto read_result=payloadMux(b,w,"READ_RESULT",bits->getOutputPinDynamic("OUT"),self.getInputPinDynamic("MEM_READ_DATA"),read_ok);
    w.connect(read_result,response ? response->getInputPinDynamic("IN_DATA") : self.getOutputPinDynamic("RSP_READ_DATA"));
    w.connect(check->getOutputPin<32>("WORD_ADDR"),self.getOutputPin<32>("MEM_WORD_ADDR"));
    for(auto p:{"WRITE_DATA","WRITE_MASK"})w.connect(bridge(b,w,p,self.getInputPinDynamic(std::string("REQ_")+p)),self.getOutputPinDynamic(std::string("MEM_")+p));
    w.connect(bridge(b,w,"DEVICE_CLOCK",self.getInputPin("CLK")),self.getOutputPin("MEM_CLK"));
    w.connect(low,self.getOutputPin("MEM_RST"));
    w.connect(w.both("WRITE_ACCEPTED",accept,check->getOutputPin("WRITE")),self.getOutputPin("MEM_WE"));
    w.connect(w.both("READ_PRESENT",accept,check->getOutputPin("READ")),self.getOutputPin("MEM_READ_EN"));
    if (response) for(auto [source,target]:std::vector<std::pair<std::string,std::string>>{
        {"OUT_VALID","RSP_VALID"},{"OUT_DATA","RSP_READ_DATA"},{"OUT_STATUS","RSP_STATUS"}})
        w.connect(response->getOutputPinDynamic(source),self.getOutputPinDynamic(target));
    w.finish();
}
}
