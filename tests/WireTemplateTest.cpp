#include "TestHelpers.hpp"
#include "basic/Pin.hpp"
#include "basic/Wire.hpp"

int main() {
    Wire<8> bus("BUS");
    bus.setValue(0xA5);
    if (bus.getValue() != 0xA5) return 1;
    if (bus.getBit(0) != LogicValue::HIGH) return 1;
    if (bus.getBit(1) != LogicValue::LOW) return 1;
    if (bus.getBit(7) != LogicValue::HIGH) return 1;

    auto owner = std::make_shared<Component>("OWNER");
    Pin<8> pin("P", PinType::INPUT, owner);
    pin.setValueFromUInt64(0x3C);
    if (pin.getWidth() != 8) return 1;
    if (pin.getValueAsUInt64() != 0x3C) return 1;
    if (pin.getBit(2) != LogicValue::HIGH) return 1;

    return 0;
}
