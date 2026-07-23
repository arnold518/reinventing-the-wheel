#include "modules/memory/Register32BitCellArray.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "modules/memory/MemoryBitDirect.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <memory>
#include <string>
#include <vector>

BEGIN_PINS(Register32BitCellArray, IOComponent)
    INPUT_PIN_WIDTH("D", 32)
    INPUT_PIN("WE")
    INPUT_PIN("CLK")
    INPUT_PIN("RST")
    OUTPUT_PIN_WIDTH("Q", 32)
END_PINS()

void Register32BitCellArray::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("D_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("Q_JOIN");

    builder.addNewWire<32>(
        "D_bus_internal",
        getInputPin<32>("D"),
        {builder.getInputPin<BitSplitter<32>, 32>("D_SPLIT", "IN")});

    std::vector<std::shared_ptr<Pin<>>> we_sinks;
    std::vector<std::shared_ptr<Pin<>>> clk_sinks;
    std::vector<std::shared_ptr<Pin<>>> rst_sinks;
    we_sinks.reserve(32);
    clk_sinks.reserve(32);
    rst_sinks.reserve(32);

    for (size_t bit = 0; bit < 32; ++bit) {
        const auto bit_text = std::to_string(bit);
        const auto cell_name = "BIT_" + bit_text;
        builder.addNewComponent<MemoryBitDirect>(cell_name);

        builder.addNewWire(
            "D_bit_" + bit_text,
            builder.getOutputPin<BitSplitter<32>>("D_SPLIT", "OUT_" + bit_text),
            {builder.getInputPin<MemoryBitDirect>(cell_name, "D")});

        builder.addNewWire(
            "Q_bit_" + bit_text,
            builder.getOutputPin<MemoryBitDirect>(cell_name, "Q"),
            {builder.getInputPin<BitJoiner<32>>("Q_JOIN", "IN_" + bit_text)});

        we_sinks.push_back(builder.getInputPin<MemoryBitDirect>(cell_name, "WE"));
        clk_sinks.push_back(builder.getInputPin<MemoryBitDirect>(cell_name, "CLK"));
        rst_sinks.push_back(builder.getInputPin<MemoryBitDirect>(cell_name, "RST"));
    }

    builder.addNewWire("WE_internal", getInputPin("WE"), we_sinks);
    builder.addNewWire("CLK_internal", getInputPin("CLK"), clk_sinks);
    builder.addNewWire("RST_internal", getInputPin("RST"), rst_sinks);

    builder.addNewWire<32>(
        "Q_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("Q_JOIN", "OUT"),
        {getOutputPin<32>("Q")});
}
