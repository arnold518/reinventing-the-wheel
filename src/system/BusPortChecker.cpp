#include "system/BusPortChecker.hpp"
#include "system/SystemRegistration.hpp"
#include "modules/basic/WordGate.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralLogic.hpp"
#include "modules/device/DeviceGroups.hpp"
#include <stdexcept>

namespace circuit::system {

std::string portCheckerContractId(size_t width) { return "bus.port-checker." + std::to_string(width); }

void busPortCheckerPins(IOComponent* self, size_t width) {
    for (const auto* n : {"CLK", "RST", "REQ_VALID", "REQ_READY", "RSP_VALID", "RSP_READY"}) self->addPin(n, PinType::INPUT);
    self->addPinDynamic("REQ_OP", PinType::INPUT, 3);
    self->addPinDynamic("REQ_ADDR", PinType::INPUT, 32);
    self->addPinDynamic("REQ_WRITE_DATA", PinType::INPUT, width);
    self->addPinDynamic("REQ_WRITE_MASK", PinType::INPUT, width / 8);
    self->addPinDynamic("RSP_READ_DATA", PinType::INPUT, width);
    self->addPinDynamic("RSP_STATUS", PinType::INPUT, 3);
    for (const auto* n : {"REQUEST_CHANGED", "RESPONSE_CHANGED", "EXTRA_RESPONSE", "VIOLATION"}) self->addPin(n, PinType::OUTPUT);
}

BusPortChecker::BusPortChecker(std::string name, size_t width)
    : IOComponent(std::move(name), [width](IOComponent* self) { busPortCheckerPins(self, width); }), width_(width),
      type_("BusPortChecker<" + std::to_string(width) + ">") {}

void BusPortChecker::buildInternals(ComponentBuilder& b) {
    using device::detail::Fields;
    using device::detail::group;
    const auto w = std::to_string(width_);
    // "Changed since the last edge": every field is captured at every edge
    // and compared with its value now, one XOR word per field; CHANGED is
    // the OR of all differences.
    const auto watch = [&](const std::string& name, const std::string& type, Fields fields) {
        Fields inputs{{"CLK", 1}, {"RST", 1}};
        inputs.insert(inputs.end(), fields.begin(), fields.end());
        return group(b, name, type, inputs, {{"CHANGED", 1}}, [fields](IOComponent& self, ComponentBuilder& gates) {
            structural::Logic l(gates);
            const auto clk = self.getInputPinDynamic("CLK"), rst = self.getInputPinDynamic("RST");
            structural::V differences;
            for (const auto& [field, width] : fields) {
                const auto pin = self.getInputPinDynamic(field);
                auto held = l.reg("SEEN_" + field, width, clk, rst, l.bit(true));
                l.wire(pin, held->getInputPinDynamic("D"));
                if (width == 1) { differences.push_back(l.lx({pin, held->getOutputPinDynamic("Q")})); continue; }
                auto x = gates.add(circuit::families::wordGate(circuit::word::GateKind::Xor, width), "DIFFER_" + field);
                l.wire(pin, x->getInputPinDynamic("A"));
                l.wire(held->getOutputPinDynamic("Q"), x->getInputPinDynamic("B"));
                differences.push_back(l.any(l.split(x->getOutputPinDynamic("OUT"))));
            }
            l.wire(l.any(differences), self.getOutputPin("CHANGED"));
            l.finish();
        });
    };
    auto request = watch("REQUEST_WATCH", "BusPortCheckerWatch<request," + w + ">",
        {{"REQ_OP", 3}, {"REQ_ADDR", 32}, {"REQ_WRITE_DATA", width_}, {"REQ_WRITE_MASK", width_ / 8}});
    auto response = watch("RESPONSE_WATCH", "BusPortCheckerWatch<response," + w + ">",
        {{"RSP_READ_DATA", width_}, {"RSP_STATUS", 3}});
    // Was the request (response) waiting at the last edge? Is a response
    // owed? Outstanding is set when a request is taken and cleared when a
    // single-word response is taken (a burst keeps it until the next request).
    auto handshake = group(b, "HANDSHAKE", "BusPortCheckerHandshake",
        {{"CLK", 1}, {"RST", 1}, {"REQ_VALID", 1}, {"REQ_READY", 1}, {"REQ_OP", 3}, {"RSP_VALID", 1}, {"RSP_READY", 1}},
        {{"REQUEST_WAITING", 1}, {"RESPONSE_WAITING", 1}, {"EXTRA", 1}},
        [](IOComponent& self, ComponentBuilder& gates) {
            structural::Logic l(gates);
            const auto in = [&](const char* n) { return self.getInputPinDynamic(n); };
            const auto clk = in("CLK"), rst = in("RST");
            auto request_waiting = l.reg("REQUEST_WAITING", 1, clk, rst, l.bit(true));
            l.wire(l.land({in("REQ_VALID"), l.inv(in("REQ_READY"))}), request_waiting->getInputPin("D"));
            auto response_waiting = l.reg("RESPONSE_WAITING", 1, clk, rst, l.bit(true));
            l.wire(l.land({in("RSP_VALID"), l.inv(in("RSP_READY"))}), response_waiting->getInputPin("D"));
            const auto request_taken = l.land({in("REQ_VALID"), in("REQ_READY")});
            const auto response_taken = l.land({in("RSP_VALID"), in("RSP_READY")});
            auto outstanding = l.reg("OUTSTANDING", 1, clk, rst, l.bit(true));
            auto burst = l.reg("BURST", 1, clk, rst, request_taken);
            const auto op = l.split(in("REQ_OP"));
            l.wire(l.land({op[2], l.land({l.inv(op[1]), l.inv(op[0])})}), burst->getInputPin("D"));
            const auto owed = l.lor({outstanding->getOutputPin("Q"), request_taken});
            l.wire(l.lor({request_taken, l.land({outstanding->getOutputPin("Q"), l.lor({l.inv(response_taken), burst->getOutputPin("Q")})})}),
                   outstanding->getInputPin("D"));
            l.wire(request_waiting->getOutputPin("Q"), self.getOutputPin("REQUEST_WAITING"));
            l.wire(response_waiting->getOutputPin("Q"), self.getOutputPin("RESPONSE_WAITING"));
            l.wire(l.land({response_taken, l.inv(owed)}), self.getOutputPin("EXTRA"));
            l.finish();
        });
    // Sticky flags, captured at the edge where the violation is seen. A
    // waiting transfer that is withdrawn or changes any field is a violation.
    auto flags = group(b, "FLAGS", "BusPortCheckerFlags",
        {{"CLK", 1}, {"RST", 1}, {"REQ_VALID", 1}, {"RSP_VALID", 1}, {"REQUEST_WAITING", 1}, {"RESPONSE_WAITING", 1},
         {"REQUEST_FIELDS_CHANGED", 1}, {"RESPONSE_FIELDS_CHANGED", 1}, {"EXTRA", 1}},
        {{"REQUEST_CHANGED", 1}, {"RESPONSE_CHANGED", 1}, {"EXTRA_RESPONSE", 1}, {"VIOLATION", 1}},
        [](IOComponent& self, ComponentBuilder& gates) {
            structural::Logic l(gates);
            const auto in = [&](const char* n) { return self.getInputPinDynamic(n); };
            const auto request_changed = l.land({in("REQUEST_WAITING"), l.lor({l.inv(in("REQ_VALID")), in("REQUEST_FIELDS_CHANGED")})});
            const auto response_changed = l.land({in("RESPONSE_WAITING"), l.lor({l.inv(in("RSP_VALID")), in("RESPONSE_FIELDS_CHANGED")})});
            structural::V held;
            for (const auto& [name, now] : std::vector<std::pair<std::string, structural::P>>{
                     {"REQUEST_CHANGED", request_changed}, {"RESPONSE_CHANGED", response_changed}, {"EXTRA_RESPONSE", in("EXTRA")}}) {
                auto flag = l.reg(name + "_FLAG", 1, in("CLK"), in("RST"), l.bit(true));
                l.wire(l.lor({now, flag->getOutputPin("Q")}), flag->getInputPin("D"));
                l.wire(flag->getOutputPin("Q"), self.getOutputPin(name));
                held.push_back(flag->getOutputPin("Q"));
            }
            l.wire(l.any(held), self.getOutputPin("VIOLATION"));
            l.finish();
        });
    structural::Logic l(b);
    const auto pass = [&](const std::shared_ptr<IOComponent>& part, std::initializer_list<const char*> names) {
        for (const auto* n : names) l.wire(getInputPinDynamic(n), part->getInputPinDynamic(n));
    };
    pass(request, {"CLK", "RST", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK"});
    pass(response, {"CLK", "RST", "RSP_READ_DATA", "RSP_STATUS"});
    pass(handshake, {"CLK", "RST", "REQ_VALID", "REQ_READY", "REQ_OP", "RSP_VALID", "RSP_READY"});
    pass(flags, {"CLK", "RST", "REQ_VALID", "RSP_VALID"});
    for (const auto* n : {"REQUEST_WAITING", "RESPONSE_WAITING", "EXTRA"}) l.wire(handshake->getOutputPin(n), flags->getInputPin(n));
    l.wire(request->getOutputPin("CHANGED"), flags->getInputPin("REQUEST_FIELDS_CHANGED"));
    l.wire(response->getOutputPin("CHANGED"), flags->getInputPin("RESPONSE_FIELDS_CHANGED"));
    for (const auto* n : {"REQUEST_CHANGED", "RESPONSE_CHANGED", "EXTRA_RESPONSE", "VIOLATION"}) l.wire(flags->getOutputPin(n), getOutputPin(n));
    l.finish();
}

void registerPortCheckers(ComponentCatalog& catalog) {
    for (size_t width : {8u, 16u, 32u}) {
        SystemPart<size_t> part;
        part.id = portCheckerContractId(width);
        part.type = "BusPortChecker<" + std::to_string(width) + ">";
        part.test = "BusPortCheckerTest";
        part.parse = [width](const ParameterMap& p) {
            if (!p.empty()) throw std::invalid_argument("A port checker takes no parameters");
            return width;
        };
        part.pins = [](const size_t& w) -> IOComponent::PinInitFunction { return [w](IOComponent* self) { busPortCheckerPins(self, w); }; };
        part.structural = [](const std::string& name, const size_t& w, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<BusPortChecker>(context, name, w);
        };
        registerSystemPart(catalog, std::move(part));
    }
}

} // namespace circuit::system
