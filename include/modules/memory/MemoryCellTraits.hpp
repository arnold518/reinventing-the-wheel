#pragma once

#include "components/ComponentBuilder.hpp"
#include "modules/basic/Latch.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/utility/Constant.hpp"

namespace circuit::memory_detail {

// Structural wiring only. Timing/state changes are implemented by the cells,
// not reimplemented here as a behavioral model. New cell interfaces get a
// specialization; the array's decoder and read mux need no cell-specific code.
struct CellInputs {
    std::shared_ptr<PinBase> data, enable, clock, reset, zero;
};

template<class Cell>
struct MemoryCellTraits;

template<>
struct MemoryCellTraits<FlipFlopCell> {
    static constexpr const char* array_name = "FFArray";
    static constexpr bool readonly = false;
    static constexpr LogicValue initial_value = LogicValue::UNKNOWN;

    template<class Connect>
    static std::shared_ptr<PinBase> buildCell(ComponentBuilder& b, const std::string& name,
                                             const CellInputs& inputs, Connect&& connect) {
        auto cell = b.add(circuit::families::FlipFlopCell, name);
        connect(inputs.enable, cell->getInputPin("WE"));
        connect(inputs.clock, cell->getInputPin("CLK"));
        connect(inputs.reset, cell->getInputPin("RST"));
        connect(inputs.data, cell->getInputPin("D"));
        return cell->getOutputPin("Q");
    }
};

template<>
struct MemoryCellTraits<GatedDLatch> {
    static constexpr const char* array_name = "SRAM";
    static constexpr bool readonly = false;
    static constexpr LogicValue initial_value = LogicValue::UNKNOWN;

    template<class Connect>
    static std::shared_ptr<PinBase> buildCell(ComponentBuilder& b, const std::string& name,
                                             const CellInputs& inputs, Connect&& connect) {
        auto cell = b.addNewComponent<GatedDLatch>(name);
        connect(inputs.enable, cell->getInputPin("EN"));
        // The existing digital SRAM array retains data on array reset.
        // GatedDLatch has an RST pin, so tie that pin low for this array port.
        connect(inputs.zero, cell->getInputPin("RST"));
        connect(inputs.data, cell->getInputPin("D"));
        return cell->getOutputPin("Q");
    }
};

template<>
struct MemoryCellTraits<ConstantValue<1>> {
    static constexpr const char* array_name = "ROM";
    static constexpr bool readonly = true;
    static constexpr LogicValue initial_value = LogicValue::LOW;

    template<class Connect>
    static std::shared_ptr<PinBase> buildCell(ComponentBuilder& b, const std::string& name,
                                             const CellInputs&, Connect&&) {
        // Preserve the current structural ROM's fixed zero image.
        return b.addNewComponent<ConstantValue<1>>(name, 0)->getOutputPin("OUT");
    }
};

} // namespace circuit::memory_detail
