#include "system/Bus.hpp"
#include "system/SystemRegistration.hpp"
#include "modules/device/DeviceBus.hpp"
#include "modules/memory/MemoryProtocol.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralLogic.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/GateListPartBehavioral.hpp"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>

namespace circuit::system {
namespace {
constexpr const char* requests[] = {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"};
constexpr const char* responses[] = {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"};

// One master's routes: each region it reaches, as a router region whose pin
// prefix is the target port's.
std::vector<device::AddressRegion> routesOf(const BusDescription& bus, const MasterDescription& master) {
    std::vector<device::AddressRegion> routes;
    for (const auto& region : bus.regions) {
        const auto port = region.targetFor(master);
        if (port.empty()) continue;
        const auto prefix = portPrefix(port);
        for (const auto& route : routes)
            if (route.prefix == prefix)
                throw std::invalid_argument("Bus " + bus.name + ": master " + master.name + " reaches port " + port
                                            + " through two regions; give the target one window");
        routes.push_back({prefix, region.base, region.bytes, region.local});
    }
    return routes;
}

// A master's request router: the address router over this master's routes.
std::string shortHash(const std::string& text);

class MasterRouter final : public IOComponent, public ComponentLayoutView {
public:
    MasterRouter(std::string name, RouterConfig config)
        : IOComponent(std::move(name), routerPinsFor(config)), config_(std::move(config)),
          type_(std::string(config_.fetch ? "BusFetchRouter<" : "BusRouter<") + std::to_string(config_.width) + ">") {}
    const char* getTypeName() const override { return type_.c_str(); }
    // Each route set has its own pins, so its own floorplan.
    std::string componentLayoutVariant() const override { return "routes-" + shortHash(routerParameters(config_).at("routes")); }
    void buildInternals(ComponentBuilder& builder) override {
        device::buildAddressRouter(*this, builder, config_.routes, config_.fetch, config_.width);
    }
private:
    RouterConfig config_;
    std::string type_;
};

std::string shortHash(const std::string& text) {
    uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : text) { hash ^= c; hash *= 1099511628211ULL; }
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 8; ++i) { out += digits[hash & 15]; hash >>= 4; }
    return out;
}
} // namespace

std::shared_ptr<IOComponent> characterizedReference(const ComponentBuildRequest& request) {
    std::vector<ProfileRule> rules{
        preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("storage.state-bit"), "characterized state bit"),
        preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("memory.ff.cell"), "characterized enabled bit")};
    for (unsigned width : {1u, 2u, 3u, 4u, 5u, 8u, 16u, 32u})
        rules.push_back(preferFidelity(Fidelity::Behavioral, ProfileSelector::contract("storage.register." + std::to_string(width)),
                                       "characterized register"));
    const auto profile = withProfileOverrides(maximallyStructural(), std::move(rules), "gate-list-reference-v1");
    return std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(request, profile).root);
}

std::string busContractId(size_t width) { return "bus.interconnect." + std::to_string(width); }
std::string routerContractId(size_t width) { return "bus.router." + std::to_string(width); }

IOComponent::PinInitFunction routerPinsFor(const RouterConfig& config) {
    return [config](IOComponent* self) { device::addressRouterPins(self, config.routes, config.width); };
}

ParameterMap routerParameters(const RouterConfig& config) {
    std::string routes;
    for (const auto& r : config.routes) {
        char text[64];
        std::snprintf(text, sizeof text, "@0x%08x+0x%llx%s", r.base, static_cast<unsigned long long>(r.bytes), r.local ? "" : "!");
        routes += (routes.empty() ? "" : ",") + r.prefix + text;
    }
    return {{"access", config.fetch ? "fetch" : "data"}, {"routes", routes}};
}

RouterConfig routerFromParameters(const ParameterMap& parameters, size_t width) {
    if (parameters.size() != 2 || !parameters.contains("access") || !parameters.contains("routes"))
        throw std::invalid_argument("A bus router takes access and routes");
    RouterConfig config;
    config.width = width;
    const auto& access = parameters.at("access");
    if (access != "fetch" && access != "data") throw std::invalid_argument("Router access must be fetch or data");
    config.fetch = access == "fetch";
    std::string text = parameters.at("routes");
    size_t start = 0;
    while (start < text.size()) {
        auto end = text.find(',', start);
        if (end == std::string::npos) end = text.size();
        auto item = text.substr(start, end - start);
        start = end + 1;
        device::AddressRegion region;
        if (!item.empty() && item.back() == '!') { region.local = false; item.pop_back(); }
        const auto at = item.find('@'), plus = item.find('+');
        if (at == std::string::npos || plus == std::string::npos || plus < at) throw std::invalid_argument("Bad router route " + item);
        region.prefix = item.substr(0, at);
        const auto base = parseUnsigned(item.substr(at + 1, plus - at - 1), "route base");
        if (base > UINT32_MAX) throw std::invalid_argument("Route base above 32 bits");
        region.base = static_cast<uint32_t>(base);
        region.bytes = parseUnsigned(item.substr(plus + 1), "route bytes");
        config.routes.push_back(region);
    }
    if (config.routes.empty()) throw std::invalid_argument("A bus router needs at least one route");
    // The same window rules as a bus description: aligned powers of two that
    // do not overlap, one route per prefix.
    for (size_t i = 0; i < config.routes.size(); ++i) {
        const auto& r = config.routes[i];
        if (r.prefix.empty() || r.bytes < 4 || r.bytes > (uint64_t{1} << 32) || (r.bytes & (r.bytes - 1)) || uint64_t(r.base) % r.bytes)
            throw std::invalid_argument("Router route " + r.prefix + " must be an aligned power-of-two window of at least 4 bytes");
        for (size_t j = 0; j < i; ++j) {
            const auto& o = config.routes[j];
            if (o.prefix == r.prefix || (uint64_t(r.base) < uint64_t(o.base) + o.bytes && uint64_t(o.base) < uint64_t(r.base) + r.bytes))
                throw std::invalid_argument("Router routes " + o.prefix + " and " + r.prefix + " overlap or share a prefix");
        }
    }
    (void)Component::create<IOComponent>("CHECK", routerPinsFor(config));
    return config;
}

std::string busConfigJson(const BusDescription& bus) {
    SystemDescription holder;
    holder.name = "bus";
    holder.buses = {bus};
    const auto json = holder.toJson();
    return json.at("buses").array("buses").front().dump();
}

BusDescription busFromConfigJson(const std::string& text) {
    // Parse through a one-bus description so the same reader and checks apply;
    // the ports name parts that are outside this bus, so part checks are skipped.
    const auto json = Json::parse(text);
    std::string wrapped = "{\"schema\":1,\"name\":\"bus\",\"parts\":[";
    std::vector<std::string> parts;
    const auto addPart = [&](const std::string& port) {
        const auto part = splitPort(port).first;
        if (std::find(parts.begin(), parts.end(), part) == parts.end()) parts.push_back(part);
    };
    for (const auto& master : json.at("masters").array("masters")) addPart(master.at("port").string("port"));
    for (const auto& region : json.at("regions").array("regions")) {
        if (const auto* target = region.find("target")) addPart(target->string("target"));
        if (const auto* targets = region.find("targets"))
            for (const auto& [_, port] : targets->object("targets")) addPart(port.string("target"));
    }
    for (size_t i = 0; i < parts.size(); ++i)
        wrapped += std::string(i ? "," : "") + "{\"name\":\"" + parts[i] + "\",\"family\":\"external\"}";
    wrapped += "],\"buses\":[" + text + "]}";
    return SystemDescription::parse(wrapped).buses.front();
}

void busPins(IOComponent* self, const BusDescription& bus) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    for (const auto& master : bus.masters) memory::protocolPins(self, bus.width, master.name + "_");
    for (const auto& port : bus.targetPorts()) memory::protocolPins(self, bus.width, portPrefix(port), true);
}

Bus::Bus(std::string name, BusDescription description)
    : IOComponent(std::move(name), [description](IOComponent* self) { busPins(self, description); }),
      description_(std::move(description)),
      type_("Bus<" + std::to_string(description_.width) + ">"),
      variant_(shortHash(busConfigJson(description_))) {
    for (const auto& master : description_.masters) (void)routesOf(description_, master);
}

std::vector<const MasterDescription*> Bus::mastersOf(const std::string& port) const {
    std::vector<const MasterDescription*> result;
    for (const auto& master : description_.masters)
        for (const auto& region : description_.regions)
            if (region.targetFor(master) == port) { result.push_back(&master); break; }
    return result;
}

void Bus::buildInternals(ComponentBuilder& builder) {
    structural::Logic l(builder);
    const auto width = description_.width;
    std::map<std::string, std::shared_ptr<IOComponent>> routers;
    for (const auto& master : description_.masters) {
        RouterConfig config{routesOf(description_, master), master.fetch, width};
        auto router = builder.addByContract({routerContractId(width), master.name + "_ROUTER", routerParameters(config)});
        routers[master.name] = router;
        const auto prefix = master.name + "_";
        l.wire(getInputPin("CLK"), router->getInputPin("CLK"));
        l.wire(getInputPin("RST"), router->getInputPin("RST"));
        for (const auto* name : requests) l.wire(getInputPinDynamic(prefix + name), router->getInputPinDynamic(name));
        for (const auto* name : responses) l.wire(router->getOutputPinDynamic(name), getOutputPinDynamic(prefix + name));
    }
    // Upstream side (a router's port pins) to downstream side (pins with the
    // same names on `lower`, under `lower_prefix`).
    const auto link = [&](const std::shared_ptr<IOComponent>& upper, const std::string& up,
                          const std::shared_ptr<IOComponent>& lower, const std::string& down, bool lower_is_parent) {
        for (const auto* name : requests) {
            const auto source = upper->getOutputPinDynamic(up + name);
            l.wire(source, lower_is_parent ? getOutputPinDynamic(down + name) : lower->getInputPinDynamic(down + name));
        }
        for (const auto* name : responses) {
            const auto sink = upper->getInputPinDynamic(up + name);
            l.wire(lower_is_parent ? getInputPinDynamic(down + name) : lower->getOutputPinDynamic(down + name), sink);
        }
    };
    for (const auto& port : description_.targetPorts()) {
        const auto prefix = portPrefix(port);
        const auto masters = mastersOf(port);
        // Masters sharing the port meet in a tree of two-port arbiters: each
        // arbiter takes turns between two requesters (first on I_, second on
        // D_) and holds a burst together; its MEM_ side is one requester of
        // the next level. One master is wired straight through.
        std::vector<std::pair<std::shared_ptr<IOComponent>, std::string>> level;
        for (const auto* master : masters) level.emplace_back(routers.at(master->name), prefix);
        size_t made = 0;
        while (level.size() > 1) {
            std::vector<std::pair<std::shared_ptr<IOComponent>, std::string>> next;
            for (size_t i = 0; i + 1 < level.size(); i += 2) {
                const auto name = prefix + "ARBITER" + (masters.size() == 2 ? "" : "_" + std::to_string(made));
                ++made;
                auto arbiter = builder.addByContract({"memory.arbiter." + std::to_string(width), name, {}});
                // The arbiter runs on I_CLK and does not read D_CLK; both
                // resets are read.
                l.wire(getInputPin("CLK"), arbiter->getInputPin("I_CLK"));
                l.wire(getInputPin("RST"), arbiter->getInputPin("I_RST"));
                l.wire(getInputPin("RST"), arbiter->getInputPin("D_RST"));
                link(level[i].first, level[i].second, arbiter, "I_", false);
                link(level[i + 1].first, level[i + 1].second, arbiter, "D_", false);
                next.emplace_back(arbiter, "MEM_");
            }
            if (level.size() % 2) next.push_back(level.back());
            level = std::move(next);
        }
        link(level.front().first, level.front().second, nullptr, prefix, true);
        // The port's own clock and reset follow the bus's.
        l.wire(getInputPin("CLK"), getOutputPin(prefix + "CLK"));
        l.wire(getInputPin("RST"), getOutputPin(prefix + "RST"));
    }
    l.finish();
}

void registerBuses(ComponentCatalog& catalog) {
    for (size_t width : {8u, 16u, 32u}) {
        const auto w = std::to_string(width);
        SystemPart<RouterConfig> router;
        router.id = routerContractId(width);
        router.type = "BusRouter<" + w + ">";
        router.test = "BusRouterScenarioTest";
        router.equivalence_test = "BusInterconnectTest";
        router.parse = [width](const ParameterMap& p) { return routerFromParameters(p, width); };
        router.pins = routerPinsFor;
        router.structural = [](const std::string& name, const RouterConfig& config, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<MasterRouter>(context, name, config);
        };
        // The fast form runs the router's own gates as a compiled gate list.
        router.behavioral = [](const std::string& name, const RouterConfig& config, const std::shared_ptr<BuildContext>& context)
            -> std::shared_ptr<Component> {
            const auto parameters = routerParameters(config);
            return circuit::detail::createGateListControl(name, std::string(config.fetch ? "BusFetchRouter<" : "BusRouter<") + std::to_string(config.width) + ">",
                routerPinsFor(config), routerContractId(config.width) + "|" + canonicalizeParameters(parameters),
                [config, parameters] { return characterizedReference({routerContractId(config.width), "GATES", parameters}); }, context);
        };
        registerSystemPart(catalog, std::move(router));

        SystemPart<BusDescription> bus;
        bus.id = busContractId(width);
        bus.type = "Bus<" + w + ">";
        bus.test = "BusInterconnectTest";
        bus.parse = [width](const ParameterMap& p) {
            if (p.size() != 1 || !p.contains("config")) throw std::invalid_argument("A bus takes one parameter, config");
            auto description = busFromConfigJson(p.at("config"));
            if (description.width != width) throw std::invalid_argument("Bus width differs from its contract");
            return description;
        };
        bus.pins = [](const BusDescription& d) -> IOComponent::PinInitFunction { return [d](IOComponent* self) { busPins(self, d); }; };
        bus.structural = [](const std::string& name, const BusDescription& d, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<Bus>(context, name, d);
        };
        // The whole bus as one compiled gate list (its routers and arbiters
        // with characterized storage), on request only.
        bus.behavioral = [](const std::string& name, const BusDescription& d, const std::shared_ptr<BuildContext>& context)
            -> std::shared_ptr<Component> {
            const auto config = busConfigJson(d);
            return circuit::detail::createGateListControl(name, "Bus<" + std::to_string(d.width) + ">",
                [d](IOComponent* self) { busPins(self, d); }, busContractId(d.width) + "|" + config,
                [d, config] { return characterizedReference({busContractId(d.width), "GATES", {{"config", config}}}); }, context);
        };
        bus.behavioral_on_request = true;
        bus.equivalence_test = "BusInterconnectTest";
        registerSystemPart(catalog, std::move(bus));
    }
}

} // namespace circuit::system
