#include "modules/basic/Logic8.hpp"

namespace {
constexpr uint8_t mask8(uint64_t value) {
    return static_cast<uint8_t>(value & 0xFFU);
}
}

AND8::AND8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void AND8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", mask8(getInputValueAsUInt64("A") & getInputValueAsUInt64("B")), current_time);
}

OR8::OR8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void OR8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", mask8(getInputValueAsUInt64("A") | getInputValueAsUInt64("B")), current_time);
}

XOR8::XOR8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void XOR8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", mask8(getInputValueAsUInt64("A") ^ getInputValueAsUInt64("B")), current_time);
}

NOT8::NOT8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void NOT8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", mask8(~getInputValueAsUInt64("A")), current_time);
}

NAND8::NAND8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void NAND8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", mask8(~(getInputValueAsUInt64("A") & getInputValueAsUInt64("B"))), current_time);
}

NOR8::NOR8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void NOR8::evaluate(size_t current_time, Simulator& simulator) {
    _updateOutputWire<8>(simulator, "OUT", mask8(~(getInputValueAsUInt64("A") | getInputValueAsUInt64("B"))), current_time);
}
