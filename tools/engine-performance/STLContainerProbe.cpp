// Standalone container comparisons. Results describe these operation mixes,
// not whole-engine speedups. Checksums validate order and last-write-wins.
#include "benchmark/Benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <memory_resource>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Pair=std::pair<size_t,uint64_t>;
uint64_t mix(uint64_t h,uint64_t v){return (h^v)*1099511628211ULL;}
uint32_t random32(uint32_t& x){x^=x<<13;x^=x>>17;x^=x<<5;return x;}

int main(int argc,char** argv) {
    try {
        if(argc<5||argc>6)throw std::invalid_argument("Usage: STLContainerProbe lookup|changes|flags variant size iterations [random|ascending]");
        const std::string family=argv[1],variant=argv[2],order=argc>5?argv[5]:"random";
        const size_t size=std::stoull(argv[3]),iterations=std::stoull(argv[4]);
        if(!size||size>4096||!iterations||iterations>50000000)throw std::invalid_argument("Experiment bounds exceeded");
        uint64_t hash=0,expected=0;double seconds=0,cpu=0;
        size_t operations=iterations;
        if(family=="lookup") {
            const std::vector<std::string> common={"A","B","IN","OUT","CLK","RST","DATA","ADDRESS","ENABLE","READY","VALID","SELECT","WRITE","READ","MASK","RESULT"};
            std::vector<std::string> names;
            for(size_t i=0;i<size;++i)names.push_back(i<common.size()?common[i]:"PIN_"+std::to_string(100000+i));
            std::map<std::string,size_t> tree;
            std::unordered_map<std::string,size_t> hashed;hashed.reserve(size);
            std::vector<std::pair<std::string,size_t>> flat,linear;
            for(size_t i=0;i<size;++i){tree.emplace(names[i],i);hashed.emplace(names[i],i);linear.emplace_back(names[i],i);}
            flat=linear;std::sort(flat.begin(),flat.end());
            uint32_t seed=0x12345678;
            std::vector<size_t> queries(4096);
            for(auto& q:queries)q=random32(seed)%size;
            for(size_t i=0;i<iterations;++i)expected+=queries[i%queries.size()]+1;
            auto bench=[&](auto lookup) {
                const auto start=std::chrono::steady_clock::now();const auto c=std::clock();
                for(size_t i=0;i<iterations;++i){const auto q=queries[i%queries.size()];hash+=lookup(names[q],q)+1;}
                cpu=double(std::clock()-c)/CLOCKS_PER_SEC;seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            };
            if(variant=="map")bench([&](const auto& k,size_t){return tree.find(k)->second;});
            else if(variant=="hash")bench([&](const auto& k,size_t){return hashed.find(k)->second;});
            else if(variant=="flat")bench([&](const auto& k,size_t){return std::lower_bound(flat.begin(),flat.end(),k,[](const auto& a,const auto& b){return a.first<b;})->second;});
            else if(variant=="linear")bench([&](const auto& k,size_t){return std::find_if(linear.begin(),linear.end(),[&](const auto& p){return p.first==k;})->second;});
            else if(variant=="indexed")bench([&](const auto&,size_t q){return linear[q].second;});
            else throw std::invalid_argument("Unknown lookup variant");
        } else if(family=="changes") {
            if(iterations>1000000||size*iterations>8000000)throw std::invalid_argument("Change operation cap exceeded");
            std::vector<size_t> ids(size);std::iota(ids.begin(),ids.end(),0);
            uint32_t seed=0x71a3b92d;
            if(order=="random")for(size_t i=size;i>1;--i)std::swap(ids[i-1],ids[random32(seed)%i]);
            else if(order!="ascending")throw std::invalid_argument("Invalid update order");
            std::vector<Pair> updates;
            for(size_t pass=0;pass<2;++pass)for(size_t i=0;i<size;++i)updates.emplace_back(ids[i],pass*size+i);
            std::vector<uint64_t> final(size);
            for(auto [k,v]:updates)final[k]=v;
            for(size_t r=0;r<iterations;++r)for(size_t k=0;k<size;++k)expected=mix(expected,k^(final[k]+r));
            std::pmr::unsynchronized_pool_resource pool(std::pmr::pool_options{256,128});
            const auto start=std::chrono::steady_clock::now();const auto c=std::clock();
            for(size_t r=0;r<iterations;++r) {
                if(variant=="map") {
                    std::map<size_t,uint64_t> changes;
                    for(auto [k,v]:updates)changes[k]=v+r;
                    for(auto [k,v]:changes)hash=mix(hash,k^v);
                } else if(variant=="pool-map") {
                    std::pmr::map<size_t,uint64_t> changes(&pool);
                    for(auto [k,v]:updates)changes[k]=v+r;
                    for(auto [k,v]:changes)hash=mix(hash,k^v);
                } else if(variant=="hash-sort") {
                    std::unordered_map<size_t,uint64_t> changes;changes.reserve(size);
                    for(auto [k,v]:updates)changes[k]=v+r;
                    std::vector<Pair> sorted(changes.begin(),changes.end());std::sort(sorted.begin(),sorted.end());
                    for(auto [k,v]:sorted)hash=mix(hash,k^v);
                } else if(variant=="flat") {
                    std::vector<Pair> changes;
                    for(auto [k,v]:updates) {
                        auto it=std::lower_bound(changes.begin(),changes.end(),k,[](const auto& p,size_t key){return p.first<key;});
                        if(it!=changes.end()&&it->first==k)it->second=v+r;
                        else changes.insert(it,{k,v+r});
                    }
                    for(auto [k,v]:changes)hash=mix(hash,k^v);
                } else if(variant=="append-sort") {
                    auto changes=updates;for(auto& p:changes)p.second+=r;
                    std::stable_sort(changes.begin(),changes.end(),[](const auto& a,const auto& b){return a.first<b.first;});
                    for(size_t i=0;i<changes.size();) {
                        size_t j=i+1;while(j<changes.size()&&changes[j].first==changes[i].first)++j;
                        hash=mix(hash,changes[j-1].first^changes[j-1].second);i=j;
                    }
                } else throw std::invalid_argument("Unknown changes variant");
            }
            cpu=double(std::clock()-c)/CLOCKS_PER_SEC;seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            operations=2*size*iterations;
        } else if(family=="flags") {
            if(size&(size-1))throw std::invalid_argument("Flags workload requires a power-of-two size");
            std::vector<bool> bits(size,false);std::vector<uint8_t> bytes(size,0);
            for(size_t i=0;i<iterations;++i)expected+=(i/size)%2==0;
            auto bench=[&](auto& flags) {
                const auto start=std::chrono::steady_clock::now();const auto c=std::clock();
                for(size_t i=0;i<iterations;++i){const size_t at=(i*17)%size;flags[at]=!flags[at];hash+=flags[at];}
                cpu=double(std::clock()-c)/CLOCKS_PER_SEC;seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            };
            if(variant=="bits")bench(bits);else if(variant=="bytes")bench(bytes);else throw std::invalid_argument("Unknown flags variant");
        } else throw std::invalid_argument("Unknown container workload");
        if(hash!=expected)throw std::runtime_error("Container checksum mismatch");
        circuit::benchmark::RunRecord r;
        r.workload={"container."+family,"engine","Bounded container operation mix",{"host-microbenchmark"},{{"size",std::to_string(size)},{"iterations",std::to_string(iterations)},{"order",order}}};
        r.target={variant,"engine",{{"compilerOptimization","O2"}}};
        r.status=circuit::benchmark::RunStatus::Completed;r.validated=true;r.host_seconds=seconds;
        r.fingerprints["checksum"]=std::to_string(hash);
        circuit::benchmark::addMetric(r,"cpu_seconds",cpu,"s");
        circuit::benchmark::addMetric(r,"operations",operations,"operations");
        circuit::benchmark::addMetric(r,"operations_per_second",operations/seconds,"operations/s");
        std::cout<<circuit::benchmark::toJson(r)<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
