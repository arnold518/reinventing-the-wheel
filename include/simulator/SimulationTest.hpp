#pragma once
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include "simulator/Simulator.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"

class SimulationTest {
protected:
    std::unique_ptr<Simulator> sim;
    std::shared_ptr<Component> root;
    std::unique_ptr<ComponentBuilder> builder;
    bool initial_events_scheduled_ = false;

public:
    SimulationTest() {
        sim = std::make_unique<Simulator>();
    }

    virtual ~SimulationTest() = default;

    std::shared_ptr<Component> getRoot() const { return root; }
    Simulator* getSimulator() const { return sim.get(); }

    struct SimulationCheckpoint {
        size_t time = 0;
        std::string label;
        std::string detail;
        size_t row_index = 0;
    };

    virtual std::vector<SimulationCheckpoint> getCheckpoints() const { return {}; }

    static void scheduleInitialEventsForTree(const std::shared_ptr<Component>& component, Simulator& simulator, size_t time = 0) {
        if (!component) {
            return;
        }
        if (auto evaluated = std::dynamic_pointer_cast<BasicComponent>(component)) {
            evaluated->scheduleInitialEvents(simulator, time);
        }
        for (const auto& child : component->getChildren()) {
            scheduleInitialEventsForTree(child, simulator, time);
        }
    }

    void scheduleInitialEvents(size_t time = 0) {
        if (initial_events_scheduled_) {
            return;
        }
        scheduleInitialEventsForTree(root, *sim, time);
        initial_events_scheduled_ = true;
    }

    virtual void setupCircuit() {
        root = std::make_shared<Component>(getTestName());
        builder = std::make_unique<ComponentBuilder>(root);

        buildCircuit();
        setInitialState();
    }

    bool run() {
        std::cout << "--- Running Test: " << getTestName() << " ---" << std::endl;
        try {
            setupCircuit();
            scheduleInitialEvents();

            runSimulation();
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
    virtual void runSimulation() { sim->runAndRecord(getRunDuration()); }
    virtual void verifyResults() = 0;
    virtual size_t getRunDuration() const { return 100; }
};
