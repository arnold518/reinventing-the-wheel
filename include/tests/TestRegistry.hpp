#pragma once

#include "simulator/SimulationTest.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum class RegisteredTestKind {
    Infrastructure,
    Semantic,
    Integration,
    TruthTable,
    Sequential,
    Memory,
    Program,
};

struct TestRegistryEntry {
    std::string name;
    std::string logical_test_id;
    RegisteredTestKind kind = RegisteredTestKind::Infrastructure;
    std::vector<std::string> contract_ids;
    std::string scenario_id;
    std::vector<std::string> labels;
    std::function<std::unique_ptr<SimulationTest>()> create;
    bool visualizable = false;
};

const char* toString(RegisteredTestKind kind);
const std::vector<TestRegistryEntry>& getTestRegistry();
std::unique_ptr<SimulationTest> createTestByName(const std::string& name);
std::vector<std::string> getRegisteredTestNames();
