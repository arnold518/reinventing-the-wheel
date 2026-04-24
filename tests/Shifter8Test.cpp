#include "TestHelpers.hpp"
#include "modules/composite/Shifter8.hpp"

int main() {
    bool ok = true;
    ok &= runRows<ShiftLeftLogical8>({
        {{{"A", bits(0x01)}}, {{"Result", bits(0x02)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x00)}, {"Carry", bit(true)}}},
    });
    ok &= runRows<ShiftRightLogical8>({{{{"A", bits(0x81)}}, {{"Result", bits(0x40)}, {"Carry", bit(true)}}}});
    ok &= runRows<ShiftRightArithmetic8>({{{{"A", bits(0x81)}}, {{"Result", bits(0xC0)}, {"Carry", bit(true)}}}});
    return ok ? 0 : 1;
}
