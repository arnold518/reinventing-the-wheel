#include "modules/device/DeviceComponents.hpp"
#include "modules/device/RegisterTable.hpp"
#include "modules/basic/WordMux.hpp"
#include "components/ComponentBuilder.tpp"
#include "DeviceGroups.hpp"

using namespace circuit::device::detail;

namespace {
void ledPins(IOComponent* p) {
    circuit::memory::protocolPins(p, 32);
    p->addPin<16>("LEDS", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily LedPanelDevice{"device.leds", "LedPanel", ledPins,
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<::LedPanel>(c, n); },
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<DeviceBehavioral<::LedPanel>>(c, n); }};
}

LedPanel::LedPanel(std::string n) : IOComponent(std::move(n), ledPins) {}

void LedPanel::buildInternals(ComponentBuilder& b) {
    Logic l(b);
    auto port = devicePort(*this, b, l);
    auto registers = circuit::device::buildRegisterFrontEnd(b, circuit::device::deviceRegisterTable("device.leds", {}));
    frontEnd(l, port, registers);
    inputs(l, *this, registers, {"CLK", "RST"});
    l.wire(l.bit(true), registers->getInputPin("READY"));
    // A free-running counter; bit k of it toggles every 2^k cycles.
    auto counter = devicePart(*this, b, circuit::families::clockedCounter(32), "BLINK_COUNTER");
    inputs(l, *this, counter, {"CLK", "RST"});
    l.wire(l.bit(true), counter->getInputPin("ENABLE"));
    auto drive = group(b, "LIGHTS", "LedBlink", {{"LEDS", 16}, {"BLINK", 5}, {"COUNT", 32}}, {{"OUT", 16}},
        [](IOComponent& self, ComponentBuilder& gates) {
            circuit::structural::Logic c(gates);
            // Shown when BLINK is 0, or when counter bit BLINK is low.
            auto phase = gates.add(circuit::word::mux(32, 1), "PHASE");
            c.wire(input(self, "BLINK"), phase->getInputPinDynamic("SEL"));
            auto count = c.split(input(self, "COUNT"));
            for (size_t i = 0; i < 32; ++i) c.wire(count[i], phase->getInputPinDynamic("IN" + std::to_string(i)));
            auto steady = c.inv(c.any(c.split(input(self, "BLINK"))));
            auto lit = c.lor({steady, c.inv(phase->getOutputPinDynamic("OUT"))});
            c.drive(c.mask(c.split(input(self, "LEDS")), lit), output(self, "OUT"));
            c.finish();
        });
    l.wire(registers->getOutputPin<16>("LEDS"), drive->getInputPin<16>("LEDS"));
    l.wire(registers->getOutputPin<5>("BLINK"), drive->getInputPin<5>("BLINK"));
    l.wire(counter->getOutputPin<32>("COUNT"), drive->getInputPin<32>("COUNT"));
    l.wire(drive->getOutputPin<16>("OUT"), getOutputPin<16>("LEDS"));
    l.finish();
}
