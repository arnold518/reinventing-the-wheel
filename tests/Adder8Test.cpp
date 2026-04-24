#include "TestHelpers.hpp"
#include "modules/composite/Adder8.hpp"

int main() {
    return runRows<Adder8>({
        {{{"A", bits(0x00)}, {"B", bits(0x00)}, {"Cin", bit(false)}}, {{"Sum", bits(0x00)}, {"Cout", bit(false)}}},
        {{{"A", bits(0x01)}, {"B", bits(0x02)}, {"Cin", bit(false)}}, {{"Sum", bits(0x03)}, {"Cout", bit(false)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x01)}, {"Cin", bit(false)}}, {{"Sum", bits(0x00)}, {"Cout", bit(true)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x00)}, {"Cin", bit(true)}}, {{"Sum", bits(0x00)}, {"Cout", bit(true)}}},
    }) ? 0 : 1;
}
