#pragma once

#include "simulator/SignalTrace.hpp"
#include "visualizer/VisualizerSession.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "components/Component.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/capabilities/DeviceView.hpp"
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <zlib.h>

// Native recording equipment. Only pending propagation and one compressed
// chunk are retained; storage does not grow in proportion to simulated time.
class TickTraceWriter final : public SignalTraceObserver {
public:
    explicit TickTraceWriter(const std::filesystem::path& directory,
                             const circuit::visualizer::VisualizerSession& view,
                             size_t chunk_bytes = 8*1024*1024)
        : directory_(directory), chunk_limit_(chunk_bytes) {
        view.forEachPin([&](size_t i, const auto& pin) {
            if (i != states_.size()) throw std::logic_error("Non-contiguous trace pins");
            pins_.emplace(pin, i); widths_.push_back(pin->getWidth());
            states_.push_back(pack(pin->getValueAsVector()));
        });
        pin_count_=states_.size();
        view.forEachWire([&](size_t i, const auto& wire) {
            if (i+pin_count_ != states_.size()) throw std::logic_error("Non-contiguous trace wires");
            wires_.emplace(wire, i+pin_count_); widths_.push_back(wire->getWidth());
            states_.push_back(pack(wire->getValueVector()));
        });
        wire_count_=states_.size()-pin_count_;
        view.forEachComponent([&](const auto& component) {
            if(auto memory=std::dynamic_pointer_cast<MemoryStateView>(component)) {
                const MemoryStorage* storage=nullptr;
                try{storage=&memory->memoryStorage();}catch(const std::logic_error&){}
                if(storage) {
                    auto found=memories_.find(storage);
                    if(found!=memories_.end())auxiliary_[found->second].ids.push_back(component->getID());
                    else {
                        if(storage->capacityBytes()>1024*1024)throw std::length_error("Trace memory capacity exceeds budget");
                        const size_t start=states_.size();
                        const auto contents=storage->readLogic(0,storage->capacityBytes());
                        for(size_t i=0;i<contents.size();i+=8){widths_.push_back(8);states_.push_back(pack({contents.begin()+i,contents.begin()+i+8}));}
                        memories_[storage]=auxiliary_.size();
                        auxiliary_.push_back({"memory",{component->getID()},start,storage->capacityBytes(),8,storage->wordBits()/8});
                    }
                }
            }
            if(auto registers=std::dynamic_pointer_cast<RegisterStateView>(component)) {
                if(component->getSelectedFidelity()=="S")throw std::invalid_argument("Tick register snapshots require behavioral register storage");
                const auto words=registers->getRegisterStateAtTime(0);const size_t start=states_.size();
                for(const auto& word:words){widths_.push_back(word.size());states_.push_back(pack(word));}
                registers_[component->getID()]=auxiliary_.size();
                auxiliary_.push_back({"registers",{component->getID()},start,words.size(),words.empty()?32:words.front().size(),0});
            }
        });
        if(states_.size()>1024*1024)throw std::length_error("Trace state exceeds signal budget");
        // Same primary/fallback wire as Simulator::signalHistory. Explicit
        // pin-only publications retain their own independent signal slot.
        pin_sources_.resize(pin_count_,-1);
        view.forEachPin([&](size_t i, const auto& pin) {
            auto primary=pin->getType()==PinType::INPUT?pin->getExternalWireBase():pin->getInternalWireBase();
            auto fallback=pin->getType()==PinType::INPUT?pin->getInternalWireBase():pin->getExternalWireBase();
            auto found=wires_.find(primary); if(found==wires_.end()) found=wires_.find(fallback);
            if(found!=wires_.end())pin_sources_[i]=found->second-pin_count_;
        });
        if(chunk_limit_<1024 || chunk_limit_>8*1024*1024)throw std::invalid_argument("Invalid trace chunk size");
    }
    void wireChanged(SimulationTime time, const std::shared_ptr<WireBase>& wire,
                     const std::vector<LogicValue>& value) override {
        if(auto found=wires_.find(wire);found!=wires_.end())queue(time,found->second,pack(value));
    }
    void pinChanged(SimulationTime time, const std::shared_ptr<PinBase>& pin,
                    const std::vector<LogicValue>& value) override {
        if(auto found=pins_.find(pin);found!=pins_.end()){
            pin_sources_[found->second]=-1;queue(time,found->second,pack(value));
        }
    }
    void settledThrough(SimulationTime time) override {
        if(!has_time_&&(pending_.empty()||pending_.begin()->first>0))append(0,{});
        while(!pending_.empty()&&pending_.begin()->first<=time) {
            auto node=pending_.extract(pending_.begin());pending_values_-=node.mapped().size();
            append(node.key(),node.mapped());
        }
    }
    void memoryByteChanged(SimulationTime time,const MemoryStorage& storage,uint32_t address,uint8_t byte,uint8_t known) override {
        if(auto found=memories_.find(&storage);found!=memories_.end()) {
            const auto& part=auxiliary_[found->second];
            if(address>=part.count)throw std::logic_error("Trace memory address out of range");
            uint64_t bits=0;for(size_t bit=0;bit<8;++bit)bits|=uint64_t((known&(1u<<bit))?((byte>>bit)&1):2)<<(2*bit);
            queue(time,part.start+address,bits);
        }
    }
    void memoryFilled(SimulationTime time,const MemoryStorage& storage,LogicValue fill) override {
        if(auto found=memories_.find(&storage);found!=memories_.end()) {
            const auto& part=auxiliary_[found->second];const auto bits=pack(std::vector<LogicValue>(8,fill==LogicValue::HIGH_Z?LogicValue::UNKNOWN:fill));
            for(size_t i=0;i<part.count;++i)queue(time,part.start+i,bits);
        }
    }
    void registersChanged(SimulationTime time,const std::shared_ptr<Component>& component,const std::vector<std::vector<LogicValue>>& words) override {
        for(auto owner=component;owner;owner=owner->getParent())if(auto found=registers_.find(owner->getID());found!=registers_.end()) {
            const auto& part=auxiliary_[found->second];if(words.size()!=part.count)throw std::logic_error("Trace register geometry changed");
            for(size_t i=0;i<words.size();++i)queue(time,part.start+i,pack(words[i]));
        }
    }
    void presentation(SimulationTime time) {
        settledThrough(time);
        if(!has_time_||last_time_<time)append(time,{});
        if(last_time_!=time)throw std::logic_error("Presentation precedes captured trace");
        markers_.push_back({count_-1,time});
    }
    void finish() {
        flush();
        std::ofstream mapping(directory_/"pin-sources.json");mapping<<'[';
        for(size_t i=0;i<pin_sources_.size();++i){if(i)mapping<<',';mapping<<pin_sources_[i];}
        mapping<<"]\n";mapping.close();if(!mapping)throw std::runtime_error("Cannot write pin sources");
        std::ofstream out(directory_/"ticks.json.tmp");
        out<<"{\"schemaVersion\":1,\"kind\":\"circuitsim-tick-recording\",\"capturePolicy\":\"all-signal-ticks\",\"presentations\":\"playlist.json\",\"pinSources\":\"pin-sources.json\",\"timestampCount\":"<<count_<<",\"pinCount\":"<<pin_count_<<",\"wireCount\":"<<wire_count_<<",\"auxiliary\":[";
        for(size_t i=0;i<auxiliary_.size();++i){if(i)out<<',';const auto& part=auxiliary_[i];
            out<<"{\"kind\":\""<<part.kind<<"\",\"start\":"<<part.start-pin_count_-wire_count_<<",\"count\":"<<part.count<<",\"width\":"<<part.width<<",\"wordBytes\":"<<part.word_bytes<<",\"componentIds\":[";
            for(size_t j=0;j<part.ids.size();++j){if(j)out<<',';out<<std::quoted(part.ids[j]);}out<<"]}";}
        out<<"],\"chunks\":[";
        for(size_t i=0;i<chunks_.size();++i){if(i)out<<',';const auto& c=chunks_[i];
            out<<"{\"file\":\"tick-"<<i<<".bin.gz\",\"start\":"<<c.start<<",\"count\":"<<c.count<<",\"firstTime\":\""<<c.first<<"\",\"lastTime\":\""<<c.last<<"\",\"bytes\":"<<c.bytes<<",\"compressedBytes\":"<<c.compressed<<'}';}
        out<<"],\"markers\":[";
        for(size_t i=0;i<markers_.size();++i){if(i)out<<',';out<<"{\"index\":"<<markers_[i].first<<",\"time\":\""<<markers_[i].second<<"\"}";}
        out<<"]}\n";out.close();if(!out)throw std::runtime_error("Cannot finish tick index");
        std::filesystem::rename(directory_/"ticks.json.tmp",directory_/"ticks.json");
    }
    size_t timestampCount() const { return count_; }
private:
    struct Chunk { size_t start,count,bytes,compressed; SimulationTime first,last; };
    static uint64_t pack(const std::vector<LogicValue>& values) {
        if(values.empty()||values.size()>32)throw std::logic_error("Invalid trace width");
        uint64_t result=0;for(size_t i=0;i<values.size();++i)result|=uint64_t(values[i])<<(2*i);return result;
    }
    void number(uint64_t value) {
        while(value>=128){buffer_.push_back(uint8_t(value)|128);value>>=7;}buffer_.push_back(uint8_t(value));
    }
    void value(size_t index,uint64_t bits) {
        for(size_t b=0;b<(widths_[index]+3)/4;++b){buffer_.push_back(uint8_t(bits));bits>>=8;}
    }
    void queue(SimulationTime time,size_t index,uint64_t bits) {
        if(has_time_&&time<=last_time_)throw std::logic_error("Late signal publication in tick trace");
        auto& changes=pending_[time];
        if(!changes.contains(index)&&++pending_values_>2*1024*1024)throw std::length_error("Pending tick trace exceeds memory budget");
        changes[index]=bits;
    }
    void append(SimulationTime time,const std::map<size_t,uint64_t>& changes) {
        if(count_>=1000000000)throw std::length_error("Tick count exceeds replay index budget");
        if(chunk_count_&&(buffer_.size()>=chunk_limit_||chunk_count_>=8192))flush();
        if(!chunk_count_) {
            chunk_first_=time;buffer_.insert(buffer_.end(),{'C','S','T',2});number(pin_count_);number(wire_count_);number(states_.size()-pin_count_-wire_count_);
            for(size_t i=0;i<states_.size();++i)value(i,states_[i]);
        }
        if(has_time_&&time<=last_time_)throw std::logic_error("Non-monotonic tick trace");
        number(time);number(changes.size());size_t previous=0;
        for(const auto& [index,bits]:changes){number(index-previous);value(index,bits);previous=index;states_[index]=bits;}
        last_time_=time;has_time_=true;++count_;++chunk_count_;
        if(buffer_.size()>16*1024*1024)throw std::length_error("Tick chunk exceeds byte budget");
    }
    void flush() {
        if(!chunk_count_)return;
        if(chunks_.size()>=65536)throw std::length_error("Tick chunk count exceeds replay index budget");
        if(std::filesystem::space(directory_).available<1024ULL*1024*1024)throw std::runtime_error("Tick capture needs at least 1 GiB free disk");
        const auto path=directory_/("tick-"+std::to_string(chunks_.size())+".bin.gz");
        auto file=gzopen((path.string()+".tmp").c_str(),"wb1");if(!file)throw std::runtime_error("Cannot open tick chunk");
        const int written=gzwrite(file,buffer_.data(),unsigned(buffer_.size()));const int closed=gzclose(file);
        if(written!=int(buffer_.size())||closed!=Z_OK)throw std::runtime_error("Cannot write tick chunk");
        std::filesystem::rename(path.string()+".tmp",path);
        chunks_.push_back({count_-chunk_count_,chunk_count_,buffer_.size(),std::filesystem::file_size(path),chunk_first_,last_time_});
        std::ofstream progress(directory_/"ticks-progress.json.tmp");
        progress<<"{\"timestamps\":"<<count_<<",\"chunks\":"<<chunks_.size()<<",\"lastTime\":\""<<last_time_<<"\"}\n";progress.close();
        if(!progress)throw std::runtime_error("Cannot write tick progress");
        std::filesystem::rename(directory_/"ticks-progress.json.tmp",directory_/"ticks-progress.json");
        buffer_.clear();chunk_count_=0;
    }
    std::filesystem::path directory_;
    size_t chunk_limit_,pin_count_=0,wire_count_=0,count_=0,chunk_count_=0,pending_values_=0;
    bool has_time_=false;SimulationTime last_time_=0,chunk_first_=0;
    std::unordered_map<std::shared_ptr<PinBase>,size_t> pins_;
    std::unordered_map<std::shared_ptr<WireBase>,size_t> wires_;
    std::vector<size_t> widths_;
    std::vector<uint64_t> states_;
    std::vector<int64_t> pin_sources_;
    std::map<SimulationTime,std::map<size_t,uint64_t>> pending_;
    std::vector<uint8_t> buffer_;
    std::vector<Chunk> chunks_;
    std::vector<std::pair<size_t,SimulationTime>> markers_;
    struct Auxiliary { std::string kind;std::vector<std::string> ids;size_t start,count,width,word_bytes; };
    std::vector<Auxiliary> auxiliary_;
    std::unordered_map<const MemoryStorage*,size_t> memories_;
    std::map<std::string,size_t> registers_;
};
