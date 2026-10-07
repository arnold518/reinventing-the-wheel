#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "components/capabilities/MemoryStateView.hpp"

namespace rv32i {
// All cores expose transaction endpoints. Fixed-cycle cores require zero-wait FF;
// the advanced pipeline additionally supports delayed technology endpoints.
struct MemoryConfiguration {
    circuit::memory::Technology technology = circuit::memory::Technology::FF;
    bool expanded_endpoint = false;
    circuit::memory::EndpointConfig device = [] {
        circuit::memory::EndpointConfig c; c.depth_words = 65536; return c;
    }();
};
inline MemoryConfiguration zeroWaitMemoryConfiguration() {
    MemoryConfiguration config;
    config.technology = circuit::memory::Technology::FF;
    config.device.always_ready = true;
    return config;
}
std::shared_ptr<IOComponent> buildMemory(ComponentBuilder&, IOComponent& parent,
    const std::string& name, const MemoryConfiguration&);
void directMemoryPins(IOComponent*, const std::string& prefix, bool instruction = false);
std::shared_ptr<MemoryStateView> memoryView(const std::shared_ptr<IOComponent>&);
void wireMemorySystem(ComponentBuilder&, const std::shared_ptr<IOComponent>& core,
    const std::shared_ptr<IOComponent>& instructions, const std::shared_ptr<IOComponent>& data,
    const std::shared_ptr<Pin<1>>& clock, const std::shared_ptr<Pin<1>>& reset);
}
