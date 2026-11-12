#pragma once
// A computer system as data: its parts, its buses and where each part sits in
// the address space. `ComputerSystem` builds the circuit from it; the guest
// platform header and the site's address-map tables are generated from the
// same description, so hardware and software cannot disagree.
//
// JSON form (schema 1):
//   {"schema":1, "name":"games-machine",
//    "parts":[{"name":"CORE","family":"rv32im.core.educational-five-stage"},
//             {"name":"MEMORY","family":"memory.subsystem.32","parameters":{"words":"65536"}}, ...],
//    "buses":[{"name":"BUS","width":32,
//              "masters":[{"name":"CPU_I","port":"CORE.IMEM_","access":"fetch"}, ...],
//              "regions":[{"name":"RAM","base":"0x00000000","bytes":"0x40000","execute":true,
//                          "targets":{"CPU_I":"MEMORY.I_","CPU_D":"MEMORY.D_"}},
//                         {"name":"CONSOLE","base":"0x10000000","bytes":"0x100","target":"CONSOLE"}, ...]}],
//    "inputs":[{"name":"ENABLE","to":"CORE.ENABLE"}, ...],
//    "outputs":[{"name":"HALTED","from":"CORE.HALTED"}, ...]}
// A port is "PART.PREFIX" (the part's pins named PREFIX + REQ_VALID, ...) or
// "PART" for unprefixed pins. Every part input named CLK or RST, or ending in
// _CLK or _RST, is driven by the system clock and reset.
#include "components/selection/SelectionTypes.hpp"
#include "system/Json.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace circuit::system {

struct PartDescription {
    std::string name, family;
    ParameterMap parameters;
};

// A master is a requester port of a part; "fetch" masters only read and only
// reach regions marked executable (instruction fetch), "data" masters read
// and write every region they reach.
struct MasterDescription {
    std::string name, port;
    bool fetch = false;
};

struct RegionDescription {
    std::string name;
    uint32_t base = 0;
    uint64_t bytes = 0;
    bool execute = false;
    // The target sees the offset in its region (a device), or the whole
    // address (a bridge to another bus).
    bool local = true;
    std::string target;                        // for every master without its own entry
    std::map<std::string, std::string> targets; // master name -> port
    // The port a master reaches here, or empty if this master cannot.
    std::string targetFor(const MasterDescription&) const;
};

struct BusDescription {
    std::string name;
    size_t width = 32;
    // § 6.3: routers and arbiters take the next request at the edge the
    // current one completes. Off by default (the chapter 5 bus).
    bool pipelined = false;
    std::vector<MasterDescription> masters;
    std::vector<RegionDescription> regions;
    // Every distinct target port, in first-use order.
    std::vector<std::string> targetPorts() const;
};

struct PinDescription { std::string name, port; };

struct SystemDescription {
    std::string name;
    std::vector<PartDescription> parts;
    std::vector<BusDescription> buses;
    std::vector<PinDescription> inputs, outputs;
    // Put a BusPortChecker on every bus link; the system gains an output
    // PORT_VIOLATION (any checker's sticky flag).
    bool check_ports = false;

    static SystemDescription parse(const std::string& json);
    static SystemDescription fromJson(const Json&);
    Json toJson() const;
    // Throws std::invalid_argument with a reason: unknown parts, overlapping
    // or misaligned regions, a fetch master reaching no executable region, ...
    void validate() const;

    const PartDescription& part(const std::string& name) const;
    const PartDescription* findPart(const std::string& name) const;
    // The region named `name` on any bus, or null.
    const RegionDescription* findRegion(const std::string& name) const;
};

// "PART.PREFIX" -> {"PART", "PREFIX"}; "PART" -> {"PART", ""}.
std::pair<std::string, std::string> splitPort(const std::string& port);
// The bus pin prefix of a target port: "MEMORY.I_" -> "MEMORY_I_", "CONSOLE" -> "CONSOLE_".
std::string portPrefix(const std::string& port);

// Built-in descriptions (systems/*.json, embedded at build time).
std::vector<std::string> builtinSystemNames();
const std::string& builtinSystemJson(const std::string& name);

} // namespace circuit::system
