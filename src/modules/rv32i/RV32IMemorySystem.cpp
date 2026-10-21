#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/memory/MemoryController.hpp"
#include <tuple>

namespace rv32i {
void directMemoryPins(IOComponent* s, const std::string& p, bool instruction) {
    s->addPin<32>(p+"WORD_ADDR",PinType::OUTPUT);
    s->addPin(p+"READ_EN",PinType::OUTPUT);
    s->addPin<32>(p+"READ_DATA",PinType::INPUT);
    s->addPin(p+"FAULT",PinType::INPUT);
    if (!instruction) {
        s->addPin<32>(p+"WRITE_DATA",PinType::OUTPUT);
        s->addPin<4>(p+"WRITE_MASK",PinType::OUTPUT);
        s->addPin(p+"WE",PinType::OUTPUT);
    }
}
std::shared_ptr<IOComponent> buildMemory(ComponentBuilder& b, IOComponent& parent,
    const std::string& name, const MemoryConfiguration& config) {
    const std::string id = circuit::memory::memoryContractId(
        config.technology, config.expanded_endpoint ? "endpoint" : "scalable-endpoint", 32);
    return circuit::memory_device::child(b, parent, id, name, circuit::memory::endpointParameters(config.device));
}
std::shared_ptr<MemoryStateView> memoryView(const std::shared_ptr<IOComponent>& memory) {
    auto view = std::dynamic_pointer_cast<MemoryStateView>(memory);
    if (!view) throw std::logic_error("Selected memory has no host image/state capability; initialize it through its pins");
    return view;
}
void wireMemorySystem(ComponentBuilder& b, const std::shared_ptr<IOComponent>& core,
    const std::shared_ptr<IOComponent>& instructions, const std::shared_ptr<IOComponent>& data,
    const std::shared_ptr<Pin<1>>& clock, const std::shared_ptr<Pin<1>>& reset,
    const std::string& instruction_prefix, const std::string& data_prefix) {
    circuit::detail::StructuralWiring w(b);
    w.connect(clock, core->getInputPin("CLK"));
    w.connect(reset, core->getInputPin("RST"));
    for (auto [prefix, memory, port] : std::vector<std::tuple<std::string,std::shared_ptr<IOComponent>,std::string>>{
             {"IMEM_",instructions,instruction_prefix},{"DMEM_",data,data_prefix}}) {
        w.connect(clock,memory->getInputPinDynamic(port + "CLK"));
        // CPU reset must not erase the loaded program or data image.
        w.connect(reset,memory->getInputPinDynamic(port + "RST"));
        for (auto name : {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA",
                          "REQ_WRITE_MASK", "RSP_READY"})
            w.connect(core->getOutputPinDynamic(prefix + name), memory->getInputPinDynamic(port + name));
        for (auto name : {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"})
            w.connect(memory->getOutputPinDynamic(port + name), core->getInputPinDynamic(prefix + name));
    }
    w.finish();
}
}
