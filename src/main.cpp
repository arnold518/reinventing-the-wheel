#include <iostream>
#include <memory>
#include "simulator/Simulator.hpp"
#include "components/ComponentBuilder.tpp"

#include "tests/ArithmeticLogicTests.hpp"

int main() {
    // 1. Create an instance of the test harness.
    // The constructor for SimulationTest already creates the simulator instance.
    FullAdderTest test;

    // 2. Run the test.
    // The run() method handles everything:
    //   - Calls setupCircuit() (which creates the IOComponent root and the builder)
    //   - Calls buildCircuit() and setInitialState()
    //   - Runs the simulation for the specified duration
    //   - Calls verifyResults()
    //   - Prints the final circuit state to std::cerr on success or failure.
    test.run();

    // 3. Keep the extra printing feature: Show the number of unique timestamps.
    // We can get the simulator instance from the test object after the run is complete.
    Simulator* sim = test.getSimulator();
    if (sim) {
        auto timestamps = sim->getUniqueTimestamps();
        std::cout << "\nTotal unique event timestamps recorded: " << timestamps.size() << std::endl;
    }

    return 0;
}
