#include <stdint.h>
volatile int32_t signed_a = -12345, signed_b = 67;
volatile uint32_t unsigned_a = 0xf1234567u, unsigned_b = 123;
volatile uint32_t results[8];
int main(void) {
    int32_t a = signed_a, b = signed_b;
    uint32_t u = unsigned_a, v = unsigned_b;
    results[0] = (uint32_t)(a * b);
    results[1] = (uint32_t)(((int64_t)a * b) >> 32);
    results[2] = (uint32_t)(((int64_t)a * (int64_t)u) >> 32);
    results[3] = (uint32_t)(((uint64_t)u * v) >> 32);
    results[4] = (uint32_t)(a / b);
    results[5] = u / v;
    results[6] = (uint32_t)(a % b);
    results[7] = u % v;
    return results[0] != 0xfff36115u || results[1] != 0xffffffffu ||
           results[2] != 0xffffd293u || results[3] != 0x73u ||
           results[4] != 0xffffff48u || results[5] != 0x01f5e158u ||
           results[6] != 0xffffffefu || results[7] != 0x1fu;
}
