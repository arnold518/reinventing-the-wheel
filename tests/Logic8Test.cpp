#include "TestHelpers.hpp"
#include "modules/basic/Logic8.hpp"

int main() {
    bool ok = true;
    ok &= runRows<AND8>({{{{"A", bits(0xF0)}, {"B", bits(0x3C)}}, {{"OUT", bits(0x30)}}}});
    ok &= runRows<OR8>({{{{"A", bits(0xF0)}, {"B", bits(0x0F)}}, {{"OUT", bits(0xFF)}}}});
    ok &= runRows<XOR8>({{{{"A", bits(0xAA)}, {"B", bits(0x55)}}, {{"OUT", bits(0xFF)}}}});
    ok &= runRows<NOT8>({{{{"A", bits(0x00)}}, {{"OUT", bits(0xFF)}}}});
    ok &= runRows<NAND8>({{{{"A", bits(0xFF)}, {"B", bits(0x0F)}}, {{"OUT", bits(0xF0)}}}});
    ok &= runRows<NOR8>({{{{"A", bits(0xF0)}, {"B", bits(0x0F)}}, {{"OUT", bits(0x00)}}}});
    return ok ? 0 : 1;
}
