#pragma once

#include "modules/memory/MemoryArrayInterface.hpp"
#include "modules/memory/FFStorageBank.hpp"
#include "modules/memory/MemoryArrayBlocks.hpp"

namespace circuit::families {
// Stable public device identity; the catalog chooses structure or compact storage.
template<size_t Width> const ComponentFamily& FFMemory();
}

// Structural implementation of the FF memory device. No bus/controller inside.
template<size_t Width>
class FFMemory : public IOComponent {
public:
    using CellType = FlipFlopCell;
    static constexpr size_t MaxStructuralCells = 4096;
    static constexpr size_t MaxDepthWords = MaxStructuralCells / Width;
    inline static const std::string TypeName = "FFMemory" + std::to_string(Width);
    const char* getTypeName() const override { return TypeName.c_str(); }
    explicit FFMemory(std::string name, MemoryConfig config = {})
        : IOComponent(std::move(name), defineMemoryArrayPins<Width>), config_(config) {
        if (!supportsConfiguration(config))
            throw std::invalid_argument("Structural FFMemory requires 1..4096/Width words, unknown initial fill, and read_delay_ticks=1");
    }
    static bool supportsConfiguration(const MemoryConfig& config) noexcept {
        return config.depth_words > 0 && config.depth_words <= MaxDepthWords &&
               config.read_delay_ticks == 1 && config.initial_value == LogicValue::UNKNOWN;
    }
    const MemoryConfig& configuration() const noexcept { return config_; }
    void buildInternals(ComponentBuilder& b) override {
        using namespace circuit::memory_detail;
        StructuralWiring w(b);
        const auto depth = config_.depth_words;
        auto decoder = b.addNewComponent<MemoryAddressDecoder>("ADDRESS_DECODE", depth, false);
        auto control = b.addNewComponent<MemoryAccessControl>("ACCESS_CONTROL", depth, false);
        // The legacy context-free builder does not forward family parameters.
        // Preserve direct construction at arbitrary depths as well as catalog selection.
        std::shared_ptr<IOComponent> storage = b.getBuildContext()
            ? b.add(circuit::families::FFStorageBank<Width>(), "STORAGE", {{"depth_words", std::to_string(depth)}})
            : b.addNewComponent<FFStorageBank<Width>>("STORAGE", depth);
        auto read = b.addNewComponent<MemoryReadSelector<Width>>("READ_SELECT", depth);
        w.connect(this->template getInputPin<32>("WORD_ADDR"), decoder->template getInputPin<32>("WORD_ADDR"));
        w.connect(decoder->getOutputPin("IN_RANGE"), control->getInputPin("IN_RANGE"));
        for (const auto* pin : {"WE", "READ_EN"}) w.connect(this->getInputPin(pin), control->getInputPin(pin));
        w.connect(control->getOutputPin("FAULT"), this->getOutputPin("FAULT"));
        w.connect(control->getOutputPin("READ_VALID"), read->getInputPin("READ_VALID"));
        for (size_t bit = 0; bit < arrayAddressBits(depth); ++bit) {
            const auto pin = "SELECT_" + std::to_string(bit);
            w.connect(decoder->getOutputPin(pin), read->getInputPin(pin));
        }
        for (const auto* pin : {"WRITE_DATA", "WRITE_MASK", "CLK", "RST"})
            w.connect(this->getInputPinDynamic(pin), storage->getInputPinDynamic(pin));
        for (size_t word = 0; word < depth; ++word) {
            const auto suffix = std::to_string(word);
            w.connect(decoder->getOutputPin("VALID_" + suffix), control->getInputPin("VALID_" + suffix));
            w.connect(control->getOutputPin("WRITE_" + suffix), storage->getInputPin("WRITE_" + suffix));
            w.connect(storage->template getOutputPin<Width>("WORD_" + suffix), read->template getInputPin<Width>("WORD_" + suffix));
        }
        w.connect(read->template getOutputPin<Width>("READ_DATA"), this->template getOutputPin<Width>("READ_DATA"));
        w.finish();
    }
private:
    MemoryConfig config_;
};
