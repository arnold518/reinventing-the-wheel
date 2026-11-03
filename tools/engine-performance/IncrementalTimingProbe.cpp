#define CIRCUITSIM_MEMO_PROBE_NO_MAIN
#include "MemoTimingProbe.cpp"
#undef CIRCUITSIM_MEMO_PROBE_NO_MAIN
#include "components/StructuralNetBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"

namespace {
Fixture feedbackNetwork(bool leaf) {
    Fixture f;f.root=Component::create<IOComponent>(leaf?"FALLBACK_NETWORK":"GATE_NETWORK");
    ComponentBuilder b(f.root);circuit::detail::StructuralWiring wires(b);
    auto data=f.root->addPin<8>("D",PinType::INPUT);
    auto clock=f.root->addPin<1>("CLK",PinType::INPUT);
    auto reset=f.root->addPin<1>("RST",PinType::INPUT);
    auto enable=f.root->addPin<1>("WE",PinType::INPUT);
    auto q=f.root->addPin<8>("Q",PinType::OUTPUT);
    auto next=f.root->addPin<8>("NEXT",PinType::OUTPUT);
    auto echo=f.root->addPin<1>("CLK_ECHO",PinType::OUTPUT);
    auto split=b.addNewComponent<BitSplitter<8>>("DATA_BITS");
    auto joined=b.addNewComponent<BitJoiner<8>>("Q_BITS");
    auto joined_next=b.addNewComponent<BitJoiner<8>>("NEXT_BITS");
    wires.connect(data,split->getInputPin<8>("IN"));
    wires.connect(joined->getOutputPin<8>("OUT"),q);
    wires.connect(joined_next->getOutputPin<8>("OUT"),next);
    auto enable_bar=wires.invert("NOT_WE",enable);
    auto pass=b.addNewComponent<Rewire>("CLK_PASS",std::vector<Rewire::WireSpec>{{"IN",1}},
        std::vector<Rewire::WireSpec>{{"OUT",1}},identity_mapping("IN",0,1,"OUT"));
    wires.connect(clock,pass->getInputPin<1>("IN"));wires.connect(pass->getOutputPin<1>("OUT"),echo);
    for(size_t bit=0;bit<8;++bit){
        const auto suffix=std::to_string(bit);
        auto ff=b.addNewComponent<DFlipFlop>("FF_"+suffix);
        wires.connect(clock,ff->getInputPin<1>("CLK"));wires.connect(reset,ff->getInputPin<1>("RST"));
        const auto stored=ff->getOutputPin<1>("Q");
        const auto toggled=wires.binary<XORGate>("XOR_"+suffix,stored,split->getOutputPin<1>("OUT_"+suffix));
        const auto write=wires.binary<ANDGate>("WRITE_"+suffix,toggled,enable);
        const auto hold=wires.binary<ANDGate>("HOLD_"+suffix,stored,enable_bar);
        const auto selected=wires.binary<ORGate>("SELECT_"+suffix,write,hold);
        wires.connect(selected,{ff->getInputPin<1>("D"),joined_next->getInputPin<1>("IN_"+suffix)});
        wires.connect(stored,joined->getInputPin<1>("IN_"+suffix));
    }
    if(leaf){
        auto buffer=b.addNewComponent<circuit::memory_device::MemoryOutputBuffer>("BUFFER",8u);
        auto gated=f.root->addPin<8>("GATED",PinType::OUTPUT);
        wires.connect(joined->getOutputPin<8>("OUT"),buffer->getInputPinDynamic("DATA"));
        wires.connect(enable,buffer->getInputPin<1>("ENABLE"));
        wires.connect(buffer->getOutputPinDynamic("OUT"),gated);
    }
    wires.finish();std::string why;f.twin=circuit::analysis::buildDelayTwin(f.root,&why);
    if(!f.twin)throw std::runtime_error("Cannot build feedback twin: "+why);
    return f;
}

uint8_t and4(uint8_t a,uint8_t b){return a==0||b==0?0:a==1&&b==1?1:2;}
uint8_t or4(uint8_t a,uint8_t b){return a==1||b==1?1:a==0&&b==0?0:2;}
uint8_t xor4(uint8_t a,uint8_t b){return a<2&&b<2?uint8_t(a^b):2;}
void timeline(const Fixture& f) {
    struct Owner {
        std::unique_ptr<circuit::DelayTwin::State> state;
        std::vector<std::vector<L>> values;
        std::vector<std::vector<long long>> changed;
        std::vector<long long> times;
        std::vector<std::vector<uint8_t>> outputs;
    } owners[2];
    for(auto& owner:owners){
        owner.state=f.twin->start();
        for(const auto&[name,width]:f.twin->inputs){owner.values.emplace_back(width,L::LOW);owner.changed.emplace_back(width,-1);}
    }
    uint64_t random=123456789;
    for(unsigned step=0;step<4096;++step){
        auto& owner=owners[step%2];const long long now=1000000+step*100000LL;
        if(step%257==0)owner.state=f.twin->start();
        for(size_t pin=0;pin<f.twin->inputs.size();++pin){
            const auto& name=f.twin->inputs[pin].first;
            for(size_t bit=0;bit<owner.values[pin].size();++bit){
                auto next=owner.values[pin][bit];
                random=random*6364136223846793005ULL+1442695040888963407ULL;
                if(name=="D"&&(step+bit)%7==0)next=L((random>>32)%(step%5==0?4:2));
                if(name=="CLK"){
                    constexpr unsigned clocks[]={0,0,1,1,0,2,2,0,1,3,3,0};next=L(clocks[(step/2)%12]);
                }
                if(name=="WE")next=L(step%11==0?2:step%19==0?3:(step/8)%2);
                if(name=="RST")next=L(step%97==0?1:step%131==0?2:0);
                const bool arrival_only=name=="D"&&step%23==0&&bit==step%8;
                if(next!=owner.values[pin][bit]||arrival_only)owner.changed[pin][bit]=now;
                owner.values[pin][bit]=next;
            }
        }
        for(unsigned repeat=0;repeat<(step%7==0?3u:1u);++repeat){
            const bool answered=f.twin->settle(owner.state.get(),now,owner.values,owner.changed,owner.times,owner.outputs);
            mix(answered);for(auto time:owner.times)mix(uint64_t(time));
            for(const auto& word:owner.outputs)for(auto value:word)mix(value);
            // Independent combinational truth from the final exposed Q bits.
            size_t qi=0,ni=0,di=0,wi=0;
            for(size_t i=0;i<f.twin->outputs.size();++i){if(f.twin->outputs[i].first=="Q")qi=i;if(f.twin->outputs[i].first=="NEXT")ni=i;}
            for(size_t i=0;i<f.twin->inputs.size();++i){if(f.twin->inputs[i].first=="D")di=i;if(f.twin->inputs[i].first=="WE")wi=i;}
            if(!owner.outputs.empty())for(size_t bit=0;bit<8;++bit){
                const auto held=owner.outputs[qi][bit],data=code(owner.values[di][bit]),we=code(owner.values[wi][0]);
                const auto expected=or4(and4(xor4(held,data),we),and4(held,we<2?uint8_t(1-we):2));
                require(owner.outputs[ni][bit]==expected,"Feedback network differs from independent gate truth");
            }
            ++cases;
        }
    }
}
}

int main(){
    if(memoTimingProbeMain())return 1;
    try{
        for(const auto& profile:circuit::timing::timingProfileNames()){
            circuit::timing::setActiveTimingProfile(profile);
            timeline(feedbackNetwork(false));timeline(feedbackNetwork(true));
            std::cout<<profile<<" incremental timeline cases="<<cases<<" exact-value-time-digest="<<digest<<'\n';
        }
        std::cout<<"PASS reconvergent feedback, arrival-only changes, interleaved/cold state, repeated clocks and leaf fallback\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
