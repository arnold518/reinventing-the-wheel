#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/MemoryController.hpp"

namespace rv32i {
std::shared_ptr<IOComponent> buildMemory(ComponentBuilder& b, IOComponent& parent,
    const std::string& name, const MemoryConfiguration& config) {
    const std::string id = std::string(config.expanded_endpoint ? "memory.endpoint." : "memory.scalable.")
        + circuit::memory::technologyName(config.technology) + ".32";
    return circuit::memory_device::child(b, parent, id, name, circuit::memory::endpointParameters(config.device));
}
std::shared_ptr<MemoryStateView> memoryView(const std::shared_ptr<IOComponent>& memory) {
    auto view = std::dynamic_pointer_cast<MemoryStateView>(memory);
    if (!view) throw std::logic_error("Selected memory has no host image/state capability; initialize it through its pins");
    return view;
}
void wireMemorySystem(ComponentBuilder& b, const std::shared_ptr<IOComponent>& core,
    const std::shared_ptr<IOComponent>& instructions, const std::shared_ptr<IOComponent>& data,
    const std::shared_ptr<Pin<1>>& clock, const std::shared_ptr<Pin<1>>& reset) {
    circuit::memory_detail::StructuralWiring w(b);
    w.connect(clock, core->getInputPin("CLK"));
    w.connect(reset, core->getInputPin("RST"));
    for (auto [prefix, memory] : std::vector<std::pair<std::string,std::shared_ptr<IOComponent>>>{{"IMEM_",instructions},{"DMEM_",data}}) {
        w.connect(clock,memory->getInputPin("CLK"));
        w.connect(reset,memory->getInputPin("RST"));
        for (auto& [name,pin] : memory->getAllInputPins()) {
            if(name=="CLK" || name=="RST") continue;
            w.connect(core->getOutputPinDynamic(prefix+name),pin);
        }
        for (auto& [name,pin] : memory->getAllOutputPins()) w.connect(pin,core->getInputPinDynamic(prefix+name));
    }
    w.finish();
}
}
