#pragma once
#include <memory>
#include <string>
#include <stdexcept>
#include <cassert>
#include "simulator/Simulator.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"

class SimulationTest {
protected:
    std::unique_ptr<Simulator> sim;
    std::shared_ptr<Component> root;
    std::unique_ptr<ComponentBuilder> builder;

public:
    // The constructor now ONLY creates objects that do NOT depend on the derived class.
    SimulationTest() {
        sim = std::make_unique<Simulator>();
    }

    virtual ~SimulationTest() = default;

    bool run() {
        try {
            root = std::make_shared<Component>(this->getTestName());
            builder = std::make_unique<ComponentBuilder>(root);
        } catch (const std::exception& e) {
            std::cerr << "[FAIL] Test setup failed during initialization: " << e.what() << std::endl;
            return false;
        }

        std::cout << "--- Running Test: " << getTestName() << " ---" << std::endl;
        try {
            buildCircuit();
            setInitialState();
            sim->run(getRunDuration());
            verifyResults();
            std::cout << "[PASS] Test '" << getTestName() << "' completed successfully." << std::endl;
            if (root) {
                std::cerr << "--- Circuit State on Success ---" << std::endl;
                std::cerr << root->format(0, true, true) << std::endl;
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[FAIL] Test '" << getTestName() << "' threw an exception: " << e.what() << std::endl;
            if (root) {
                std::cerr << "--- Circuit State on Failure ---" << std::endl;
                std::cerr << root->format(0, true, true) << std::endl;
            }
            return false;
        }
    }

protected:
    // The rest of the class is unchanged. The derived classes will override these.
    virtual std::string getTestName() const = 0;
    virtual void buildCircuit() = 0;
    virtual void setInitialState() = 0;
    virtual void verifyResults() = 0;
    virtual size_t getRunDuration() const { return 100; }
};