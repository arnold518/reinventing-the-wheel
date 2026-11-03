#define main primitiveTimingProbeMain
#include "TimingTwinProbe.cpp"
#undef main
#include "modules/basic/DFlipFlop.hpp"
#include "modules/utility/Rewire.hpp"

namespace {
void repeatCalls(const Fixture& f,bool stateful) {
    const auto first=f.twin->start(),second=f.twin->start();
    for(unsigned pattern=0;pattern<256;++pattern) {
        const long long now=100000+pattern*100000LL;
        std::vector<std::vector<L>> values;
        std::vector<std::vector<long long>> changed;
        for(const auto&[name,width]:f.twin->inputs) {
            values.emplace_back(width);changed.emplace_back(width,now-10000);
            for(size_t bit=0;bit<width;++bit)
                values.back()[bit]=L((pattern/4+bit)%4);
            if(name=="CLK") {
                constexpr unsigned clocks[]={0,1,0,0,1,2,2,0,1,3,3,0};
                values.back()[0]=L(clocks[pattern%12]);changed.back()[0]=now;
            }
            if(name=="RST") values.back()[0]=L(pattern%17==0?1:pattern%31==0?2:0);
            if(name=="ENABLE")values.back()[0]=L(pattern%4);
        }
        std::vector<long long> firstTimes;
        std::vector<std::vector<uint8_t>> firstValues;
        bool firstAnswer=false;
        for(unsigned repeat=0;repeat<3;++repeat) {
            std::vector<long long> times;
            std::vector<std::vector<uint8_t>> result;
            const bool answered=f.twin->settle(first.get(),now,values,changed,times,result);
            if(!repeat){firstTimes=times;firstValues=result;firstAnswer=answered;}
            else if(!stateful)require(times==firstTimes&&result==firstValues&&answered==firstAnswer,
                                     "Repeated pure timing input changed its answer");
            mix(answered);for(auto t:times)mix(uint64_t(t));
            for(const auto& word:result)for(auto bit:word)mix(bit);
            // Interleave a second owner's state and mutate returned buffers.
            // The first owner's cached answer must remain independent.
            auto other=values;for(auto& word:other)for(auto& bit:word)bit=L((unsigned(bit)+1)%4);
            f.twin->settle(second.get(),now,other,changed,times,result);
            for(auto& word:result)for(auto& bit:word)bit=99;
            ++cases;
        }
    }
}
Fixture clockEcho() {
    auto f=fixture<DFlipFlop>();ComponentBuilder builder(f.root);
    auto echo=builder.addNewComponent<Rewire>("CLOCK_ECHO",std::vector<Rewire::WireSpec>{{"IN",1}},
        std::vector<Rewire::WireSpec>{{"OUT",1}},identity_mapping("IN",0,1,"OUT"));
    auto clock=f.root->getInputPin<1>("CLK")->getInternalWire();
    require(bool(clock),"Root clock has no internal wire");
    clock->addSinkPin(echo->getInputPin<1>("IN"));echo->getInputPin<1>("IN")->connectExternal(clock);
    auto output=f.root->addPin<1>("CLK_ECHO",PinType::OUTPUT);
    builder.addNewWire("CLK_ECHO",echo->getOutputPin<1>("OUT"),{output});
    std::string why;f.twin=circuit::analysis::buildDelayTwin(f.root,&why);
    if(!f.twin)throw std::runtime_error("Cannot build clock-echo twin: "+why);
    return f;
}
}
int memoTimingProbeMain() {
    if(primitiveTimingProbeMain())return 1;
    try{
        for(const auto& profile:circuit::timing::timingProfileNames()){
            circuit::timing::setActiveTimingProfile(profile);
            repeatCalls(fixture<circuit::memory_device::MemoryOutputBuffer>(8u),false);
            repeatCalls(fixture<circuit::memory_device::StaticReadColumn>(4u),false);
            repeatCalls(fixture<ProbeSelectivePrimitive>(),false);
            repeatCalls(fixture<DFlipFlop>(),true);
            repeatCalls(clockEcho(),true);
            std::cout<<profile<<" repeated/owner/stateful cases="<<cases<<" digest="<<digest<<'\n';
        }
        std::cout<<"PASS repeated exact arguments, independent owner states, output ownership, unknown clocks/reset and stateful delegation\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
#ifndef CIRCUITSIM_MEMO_PROBE_NO_MAIN
int main(){return memoTimingProbeMain();}
#endif
