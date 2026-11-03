#include "system/GuestHeader.hpp"
#include "modules/device/RegisterTable.hpp"
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>

namespace circuit::system {
namespace {
std::string hex(uint64_t value, int digits = 8) {
    char text[32];
    std::snprintf(text, sizeof text, "0x%0*llxu", digits, static_cast<unsigned long long>(value));
    return text;
}
std::string upper(std::string s) {
    for (auto& c : s) c = c == '-' || c == '.' ? '_' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
// The address at which `port` is reached from the top bus.
uint64_t addressOf(const SystemDescription& d, const std::string& port, std::set<std::string>& seen) {
    for (size_t b = 0; b < d.buses.size(); ++b) {
        for (const auto& region : d.buses[b].regions) {
            bool hit = region.target == port;
            for (const auto& [_, p] : region.targets) hit = hit || p == port;
            if (!hit) continue;
            if (b == 0) return region.base;
            // A lower bus: its masters are bridges whose target side sits on
            // a bus above. A whole-address window adds nothing; a local one
            // adds the window's base.
            for (const auto& master : d.buses[b].masters) {
                const auto part = splitPort(master.port).first;
                if (!seen.insert(part).second) continue;
                for (const auto& above : d.buses)
                    for (const auto& r : above.regions)
                        if (splitPort(r.target).first == part) {
                            const auto base = addressOf(d, r.target, seen);
                            return r.local ? base + region.base : region.base;
                        }
            }
        }
    }
    throw std::invalid_argument("Port " + port + " is not reachable from the first bus");
}
}

uint64_t cpuAddressOf(const SystemDescription& d, const std::string& port) {
    std::set<std::string> seen;
    return addressOf(d, port, seen);
}

std::string guestHeader(const SystemDescription& d, const std::string& source) {
    std::ostringstream h;
    const auto guard = "CIRCUITSIM_SYSTEM_" + upper(d.name) + "_H";
    h << "/* Generated from " << source << " by system_header; do not edit. */\n";
    h << "#ifndef " << guard << "\n#define " << guard << "\n\n";
    h << "/* Address map: what the CPU sees. */\n";
    std::set<std::string> written;
    for (const auto& bus : d.buses)
        for (const auto& region : bus.regions) {
            const auto port = region.target.empty() ? region.targets.begin()->second : region.target;
            const auto part = d.findPart(splitPort(port).first);
            if (part && part->family.starts_with("bus.bridge.")) continue;  // a window onto another bus
            if (!written.insert(region.name).second) continue;
            h << "#define " << region.name << "_BASE " << hex(cpuAddressOf(d, port)) << "\n";
            h << "#define " << region.name << "_BYTES " << hex(region.bytes) << "\n";
        }
    for (const auto& part : d.parts) {
        const auto table = device::deviceRegisterTable(part.family, part.parameters);
        if (table.registers.empty()) continue;
        const auto base = cpuAddressOf(d, part.name);
        h << "\n/* " << part.name << " (" << part.family << ") registers */\n";
        for (const auto& r : table.registers) {
            const auto name = part.name + "_" + r.name;
            h << "#define " << name << " " << hex(base + r.offset) << " /* " << device::accessName(r.access) << ": " << r.summary << " */\n";
            h << "#define " << name << "_OFFSET " << hex(r.offset, 3) << "\n";
            for (const auto& f : r.fields) {
                if (f.width == 1) h << "#define " << name << "_" << f.name << " (1u << " << f.bit << ")\n";
                else {
                    h << "#define " << name << "_" << f.name << "_SHIFT " << f.bit << "u\n";
                    h << "#define " << name << "_" << f.name << "_MASK " << hex((uint64_t{1} << f.width) - 1, 2) << "\n";
                }
            }
        }
    }
    h << "\n#endif\n";
    return h.str();
}

} // namespace circuit::system
