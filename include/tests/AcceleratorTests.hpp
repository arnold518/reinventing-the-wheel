#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/selection/BuildProfile.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct TestRegistryEntry;
// Chapter 6 tests and recorded scenarios (accelerator parts, device, machine).
void appendAcceleratorTests(std::vector<TestRegistryEntry>&);

namespace circuit::accelerator::test {
void require(bool ok, const std::string& why);
BuildProfile compactProfile();
// A headless harness: one root component, every input on a wire, a 1000-tick
// step per input change and a three-step clock.
class Rig {
public:
    std::shared_ptr<IOComponent> root;
    Simulator sim;
    std::map<std::string, std::shared_ptr<WireBase>> inputs;
    SimulationTime now = 0;
    Rig(const ComponentFamily&, bool compact, ParameterMap parameters = {});
    Rig(const std::string& contract, bool compact, ParameterMap parameters = {});
    void drive(const std::map<std::string, uint64_t>&);
    void unknown(const std::string& input);   // every bit X
    uint64_t out(const std::string&) const;
    void cycle();
private:
    void attach(std::shared_ptr<Component>);
};
int64_t signExtend(uint64_t value, size_t width);
uint64_t referenceProduct(uint64_t a, uint64_t b, bool signed_a, bool signed_b, size_t width);
int8_t referenceRequantize(int32_t acc, uint16_t scale, unsigned shift, bool relu);
void appendStageOneTwo(std::vector<TestRegistryEntry>&);
void appendDeviceTests(std::vector<TestRegistryEntry>&);
void appendMachineTests(std::vector<TestRegistryEntry>&);
}
