#include "TestHelpers.hpp"
#include "modules/composite/ALU8.hpp"

namespace {
TestRow row(uint64_t a, uint64_t b, uint64_t op, uint64_t out, bool zero, bool carry, bool overflow, bool negative) {
    return {{{"A", bits(a)}, {"B", bits(b)}, {"OP", bits(op)}},
            {{"OUT", bits(out)}, {"ZERO", bit(zero)}, {"CARRY", bit(carry)}, {"OVERFLOW", bit(overflow)}, {"NEGATIVE", bit(negative)}}};
}
}

int main() {
    return runRows<ALU8>({
        row(0x00, 0x00, 0x0, 0x00, true, false, false, false),
        row(0xFF, 0x01, 0x0, 0x00, true, true, false, false),
        row(0x7F, 0x01, 0x0, 0x80, false, false, true, true),
        row(0x05, 0x03, 0x1, 0x02, false, true, false, false),
        row(0x00, 0x01, 0x1, 0xFF, false, false, false, true),
        row(0xFF, 0x0F, 0x2, 0x0F, false, false, false, false),
        row(0xF0, 0x0F, 0x3, 0xFF, false, false, false, true),
        row(0xAA, 0x55, 0x4, 0xFF, false, false, false, true),
        row(0x00, 0x00, 0x5, 0xFF, false, false, false, true),
        row(0x80, 0x00, 0x6, 0x00, true, true, false, false),
        row(0x01, 0x00, 0x7, 0x00, true, true, false, false),
        row(0x80, 0x00, 0x8, 0xC0, false, false, false, true),
        row(0xFF, 0x00, 0x9, 0x00, true, true, false, false),
        row(0x00, 0x00, 0xA, 0xFF, false, false, false, true),
        row(0x01, 0x00, 0xB, 0xFF, false, true, false, true),
        row(0x42, 0xFF, 0xC, 0x42, false, false, false, false),
        row(0xFF, 0x42, 0xD, 0x42, false, false, false, false),
        row(0x05, 0x03, 0xE, 0x02, false, true, false, false),
        row(0xFF, 0xFF, 0xF, 0x00, true, false, false, false),
    }) ? 0 : 1;
}
