#pragma once
#include "modules/memory/DigitalMemory.hpp"

namespace circuit::memory {
enum class Technology { FF, SRAM, DRAM, ROM, NORFlash };
enum class Operation : unsigned { Read, Write, Program, EraseBlock };
enum class Status : unsigned { Ok, Alignment, Range, ReadOnly, Unsupported, Device, InvalidRequest };
const char* technologyName(Technology);
// Catalog name of a memory part: memory.<technology>.<what>.<width>, where
// <what> is for example "device", "scalable-device", "controller",
// "endpoint" or "scalable-endpoint".
std::string memoryContractId(Technology, std::string_view what, size_t width);
Technology technologyFromName(const std::string&);

// Device geometry and timing are distinct from host resource budgets.
struct EndpointConfig {
    size_t depth_words = 4, words_per_row = 2, block_words = 2;
    size_t phase_ticks = 32, guard_ticks = 64;
    size_t program_ticks = 128, erase_ticks = 256;
    size_t retention_ticks = 1000000000, refresh_interval_ticks = 10000;
    size_t max_resident_bytes = 64 * 1024 * 1024;
    size_t max_history_bytes = 16 * 1024 * 1024;
    bool paged = true, record_history = true;
    // FF-only contract: the consumer holds RSP_READY high, so no response
    // needs to be retained after the accepting edge.
    bool always_ready = false;
    std::string image_hex;
    // expanded_cells: the caller builds one component per stored cell.
    void validate(size_t width, Technology, bool expanded_cells = false) const;
};
EndpointConfig endpointConfig(const ParameterMap&);
ParameterMap endpointParameters(const EndpointConfig&);
void protocolPins(IOComponent*, size_t width, const std::string& prefix = {}, bool requester = false);
void nativePins(IOComponent*, size_t width, Technology, bool controller);
void registerMemoryEndpoints(ComponentCatalog&);
}
