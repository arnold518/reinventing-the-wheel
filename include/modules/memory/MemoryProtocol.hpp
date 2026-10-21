#pragma once

#include "simulator/SimulationTime.hpp"
#include "modules/memory/DigitalMemory.hpp"

namespace circuit::memory {
enum class Technology { FF, SRAM, DRAM, ROM, NORFlash };
// ReadBurst: `block_words` words of the aligned block holding the address,
// from that word on and wrapping within the block, as successive responses.
enum class Operation : unsigned { Read, Write, Program, EraseBlock, ReadBurst };
enum class Status : unsigned { Ok, Alignment, Range, ReadOnly, Unsupported, Device, InvalidRequest };
const char* technologyName(Technology);
// Catalog name of a memory part: memory.<technology>.<what>.<width>, where
// <what> is for example "device", "controller",
// "endpoint" or "scalable-endpoint".
std::string memoryContractId(Technology, std::string_view what, size_t width);
Technology technologyFromName(const std::string&);

// Device geometry and timing are distinct from host resource budgets.
struct EndpointConfig {
    size_t depth_words = 4, words_per_row = 2, block_words = 2;
    SimulationTime phase_ticks = 32, guard_ticks = 64;
    SimulationTime program_ticks = timing::activeTimingProfile().memory.nor_program;
    SimulationTime erase_ticks = timing::activeTimingProfile().memory.nor_erase;
    SimulationTime retention_ticks = timing::activeTimingProfile().memory.dram_endpoint_retention, refresh_interval_ticks = 10000;
    // The gate-level DRAM controller counts clock cycles between refreshes;
    // 0 disables refresh.
    size_t refresh_interval_cycles = 96;
    // SRAM/ROM controller: edges from START to the capturing edge (counted
    // wait states plus one), 1..255.
    size_t access_cycles = 2;
    size_t max_resident_bytes = 64 * 1024 * 1024;
    size_t max_history_bytes = 16 * 1024 * 1024;
    bool paged = true, record_history = true;
    // FF-only contract: the consumer holds RSP_READY high, so no response
    // needs to be retained after the accepting edge.
    bool always_ready = false;
    // SRAM-only: an on-chip clocked macro instead of an asynchronous chip.
    // The controller shows each request to the macro combinationally on
    // START, the macro registers it at that edge, and access_cycles counts
    // from there to the capturing edge.
    bool clocked = false;
    std::string image_hex;
    // expanded_cells: the caller builds one component per stored cell.
    void validate(size_t width, Technology, bool expanded_cells = false) const;
};
EndpointConfig endpointConfig(const ParameterMap&);
ParameterMap endpointParameters(const EndpointConfig&);
void protocolPins(IOComponent*, size_t width, const std::string& prefix = {}, bool requester = false);
void nativePins(IOComponent*, size_t width, Technology, bool controller, bool clocked = false);
// The access_cycles an SRAM/ROM endpoint needs at a clock period: the proven
// controller launch plus device access time plus a capture margin, in whole
// periods. A clocked SRAM macro's access counts from the edge itself.
size_t staticAccessCycles(size_t width, Technology, const EndpointConfig&, SimulationTime period_ticks);
void registerMemoryEndpoints(ComponentCatalog&);
}
