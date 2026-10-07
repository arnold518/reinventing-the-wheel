#include "analysis/RV32IBenchmarks.hpp"
#include <algorithm>
#include <map>
#include <numeric>
#include <stdexcept>

namespace circuit::analysis {
namespace {
constexpr uint32_t Input=0x1000, Output=0x2000, Result=0x3000, Stack=0x4000;
void require(bool v,const char* message) { if(!v)throw std::invalid_argument(message); }
uint32_t randomWord(uint32_t& s) { s^=s<<13;s^=s>>17;s^=s<<5;return s; }
std::vector<uint8_t> bytes(const std::vector<uint32_t>& words) {
    std::vector<uint8_t> out;
    for(auto w:words)for(unsigned b=0;b<4;++b)out.push_back(uint8_t(w>>(b*8)));
    return out;
}
// Small label-aware RV32I emitter. Range checks prevent silently truncated
// branch/immediate fields when a benchmark parameter grows.
struct Code {
    struct Fix { size_t word; std::string label; bool jump; unsigned a,b,f; };
    std::vector<uint32_t> words, branches;
    std::map<std::string,uint32_t> labels;
    std::vector<Fix> fixes;
    uint32_t pc() const { return uint32_t(words.size()*4); }
    void label(const std::string& s) { require(labels.emplace(s,pc()).second,"Duplicate benchmark label"); }
    void i(unsigned rd,unsigned rs,int imm,unsigned f=0,unsigned op=0x13) {
        require(imm>=-2048&&imm<=2047,"Benchmark immediate out of range");
        words.push_back((uint32_t(imm)&4095)<<20|rs<<15|f<<12|rd<<7|op);
    }
    void r(unsigned rd,unsigned a,unsigned b,unsigned f=0,unsigned f7=0) {
        words.push_back(f7<<25|b<<20|a<<15|f<<12|rd<<7|0x33);
    }
    void li(unsigned rd,uint32_t value) {
        words.push_back(((value+0x800U)&0xfffff000U)|rd<<7|0x37);
        i(rd,rd,int(value&4095)-(value&2048?4096:0));
    }
    void load(unsigned rd,unsigned base,int offset=0,unsigned f=2) { i(rd,base,offset,f,3); }
    void store(unsigned rs,unsigned base,int offset=0) {
        require(offset>=-2048&&offset<=2047,"Benchmark store offset out of range");
        uint32_t v=uint32_t(offset)&4095;
        words.push_back((v>>5)<<25|rs<<20|base<<15|2<<12|(v&31)<<7|0x23);
    }
    void branch(unsigned a,unsigned b,unsigned f,const std::string& target) {
        branches.push_back(pc());fixes.push_back({words.size(),target,false,a,b,f});words.push_back(0);
    }
    void jump(unsigned rd,const std::string& target) {
        fixes.push_back({words.size(),target,true,rd,0,0});words.push_back(0);
    }
    void address(unsigned rd,const std::string& target) {
        // All generated code fits the signed 12-bit absolute address range for
        // this helper (used only for the compact calls workload).
        fixes.push_back({words.size(),target,true,rd,0,8});words.push_back(0);
    }
    void finish() {
        for(const auto& f:fixes) {
            const auto target=labels.at(f.label);
            if(f.f==8) {
                require(target<2048,"Benchmark function address out of range");
                words[f.word]=target<<20|f.a<<7|0x13;continue;
            }
            int offset=int(target)-int(f.word*4);uint32_t v=uint32_t(offset);
            require(offset%4==0,"Unaligned benchmark target");
            if(f.jump) {
                require(offset>=-1048576&&offset<=1048574,"Benchmark JAL out of range");
                words[f.word]=((v>>20)&1)<<31|((v>>1)&1023)<<21|((v>>11)&1)<<20
                    |((v>>12)&255)<<12|f.a<<7|0x6f;
            } else {
                require(offset>=-4096&&offset<=4094,"Benchmark branch out of range");
                words[f.word]=((v>>12)&1)<<31|((v>>5)&63)<<25|f.b<<20|f.a<<15|f.f<<12
                    |((v>>1)&15)<<8|((v>>11)&1)<<7|0x63;
            }
        }
    }
};
}
const std::vector<std::string>& rv32iBenchmarkNames() {
    static const std::vector<std::string> names{"independent","dependency","load-use","load-scheduled",
        "branch-pattern","branch-alias","calls","stream","pointer-chase","sort","crc32","scan"};
    return names;
}
bool isRV32IBenchmark(const std::string& n) {
    const auto& names=rv32iBenchmarkNames();return std::find(names.begin(),names.end(),n)!=names.end();
}
void validateRV32IBenchmark(const std::string& name,size_t iterations,const RV32IBenchmarkOptions& o) {
    require(isRV32IBenchmark(name),"Unknown benchmark");
    require(iterations>0&&iterations<=1000000&&o.training_iterations<=1000000-iterations,
        "Total benchmark iterations must be 1..1000000");
    require(o.size>=1&&o.size<=256,"Benchmark size must be 1..256");
    require(o.distance==1||o.distance==2||o.distance==4||o.distance==8,"Dependency distance must be 1,2,4,8");
    require(o.branch_spacing>=4&&o.branch_spacing<=256&&(o.branch_spacing&(o.branch_spacing-1))==0,
        "Branch spacing must be a power of two in 4..256 words");
    require(o.seed!=0,"Benchmark seed must be nonzero");
    require(o.pattern=="taken"||o.pattern=="not-taken"||o.pattern=="alternating"||o.pattern=="bursts"
        ||o.pattern=="irregular","Unknown branch pattern");
    require(o.training_iterations==0||o.measure_body,"Training iterations require --measure-body");
    require(o.distance==1||name=="dependency","--distance applies only to dependency");
    require(o.branch_spacing==16||name=="branch-alias","--branch-spacing applies only to branch-alias");
    require(o.pattern=="alternating"||name=="branch-pattern","--pattern applies only to branch-pattern");
}
RV32IBenchmarkImage buildRV32IBenchmark(const std::string& name,size_t iterations,const RV32IBenchmarkOptions& o) {
    validateRV32IBenchmark(name,iterations,o);
    RV32IBenchmarkImage image;Code c;const size_t total=iterations+o.training_iterations,n=o.size;
    uint32_t seed=o.seed,per_iteration=0;
    std::vector<uint32_t> data(n);
    for(auto& x:data)x=randomWord(seed);
    auto initial=[&](uint32_t address,const std::vector<uint32_t>& words) { image.initial_data.push_back({address,bytes(words)}); };
    auto expected=[&](uint32_t address,const std::vector<uint32_t>& words) { image.expected_data.push_back({address,bytes(words)}); };
    c.li(28,uint32_t(total));c.i(10,0,0);
    if(name=="independent"||name=="dependency") {
        for(unsigned r=16;r<24;++r)c.i(r,0,int(r-15));
        image.description=name=="independent"?"64 mixed ALU operations on eight interleaved chains per iteration"
            :"64 ADDI operations across distance independent chains per iteration";
    } else if(name=="load-use"||name=="load-scheduled") {
        initial(Input,data);c.li(2,Input);c.i(7,0,0);
        per_iteration=std::accumulate(data.begin(),data.end(),uint32_t(0));
        image.description="Same loads, additions and independent counter increments; only instruction order differs";
    } else if(name=="branch-pattern") {
        data.resize(32);
        for(size_t i=0;i<data.size();++i) data[i]=o.pattern=="taken"?1:o.pattern=="not-taken"?0:
            o.pattern=="alternating"?i%2:o.pattern=="bursts"?(i/8)%2:randomWord(seed)&1;
        initial(Input,data);c.li(2,Input);c.i(3,0,0);
        for(size_t i=0;i<total;++i)image.expected_result+=data[i%32]==0;
        image.description="One data-driven forward branch per iteration plus the outer loop; 32-outcome repeating pattern";
    } else if(name=="branch-alias") {
        c.i(9,0,0);per_iteration=uint32_t(o.branch_spacing-1);
        image.description="Opposite constant outcomes at two branch PCs separated by branch-spacing words; loop branch also participates";
    } else if(name=="calls") {
        c.li(2,Stack);c.address(12,"function");per_iteration=6;
        image.description="Direct and indirect calls to a nested stack-using function; JALR returns";
    } else if(name=="stream"||name=="sort") {
        initial(Input,data);per_iteration=std::accumulate(data.begin(),data.end(),uint32_t(0));
        auto out=data;if(name=="sort")std::sort(out.begin(),out.end());expected(Output,out);
        image.description=name=="stream"?"Contiguous word copy with checksum":"Unsigned insertion sort; restore original input and checksum sorted output each iteration";
    } else if(name=="pointer-chase") {
        std::vector<uint32_t> order(n),nodes(n*2);std::iota(order.begin(),order.end(),0);
        for(size_t i=n-1;i>0;--i)std::swap(order[i],order[randomWord(seed)%(i+1)]);
        for(size_t i=0;i<n;++i){nodes[2*order[i]]=Input+8*order[(i+1)%n];nodes[2*i+1]=data[i];}
        initial(Input,nodes);per_iteration=std::accumulate(data.begin(),data.end(),uint32_t(0));
        image.description="Randomly permuted ring; each loaded next pointer immediately supplies another load address";
    } else {
        std::vector<uint8_t> input;for(auto v:data)input.push_back(uint8_t(v));
        image.initial_data.push_back({Input,input});
        if(name=="crc32") {
            uint32_t crc=~0U;
            for(auto b:input){crc^=b;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
            per_iteration=~crc;c.li(12,0xedb88320U);
            image.description="Bitwise reflected CRC-32, polynomial 0xEDB88320, initial/final XOR 0xFFFFFFFF";
        } else {
            for(auto b:input)per_iteration+=b<128;
            image.description="Count unsigned bytes below 128 with load, comparison and accumulation";
        }
    }
    c.label("body");image.body_begin=c.pc();
    if(name=="independent"||name=="dependency") {
        for(unsigned k=0;k<64;++k) {
            const unsigned r=16+k%(name=="independent"?8:o.distance);
            if(name=="dependency")c.i(r,r,1);
            else {const unsigned op=(k/8)%4;c.i(r,r,op==0?3:op==1?85:1,op==0?0:op==1?4:op==2?1:5);}
        }
        // Independent high-level reference, not execution through the ISA oracle.
        std::vector<uint32_t> accum{1,2,3,4,5,6,7,8};
        if(name=="dependency")for(size_t r=0;r<o.distance;++r)accum[r]+=uint32_t(total*(64/o.distance));
        else for(size_t rep=0;rep<total;++rep)for(unsigned round=0;round<2;++round)
            for(auto& x:accum)x=uint32_t((x+3)^85)<<1>>1;
        image.expected_result=std::accumulate(accum.begin(),accum.end(),uint32_t(0));
    } else if(name=="load-use"||name=="load-scheduled") {
        for(size_t i=0;i<n;++i){c.load(5,2,int(i*4));
            if(name=="load-scheduled")c.i(7,7,1);
            c.r(10,10,5);if(name=="load-use")c.i(7,7,1);}
    } else if(name=="branch-pattern") {
        c.r(4,2,3);c.load(5,4);c.branch(5,0,1,"pattern_skip");c.i(10,10,1);c.label("pattern_skip");
        c.i(3,3,4);c.i(3,3,127,7);
    } else if(name=="branch-alias") {
        const auto first=c.words.size();c.branch(0,0,0,"alias_taken");c.i(10,10,99);c.label("alias_taken");
        while(c.words.size()<first+o.branch_spacing)c.i(9,9,1);
        c.branch(0,0,1,"alias_skip");c.i(10,10,1);c.label("alias_skip");
    } else if(name=="calls") {
        c.jump(1,"function");c.i(1,12,0,0,0x67);
    } else if(name=="stream"||name=="pointer-chase") {
        c.li(2,Input);c.li(3,Output);c.li(4,uint32_t(n));c.label("memory_loop");
        if(name=="pointer-chase"){c.load(2,2);c.load(5,2,4);}
        else {c.load(5,2);c.store(5,3);c.i(2,2,4);c.i(3,3,4);}
        c.r(10,10,5);c.i(4,4,-1);c.branch(4,0,1,"memory_loop");
    } else if(name=="sort") {
        c.li(2,Input);c.li(3,Output);c.li(4,uint32_t(n));c.label("restore");
        c.load(5,2);c.store(5,3);c.i(2,2,4);c.i(3,3,4);c.i(4,4,-1);c.branch(4,0,1,"restore");
        c.li(2,Output);c.li(3,Output+uint32_t(4*n));c.i(4,2,4);c.label("sort_outer");
        c.branch(4,3,7,"sorted");c.load(5,4);c.i(6,4,0);c.label("sort_inner");
        c.branch(6,2,0,"insert");c.load(7,6,-4);c.branch(5,7,7,"insert");
        c.store(7,6);c.i(6,6,-4);c.jump(0,"sort_inner");c.label("insert");
        c.store(5,6);c.i(4,4,4);c.jump(0,"sort_outer");c.label("sorted");
        c.label("sum_sorted");c.load(5,2);c.r(10,10,5);c.i(2,2,4);c.branch(2,3,6,"sum_sorted");
    } else {
        c.li(2,Input);c.li(4,uint32_t(n));if(name=="crc32")c.i(11,0,-1);
        c.label("byte_loop");c.load(5,2,0,4);
        if(name=="crc32") {
            c.r(11,11,5,4);c.i(6,0,8);c.label("crc_bit");
            c.i(7,11,1,7);c.i(11,11,1,5);c.branch(7,0,0,"crc_skip");c.r(11,11,12,4);
            c.label("crc_skip");c.i(6,6,-1);c.branch(6,0,1,"crc_bit");
        } else {c.i(6,5,128,3);c.r(10,10,6);}
        c.i(2,2,1);c.i(4,4,-1);c.branch(4,0,1,"byte_loop");
        if(name=="crc32"){c.i(11,11,-1,4);c.r(10,10,11);}
    }
    c.i(28,28,-1);c.branch(28,0,1,"body");c.label("end");image.body_end=c.pc();
    if(name=="independent"||name=="dependency")for(unsigned r=16;r<24;++r)c.r(10,10,r);
    if(name=="branch-alias")c.r(10,10,9);
    c.li(30,Result);c.store(10,30);c.words.push_back(0x00100073);
    if(name=="calls") {
        c.label("function");c.i(2,2,-16);c.store(1,2,12);c.jump(1,"leaf");
        c.load(1,2,12);c.i(2,2,16);c.i(0,1,0,0,0x67);
        c.label("leaf");c.i(10,10,3);c.i(0,1,0,0,0x67);
    }
    if(name!="independent"&&name!="dependency"&&name!="branch-pattern")
        image.expected_result=uint32_t(uint64_t(per_iteration)*total);
    expected(Result,{image.expected_result});
    c.finish();image.branch_pcs=c.branches;image.program=rv32i::RV32IProgram::fromWords(c.words);
    return image;
}
}
