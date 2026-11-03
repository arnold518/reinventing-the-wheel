#pragma once
// A device's registers as data. One table builds the device's register front
// end (decoding, access checks, read selection, write strobes), the guest
// header's offsets and the site's register tables, so they cannot disagree.
#include "components/selection/SelectionTypes.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class ComponentBuilder;
class IOComponent;

namespace circuit::device {

enum class RegisterAccess {
    ReadOnly,   // a value the device core supplies (COUNT, STATUS, LEVELS)
    Setting,    // a constant fixed when the device is built (WIDTH, DIVISOR)
    ReadWrite,  // stored in the front end; the device core reads it
    WriteData,  // a write hands data to the device core (console TX)
    Command,    // writing 1 acts once (POP, PRESENT); writing 0 does nothing
};
const char* accessName(RegisterAccess);

struct RegisterField { std::string name; unsigned bit = 0, width = 1; };

struct RegisterEntry {
    std::string name;
    uint32_t offset = 0;
    RegisterAccess access = RegisterAccess::ReadOnly;
    unsigned width = 32;        // significant bits; the rest read as zero
    uint32_t value = 0;         // reset value (read-write) or constant (setting)
    std::string summary;
    std::vector<RegisterField> fields = {};
};

struct RegisterTable {
    std::string device;         // the device's name in headers, e.g. "CONSOLE"
    uint32_t window_bytes = 0x100;
    std::vector<RegisterEntry> registers;
    // Offsets word aligned, unique and inside a window of at most 32 words;
    // widths 1..32; writable payloads fit their width.
    void validate() const;
    const RegisterEntry* find(const std::string& name) const;
    // Index bits of the offset decoder (1..5).
    size_t indexBits() const;
};

// The register table of a device contract with these parameters, or empty
// (no registers) for a contract that is not a register device.
RegisterTable deviceRegisterTable(const std::string& contract, const ParameterMap& parameters);

// Builds the front end as one group named REGISTERS. Inputs: ADDRESS[32],
// IS_WRITE, WRITE_DATA[32], WRITE_MASK[4], WRITE_STROBE, READY and, per
// read-only register, <NAME>_VALUE[width]; CLK and RST when the table has a
// read-write register. Outputs: BACK_STATUS[3], BACK_READY, BACK_READ_DATA[32],
// per writable register <NAME>_WRITE, per read-write register <NAME>[width].
std::shared_ptr<IOComponent> buildRegisterFrontEnd(ComponentBuilder&, const RegisterTable&);

} // namespace circuit::device
