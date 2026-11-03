// Within-process reference/candidate comparisons. This is a timing-model
// kernel, not full-engine or gameplay throughput.
#include "analysis/DelayTwin.hpp"
#include "components/IOComponent.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/storage/Register.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iostream>
#include <stdexcept>

namespace circuit::analysis::reference {
std::shared_ptr<const circuit::DelayTwin> buildDelayTwin(const std::shared_ptr<Component>&,std::string*);
}
namespace {
using L=LogicValue;
struct Input {
    long long now;
    std::vector<std::vector<L>> values;
    std::vector<std::vector<long long>> changed;
};
struct Instance {
    std::shared_ptr<const circuit::DelayTwin> twin;
    std::unique_ptr<circuit::DelayTwin::State> state;
    std::vector<long long> times;
    std::vector<std::vector<uint8_t>> result;
    bool answered=false;
    explicit Instance(std::shared_ptr<const circuit::DelayTwin> t):twin(std::move(t)),state(twin->start()){}
    void step(const Input& input){answered=twin->settle(state.get(),input.now,input.values,input.changed,times,result);}
};
void equal(const Instance& a,const Instance& b,size_t at){
    if(a.answered!=b.answered||a.times!=b.times||a.result!=b.result)
        throw std::runtime_error("Reference/candidate kernel mismatch at step "+std::to_string(at));
}
std::vector<Input> inputs(const circuit::DelayTwin& twin,size_t count,bool dense){
    std::vector<Input> all;all.reserve(count);Input current;
    for(const auto&[name,width]:twin.inputs){current.values.emplace_back(width,L::LOW);current.changed.emplace_back(width,-1);}
    uint64_t random=123456789;
    for(size_t step=0;step<count;++step){
        current.now=1000000+step*10000LL;
        for(size_t pin=0;pin<twin.inputs.size();++pin){
            const auto& name=twin.inputs[pin].first;
            for(size_t bit=0;bit<current.values[pin].size();++bit){
                auto value=current.values[pin][bit];
                if(name=="CLK")value=step%4==1?L::HIGH:L::LOW;
                if(name=="WE")value=(step/512)%3==2?L::LOW:L::HIGH;
                if(name=="RST")value=step%2048<4?L::HIGH:L::LOW;
                if(name=="D"&&step%4==0&&(dense||bit==(step/4)%current.values[pin].size())){
                    random=random*6364136223846793005ULL+1442695040888963407ULL;
                    value=L((random>>32)%(step%64==0?4:2));
                }
                const bool arrival_only=name=="D"&&step%37==0&&bit==step%current.values[pin].size();
                if(value!=current.values[pin][bit]||arrival_only)current.changed[pin][bit]=current.now;
                current.values[pin][bit]=value;
            }
        }
        all.push_back(current); // Each case owns its input and arrival snapshots.
    }
    return all;
}
void benchmark(size_t width,bool dense){
    auto root=circuit::builtinComponentCatalog().createRoot(
        circuit::families::registerWord(width).request("REGISTER_KERNEL"),circuit::maximallyStructural()).root;
    std::string why;
    const auto baseline=circuit::analysis::reference::buildDelayTwin(root,&why);
    if(!baseline)throw std::runtime_error("Reference kernel unavailable: "+why);
    const auto candidate=circuit::analysis::buildDelayTwin(root,&why);
    if(!candidate)throw std::runtime_error("Candidate kernel unavailable: "+why);
    if(baseline->inputs!=candidate->inputs||baseline->outputs!=candidate->outputs)
        throw std::runtime_error("Kernel interface differs");
    constexpr size_t block_size=256,blocks=32;
    const auto cases=inputs(*baseline,block_size*blocks,dense);
    {
        Instance reference(baseline),test(candidate);
        for(size_t i=0;i<cases.size();++i){reference.step(cases[i]);test.step(cases[i]);equal(reference,test,i);}
    }
    std::array<Instance,4> models{Instance(baseline),Instance(candidate),Instance(candidate),Instance(baseline)};
    std::vector<double> ratios;double reference_cpu=0,candidate_cpu=0;
    for(size_t block=0;block<blocks;++block){
        std::array<double,4> cpu{};
        // Alternate ABBA/BAAB so either implementation gets the first/cold
        // input chunk and both middle/warm positions equally often.
        std::array<size_t,4> sequence=block%2?std::array<size_t,4>{1,0,3,2}:std::array<size_t,4>{0,1,2,3};
        for(auto model:sequence){
            const auto start=std::clock();
            for(size_t i=block*block_size;i<(block+1)*block_size;++i)models[model].step(cases[i]);
            cpu[model]=double(std::clock()-start)/CLOCKS_PER_SEC;
            if(cpu[model]<=0)throw std::runtime_error("Timing clock has inadequate resolution");
        }
        for(size_t i=1;i<4;++i)equal(models[0],models[i],(block+1)*block_size);
        reference_cpu+=cpu[0]+cpu[3];candidate_cpu+=cpu[1]+cpu[2];
        ratios.push_back(std::sqrt(cpu[1]*cpu[2]/(cpu[0]*cpu[3])));
    }
    std::sort(ratios.begin(),ratios.end());
    std::cout<<"{\"profile\":\""<<circuit::timing::activeTimingProfile().name<<"\",\"width\":"<<width
        <<",\"activity\":\""<<(dense?"dense":"sparse")<<"\",\"cases\":"<<cases.size()<<",\"blocks\":"<<blocks
        <<",\"referenceCpuSeconds\":"<<reference_cpu<<",\"candidateCpuSeconds\":"<<candidate_cpu
        <<",\"cpuRatio\":"<<candidate_cpu/reference_cpu<<",\"medianBlockRatio\":"<<(ratios[15]+ratios[16])/2
        <<",\"minBlockRatio\":"<<ratios.front()<<",\"maxBlockRatio\":"<<ratios.back()<<"}\n";
}
}
int main(){
    try{
        for(const auto& profile:circuit::timing::timingProfileNames()){
            circuit::timing::setActiveTimingProfile(profile);
            for(size_t width:{8u,32u})for(bool dense:{false,true})benchmark(width,dense);
        }
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
