#include "components/ComponentBuilder.tpp"
#include "tests/RV32IMPhase3Tests.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/TestRegistry.hpp"
#include "modules/rv32im/MulDiv.hpp"
#include "modules/rv32im/RV32IMCore.hpp"
#include "modules/device/DeviceComponents.hpp"
#include "rv32im/RV32IMInstruction.hpp"
#include "rv32i/RV32IElfImage.hpp"
#include "components/BasicComponent.hpp"
#include "visualizer/DeviceInspector.hpp"
#include "modules/memory/MemoryStorage.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <deque>
#include <functional>
#include <iostream>
#include <random>
void appendRV32IMPhase3Scenarios(std::vector<TestRegistryEntry>&);
namespace {
#include "RV32IMSmokeImage.generated.inc"
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
circuit::BuildProfile compactProfile() {
    return circuit::BuildProfileBuilder("phase3-compact-children")
        .addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::any()))
        .unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build();
}
class Rig {
public:
    std::shared_ptr<IOComponent> root;Simulator sim;
    std::map<std::string,std::shared_ptr<WireBase>> inputs;
    SimulationTime now=0;
    explicit Rig(const circuit::ComponentFamily&f,bool compact=true,circuit::ParameterMap parameters={},bool history=false) {
        auto built=circuit::builtinComponentCatalog().createRoot(f.request("DUT",std::move(parameters)),compact?compactProfile():circuit::strictAllStructural());
        root=std::dynamic_pointer_cast<IOComponent>(built.root);
        ComponentBuilder b(root);
        for(auto&[n,p]:root->getAllInputPins())inputs[n]=b.addNewWireDynamic("IN_"+n,p->getWidth(),nullptr,{p});
        for(auto&[n,p]:root->getAllOutputPins())b.addNewWireDynamic("OUT_"+n,p->getWidth(),p,{});
        sim.setHistoryRecordingEnabled(history);SimulationTest::scheduleInitialEventsForTree(root,sim);
        for(auto&[n,p]:inputs)sim.scheduleEvent(makeTestWireUpdate(0,p,TestValue(uint64_t(n=="RST"||n.ends_with("_RST")||n=="ENABLE"||n=="BACK_READY"))));
        sim.advanceAndRecord(now=1000);cycle();std::map<std::string,uint64_t> reset;
        for(auto&[n,p]:inputs)if(n=="RST"||n.ends_with("_RST"))reset[n]=0;
        drive(reset);
    }
    void drive(const std::map<std::string,uint64_t>&v){
        for(auto&[n,x]:v)sim.scheduleEvent(makeTestWireUpdate(now+1,inputs.at(n),TestValue(x)));
        sim.advanceAndRecord(now+=1000);
    }
    void unknown(const std::string&n,LogicValue value){
        sim.scheduleEvent(makeTestWireUpdate(now+1,inputs.at(n),TestValue(std::vector<LogicValue>(inputs.at(n)->getWidth(),value))));
        sim.advanceAndRecord(now+=1000);
    }
    uint32_t out(const std::string&n)const {
        auto p=root->getOutputPinDynamic(n);
        for(auto x:p->getValueAsVector())require(x==LogicValue::LOW||x==LogicValue::HIGH,"Unknown output "+n);
        return static_cast<uint32_t>(p->getValueAsUInt64());
    }
    void cycle(){
        std::map<std::string,uint64_t> low,high;
        for(auto&[n,p]:inputs)if(n=="CLK"||n.ends_with("_CLK")){low[n]=0;high[n]=1;}
        drive(low);drive(high);drive(low);
    }
    std::pair<uint32_t,uint32_t> access(bool write,uint32_t address,uint32_t data=0,uint32_t mask=15,const std::string&prefix=""){
        drive({{prefix+"REQ_VALID",1},{prefix+"REQ_OP",write?1u:0u},{prefix+"REQ_ADDR",address},
               {prefix+"REQ_WRITE_DATA",data},{prefix+"REQ_WRITE_MASK",mask},{prefix+"RSP_READY",1}});
        bool accepted=false;
        for(size_t i=0;i<64;++i){
            bool request=!accepted&&out(prefix+"REQ_READY");
            bool response=out(prefix+"RSP_VALID");
            auto result=response?std::pair{out(prefix+"RSP_STATUS"),out(prefix+"RSP_READ_DATA")}:std::pair<uint32_t,uint32_t>{0,0};
            require(!response||accepted||request,"Response before request ownership");
            cycle();
            if(request){accepted=true;drive({{prefix+"REQ_VALID",0}});}
            if(response){drive({{prefix+"RSP_READY",0}});return result;}
        }throw std::runtime_error("Device transaction timed out");
    }
};
void steps(bool divide){
    for(bool compact:{false,true})for(size_t w:{4u,32u}) {
        Rig r(divide?circuit::families::divideStep(w):circuit::families::multiplyStep(w),compact);
        unsigned limit=w==4?16:8;
        for(unsigned a=0;a<limit;++a)for(unsigned b=0;b<limit;++b)for(unsigned bit=0;bit<2;++bit) {
            const uint64_t mask=(uint64_t(1)<<w)-1;
            uint64_t aa=w==4?a:uint32_t(a*0x2468ace1u),bb=w==4?b:uint32_t(b*0x7654321u);
            if(divide){
                r.drive({{"REMAINDER",aa},{"DIVISOR",bb},{"BIT",bit}});
                uint64_t shifted=(aa<<1)|bit;bool ge=shifted>=bb;
                require(r.out("QUOTIENT_BIT")==ge&&r.out("NEXT_REMAINDER")==((ge?shifted-bb:shifted)&mask),"Divide step mismatch");
            }else{
                r.drive({{"HI",aa},{"A",bb},{"BIT",bit}});uint64_t sum=aa+(bit?bb:0);
                require(r.out("SUM")==(sum&mask)&&r.out("CARRY")==((sum>>w)&1),"Multiply step mismatch");
            }
        }
    }
}
void arithmetic(const circuit::ComponentFamily&f,bool combined,bool divide){
    Rig r(f);std::mt19937 rng(0x32a11);
    const std::vector<uint32_t> edges{0,1,2,0x7fffffff,0x80000000,0xffffffff,0x12345678,0x87654321};
    const std::array<std::pair<uint32_t,uint32_t>,8> directed{{
        {0x80000000,0xffffffff},{0x80000000,0},{0x80000000,0x80000000},{0xffffffff,0xffffffff},
        {0x7fffffff,0x7fffffff},{1,0},{0,0},{0x7fffffff,0xffffffff}}};
    for(unsigned i=0;i<48;++i) {
        uint32_t a=i<edges.size()?edges[i]:rng(),b=i<edges.size()?edges[edges.size()-1-i]:rng();
        if(i>=8&&i<16){a=directed[i-8].first;b=directed[i-8].second;}
        unsigned first=combined?0:divide?4:0,last=combined?8:divide?8:4;
        for(unsigned op=first;op<last;++op){
            require(r.out("READY"),"Arithmetic slot not ready");
            std::map<std::string,uint64_t> pins{{"A",a},{"B",b},{"START",1},{"TAKE",0}};
            if(combined)pins["OP"]=op;
            else if(divide)pins["SIGNED"]=!(op&1);
            else{pins["SIGNED_A"]=op==1||op==2;pins["SIGNED_B"]=op==1;}
            r.drive(pins);r.cycle();r.drive({{"START",0}});
            require(r.out("BUSY")&&!r.out("DONE"),"Arithmetic launch ownership");
            for(unsigned step=0;step<32;++step){require(!r.out("DONE"),"Arithmetic completed early");r.cycle();}
            require(r.out("DONE")&&!r.out("BUSY")&&!r.out("READY"),"Arithmetic latency/completion");
            auto value=r.out(combined?"RESULT":divide?(op&2?"REMAINDER":"QUOTIENT"):op?"HI":"LO");
            auto expect=rv32im::referenceResult(static_cast<rv32im::MOperation>(op),a,b);
            require(value==expect,"M result mismatch "+std::string(rv32im::name(static_cast<rv32im::MOperation>(op)))+" A="+std::to_string(a)+" B="+std::to_string(b));
            r.drive({{"A",~a},{"B",~b}});r.cycle();
            require(r.out("DONE")&&r.out(combined?"RESULT":divide?(op&2?"REMAINDER":"QUOTIENT"):op?"HI":"LO")==value,"Held arithmetic response changed");
            r.drive({{"TAKE",1}});r.cycle();r.drive({{"TAKE",0}});require(r.out("READY"),"Arithmetic result not released");
        }
    }
    r.drive({{"A",19},{"B",7},{"START",1}});r.cycle();r.drive({{"START",0},{"ENABLE",0}});
    for(int i=0;i<4;++i)r.cycle();require(r.out("BUSY")&&!r.out("DONE"),"Arithmetic advanced while disabled");
    r.drive({{"CANCEL",1}});r.cycle();r.drive({{"CANCEL",0},{"ENABLE",1}});
    require(!r.out("BUSY")&&!r.out("DONE")&&r.out("READY"),"Arithmetic cancellation");
    r.drive({{"START",1}});r.cycle();r.drive({{"START",0}});
    for(int i=0;i<12;++i)r.cycle();
    r.drive({{"ENABLE",0}});for(int i=0;i<5;++i)r.cycle();
    require(!r.out("DONE"),"Arithmetic pause advanced iteration count");
    r.drive({{"ENABLE",1}});for(int i=0;i<20;++i)r.cycle();
    require(r.out("DONE"),"Arithmetic did not resume to original completion edge");
    r.drive({{"RST",1}});r.cycle();r.drive({{"RST",0}});
    require(r.out("READY")&&!r.out("DONE"),"Arithmetic reset held completion");
    r.drive({{"START",1}});r.cycle();r.drive({{"START",0},{"RST",1}});r.cycle();r.drive({{"RST",0}});
    require(r.out("READY")&&!r.out("BUSY")&&!r.out("DONE"),"Arithmetic reset in flight");
}
void fifo(){
    for(size_t depth=1;depth<=7;++depth)for(size_t width:{8u,32u}) {
        std::vector<std::vector<uint64_t>> traces;
        for(bool compact:{false,true}) {
            Rig r(circuit::families::synchronousFifo(width),compact,{{"depth",std::to_string(depth)}});
            if(!compact) {
                const auto key=circuit::visualizer::componentLayoutVariant(r.root);
                require(key==(depth==4?"":"depth="+std::to_string(depth)),"Queue layout variant/default compatibility");
            }
            std::deque<uint32_t> queue;bool overflow=false;std::vector<uint64_t> trace;
            for(unsigned edge=0;edge<128;++edge) {
                const bool push=edge%11<8,pop=edge%13>=6,clear=edge%17==0;
                const auto value=(edge*73+11)&(width==8?255u:0xffffffffu);
                const bool full=queue.size()==depth,empty=queue.empty();
                r.drive({{"PUSH",push},{"POP",pop},{"CLEAR_OVERFLOW",clear},{"PUSH_DATA",value}});r.cycle();
                if(pop&&!empty)queue.pop_front();if(push&&!full)queue.push_back(value);
                overflow=(push&&full)||(overflow&&!clear);
                require(r.out("COUNT")==queue.size()&&r.out("DATA")== (queue.empty()?0:queue.front()),"Parameterized FIFO wrap/order/count");
                require(r.out("FULL")== (queue.size()==depth)&&r.out("VALID")==!queue.empty()&&r.out("OVERFLOW")==overflow,"Parameterized FIFO state");
                for(auto pin:{"COUNT","DATA","FULL","VALID","PUSH_READY","OVERFLOW"})trace.push_back(r.out(pin));
            }
            r.drive({{"RST",1}});r.cycle();r.drive({{"RST",0}});
            require(!r.out("COUNT")&&!r.out("VALID")&&!r.out("OVERFLOW"),"Parameterized FIFO reset");
            traces.push_back(std::move(trace));
        }
        require(traces[0]==traces[1],"Parameterized FIFO structural/compact mismatch");
    }
    for(const auto& bad:std::vector<circuit::ParameterMap>{{{"depth","0"}},{{"depth","8"}},{{"depth","-1"}},{{"unknown","1"}}}) {
        bool rejected=false;try{Rig invalid(circuit::families::synchronousFifo(8),true,bad);}catch(const std::exception&){rejected=true;}
        require(rejected,"Invalid FIFO parameters accepted");
    }

    for(bool compact:{false,true})for(size_t w:{8u,32u}){
        Rig r(circuit::families::synchronousFifo(w),compact);std::deque<uint32_t> q;
        for(uint32_t i=1;i<=5;++i){r.drive({{"PUSH",1},{"PUSH_DATA",i}});r.cycle();if(i<=4)q.push_back(i);}
        r.drive({{"PUSH",0}});
        require(r.out("FULL")&&r.out("OVERFLOW")&&r.out("COUNT")==4,"FIFO full/overflow");
        r.drive({{"POP",1}});
        while(!q.empty()){require(r.out("DATA")==q.front(),"FIFO order");r.cycle();q.pop_front();}
        r.drive({{"POP",0},{"CLEAR_OVERFLOW",1}});r.cycle();
        require(!r.out("VALID")&&!r.out("OVERFLOW")&&r.out("PUSH_READY")&&!r.out("COUNT"),"FIFO drain/clear");
        r.drive({{"PUSH",1},{"PUSH_DATA",99},{"POP",1},{"CLEAR_OVERFLOW",0}});r.cycle();
        require(r.out("COUNT")==1&&r.out("DATA")==99,"Empty simultaneous push/pop policy");
        r.drive({{"POP",0},{"PUSH_DATA",100}});for(int i=0;i<3;++i)r.cycle();
        r.drive({{"POP",1},{"PUSH_DATA",101}});r.cycle();
        require(r.out("COUNT")==3&&r.out("DATA")==100&&r.out("OVERFLOW"),"Full simultaneous push/pop policy");
        r.drive({{"PUSH",0},{"POP",0},{"RST",1}});r.cycle();r.drive({{"RST",0}});
        require(!r.out("COUNT")&&!r.out("VALID")&&!r.out("OVERFLOW"),"FIFO reset retained queue/overflow");
    }
}
void counter(){for(bool compact:{false,true})for(size_t w:{4u,32u}){Rig r(circuit::families::clockedCounter(w),compact);for(unsigned i=1;i<=20;++i){r.cycle();require(r.out("COUNT")==(i&((uint64_t(1)<<w)-1)),"Counter increment/wrap");}
    r.drive({{"ENABLE",0}});auto saved=r.out("COUNT");r.cycle();require(r.out("COUNT")==saved,"Counter disable");}
    for(bool compact:{false,true}){
        Rig divided(circuit::families::clockedCounter(4),compact,{{"divisor","3"}});
        for(unsigned i=1;i<=12;++i){divided.cycle();require(divided.out("COUNT")==i/3,"Counter divisor parameter");}
    }
}
void port(){
    for(bool compact:{false,true}){

    Rig r(circuit::families::DeviceRegisterPort32,compact);
    r.drive({{"BACK_READ_DATA",0x12345678},{"BACK_STATUS",0},{"REQ_VALID",1},{"REQ_OP",0},{"REQ_ADDR",0},{"RSP_READY",0}});
    require(r.out("REQ_READY")&&!r.out("RSP_VALID"),"Register port request admission");
    r.cycle();require(r.out("RSP_VALID")&&r.out("READ_STROBE"),"Register port completion offer");
    r.drive({{"REQ_VALID",0}});r.cycle();r.drive({{"BACK_READ_DATA",0xabcdef},{"REQ_ADDR",4}});
    require(!r.out("REQ_READY")&&r.out("RSP_VALID")&&r.out("RSP_READ_DATA")==0x12345678,"Sampled register response not held");
    r.drive({{"RSP_READY",1}});r.cycle();r.drive({{"RSP_READY",0}});require(r.out("REQ_READY"),"Register response not released");
    require(r.access(true,0,7,1).first==6,"Partial command mask");
    require(r.access(true,0,7,0).first==0,"Zero command mask");require(r.access(false,1).first==1,"Register alignment");
    r.drive({{"REQ_VALID",1},{"REQ_ADDR",0},{"REQ_OP",4},{"RSP_READY",0}});r.cycle();r.drive({{"REQ_VALID",0}});
    require(r.out("RSP_VALID")&&r.out("RSP_STATUS")==4&&!r.out("READ_STROBE"),"Burst accepted by register port");
    r.drive({{"RSP_READY",1}});r.cycle();r.drive({{"RSP_READY",0},{"REQ_VALID",1}});
    r.unknown("REQ_OP",LogicValue::UNKNOWN);r.cycle();r.drive({{"REQ_VALID",0}});
    require(r.out("RSP_VALID")&&r.out("RSP_STATUS")==6&&!r.out("WRITE_STROBE"),"Unknown operation side effect");
    for(auto value:{LogicValue::UNKNOWN,LogicValue::HIGH_Z})for(const auto& field:{"REQ_ADDR","REQ_WRITE_MASK"}) {
        r.drive({{"RST",1},{"REQ_VALID",0}});r.cycle();r.drive({{"RST",0},{"REQ_VALID",1},{"REQ_OP",1},{"REQ_ADDR",0},{"REQ_WRITE_MASK",15},{"RSP_READY",0}});
        r.unknown(field,value);r.cycle();r.drive({{"REQ_VALID",0}});
        require(r.out("RSP_VALID")&&r.out("RSP_STATUS")==6&&!r.out("WRITE_STROBE"),"Unknown/high-Z command side effect");
    }
    r.drive({{"RST",1}});r.cycle();r.drive({{"RST",0}});
    require(!r.out("RSP_VALID")&&r.out("REQ_READY"),"Frontend reset retained response");
    }
}
void console(){
    for(size_t depth:{1u,3u,7u}) {
        Rig queue(circuit::families::ConsoleDevice32,true,{{"fifo_depth",std::to_string(depth)}});
        for(unsigned i=0;i<depth;++i)require(queue.access(true,0,'A'+i).first==0,"Configured console write");
        require(queue.access(false,8).second==depth&&queue.access(false,4).second==0,"Configured console full/count");
        queue.drive({{"TX_READY",1}});
        for(unsigned i=0;i<depth;++i){require(queue.out("TX_DATA")=='A'+i,"Configured console ordering");queue.cycle();}
        queue.drive({{"TX_READY",0}});require(!queue.out("TX_VALID")&&queue.access(false,8).second==0,"Configured console drain");
    }

    Rig r(circuit::families::ConsoleDevice32);
    for(unsigned i=0;i<4;++i)require(r.access(true,0,'A'+i).first==0,"Console write");
    require(r.out("TX_VALID")&&r.out("TX_DATA")=='A'&&r.access(false,4).second==0,"Console queue/full status");
    r.drive({{"REQ_VALID",1},{"REQ_OP",1},{"REQ_ADDR",0},{"REQ_WRITE_DATA",'Z'},{"REQ_WRITE_MASK",15},{"RSP_READY",1}});r.cycle();r.drive({{"REQ_VALID",0}});
    require(!r.out("REQ_READY")&&!r.out("RSP_VALID"),"Full console did not retain its accepted write");r.drive({{"TX_READY",1}});
    for(unsigned i=0;i<5;++i){require(r.out("TX_DATA")==uint32_t(i==4?'Z':'A'+i),"Console byte order");r.cycle();}
    r.drive({{"TX_READY",0}});require(!r.out("TX_VALID"),"Console queue did not drain");
    require(r.access(true,0,0x100).first==6,"Console reserved data accepted");

}
void timer(){

    Rig r(circuit::families::TimerDevice32);
    r.drive({{"REQ_VALID",1},{"REQ_OP",0},{"REQ_ADDR",0},{"RSP_READY",0}});r.cycle();r.drive({{"REQ_VALID",0}});
    auto before=r.out("RSP_READ_DATA");r.cycle();for(int i=0;i<4;++i)r.cycle();
    require(r.out("RSP_VALID")&&r.out("RSP_READ_DATA")==before,"Timer read changed under response backpressure");
    r.drive({{"RSP_READY",1}});r.cycle();r.drive({{"RSP_READY",0}});require(r.access(false,0).second>before,"Timer did not advance");
    require(r.access(false,4).second==1&&r.access(false,8).second==0,"Timer configuration");require(r.access(true,0,1).first==3,"Timer is not read-only");

}
void buttons(){
    for(size_t depth:{1u,3u,7u}) {
        Rig queue(circuit::families::ButtonDevice32,true,{{"fifo_depth",std::to_string(depth)}});
        const auto input=[&](unsigned id){queue.drive({{"INPUT_VALID",1},{"INPUT_BUTTON",id},{"INPUT_DOWN",1}});queue.cycle();queue.drive({{"INPUT_VALID",0}});};
        for(unsigned id=0;id<=depth;++id)input(id);
        require(queue.access(false,8).second==depth&&queue.access(false,4).second==256,"Configured button occupancy/head");
        require(queue.access(false,0).second==(1u<<(depth+1))-1&&(queue.access(false,12).second&2),"Configured overflow must retain levels");
        require(queue.access(true,16,1).first==0,"Configured button POP");input(31);
        require(queue.access(false,8).second==depth&&queue.access(false,4).second==(depth==1?287u:257u),"Configured button queue wrap");
        require(queue.access(true,20,1).first==0&&!(queue.access(false,12).second&2),"Configured button overflow clear");
    }
    Rig r(circuit::families::ButtonDevice32);
    auto event=[&](unsigned button,bool down){r.drive({{"INPUT_VALID",1},{"INPUT_BUTTON",button},{"INPUT_DOWN",down}});r.cycle();r.drive({{"INPUT_VALID",0}});};
    event(2,true);event(2,false);
    require(r.access(false,0).second==0&&r.access(false,8).second==2,"Button tap missing");
    auto head=r.access(false,4).second;require(head==258&&r.access(false,4).second==head,"Input head read popped event");
    require(r.access(true,16,1).first==0&&r.access(false,4).second==2,"Button POP/release");
    event(3,true);event(4,true);event(5,true);event(6,true);
    require((r.access(false,12).second&2)&&((r.access(false,0).second>>6)&1),"Button overflow lost level");
    require(r.access(true,20,1).first==0&&!(r.access(false,12).second&2),"Button overflow clear");
}
void pixels(){
    Rig r(circuit::families::IndexedPixelSelector32,false);
    uint32_t word=0xff031ce0;const std::array<uint32_t,4> expected{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};
    for(unsigned i=0;i<4;++i){r.drive({{"WORD",word},{"OFFSET",i}});require(r.out("ARGB")==expected[i]&&r.out("INDEX")==((word>>(8*i))&255),"Pixel order/palette");}
    for(uint32_t index=0;index<256;++index)for(unsigned lane=0;lane<4;++lane) {
        auto red=index>>5,green=(index>>2)&7,blue=index&3;
        red=(red<<5)|(red<<2)|(red>>1);green=(green<<5)|(green<<2)|(green>>1);blue=(blue<<6)|(blue<<4)|(blue<<2)|blue;
        r.drive({{"WORD",index*0x01010101u},{"OFFSET",lane}});
        require(r.out("INDEX")==index&&r.out("ARGB")== (0xff000000u|(red<<16)|(green<<8)|blue),"Full RGB332 palette/byte selection");
    }
}
void display(){
    Rig large(circuit::families::FramebufferDisplay32,true,{{"width","64"},{"height","48"}});
    require(large.access(false,0).second==64&&large.access(false,4).second==48,"Catalog display geometry parameters");
    auto storage=std::dynamic_pointer_cast<FramebufferDisplay32>(large.root);
    require(storage->memoryStorage().capacityBytes()==4096,"Display padded native storage");
    require(storage->memoryStorage().isHistoryRecordingEnabled(),"Recorded display must retain memory history by default");
    for(const auto& bad:std::vector<circuit::ParameterMap>{{{"width","0"}},{{"height","0"}},{{"width","4096"},{"height","65"}},{{"width","4294967296"}},{{"unknown","1"}}}) {
        bool rejected=false;
        try{(void)circuit::builtinComponentCatalog().createRoot(circuit::families::FramebufferDisplay32.request("INVALID",bad),compactProfile());}
        catch(const std::exception&){rejected=true;}
        require(rejected,"Invalid display parameters accepted");
    }
    Rig padded(circuit::families::FramebufferDisplay32,true,{{"width","4"},{"height","4"},{"storage_bytes","4096"}});
    require(std::dynamic_pointer_cast<FramebufferDisplay32>(padded.root)->memoryStorage().capacityBytes()==4096,"Explicit video storage capacity");
    require(padded.access(true,4092,0x12345678,15,"FB_").first==0 && padded.access(false,4092,0,15,"FB_").second==0x12345678,"Unshown video padding access");
    for(const auto& bad:std::vector<circuit::ParameterMap>{{{"storage_bytes","3"}},{{"storage_bytes","8"}},{{"storage_bytes","524288"}}}) {
        bool rejected=false;
        try{(void)circuit::builtinComponentCatalog().createRoot(circuit::families::FramebufferDisplay32.request("INVALID",bad),compactProfile());}
        catch(const std::exception&){rejected=true;}
        require(rejected,"Invalid framebuffer storage size accepted");
    }
    for(const auto& shape:std::vector<std::pair<size_t,size_t>>{{128,96},{320,200},{97,65}}) {
        const auto [width,height]=shape;
        Rig larger(circuit::families::FramebufferDisplay32,true,{{"width",std::to_string(width)},{"height",std::to_string(height)}});
        auto display=std::dynamic_pointer_cast<FramebufferDisplay32>(larger.root);
        const auto capacity=display->configuration().capacityBytes();
        require(circuit::visualizer::componentLayoutVariant(display).find("width="+std::to_string(width)+",height="+std::to_string(height))==0,"Display geometry layout key");
        require(larger.access(false,0).second==width&&larger.access(false,4).second==height&&larger.access(false,8).second==width,"Larger framebuffer geometry/stride");
        require(display->memoryStorage().capacityBytes()==capacity,"Larger framebuffer capacity");
        const uint32_t address=uint32_t((width*height-1)&~size_t{3});
        require(larger.access(true,address,0xff031ce0,15,"FB_").first==0&&larger.access(false,address,0,15,"FB_").second==0xff031ce0,"Larger framebuffer last visible word");
        require(larger.access(true,capacity-4,0x12345678,15,"FB_").first==0&&larger.access(false,capacity-4,0,15,"FB_").second==0x12345678,"Larger framebuffer backing boundary");
        require(larger.access(false,capacity,0,15,"FB_").first!=0,"Framebuffer accepted beyond capacity");
    }
    Rig r(circuit::families::FramebufferDisplay32);
    require(circuit::visualizer::componentLayoutVariant(r.root).empty(),"Default framebuffer layout key changed");
    require(r.access(false,0).second==4&&r.access(false,4).second==4&&r.access(false,8).second==4,"Framebuffer geometry");
    require(r.access(true,0,1).first==3&&r.access(true,20,3).first==6,"Framebuffer register validation");
    require(r.access(true,0,0x031ce0ff,15,"FB_").first==0,"Video write");
    require(r.access(false,0,0,15,"FB_").second==0x031ce0ff,"Video read");
    require(r.access(true,0,0xaa,1,"FB_").first==0&&r.access(false,0,0,15,"FB_").second==0x031ce0aa,"Video byte lane");
    require(r.access(true,20,1).first==0&&r.out("FRAME_SEQUENCE")==1,"PRESENT sequence");
    require(r.access(true,20,1,0).first==0&&r.out("FRAME_SEQUENCE")==1,"Zero mask presented");
    r.drive({{"RST",1},{"FB_RST",1}});r.cycle();r.drive({{"RST",0},{"FB_RST",0}});
    require(r.out("FRAME_SEQUENCE")==0&&r.access(false,0,0,15,"FB_").second==0x031ce0aa,"Display reset must retain drawing storage");
}
void deviceViews(){
    {
        Rig router(circuit::families::AddressRouter32,false,{{"video_bytes","16384"}});
        require(circuit::visualizer::componentLayoutVariant(router.root)=="video_bytes=16384","Router aperture layout variant");
    }
    using circuit::visualizer::deviceStateJson;
    for(bool compact:{false,true}) {
        Rig r(circuit::families::FramebufferDisplay32,compact,{},true);
        r.access(true,0,0xff031ce0,15,"FB_");const auto first=r.now;
        const auto before=deviceStateJson(r.root,r.sim,1,first);
        require(before.find("\"pixels\":\"e01c03ff")!=std::string::npos,"Device display preview pixel order");
        r.access(true,0,0x11223344,15,"FB_");
        const auto count=r.sim.getPerformanceCounters();
        require(deviceStateJson(r.root,r.sim,1,first)==before,"Device preview did not rewind pixel history");
        require(deviceStateJson(r.root,r.sim,2,r.now).find("\"pixels\":\"44332211")!=std::string::npos,"Device display preview current pixels");
        require(r.sim.getPerformanceCounters().processed_events==count.processed_events&&r.sim.getPerformanceCounters().scheduled_events==count.scheduled_events,"Device inspection scheduled or processed events");
        r.sim.setHistoryRecordingEnabled(false);
        require(deviceStateJson(r.root,r.sim,0,r.now).find("\"supported\":false")!=std::string::npos,"Device observer fabricated disabled history");
    }
    Rig keyboard(circuit::families::ButtonDevice32,true,{},true);
    keyboard.drive({{"INPUT_VALID",1},{"INPUT_BUTTON",31},{"INPUT_DOWN",1}});keyboard.cycle();
    const auto pressed=keyboard.now;
    keyboard.drive({{"INPUT_DOWN",0}});keyboard.cycle();keyboard.drive({{"INPUT_VALID",0}});
    require(deviceStateJson(keyboard.root,keyboard.sim,0,pressed).find("\"levels\":\"10000000000000000000000000000000\"")!=std::string::npos,"Keyboard preview lost held input");
    const auto released=deviceStateJson(keyboard.root,keyboard.sim,1,keyboard.now);
    require(released.find("\"levels\":\"00000000000000000000000000000000\"")!=std::string::npos&&released.find("\"count\":\"010\"")!=std::string::npos,"Keyboard preview lost released level or tap queue");
}
void routing(const circuit::ComponentFamily&f,bool guard){
    Rig r(f);const std::string target=guard?"MEM_":"CONSOLE_";
    uint32_t address=guard?4:0x10000004;
    r.drive({{"REQ_VALID",1},{"REQ_OP",0},{"REQ_ADDR",address},{"RSP_READY",0}});
    r.cycle();r.drive({{"REQ_VALID",0}});
    require(r.out(target+"REQ_VALID")&&r.out(target+"REQ_ADDR")==4,"Router decode/translation");
    r.drive({{target+"REQ_READY",1}});r.cycle();r.drive({{target+"REQ_READY",0},{target+"RSP_VALID",1},{target+"RSP_STATUS",0},{target+"RSP_READ_DATA",0x1234}});
    require(r.out("RSP_VALID")&&r.out("RSP_READ_DATA")==0x1234&&!r.out(target+"RSP_READY"),"Router held response ownership");
    r.drive({{"RSP_READY",1}});r.cycle();r.drive({{target+"RSP_VALID",0},{"RSP_READY",0}});
    require(r.out("REQ_READY"),"Router did not release transaction");
    r.drive({{"REQ_VALID",1},{"REQ_ADDR",guard?0x10000000u:0x30000000u},{"REQ_OP",0}});
    r.cycle();r.drive({{"REQ_VALID",0}});
    require(r.out("RSP_VALID")&&r.out("RSP_STATUS")==2,"Unmapped/fetch device address did not fault");
    require(!r.out(target+"REQ_VALID"),"Invalid request reached device");
    r.drive({{"RSP_READY",1}});r.cycle();r.drive({{"RSP_READY",0},{"REQ_VALID",1},{"REQ_ADDR",0},{"REQ_OP",4}});
    r.cycle();r.drive({{"REQ_VALID",0}});require(r.out("RSP_VALID")&&r.out("RSP_STATUS")==4,"Router forwarded burst");
}
class TableOnlyEcho : public BasicComponent {
public:
    explicit TableOnlyEcho(std::string n):BasicComponent(std::move(n),1,[](IOComponent*p){p->addPin("IN",PinType::INPUT);p->addPin("OUT",PinType::OUTPUT);}){}
    const char* getTypeName()const override{return "TableOnlyEchoFixture";}
    void evaluate(SimulationTime t,Simulator&sim)override{_updateOutputWire(sim,"OUT",getInputPin("IN")->getValue(),t);}
};
void tableOnlyTiming(){
    auto part=Component::create<TableOnlyEcho>("DUT");circuit::PartDelays delays;
    delays.arcs.push_back({"IN","OUT",{3,7}});part->setDelays(delays);
    ComponentBuilder b(part);auto input=b.addNewWire("INPUT",nullptr,{part->getInputPin("IN")});
    auto output=b.addNewWire("OUTPUT",part->getOutputPin("OUT"),{});
    Simulator sim;SimulationTest::scheduleInitialEventsForTree(part,sim);
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<1>>(0,input,0));sim.advanceAndRecord(10);
    require(output->getValue()==0,"Table-only initial value");
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<1>>(20,input,1));sim.advanceAndRecord(26);
    require(output->getValue()==0,"Table-only result published early");sim.advanceAndRecord(27);
    require(output->getValue()==1,"Table-only maximum delay not used");
}

void semantics(){
    using namespace rv32im;const uint32_t raw=0x02000033;
    for(unsigned op=0;op<8;++op){require(decode(raw|(op<<12)).m==MOperation(op),"M decode");require(!rv32i::RV32IDecoder::decode(raw|(op<<12)).legal,"Existing RV32I accepts M");}
    require(!decode(0x04000033).legal(),"Reserved funct7 accepted");
    require(referenceResult(MOperation::DIV,0x80000000,0xffffffff)==0x80000000&&referenceResult(MOperation::REM,0x80000000,0xffffffff)==0,"Signed overflow reference");
    for(unsigned op=4;op<8;++op)require(referenceResult(MOperation(op),0x87654321,0)==(op&2?0x87654321:0xffffffff),"Divide-zero reference");
    rv32i::RV32IFunctionalMemory m;rv32i::RV32IState s;m.loadWords(0,{raw|(3<<7)|(1<<15)|(2<<20)});s.x[1]=13;s.x[2]=17;
    auto t=step(s,m,m);require(t.m==MOperation::MUL&&s.x[3]==221&&s.pc==4&&s.instruction_count==1,"M oracle commit");
}

uint32_t mword(unsigned op,unsigned rd,unsigned a,unsigned b){return 0x02000033u|(op<<12)|(rd<<7)|(a<<15)|(b<<20);}
void mdecode(){
    Rig r(circuit::families::RV32IMDecodeControl);
    for(unsigned op=0;op<8;++op){r.drive({{"INSTRUCTION",mword(op,3,1,2)}});
        require(r.out("LEGAL")&&r.out("REG_WRITE")&&r.out("USES_RS1")&&r.out("USES_RS2")&&r.out("WRITEBACK_SEL")==1,"M decode control");
        require(!r.out("MEM_READ")&&!r.out("MEM_WRITE")&&!r.out("TRAP_REQUEST")&&!r.out("HALT_REQUEST"),"M decode side effects");
        require(r.out("RS1_ADDR")==1&&r.out("RS2_ADDR")==2&&r.out("RD_ADDR")==3,"M decode addresses");}
    r.drive({{"INSTRUCTION",0x04000033}});require(!r.out("LEGAL"),"M decoder accepts reserved funct7");
}
void mdecodeStage(){
    Rig r(circuit::families::RV32IMDecodeStage);
    r.drive({{"WB_VALID",1},{"WB_REG_WRITE",1},{"WB_REGISTER_WRITE",1},{"WB_RD",1},{"WB_VALUE",123}});r.cycle();
    r.drive({{"WB_VALID",0},{"WB_REG_WRITE",0},{"WB_REGISTER_WRITE",0},{"IN_VALID",1},{"IN_INSTRUCTION",mword(0,3,1,2)}});
    require(r.out("OUT_RS1_VALUE")==123&&r.out("OUT_RS2_VALUE")==0&&!r.out("TERMINAL"),"M decode-stage register path");
}
void mexecute(){
    Rig dec(circuit::families::RV32IMDecodeStage);dec.drive({{"IN_VALID",1},{"IN_INSTRUCTION",mword(0,3,1,2)}});
    Rig r(circuit::families::RV32IMExecuteStage);
    r.drive({{"IN_VALID",1},{"IN_CONTROL",dec.out("OUT_CONTROL")},{"IN_INSTRUCTION",mword(0,3,1,2)},
             {"IN_RS1_VALUE",13},{"IN_RS2_VALUE",17},{"IN_REGISTER_ADDRESSES",1|(2<<5)|(3<<10)}});
    require(r.out("EXECUTION_WAIT"),"M execute did not hold");
    for(int i=0;i<33;++i)r.cycle();
    require(!r.out("EXECUTION_WAIT")&&r.out("OUT_ALU_RESULT")==221&&!r.out("PRETERMINAL"),"M execute result");
    r.drive({{"TAKE",1}});r.cycle();r.drive({{"TAKE",0},{"IN_VALID",0}});require(!r.out("EXECUTION_WAIT"),"Invalid M packet held execution");
    std::shared_ptr<IOComponent> slot;
    for(const auto& child:r.root->getChildren())if(child->getTypeName()==std::string("MulDivUnit32"))slot=std::dynamic_pointer_cast<IOComponent>(child);
    require(bool(slot),"Execute fixture lacks arithmetic slot");
    r.drive({{"IN_VALID",1},{"IN_RS1_VALUE",99},{"EX_MEM_VALID",1},{"EX_MEM_REG_WRITE",1},{"EX_MEM_RD",1},{"EX_MEM_RESULT_READY",0},{"EX_MEM_VALUE",13}});
    for(int i=0;i<5;++i)r.cycle();
    require(r.out("EXECUTION_WAIT")&&slot->getOutputPin("BUSY")->getValue()==LogicValue::LOW,"M launched with an unavailable older load operand");
    r.drive({{"EX_MEM_RESULT_READY",1}});for(int i=0;i<33;++i)r.cycle();
    require(!r.out("EXECUTION_WAIT")&&r.out("OUT_ALU_RESULT")==221,"M did not launch with the completed forwarded load");
    r.drive({{"TAKE",1}});r.cycle();r.drive({{"TAKE",0},{"IN_FETCH_STATUS",2}});
    for(int i=0;i<3;++i)r.cycle();
    require(r.out("PRETERMINAL")&&!r.out("EXECUTION_WAIT")&&slot->getOutputPin("BUSY")->getValue()==LogicValue::LOW,"Faulted M packet started arithmetic");
}
void mcoordinate(){
    Rig r(circuit::families::RV32IMPipelineCoordinator);
    r.drive({{"IMEM_READY",1},{"DMEM_READY",1}});require(r.out("COMMIT_ENABLE")&&r.out("EX_MEM_WRITE"),"Coordinator normal step");
    r.drive({{"EXECUTION_WAIT",1}});require(r.out("EXECUTION_STALL")&&!r.out("COMMIT_ENABLE")&&!r.out("EX_MEM_WRITE")&&!r.out("MEM_WB_WRITE"),"M wait repeats old work");
    r.drive({{"MEMORY_FAULT",1}});require(!r.out("EXECUTION_STALL")&&r.out("EX_MEM_FLUSH"),"Older fault did not override M wait");
}
std::vector<uint64_t> coreProgram(bool fault, bool compiled=false, bool reset_in_flight=false, bool compact_children=true, bool short_program=false,circuit::ParameterMap parameters={}){
    std::vector<uint32_t> program{0xff900093,0x00300113};
    if(fault){program.push_back(0x40002583);program.push_back(mword(0,12,1,2));}
    else{
        program.push_back(mword(0,3,1,2));program.push_back(mword(4,4,3,2));program.push_back(mword(6,5,3,2));
        for(unsigned op:{1u,3u,2u,5u,7u})program.push_back(mword(op,6+op,1,2));
        program.push_back(0x10302023); // sw x3,256(x0)
        program.push_back(0x10002583); // lw x11,256(x0)
        program.push_back(mword(0,12,11,2));
        program.push_back(0x00000463); // beq x0,x0,+8
        program.push_back(mword(0,13,1,2)); // wrong path
        program.push_back(mword(0,0,1,2));
        program.push_back(mword(0,1,1,2)); // rd=rs1
        program.push_back(mword(4,2,1,2)); // rd=rs2, back-to-back dependency
        program.push_back(0xff900813); // addi x16,x0,-7
        program.push_back(0x01010463); // beq x2,x16,+8, M-to-branch
        program.push_back(mword(0,17,1,2)); // wrong path
        program.push_back(mword(4,14,1,0));program.push_back(mword(6,15,1,0));
    }
    if(short_program) {
        program={0xff900093,0x00300113};
        for(unsigned op=0;op<8;++op)program.push_back(mword(op,3+op,1,2));
        program.push_back(0x10302023);program.push_back(0x10002583);program.push_back(mword(0,12,11,2));
    }
    program.push_back(0x00100073);
    const size_t capacity=compiled?4096:1024;
    rv32i::RV32IFunctionalMemory instructions(capacity),data(capacity),actual(capacity);
    if(compiled) {
        auto elf=rv32i::RV32IElfImage::fromBytes(rv32imSmokeElfBytes());
        elf.requireFitsMemory(capacity);require(elf.entryPoint()==0,"M smoke reset vector");
        for(const auto& segment:elf.segments()) {
            instructions.loadBytes(segment.address,segment.bytes);
            data.loadBytes(segment.address,segment.bytes);actual.loadBytes(segment.address,segment.bytes);
        }
    } else instructions.loadWords(0,program);
    rv32i::RV32IState reference;
    Rig r(circuit::families::RV32IMFiveStageCore,compact_children,std::move(parameters));
    struct Pending {bool valid=false;unsigned wait=0;uint32_t status=0,data=0;};
    Pending ip,dp;size_t commits=0,writes=0,stalls=0;bool did_reset=false;std::vector<uint64_t> protocol_trace;
    auto view=std::dynamic_pointer_cast<RV32IRetirementView>(r.root);
    auto state=std::dynamic_pointer_cast<RV32IPipelineStateView>(r.root);
    BasicComponent::resetSetupViolations();
    for(unsigned cycle=0;cycle<3000;++cycle){
        const bool resetting=reset_in_flight&&!did_reset&&stalls>=4&&r.out("EXECUTION_STALL");
        if(resetting){require(writes==0,"Reset test passed the first M instruction");ip={};dp={};}
        const bool ir=!ip.valid&&(cycle%5!=0),dr=!dp.valid&&(cycle%7!=0);
        // Fixed 100-tick unit clock, binary responses one tick after falling
        // edges. RV32IMTimingTest independently certifies this boundary.
        const auto base=r.now;
        const std::map<std::string,uint64_t> external{{"RST",resetting},{"ENABLE",cycle%41<37},{"IMEM_REQ_READY",ir},{"DMEM_REQ_READY",dr},{"IMEM_RSP_VALID",ip.valid&&!ip.wait},
                 {"IMEM_RSP_STATUS",ip.status},{"IMEM_RSP_READ_DATA",ip.data},{"DMEM_RSP_VALID",dp.valid&&!dp.wait},
                 {"DMEM_RSP_STATUS",dp.status},{"DMEM_RSP_READ_DATA",dp.data}};
        for(const auto& [name,value]:external)r.sim.scheduleEvent(makeTestWireUpdate(base+1,r.inputs.at(name),TestValue(value)));
        r.sim.advanceAndRecord(r.now=base+49);
        bool ia=r.out("IMEM_REQ_VALID")&&ir,da=r.out("DMEM_REQ_VALID")&&dr;
        bool it=ip.valid&&!ip.wait&&r.out("IMEM_RSP_READY"),dt=dp.valid&&!dp.wait&&r.out("DMEM_RSP_READY");
        auto iaddr=r.out("IMEM_REQ_ADDR"),daddr=r.out("DMEM_REQ_ADDR"),dop=r.out("DMEM_REQ_OP"),mask=r.out("DMEM_REQ_WRITE_MASK"),value=r.out("DMEM_REQ_WRITE_DATA");
        bool commit=r.out("COMMIT_VALID");
        auto iv=r.out("IMEM_REQ_VALID"),dv=r.out("DMEM_REQ_VALID");
        protocol_trace.insert(protocol_trace.end(),{iv,dv,r.out("IMEM_RSP_READY"),r.out("DMEM_RSP_READY"),commit,r.out("EXECUTION_STALL"),
            iv?iaddr:0,dv?daddr:0,dv?dop:0,dv&&dop==1?mask:0,dv&&dop==1?value:0});
        if(commit){auto observed=view->pendingRetirement();auto expected=rv32im::step(reference,instructions,data);
            require(observed.pc==expected.base.pc_before&&observed.instruction==expected.base.raw_instruction,"M core retirement mismatch at "+std::to_string(observed.pc));++commits;}
        stalls+=r.out("EXECUTION_STALL");
        r.sim.scheduleEvent(makeTestWireUpdate(base+50,r.inputs.at("CLK"),TestValue(uint64_t(1))));
        r.sim.scheduleEvent(makeTestWireUpdate(base+100,r.inputs.at("CLK"),TestValue(uint64_t(0))));
        r.sim.advanceAndRecord(r.now=base+100);
        if(ip.wait)--ip.wait;if(dp.wait)--dp.wait;if(it)ip.valid=false;if(dt)dp.valid=false;
        if(ia){ip={true,1,0,0};if((iaddr&3)||!instructions.canAccess(iaddr,4))ip.status=2;else ip.data=instructions.readU32(iaddr);}
        if(da){dp={true,17,0,0};if((daddr&3)||!actual.canAccess(daddr,4))dp.status=2;
            else if(dop==0)dp.data=actual.readU32(daddr);
            else{++writes;for(unsigned byte=0;byte<4;++byte)if((mask>>byte)&1)actual.writeU8(daddr+byte,value>>(8*byte));}}
        if(commit){auto observed=state->snapshotPipelineState(reference.pc).toKnownState();
            require(observed.x==reference.x,"M core register mismatch after PC "+std::to_string(reference.pc));
            require(observed.halted==reference.halted&&observed.trapped==reference.trapped&&observed.trap_cause==reference.trap_cause,"M core terminal mismatch");}
        if(resetting){reference={};ip={};dp={};commits=writes=stalls=0;did_reset=true;continue;}
        if(reference.halted||reference.trapped){require(commits>2,"Core stopped before program");
            require(!reset_in_flight||did_reset,"Core reset case did not reset");
            require(actual.readBytes(0,capacity)==data.readBytes(0,capacity),"M core final data mismatch");
            require(BasicComponent::setupViolations()==0,"M core setup violation");
            if(compiled)require(reference.halted&&!reference.trapped&&reference.x[10]==0&&writes>=8,"Compiled C self-check failed");
            else if(fault)require(reference.trapped&&reference.x[12]==0&&writes==0,"Older fault failed to cancel M");
            else require(reference.halted&&!reference.trapped&&writes==1&&reference.x[13]==0&&reference.x[17]==0&&stalls>0,"M core program/flush/store result");
            return protocol_trace;}
    }throw std::runtime_error("M core program timed out");
}

class CheckCase : public StandaloneVerificationTest {
    std::string name_;std::function<void()> check_;
public:CheckCase(std::string n,std::function<void()>f):name_(std::move(n)),check_(std::move(f)){}
    std::string getTestName()const override{return name_;}
    void verifyResults()override{BasicComponent::resetSetupViolations();check_();require(BasicComponent::setupViolations()==0,"Component check observed setup violations");}
};
}
void appendRV32IMPhase3Tests(std::vector<TestRegistryEntry>&entries) {
    auto add=[&](std::string name,std::function<void()> check){
        entries.push_back({name,name,RegisteredTestKind::Integration,{}, {},{"simulation","rv32im","phase3"},
          [name,check]{return std::make_unique<CheckCase>(name,check);},false});
    };
    add("MultiplyStepTest",[]{steps(false);});add("DivideStepTest",[]{steps(true);});
    add("Multiplier32Test",[]{arithmetic(circuit::families::Multiplier32,false,false);});
    add("Divider32Test",[]{arithmetic(circuit::families::Divider32,false,true);});
    add("MulDivUnit32Test",[]{arithmetic(circuit::families::MulDivUnit32,true,false);});
    add("TableOnlyDelayPublicationTest",tableOnlyTiming);
    add("RV32IMSemanticsTest",semantics);
    add("RV32IMDecodeControlTest",mdecode);add("RV32IMDecodeStageTest",mdecodeStage);
    add("RV32IMExecuteStageTest",mexecute);add("RV32IMPipelineCoordinatorTest",mcoordinate);
    add("RV32IMFiveStageCoreTest",[]{
        coreProgram(false);coreProgram(true);coreProgram(false,false,true);
        const auto gates=coreProgram(false,false,false,false,true);
        const auto compact=coreProgram(false,false,false,true,true);
        require(gates==compact,"Structural and compact-child core protocol/retirement timing differs");
        for(const char* prediction:{"taken","backward-taken","one-bit","two-bit"})
            coreProgram(false,false,false,true,false,{{"prediction",prediction},{"prediction_entries","4"}});
    });
    add("RV32IMCompiledProgramTest",[]{coreProgram(false,true);});
    add("ClockedCounterTest",counter);add("SynchronousFifoTest",fifo);
    add("DeviceRegisterPort32Test",port);add("BusErrorTarget32Test",[]{Rig r(circuit::families::BusErrorTarget32);require(r.access(false,0).first==2,"Bus error target");});
    add("AddressRouter32Test",[]{routing(circuit::families::AddressRouter32,false);});
    add("InstructionAccessGuard32Test",[]{routing(circuit::families::InstructionAccessGuard32,true);});
    add("ConsoleDevice32Test",console);add("TimerDevice32Test",timer);add("ButtonDevice32Test",buttons);
    add("IndexedPixelSelector32Test",pixels);add("FramebufferDisplay32Test",display);
    add("DeviceInspectorTest",deviceViews);
    appendRV32IMPhase3Scenarios(entries);
}
