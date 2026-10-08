#pragma once
#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/utility/BitAdapter.hpp"

namespace rv32i {
// Construction helpers only. The requester blocks belong to the CPU and their
// bus wires terminate directly on the CPU's public universal memory ports.
inline void wireRequestBus(ComponentBuilder& b, IOComponent& core,
                           const std::shared_ptr<IOComponent>& request, const std::string& prefix) {
    circuit::detail::StructuralWiring w(b);
    for (auto& [name,pin] : request->getAllInputPins()) if(name.starts_with("BUS_"))
        w.connect(core.getInputPinDynamic(prefix+name.substr(4)), {pin});
    for (auto& [name,pin] : request->getAllOutputPins()) if(name.starts_with("BUS_"))
        w.connect(pin, {core.getOutputPinDynamic(prefix+name.substr(4))});
    w.finish();
}
inline void wireRequestFields(ComponentBuilder& b, const std::shared_ptr<IOComponent>& fetch,
                              const std::shared_ptr<IOComponent>& data, const std::shared_ptr<IOComponent>& lanes,
                              const std::shared_ptr<IOComponent>& operation, bool read_only_fetch = false) {
    circuit::detail::StructuralWiring w(b);
    auto zero=w.constant("MEMORY_ZERO",false);
    if (!read_only_fetch) {
        auto zero32=b.addNewComponent<ConstantValue<32>>("FETCH_WRITE_DATA",0);
        auto read=b.addNewComponent<ConstantValue<3>>("FETCH_OPERATION",0);
        auto mask=b.addNewComponent<ConstantValue<4>>("FETCH_MASK",15);
        w.connect(zero32->getOutputPin<32>("OUT"),fetch->getInputPin<32>("WRITE_DATA"));
        w.connect(read->getOutputPin<3>("OUT"),fetch->getInputPin<3>("OP"));
        w.connect(mask->getOutputPin<4>("OUT"),fetch->getInputPin<4>("WRITE_MASK"));
    }
    w.connect(zero,operation->getInputPin("IN_1"));
    w.connect(zero,operation->getInputPin("IN_2"));
    w.connect(operation->getOutputPin<3>("OUT"),data->getInputPin<3>("OP"));
    w.connect(lanes->getOutputPin<32>("WORD_ADDR"),data->getInputPin<32>("ADDR"));
    w.connect(lanes->getOutputPin<32>("WORD_WRITE_DATA"),data->getInputPin<32>("WRITE_DATA"));
    w.connect(lanes->getOutputPin<4>("WRITE_MASK"),data->getInputPin<4>("WRITE_MASK"));
    w.connect(data->getOutputPin<32>("READ_DATA"),lanes->getInputPin<32>("WORD_DATA"));
    w.finish();
}
}
