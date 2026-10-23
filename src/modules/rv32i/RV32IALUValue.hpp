#pragma once
#include <cstdint>
#include <cstddef>

// Known-binary rule shared by compact integer ALUs and their parent cores.
// The instruction oracle remains independent.
namespace rv32i::alu_value {
struct Arithmetic { uint32_t out; bool carry, overflow; };
template<size_t W> constexpr uint64_t mask = (uint64_t{1} << W) - 1;
template<size_t W> constexpr uint64_t sign = uint64_t{1} << (W-1);
template<size_t W> int64_t signedValue(uint32_t value) {
    static_assert(W==8 || W==16 || W==32);
    return int64_t(value) - (value & sign<W> ? int64_t(mask<W>+1) : 0);
}
template<size_t W> Arithmetic addsub(uint32_t a,uint32_t b,bool sub) {
    const uint64_t y=sub ? uint64_t(b)^mask<W> : b;
    const uint64_t total=uint64_t(a)+y+sub;
    const uint32_t out=total&mask<W>;
    return {out,bool(total>>W),bool((~(uint64_t(a)^y)&(a^out))&sign<W>)};
}
template<size_t W> uint32_t shift(uint32_t a,uint32_t b,bool left,bool arithmetic) {
    const unsigned amount=b&(W-1);
    if(left) return (uint64_t(a)<<amount)&mask<W>;
    uint64_t out=a>>amount;
    if(amount && arithmetic && (a&sign<W>)) out|=mask<W><<(W-amount);
    return out&mask<W>;
}
template<size_t W> uint32_t result(uint8_t op,uint32_t a,uint32_t b) {
    switch(op) {
    case 0:return addsub<W>(a,b,false).out;
    case 1:return addsub<W>(a,b,true).out;
    case 2:return a&b; case 3:return a|b; case 4:return a^b;
    case 5:return shift<W>(a,b,true,false);
    case 6:return shift<W>(a,b,false,false);
    case 7:return shift<W>(a,b,false,true);
    case 8:return signedValue<W>(a)<signedValue<W>(b);
    case 9:return a<b; case 10:return a; case 11:return b;
    default:return 0;
    }
}
}
