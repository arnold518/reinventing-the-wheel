#include "system/Bridge.hpp"
#include "system/SystemRegistration.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralLogic.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <stdexcept>

namespace circuit::system {
namespace {
constexpr const char* requests[] = {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"};
constexpr const char* responses[] = {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"};
}

std::string bridgeContractId(size_t width) { return "bus.bridge." + std::to_string(width); }

void bridgePins(IOComponent* self, size_t width) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    memory::protocolPins(self, width, "S_");
    memory::protocolPins(self, width, "M_", true);
}

Bridge::Bridge(std::string name, BridgeConfig config)
    : IOComponent(std::move(name), [w = config.width](IOComponent* self) { bridgePins(self, w); }), config_(config),
      type_(std::string(config.registered ? "RegisteredBridge<" : "BusBridge<") + std::to_string(config.width) + ">") {}

void Bridge::buildInternals(ComponentBuilder& b) {
    structural::Logic l(b);
    // The lower bus has its own clock and reset from the system; the port's
    // clock and reset outputs are unused and held low (a clock passed on as
    // data could not be timed).
    l.wire(l.bit(false), getOutputPin("M_CLK"));
    l.wire(l.bit(false), getOutputPin("M_RST"));
    if (!config_.registered) {
        for (const auto* n : requests) l.wire(getInputPinDynamic(std::string("S_") + n), getOutputPinDynamic(std::string("M_") + n));
        for (const auto* n : responses) l.wire(getInputPinDynamic(std::string("M_") + n), getOutputPinDynamic(std::string("S_") + n));
        l.finish();
        return;
    }
    const auto w = config_.width;
    const auto clk = getInputPin("CLK");
    const auto rst = getInputPin("RST");
    const auto in = [&](const std::string& n) { return getInputPinDynamic(n); };
    const auto out = [&](const std::string& n) { return getOutputPinDynamic(n); };
    // Three state bits: a request held for M_, a request sent and waiting,
    // a response held for S_. Idle when all are clear.
    auto hold_request = l.reg("HOLD_REQUEST", 1, clk, rst, l.bit(true));
    auto waiting = l.reg("WAITING", 1, clk, rst, l.bit(true));
    auto hold_response = l.reg("HOLD_RESPONSE", 1, clk, rst, l.bit(true));
    const auto holding = hold_request->getOutputPin("Q"), wait = waiting->getOutputPin("Q"), answer = hold_response->getOutputPin("Q");
    const auto idle = l.land({l.inv(l.lor({holding, wait})), l.inv(answer)});
    const auto ready = l.land({idle, l.inv(rst)});
    const auto accept = l.land({in("S_REQ_VALID"), ready});
    const auto sent = l.land({holding, in("M_REQ_READY")});
    const auto got = l.land({wait, in("M_RSP_VALID")});
    const auto taken = l.land({answer, in("S_RSP_READY")});
    l.wire(l.lor({accept, l.land({holding, l.inv(in("M_REQ_READY"))})}), hold_request->getInputPin("D"));
    l.wire(l.lor({sent, l.land({wait, l.inv(in("M_RSP_VALID"))})}), waiting->getInputPin("D"));
    l.wire(l.lor({got, l.land({answer, l.inv(taken)})}), hold_response->getInputPin("D"));
    // The request and response words, captured when taken.
    for (const auto& [field, width] : std::vector<std::pair<std::string, size_t>>{{"REQ_OP", 3}, {"REQ_ADDR", 32}, {"REQ_WRITE_DATA", w}, {"REQ_WRITE_MASK", w / 8}}) {
        auto r = l.reg(field, width, clk, rst, accept);
        l.wire(in("S_" + field), r->getInputPinDynamic("D"));
        l.wire(r->getOutputPinDynamic("Q"), out("M_" + field));
    }
    for (const auto& [field, width] : std::vector<std::pair<std::string, size_t>>{{"RSP_READ_DATA", w}, {"RSP_STATUS", 3}}) {
        auto r = l.reg(field, width, clk, rst, got);
        l.wire(in("M_" + field), r->getInputPinDynamic("D"));
        l.wire(r->getOutputPinDynamic("Q"), out("S_" + field));
    }
    l.wire(ready, out("S_REQ_READY"));
    l.wire(holding, out("M_REQ_VALID"));
    l.wire(wait, out("M_RSP_READY"));
    l.wire(answer, out("S_RSP_VALID"));
    l.finish();
}

void registerBridges(ComponentCatalog& catalog) {
    for (size_t width : {8u, 16u, 32u}) {
        SystemPart<BridgeConfig> part;
        part.id = bridgeContractId(width);
        part.type = "BusBridge<" + std::to_string(width) + ">";
        part.test = "BusBridgeScenarioTest";
        part.equivalence_test = "BusBridgeScenarioTest";
        part.parse = [width](const ParameterMap& p) {
            BridgeConfig c;
            c.width = width;
            for (const auto& [key, value] : p) {
                if (key != "registered" || (value != "0" && value != "1")) throw std::invalid_argument("A bridge takes registered=0|1");
                c.registered = value == "1";
            }
            return c;
        };
        part.pins = [](const BridgeConfig& c) -> IOComponent::PinInitFunction { return [w = c.width](IOComponent* self) { bridgePins(self, w); }; };
        part.structural = [](const std::string& name, const BridgeConfig& c, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<Bridge>(context, name, c);
        };
        // The compact form runs the bridge's own gates as a compiled list.
        part.behavioral = [](const std::string& name, const BridgeConfig& c, const std::shared_ptr<BuildContext>& context)
            -> std::shared_ptr<Component> {
            return circuit::detail::createGateListControl(name, std::string(c.registered ? "RegisteredBridge<" : "BusBridge<") + std::to_string(c.width) + ">",
                [w = c.width](IOComponent* self) { bridgePins(self, w); },
                bridgeContractId(c.width) + (c.registered ? "|registered" : "|pass-through"), [c] { return characterizedReference({bridgeContractId(c.width), "GATES", {{"registered", c.registered ? "1" : "0"}}}); },
                context);
        };
        registerSystemPart(catalog, std::move(part));
    }
}

} // namespace circuit::system
