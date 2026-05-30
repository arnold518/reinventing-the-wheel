#include "modules/memory/Memory32x32.hpp"

#include "basic/Pin.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/BehavioralRegister32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr size_t WordCount = 32;
constexpr size_t WordSelectWidth = 5;
constexpr size_t HighAddressStartBit = 7;

std::string wordName(size_t index) {
    return "WORD_" + std::to_string(index);
}
}

BEGIN_PINS(Memory32x32, IOComponent)
    INPUT_PIN_WIDTH("ADDR", 32)
    INPUT_PIN_WIDTH("WRITE_DATA", 32)
    INPUT_PIN("READ_EN")
    INPUT_PIN("WRITE_EN")
    INPUT_PIN_WIDTH("SIZE", 2)
    INPUT_PIN("SIGN_EXTEND")
    INPUT_PIN("CLK")
    INPUT_PIN("RST")
    OUTPUT_PIN_WIDTH("READ_DATA", 32)
    OUTPUT_PIN("READY")
    OUTPUT_PIN("FAULT")
END_PINS()

void Memory32x32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("ADDR_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("SIZE_SPLIT");
    builder.addNewComponent<BitJoiner<WordSelectWidth>>("WORD_SEL_JOIN");
    builder.addNewComponent<Mux32to1_32bit>("READ_MUX");
    builder.addNewComponent<ConstantValue<1, 1>>("READY_ONE", 1);
    builder.addNewComponent<Decoder5to32>("WRITE_DECODER");

    for (size_t word = 0; word < WordCount; ++word) {
        builder.addNewComponent<BehavioralRegister32>(wordName(word));
    }

    builder.addNewComponent<NOTGate>("NOT_ADDR0");
    builder.addNewComponent<NOTGate>("NOT_ADDR1");
    builder.addNewComponent<ANDGate>("WORD_ALIGNED_GATE");
    builder.addNewComponent<ORGate>("LOW_ADDR_OR");

    builder.addNewComponent<NOTGate>("NOT_SIZE0");
    builder.addNewComponent<ANDGate>("WORD_SIZE_GATE");
    builder.addNewComponent<NOTGate>("NOT_WORD_SIZE");
    builder.addNewComponent<ANDGate>("SIGN_EXTEND_TAP");
    builder.addNewComponent<ORGate>("INVALID_SIZE_OBSERVED");
    builder.addNewComponent<ORGate>("INVALID_SIZE_OR_ALIGN");
    builder.addNewComponent<ORGate>("INVALID_ACCESS_OR_HIGH");
    builder.addNewComponent<NOTGate>("NOT_HIGH_ADDR");
    builder.addNewComponent<ORGate>("ACCESS_GATE");
    builder.addNewComponent<ANDGate>("FAULT_GATE");

    builder.addNewComponent<ANDGate>("WRITE_WORD_GATE");
    builder.addNewComponent<ANDGate>("WRITE_ALIGNED_GATE");
    builder.addNewComponent<ANDGate>("WRITE_VALID_GATE");

    builder.addNewWire<32>(
        "ADDR_internal",
        getInputPin<32>("ADDR"),
        {builder.getInputPin<BitSplitter<32>, 32>("ADDR_SPLIT", "IN")});

    builder.addNewWire<2>(
        "SIZE_internal",
        getInputPin<2>("SIZE"),
        {builder.getInputPin<BitSplitter<2>, 2>("SIZE_SPLIT", "IN")});

    std::array<std::vector<std::shared_ptr<Pin<>>>, 32> addr_sinks;
    addr_sinks[0] = {
        builder.getInputPin<NOTGate>("NOT_ADDR0", "IN"),
        builder.getInputPin<ORGate>("LOW_ADDR_OR", "A"),
    };
    addr_sinks[1] = {
        builder.getInputPin<NOTGate>("NOT_ADDR1", "IN"),
        builder.getInputPin<ORGate>("LOW_ADDR_OR", "B"),
    };

    for (size_t bit = 0; bit < WordSelectWidth; ++bit) {
        addr_sinks[bit + 2] = {
            builder.getInputPin<BitJoiner<WordSelectWidth>>("WORD_SEL_JOIN", "IN_" + std::to_string(bit)),
        };
    }

    builder.addNewComponent<ORGate>("HIGH_ADDR_OR_7_8");
    addr_sinks[7].push_back(builder.getInputPin<ORGate>("HIGH_ADDR_OR_7_8", "A"));
    addr_sinks[8].push_back(builder.getInputPin<ORGate>("HIGH_ADDR_OR_7_8", "B"));
    std::string previous_high_or = "HIGH_ADDR_OR_7_8";
    for (size_t bit = HighAddressStartBit + 2; bit < 32; ++bit) {
        const auto gate_name = "HIGH_ADDR_OR_" + std::to_string(bit);
        builder.addNewComponent<ORGate>(gate_name);
        builder.addNewWire(
            previous_high_or + "_to_" + gate_name,
            builder.getOutputPin<ORGate>(previous_high_or, "OUT"),
            {builder.getInputPin<ORGate>(gate_name, "A")});
        addr_sinks[bit].push_back(builder.getInputPin<ORGate>(gate_name, "B"));
        previous_high_or = gate_name;
    }

    for (size_t bit = 0; bit < 32; ++bit) {
        builder.addNewWire(
            "ADDR_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<32>>("ADDR_SPLIT", "OUT_" + std::to_string(bit)),
            addr_sinks[bit]);
    }

    builder.addNewWire(
        "NOT_ADDR0_internal",
        builder.getOutputPin<NOTGate>("NOT_ADDR0", "OUT"),
        {builder.getInputPin<ANDGate>("WORD_ALIGNED_GATE", "A")});

    builder.addNewWire(
        "NOT_ADDR1_internal",
        builder.getOutputPin<NOTGate>("NOT_ADDR1", "OUT"),
        {builder.getInputPin<ANDGate>("WORD_ALIGNED_GATE", "B")});

    builder.addNewWire(
        "WORD_ALIGNED_internal",
        builder.getOutputPin<ANDGate>("WORD_ALIGNED_GATE", "OUT"),
        {builder.getInputPin<ANDGate>("WRITE_ALIGNED_GATE", "B")});

    builder.addNewWire(
        "LOW_ADDR_ANY_internal",
        builder.getOutputPin<ORGate>("LOW_ADDR_OR", "OUT"),
        {builder.getInputPin<ORGate>("INVALID_SIZE_OR_ALIGN", "B")});

    builder.addNewWire(
        "HIGH_ADDR_ANY_internal",
        builder.getOutputPin<ORGate>(previous_high_or, "OUT"),
        {builder.getInputPin<NOTGate>("NOT_HIGH_ADDR", "IN"),
         builder.getInputPin<ORGate>("INVALID_ACCESS_OR_HIGH", "B")});

    std::array<std::vector<std::shared_ptr<Pin<>>>, 2> size_sinks;
    size_sinks[0] = {builder.getInputPin<NOTGate>("NOT_SIZE0", "IN")};
    size_sinks[1] = {builder.getInputPin<ANDGate>("WORD_SIZE_GATE", "A")};

    for (size_t bit = 0; bit < 2; ++bit) {
        builder.addNewWire(
            "SIZE_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<2>>("SIZE_SPLIT", "OUT_" + std::to_string(bit)),
            size_sinks[bit]);
    }

    builder.addNewWire(
        "NOT_SIZE0_internal",
        builder.getOutputPin<NOTGate>("NOT_SIZE0", "OUT"),
        {builder.getInputPin<ANDGate>("WORD_SIZE_GATE", "B")});

    builder.addNewWire(
        "WORD_SIZE_internal",
        builder.getOutputPin<ANDGate>("WORD_SIZE_GATE", "OUT"),
        {builder.getInputPin<NOTGate>("NOT_WORD_SIZE", "IN"),
         builder.getInputPin<ANDGate>("WRITE_WORD_GATE", "B")});

    builder.addNewWire(
        "NOT_WORD_SIZE_internal",
        builder.getOutputPin<NOTGate>("NOT_WORD_SIZE", "OUT"),
        {builder.getInputPin<ANDGate>("SIGN_EXTEND_TAP", "B"),
         builder.getInputPin<ORGate>("INVALID_SIZE_OBSERVED", "A")});

    builder.addNewWire(
        "SIGN_EXTEND_internal",
        getInputPin("SIGN_EXTEND"),
        {builder.getInputPin<ANDGate>("SIGN_EXTEND_TAP", "A")});

    builder.addNewWire(
        "SIGN_EXTEND_TAP_internal",
        builder.getOutputPin<ANDGate>("SIGN_EXTEND_TAP", "OUT"),
        {builder.getInputPin<ORGate>("INVALID_SIZE_OBSERVED", "B")});

    builder.addNewWire(
        "INVALID_SIZE_internal",
        builder.getOutputPin<ORGate>("INVALID_SIZE_OBSERVED", "OUT"),
        {builder.getInputPin<ORGate>("INVALID_SIZE_OR_ALIGN", "A")});

    builder.addNewWire(
        "INVALID_SIZE_OR_ALIGN_internal",
        builder.getOutputPin<ORGate>("INVALID_SIZE_OR_ALIGN", "OUT"),
        {builder.getInputPin<ORGate>("INVALID_ACCESS_OR_HIGH", "A")});

    builder.addNewWire(
        "INVALID_ACCESS_internal",
        builder.getOutputPin<ORGate>("INVALID_ACCESS_OR_HIGH", "OUT"),
        {builder.getInputPin<ANDGate>("FAULT_GATE", "B")});

    builder.addNewWire(
        "READ_EN_internal",
        getInputPin("READ_EN"),
        {builder.getInputPin<ORGate>("ACCESS_GATE", "A"),
         builder.getInputPin<ConstantValue<1, 1>>("READY_ONE", "TRIGGER")});

    builder.addNewWire(
        "WRITE_EN_internal",
        getInputPin("WRITE_EN"),
        {builder.getInputPin<ORGate>("ACCESS_GATE", "B"),
         builder.getInputPin<ANDGate>("WRITE_WORD_GATE", "A")});

    builder.addNewWire(
        "ACCESS_internal",
        builder.getOutputPin<ORGate>("ACCESS_GATE", "OUT"),
        {builder.getInputPin<ANDGate>("FAULT_GATE", "A")});

    builder.addNewWire(
        "FAULT_internal",
        builder.getOutputPin<ANDGate>("FAULT_GATE", "OUT"),
        {getOutputPin("FAULT")});

    builder.addNewWire(
        "READY_internal",
        builder.getOutputPin<ConstantValue<1, 1>>("READY_ONE", "OUT"),
        {getOutputPin("READY")});

    builder.addNewWire(
        "NOT_HIGH_ADDR_internal",
        builder.getOutputPin<NOTGate>("NOT_HIGH_ADDR", "OUT"),
        {builder.getInputPin<ANDGate>("WRITE_VALID_GATE", "B")});

    builder.addNewWire(
        "WRITE_WORD_internal",
        builder.getOutputPin<ANDGate>("WRITE_WORD_GATE", "OUT"),
        {builder.getInputPin<ANDGate>("WRITE_ALIGNED_GATE", "A")});

    builder.addNewWire(
        "WRITE_ALIGNED_internal",
        builder.getOutputPin<ANDGate>("WRITE_ALIGNED_GATE", "OUT"),
        {builder.getInputPin<ANDGate>("WRITE_VALID_GATE", "A")});

    builder.addNewWire(
        "WRITE_VALID_internal",
        builder.getOutputPin<ANDGate>("WRITE_VALID_GATE", "OUT"),
        {builder.getInputPin<Decoder5to32>("WRITE_DECODER", "ENABLE")});

    std::vector<std::shared_ptr<Pin<32>>> write_data_sinks;
    std::vector<std::shared_ptr<Pin<>>> clk_sinks;
    std::vector<std::shared_ptr<Pin<>>> rst_sinks;
    write_data_sinks.reserve(WordCount);
    clk_sinks.reserve(WordCount);
    rst_sinks.reserve(WordCount);

    for (size_t word = 0; word < WordCount; ++word) {
        const auto name = wordName(word);
        write_data_sinks.push_back(builder.getInputPin<BehavioralRegister32, 32>(name, "D"));
        clk_sinks.push_back(builder.getInputPin<BehavioralRegister32>(name, "CLK"));
        rst_sinks.push_back(builder.getInputPin<BehavioralRegister32>(name, "RST"));

        builder.addNewWire(
            "WORD" + std::to_string(word) + "_WE_internal",
            builder.getOutputPin<Decoder5to32>("WRITE_DECODER", "OUT" + std::to_string(word)),
            {builder.getInputPin<BehavioralRegister32>(name, "WE")});

        builder.addNewWire<32>(
            name + "_to_READ_MUX",
            builder.getOutputPin<BehavioralRegister32, 32>(name, "Q"),
            {builder.getInputPin<Mux32to1_32bit, 32>("READ_MUX", "IN" + std::to_string(word))});
    }

    builder.addNewWire<32>(
        "WRITE_DATA_internal",
        getInputPin<32>("WRITE_DATA"),
        write_data_sinks);

    builder.addNewWire("CLK_internal", getInputPin("CLK"), clk_sinks);
    builder.addNewWire("RST_internal", getInputPin("RST"), rst_sinks);

    builder.addNewWire<WordSelectWidth>(
        "WORD_SEL_internal",
        builder.getOutputPin<BitJoiner<WordSelectWidth>, WordSelectWidth>("WORD_SEL_JOIN", "OUT"),
        {builder.getInputPin<Mux32to1_32bit, WordSelectWidth>("READ_MUX", "SEL"),
         builder.getInputPin<Decoder5to32, WordSelectWidth>("WRITE_DECODER", "ADDR")});

    builder.addNewWire<32>(
        "READ_DATA_internal",
        builder.getOutputPin<Mux32to1_32bit, 32>("READ_MUX", "OUT"),
        {getOutputPin<32>("READ_DATA")});
}
