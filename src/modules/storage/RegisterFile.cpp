#include "components/ComponentBuilder.tpp"
#include "modules/storage/RegisterFile.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/storage/Register.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/WordMux.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "RegisterFileBehavioral.hpp"
#include <map>

namespace circuit::storage {
namespace {
void validate(size_t width,size_t count) {
    if((width!=8 && width!=16 && width!=32) || count<2 || count>32 || !std::has_single_bit(count))
        throw std::invalid_argument("Unsupported register-file geometry");
}
void pins(IOComponent* p,size_t width,size_t count) {
    validate(width,count);
    for(auto n:{"CLK","RST","REG_WRITE"})p->addPin(n,PinType::INPUT);
    for(auto n:{"RS1_ADDR","RS2_ADDR","RD_ADDR"})p->addPinDynamic(n,PinType::INPUT,std::countr_zero(count));
    p->addPinDynamic("WRITE_DATA",PinType::INPUT,width);
    for(auto n:{"RS1_DATA","RS2_DATA"})p->addPinDynamic(n,PinType::OUTPUT,width);
}
std::string type(size_t width,size_t count,RegisterFileConfig c) {
    return "RegisterFile<"+std::to_string(width)+","+std::to_string(count)+(c.hardwire_zero?",zero>":",writable-zero>");
}
class RegisterFileStorageBank final : public IOComponent {
    size_t width_,count_;std::string type_;
public:
    RegisterFileStorageBank(std::string name,size_t width,size_t count)
        :IOComponent(std::move(name),[width,count](IOComponent* p){
            p->addPinDynamic("WRITE_DATA",PinType::INPUT,width);
            p->addPin("CLK",PinType::INPUT);p->addPin("RST",PinType::INPUT);
            for(size_t i=0;i<count;++i){p->addPin("WRITE_"+std::to_string(i),PinType::INPUT);p->addPinDynamic("WORD_"+std::to_string(i),PinType::OUTPUT,width);}
        }),width_(width),count_(count),type_("RegisterFileStorageBank<"+std::to_string(width)+","+std::to_string(count)+">"){}
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder& b)override {
        detail::StructuralWiring w(b);
        for(size_t i=0;i<count_;++i){
            const auto name="WORD_"+std::to_string(i);
            auto reg=b.add(families::registerWord(width_),name);
            w.connect(getInputPinDynamic("WRITE_DATA"),reg->getInputPinDynamic("D"));
            for(auto n:{"CLK","RST"})w.connect(getInputPin(n),reg->getInputPin(n));
            w.connect(getInputPin("WRITE_"+std::to_string(i)),reg->getInputPin("WE"));
            w.connect(reg->getOutputPinDynamic("Q"),getOutputPinDynamic(name));
        }
        w.finish();
    }
};
std::string muxInput(size_t count,size_t i) {return count==2?(i?"B":"A"):"IN"+std::to_string(i);}
}
WordRegisterFile::WordRegisterFile(std::string name,size_t width,size_t count,RegisterFileConfig c)
    :IOComponent(std::move(name),[width,count](IOComponent* p){pins(p,width,count);}),width_(width),count_(count),config_(c),type_(type(width,count,c)){}
void WordRegisterFile::buildInternals(ComponentBuilder& b) {
    detail::StructuralWiring w(b);
    auto rs1=b.addNewComponent<word::WordMux>("RS1_MUX",count_,width_);
    auto rs2=b.addNewComponent<word::WordMux>("RS2_MUX",count_,width_);
    auto decoder=b.addNewComponent<logic::WordDecoder>("RD_DECODER",std::countr_zero(count_),config_.hardwire_zero);
    const size_t first=config_.hardwire_zero?1:0;
    auto storage=b.addNewComponent<RegisterFileStorageBank>("STORAGE",width_,count_-first);
    for(auto n:{"WRITE_DATA","CLK","RST"})w.connect(getInputPinDynamic(n),storage->getInputPinDynamic(n));
    w.connect(getInputPinDynamic("RD_ADDR"),decoder->getInputPinDynamic("ADDR"));
    w.connect(getInputPin("REG_WRITE"),decoder->getInputPin("ENABLE"));
    w.connect(getInputPinDynamic("RS1_ADDR"),rs1->getInputPinDynamic("SEL"));
    w.connect(getInputPinDynamic("RS2_ADDR"),rs2->getInputPinDynamic("SEL"));
    if(first) {
        std::shared_ptr<IOComponent> zero;
        switch(width_) {
        case 8:zero=b.addNewComponent<ConstantValue<8>>("X0_ZERO",0);break;
        case 16:zero=b.addNewComponent<ConstantValue<16>>("X0_ZERO",0);break;
        case 32:zero=b.addNewComponent<ConstantValue<32>>("X0_ZERO",0);break;
        }
        for(auto mux:{rs1,rs2})w.connect(zero->getOutputPinDynamic("OUT"),mux->getInputPinDynamic(muxInput(count_,0)));
    }
    for(size_t i=first;i<count_;++i) {
        const auto physical=std::to_string(i-first);
        w.connect(decoder->getOutputPin("OUT"+std::to_string(i)),storage->getInputPin("WRITE_"+physical));
        for(auto mux:{rs1,rs2})w.connect(storage->getOutputPinDynamic("WORD_"+physical),mux->getInputPinDynamic(muxInput(count_,i)));
    }
    w.connect(rs1->getOutputPinDynamic("OUT"),getOutputPinDynamic("RS1_DATA"));
    w.connect(rs2->getOutputPinDynamic("OUT"),getOutputPinDynamic("RS2_DATA"));
    w.finish();
}
std::vector<std::vector<LogicValue>> WordRegisterFile::getRegisterStateAtTime(SimulationTime)const {
    std::vector<std::vector<LogicValue>> words(count_,std::vector<LogicValue>(width_,LogicValue::LOW));
    const size_t first=config_.hardwire_zero?1:0;
    for(const auto& child:getChildren())if(child->getName()=="STORAGE") {
        const auto storage=std::dynamic_pointer_cast<IOComponent>(child);
        for(size_t i=first;i<count_;++i)words[i]=storage->getOutputPinDynamic("WORD_"+std::to_string(i-first))->getValueAsVector();
    }
    return words;
}
const ComponentFamily& registerFile(size_t width,size_t count,RegisterFileConfig c) {
    validate(width,count);
    if(width==32&&count==32&&c.hardwire_zero)return families::RegisterFile32x32;
    struct Entry {
        std::string id,name;ComponentFamily family;
        Entry(size_t width,size_t count,RegisterFileConfig c)
            :id("storage.register-file."+std::to_string(width)+"."+std::to_string(count)+(c.hardwire_zero?".zero":".all")),name(type(width,count,c)),
             family(id,name,[width,count](IOComponent*p){pins(p,width,count);},
                [width,count,c](const std::string& n,const std::shared_ptr<BuildContext>& context){return Component::createWithContext<WordRegisterFile>(context,n,width,count,c);},
                width==32&&count==32&&c.hardwire_zero?FamilyFactory([width,count,c](const std::string& n,const std::shared_ptr<BuildContext>& context){return Component::createWithContext<RegisterFileBehavioral>(context,n,width,count,c);}):FamilyFactory{}){}
    };
    static std::map<std::tuple<size_t,size_t,bool>,std::unique_ptr<Entry>> entries;
    auto& e=entries[{width,count,c.hardwire_zero}];if(!e)e=std::make_unique<Entry>(width,count,c);return e->family;
}
void registerRegisterFiles(ComponentCatalog& catalog) {
    for(size_t width:{8u,16u,32u})for(size_t count:{2u,4u,8u,16u,32u})for(bool zero:{false,true}) {
        const auto& f=registerFile(width,count,{zero});
        const auto owner=width==32&&count==32&&zero?"RegisterFileWidth32Count32Test":"WidthComponentsTest";
        const bool architectural=width==32&&count==32&&zero;
        registration::registerStructural(catalog,f,owner,"clock-boundary",{},{"register-state-view"},"four-state",
            architectural?"rv32i.register-file.structural.decoder-mux":"");
        if(f.supports(Fidelity::Behavioral))registration::registerBehavioral(catalog,f,owner,owner,{},"clock-boundary","four-state",{"register-state-view"},
            architectural?"rv32i.register-file.behavioral.direct":"");
    }
}
}

namespace circuit::families {
const ComponentFamily RegisterFile32x32{
    "rv32i.register-file","RegisterFile32x32",
    [](IOComponent* p){
        for(auto n:{"CLK","RST","REG_WRITE"})p->addPin(n,PinType::INPUT);
        for(auto n:{"RS1_ADDR","RS2_ADDR","RD_ADDR"})p->addPin<5>(n,PinType::INPUT);
        p->addPin<32>("WRITE_DATA",PinType::INPUT);
        for(auto n:{"RS1_DATA","RS2_DATA"})p->addPin<32>(n,PinType::OUTPUT);
    },
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::RegisterFile32x32>(c,n);},
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<storage::RegisterFileBehavioral>(c,n,32,32,storage::RegisterFileConfig{});}};
}
