/* Small, complete compiler-generated workload: stack, call, loop, branches,
 * loads and unsigned wraparound. The return code feeds the tohost adapter. */
__attribute__((noinline))
unsigned reduce(const volatile unsigned *values, unsigned count) {
    unsigned result = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned value = values[i];
        if (value & 1) result += value;
        else result ^= value;
    }
    return result;
}

int main(void) {
    volatile unsigned values[4] = {7, 0xffffffffu, 0x80000000u, 0x5a5a5a5au};
    return reduce(values, 4) != 0xda5a5a5cu;
}
