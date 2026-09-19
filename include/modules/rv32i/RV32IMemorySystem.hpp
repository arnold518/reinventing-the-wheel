#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "components/capabilities/MemoryStateView.hpp"

namespace rv32i {
// Both choices use exactly the same transaction wiring. Expanded endpoints are
// limited to small geometries; large CPU memories use the scalable container.
struct MemoryConfiguration {
    circuit::memory::Technology technology = circuit::memory::Technology::SRAM;
    bool expanded_endpoint = false;
    circuit::memory::EndpointConfig device = [] {
        circuit::memory::EndpointConfig c; c.depth_words = 65536; return c;
    }();
};
std::shared_ptr<IOComponent> buildMemory(ComponentBuilder&, IOComponent& parent,
    const std::string& name, const MemoryConfiguration&);
std::shared_ptr<MemoryStateView> memoryView(const std::shared_ptr<IOComponent>&);
void wireMemorySystem(ComponentBuilder&, const std::shared_ptr<IOComponent>& core,
    const std::shared_ptr<IOComponent>& instructions, const std::shared_ptr<IOComponent>& data,
    const std::shared_ptr<Pin<1>>& clock, const std::shared_ptr<Pin<1>>& reset);
}
