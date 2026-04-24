#include "TestHelpers.hpp"
#include "modules/composite/Comparator8.hpp"

int main() {
    bool ok = true;
    ok &= runRows<EqualityChecker8>({
        {{{"A", bits(0x42)}, {"B", bits(0x42)}}, {{"EQ", bit(true)}}},
        {{{"A", bits(0x42)}, {"B", bits(0x43)}}, {{"EQ", bit(false)}}},
    });
    ok &= runRows<Comparator8>({
        {{{"A", bits(0x01)}, {"B", bits(0x02)}}, {{"LT", bit(true)}, {"GT", bit(false)}, {"EQ", bit(false)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x02)}}, {{"LT", bit(false)}, {"GT", bit(true)}, {"EQ", bit(false)}}},
    });
    ok &= runRows<SignedComparator8>({
        {{{"A", bits(0xFF)}, {"B", bits(0x01)}}, {{"SLT", bit(true)}, {"SGT", bit(false)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0x80)}}, {{"SLT", bit(false)}, {"SGT", bit(true)}, {"SEQ", bit(false)}}},
    });
    return ok ? 0 : 1;
}
