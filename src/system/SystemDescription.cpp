#include "system/SystemDescription.hpp"
#include "BuiltinSystems.generated.hpp"
#include <algorithm>
#include <bit>
#include <set>
#include <stdexcept>

namespace circuit::system {

std::pair<std::string, std::string> splitPort(const std::string& port) {
    const auto dot = port.find('.');
    if (dot == std::string::npos) return {port, ""};
    return {port.substr(0, dot), port.substr(dot + 1)};
}

std::string portPrefix(const std::string& port) {
    auto [part, prefix] = splitPort(port);
    std::string name = part + "_" + prefix;
    if (name.back() != '_') name += '_';
    return name;
}

std::string RegionDescription::targetFor(const MasterDescription& master) const {
    if (master.fetch && !execute) return {};
    if (const auto found = targets.find(master.name); found != targets.end()) return found->second;
    return targets.empty() || !target.empty() ? target : std::string{};
}

std::vector<std::string> BusDescription::targetPorts() const {
    std::vector<std::string> ports;
    for (const auto& region : regions)
        for (const auto& master : masters)
            if (const auto port = region.targetFor(master); !port.empty()
                && std::find(ports.begin(), ports.end(), port) == ports.end())
                ports.push_back(port);
    return ports;
}

namespace {
ParameterMap parameters(const Json* json, const std::string& where) {
    ParameterMap result;
    if (!json) return result;
    for (const auto& [key, value] : json->object(where)) {
        if (value.kind() == Json::Kind::String) result[key] = value.string(where + "." + key);
        else if (value.kind() == Json::Kind::Number) result[key] = std::to_string(value.unsignedInteger(where + "." + key));
        else if (value.kind() == Json::Kind::Bool) result[key] = value.boolean(where + "." + key) ? "1" : "0";
        else result[key] = value.dump();  // nested settings stay JSON text
    }
    return result;
}
std::string hex(uint64_t value) {
    static const char* digits = "0123456789abcdef";
    std::string text;
    do { text.insert(text.begin(), digits[value & 15]); value >>= 4; } while (value);
    while (text.size() < 8) text.insert(text.begin(), '0');
    return "0x" + text;
}
}

SystemDescription SystemDescription::fromJson(const Json& json) {
    if (json.at("schema").unsignedInteger("schema") != 1) throw std::invalid_argument("Unsupported system description schema");
    SystemDescription d;
    d.name = json.at("name").string("name");
    for (const auto& part : json.at("parts").array("parts")) {
        PartDescription p;
        p.name = part.at("name").string("parts[].name");
        p.family = part.at("family").string("part " + p.name + " family");
        p.parameters = parameters(part.find("parameters"), "part " + p.name + " parameters");
        d.parts.push_back(std::move(p));
    }
    for (const auto& bus : json.at("buses").array("buses")) {
        BusDescription b;
        b.name = bus.at("name").string("buses[].name");
        if (const auto* width = bus.find("width")) b.width = width->unsignedInteger("bus " + b.name + " width");
        if (const auto* pipelined = bus.find("pipelined")) b.pipelined = pipelined->boolean("bus " + b.name + " pipelined");
        for (const auto& master : bus.at("masters").array("bus " + b.name + " masters")) {
            MasterDescription m;
            m.name = master.at("name").string("master name");
            m.port = master.at("port").string("master " + m.name + " port");
            const auto access = master.find("access") ? master.at("access").string("master " + m.name + " access") : "data";
            if (access != "fetch" && access != "data") throw std::invalid_argument("Master " + m.name + " access must be fetch or data");
            m.fetch = access == "fetch";
            b.masters.push_back(std::move(m));
        }
        for (const auto& region : bus.at("regions").array("bus " + b.name + " regions")) {
            RegionDescription r;
            r.name = region.at("name").string("region name");
            const auto where = "region " + r.name;
            const auto base = region.at("base").unsignedInteger(where + " base");
            if (base > UINT32_MAX) throw std::invalid_argument(where + ": base above 32 bits");
            r.base = static_cast<uint32_t>(base);
            r.bytes = region.at("bytes").unsignedInteger(where + " bytes");
            if (const auto* execute = region.find("execute")) r.execute = execute->boolean(where + " execute");
            if (const auto* local = region.find("local")) r.local = local->boolean(where + " local");
            if (const auto* target = region.find("target")) r.target = target->string(where + " target");
            if (const auto* targets = region.find("targets"))
                for (const auto& [master, port] : targets->object(where + " targets")) r.targets[master] = port.string(where + " target");
            b.regions.push_back(std::move(r));
        }
        d.buses.push_back(std::move(b));
    }
    for (auto [key, list, member] : {std::tuple{"inputs", &d.inputs, "to"}, std::tuple{"outputs", &d.outputs, "from"}})
        if (const auto* pins = json.find(key))
            for (const auto& pin : pins->array(key))
                list->push_back({pin.at("name").string(std::string(key) + " name"), pin.at(member).string(std::string(key) + " " + member)});
    if (const auto* check = json.find("check_ports")) d.check_ports = check->boolean("check_ports");
    d.validate();
    return d;
}

SystemDescription SystemDescription::parse(const std::string& text) { return fromJson(Json::parse(text)); }

Json SystemDescription::toJson() const {
    // Built through the text form so the reader stays the single parser.
    std::string out = "{\"schema\":1,\"name\":\"" + name + "\",\"parts\":[";
    for (size_t i = 0; i < parts.size(); ++i) {
        out += std::string(i ? "," : "") + "{\"name\":\"" + parts[i].name + "\",\"family\":\"" + parts[i].family + "\"";
        if (!parts[i].parameters.empty()) {
            out += ",\"parameters\":{";
            size_t k = 0;
            for (const auto& [key, value] : parts[i].parameters) {
                const bool nested = !value.empty() && (value.front() == '{' || value.front() == '[');
                out += std::string(k++ ? "," : "") + "\"" + key + "\":" + (nested ? value : "\"" + value + "\"");
            }
            out += "}";
        }
        out += "}";
    }
    out += "],\"buses\":[";
    for (size_t i = 0; i < buses.size(); ++i) {
        const auto& b = buses[i];
        out += std::string(i ? "," : "") + "{\"name\":\"" + b.name + "\",\"width\":" + std::to_string(b.width)
             + (b.pipelined ? ",\"pipelined\":true" : "") + ",\"masters\":[";
        for (size_t m = 0; m < b.masters.size(); ++m)
            out += std::string(m ? "," : "") + "{\"name\":\"" + b.masters[m].name + "\",\"port\":\"" + b.masters[m].port
                 + "\",\"access\":\"" + (b.masters[m].fetch ? "fetch" : "data") + "\"}";
        out += "],\"regions\":[";
        for (size_t r = 0; r < b.regions.size(); ++r) {
            const auto& g = b.regions[r];
            out += std::string(r ? "," : "") + "{\"name\":\"" + g.name + "\",\"base\":\"" + hex(g.base) + "\",\"bytes\":\"" + hex(g.bytes) + "\"";
            if (g.execute) out += ",\"execute\":true";
            if (!g.local) out += ",\"local\":false";
            if (!g.target.empty()) out += ",\"target\":\"" + g.target + "\"";
            if (!g.targets.empty()) {
                out += ",\"targets\":{";
                size_t k = 0;
                for (const auto& [master, port] : g.targets) out += std::string(k++ ? "," : "") + "\"" + master + "\":\"" + port + "\"";
                out += "}";
            }
            out += "}";
        }
        out += "]}";
    }
    out += "]";
    for (auto [key, list, member] : {std::tuple{"inputs", &inputs, "to"}, std::tuple{"outputs", &outputs, "from"}}) {
        out += std::string(",\"") + key + "\":[";
        for (size_t i = 0; i < list->size(); ++i)
            out += std::string(i ? "," : "") + "{\"name\":\"" + (*list)[i].name + "\",\"" + member + "\":\"" + (*list)[i].port + "\"}";
        out += "]";
    }
    if (check_ports) out += ",\"check_ports\":true";
    return Json::parse(out + "}");
}

const PartDescription* SystemDescription::findPart(const std::string& wanted) const {
    for (const auto& p : parts) if (p.name == wanted) return &p;
    return nullptr;
}
const PartDescription& SystemDescription::part(const std::string& wanted) const {
    if (const auto* p = findPart(wanted)) return *p;
    throw std::invalid_argument("No part named " + wanted);
}
const RegionDescription* SystemDescription::findRegion(const std::string& wanted) const {
    for (const auto& b : buses) for (const auto& r : b.regions) if (r.name == wanted) return &r;
    return nullptr;
}

void SystemDescription::validate() const {
    const auto fail = [this](const std::string& why) { throw std::invalid_argument("System " + name + ": " + why); };
    if (name.empty()) fail("no name");
    std::set<std::string> names;
    for (const auto& p : parts) {
        if (p.name.empty() || !names.insert(p.name).second) fail("part names must be unique and not empty (" + p.name + ")");
        if (p.family.empty()) fail("part " + p.name + " has no family");
    }
    const auto knownPart = [&](const std::string& port, const std::string& where) {
        if (!findPart(splitPort(port).first)) fail(where + " names unknown part '" + splitPort(port).first + "'");
    };
    std::set<std::string> bus_names;
    for (const auto& b : buses) {
        if (b.name.empty() || !bus_names.insert(b.name).second || names.contains(b.name)) fail("bus names must be unique (" + b.name + ")");
        if (b.width != 8 && b.width != 16 && b.width != 32) fail("bus " + b.name + " width must be 8, 16 or 32");
        if (b.masters.empty()) fail("bus " + b.name + " has no master");
        std::set<std::string> masters;
        for (const auto& m : b.masters) {
            if (!masters.insert(m.name).second) fail("bus " + b.name + ": master names must be unique (" + m.name + ")");
            knownPart(m.port, "master " + m.name);
        }
        std::set<std::string> region_names;
        for (size_t i = 0; i < b.regions.size(); ++i) {
            const auto& r = b.regions[i];
            if (!region_names.insert(r.name).second) fail("bus " + b.name + ": region names must be unique (" + r.name + ")");
            if (r.bytes < 4 || r.bytes > (uint64_t{1} << 32) || !std::has_single_bit(r.bytes) || uint64_t(r.base) % r.bytes
                || uint64_t(r.base) + r.bytes > (uint64_t{1} << 32))
                fail("region " + r.name + " must be an aligned power-of-two window of at least 4 bytes");
            for (size_t j = 0; j < i; ++j) {
                const auto& o = b.regions[j];
                if (uint64_t(r.base) < uint64_t(o.base) + o.bytes && uint64_t(o.base) < uint64_t(r.base) + r.bytes)
                    fail("regions " + o.name + " and " + r.name + " overlap");
            }
            if (r.target.empty() && r.targets.empty()) fail("region " + r.name + " has no target");
            if (!r.target.empty()) knownPart(r.target, "region " + r.name);
            for (const auto& [master, port] : r.targets) {
                if (!masters.contains(master)) fail("region " + r.name + " names unknown master " + master);
                knownPart(port, "region " + r.name);
            }
        }
        for (const auto& m : b.masters) {
            bool reaches = false;
            for (const auto& r : b.regions) reaches = reaches || !r.targetFor(m).empty();
            if (!reaches) fail("master " + m.name + " reaches no region" + std::string(m.fetch ? " (fetch masters need an executable region)" : ""));
        }
    }
    for (const auto& pin : inputs) knownPart(pin.port, "input " + pin.name);
    for (const auto& pin : outputs) knownPart(pin.port, "output " + pin.name);
}

std::vector<std::string> builtinSystemNames() {
    std::vector<std::string> result;
    for (const auto& [name, _] : generated::builtinSystems()) result.push_back(name);
    return result;
}
const std::string& builtinSystemJson(const std::string& wanted) {
    for (const auto& [name, text] : generated::builtinSystems()) if (name == wanted) return text;
    throw std::invalid_argument("No built-in system named " + wanted);
}

} // namespace circuit::system
