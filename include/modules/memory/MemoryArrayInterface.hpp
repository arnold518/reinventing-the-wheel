#pragma once

#include "components/IOComponent.hpp"

// Shared geometry and port, independent of either a cell implementation or a
// behavioral model. Width fixes the pins; depth is a construction parameter.
struct MemoryConfig {
    size_t depth_words = 4;
    size_t read_delay_ticks = 1;
    LogicValue initial_value = LogicValue::UNKNOWN;
    bool record_history = true;
};

template<size_t Width>
void defineMemoryArrayPins(IOComponent* self) {
    static_assert(Width == 8 || Width == 16 || Width == 32, "Supported memory widths: 8, 16, 32");
    self->addPin<32>("WORD_ADDR", PinType::INPUT);
    self->addPin<Width>("WRITE_DATA", PinType::INPUT);
    self->addPin<Width / 8>("WRITE_MASK", PinType::INPUT);
    self->addPin("WE", PinType::INPUT);
    self->addPin("READ_EN", PinType::INPUT);
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin<Width>("READ_DATA", PinType::OUTPUT);
    self->addPin("FAULT", PinType::OUTPUT);
}
