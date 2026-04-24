#include "TestHelpers.hpp"
#include "modules/composite/Arithmetic8.hpp"

int main() {
    bool ok = true;
    ok &= runRows<TwosComplement8>({
        {{{"A", bits(0x01)}}, {{"Result", bits(0xFF)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x80)}, {"Cout", bit(true)}, {"Overflow", bit(true)}}},
    });
    ok &= runRows<Subtractor8>({
        {{{"A", bits(0x05)}, {"B", bits(0x03)}}, {{"Result", bits(0x02)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x01)}}, {{"Result", bits(0xFF)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
    });
    ok &= runRows<SubtractorWithBorrow8>({{{{"A", bits(0x05)}, {"B", bits(0x03)}, {"Bin", bit(true)}}, {{"Result", bits(0x01)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}}});
    ok &= runRows<Incrementer8>({{{{"A", bits(0xFF)}}, {{"Result", bits(0x00)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}}});
    ok &= runRows<Decrementer8>({{{{"A", bits(0x00)}}, {{"Result", bits(0xFF)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}}});
    return ok ? 0 : 1;
}
