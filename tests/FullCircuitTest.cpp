#include "tests/FullCircuitTest.hpp"

int main() {
    FullCircuitTest test;
    bool passed = test.run();
    
    // Return 0 for PASS, 1 for FAIL, a standard contract for testing frameworks.
    return passed ? 0 : 1;
}