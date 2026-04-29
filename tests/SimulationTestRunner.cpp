#include "tests/TestRegistry.hpp"
#include <iostream>
#include <memory>
#include <string>

namespace {
void printUsage(const char* executable_name) {
    std::cerr << "Usage: " << executable_name << " <test-name>\n";
    std::cerr << "Available tests:\n";
    for (const auto& name : getRegisteredTestNames()) {
        std::cerr << "  " << name << "\n";
    }
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 2;
    }

    std::string test_name = argv[1];
    auto test = createTestByName(test_name);
    if (!test) {
        std::cerr << "Unknown test: " << test_name << "\n";
        printUsage(argv[0]);
        return 2;
    }

    return test->run() ? 0 : 1;
}
