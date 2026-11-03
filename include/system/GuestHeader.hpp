#pragma once
// The guest software's view of a system, generated from its description:
// every region's address and size, and every register device's registers
// (absolute addresses, offsets and bit fields) from its register table.
#include "system/SystemDescription.hpp"
#include <string>

namespace circuit::system {
// The address the first master of the first bus (the CPU) uses for a target
// port's region, following bridges that pass whole addresses. Throws if the
// port is not reachable.
uint64_t cpuAddressOf(const SystemDescription&, const std::string& port);
std::string guestHeader(const SystemDescription&, const std::string& source_name);
}
