#include "TestHelpers.hpp"
#include "modules/composite/ZeroDetect8.hpp"

int main() {
    return runRows<ZeroDetect8>({
        {{{"A", bits(0x00)}}, {{"ZERO", bit(true)}}},
        {{{"A", bits(0x01)}}, {{"ZERO", bit(false)}}},
    }) ? 0 : 1;
}
