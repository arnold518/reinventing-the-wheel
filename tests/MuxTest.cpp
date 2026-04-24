#include "TestHelpers.hpp"
#include "modules/basic/Mux.hpp"

int main() {
    bool ok = true;
    ok &= runRows<Mux2to1>({
        {{{"A", bit(false)}, {"B", bit(true)}, {"SEL", bit(false)}}, {{"OUT", bit(false)}}},
        {{{"A", bit(false)}, {"B", bit(true)}, {"SEL", bit(true)}}, {{"OUT", bit(true)}}},
    });
    ok &= runRows<Mux4to1>({{{{"IN0", bit(false)}, {"IN1", bit(false)}, {"IN2", bit(true)}, {"IN3", bit(false)}, {"SEL", bits(2)}}, {{"OUT", bit(true)}}}});
    ok &= runRows<Mux16to1>({{{{"IN0", bit(false)}, {"IN1", bit(false)}, {"IN2", bit(false)}, {"IN3", bit(false)}, {"IN4", bit(false)}, {"IN5", bit(true)}, {"IN6", bit(false)}, {"IN7", bit(false)}, {"IN8", bit(false)}, {"IN9", bit(false)}, {"IN10", bit(false)}, {"IN11", bit(false)}, {"IN12", bit(false)}, {"IN13", bit(false)}, {"IN14", bit(false)}, {"IN15", bit(false)}, {"SEL", bits(5)}}, {{"OUT", bit(true)}}}});
    ok &= runRows<Mux16to1_8bit>({{{{"IN0", bits(0x00)}, {"IN1", bits(0x11)}, {"IN2", bits(0x22)}, {"IN3", bits(0x33)}, {"IN4", bits(0x44)}, {"IN5", bits(0x55)}, {"IN6", bits(0x66)}, {"IN7", bits(0x77)}, {"IN8", bits(0x88)}, {"IN9", bits(0x99)}, {"IN10", bits(0xAA)}, {"IN11", bits(0xBB)}, {"IN12", bits(0xCC)}, {"IN13", bits(0xDD)}, {"IN14", bits(0xEE)}, {"IN15", bits(0xFF)}, {"SEL", bits(10)}}, {{"OUT", bits(0xAA)}}}});
    return ok ? 0 : 1;
}
