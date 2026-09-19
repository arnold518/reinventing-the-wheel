#pragma once

#include "components/IOComponent.hpp"
#include "modules/memory/MemoryArrayInterface.hpp"
#include "modules/memory/MemoryCellTraits.hpp"
#include "modules/memory/FFMemory.hpp"
#include <stdexcept>
#include <string>
#include <type_traits>

// Configurable structural arrays, with exactly the container pins.
// SRAM uses gated static latches (a digital abstraction, not a 6T cell model).
// ROM contains a fixed zero image. Only known-binary stable accesses are the
// structural/behavioral equivalence domain; transistor timing is not modeled.
template<size_t Width, class Cell>
class MemoryArray : public IOComponent {
    static_assert(std::is_base_of_v<IOComponent, Cell>, "MemoryArray requires a cell component type, not a behavioral model");
    using Traits = circuit::memory_detail::MemoryCellTraits<Cell>;
public:
    using CellType = Cell;
    // A construction guard, not a hardware capacity limit. Larger memories use
    // MemoryContainer's compact behavioral implementation of the same port.
    static constexpr size_t MaxStructuralCells = 4096;
    static constexpr size_t MaxDepthWords = MaxStructuralCells / Width;
    inline static const std::string TypeName = std::string(Traits::array_name) + std::to_string(Width);
    const char* getTypeName() const override { return TypeName.c_str(); }
    explicit MemoryArray(std::string name, MemoryConfig config = MemoryConfig{
                            .initial_value = Traits::initial_value})
        : IOComponent(std::move(name), defineMemoryArrayPins<Width>), config_(config) {
        if (!supportsConfiguration(config))
            throw std::invalid_argument("MemoryArray requires 1.." + std::to_string(MaxDepthWords) +
                " words, default initial fill, and read_delay_ticks=1; use MemoryContainer for larger/custom memories");
    }
    static bool supportsConfiguration(const MemoryConfig& config) noexcept {
        return config.depth_words > 0 && config.depth_words <= MaxDepthWords &&
               config.read_delay_ticks == 1 &&
               config.initial_value == Traits::initial_value;
    }
    const MemoryConfig& configuration() const noexcept { return config_; }
    void buildInternals(ComponentBuilder& builder) override;
private:
    MemoryConfig config_;
};

// Keep callers using the original template source-compatible.
template<size_t Width>
class MemoryArray<Width, FlipFlopCell> : public FFMemory<Width> {
public:
    using FFMemory<Width>::FFMemory;
};

#include "modules/memory/MemoryArray.tpp"
