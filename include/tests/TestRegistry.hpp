#pragma once

#include "simulator/SimulationTest.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct TestRegistryEntry {
    std::string name;
    std::function<std::unique_ptr<SimulationTest>()> create;
};

const std::vector<TestRegistryEntry>& getTestRegistry();
std::unique_ptr<SimulationTest> createTestByName(const std::string& name);
std::vector<std::string> getRegisteredTestNames();
