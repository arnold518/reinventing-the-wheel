#pragma once

#include "components/Component.hpp"
#include "components/ComponentBuilder.hpp"
#include "simulator/TruthTableTest.hpp"
#include <memory>
#include <string>
#include <utility>

template<typename ComponentT>
class ComponentTruthTableTest : public TruthTableTest {
public:
    explicit ComponentTruthTableTest(std::string test_name, std::string root_name = "ROOT")
        : test_name_(std::move(test_name)), root_name_(std::move(root_name)) {}

    std::string getTestName() const override {
        return test_name_;
    }

    void setupCircuit() override {
        root = Component::create<ComponentT>(root_name_);
        builder = std::make_unique<ComponentBuilder>(root);
        buildCircuit();
        setInitialState();
    }

private:
    std::string test_name_;
    std::string root_name_;
};
