#pragma once
#include "runtime/RecordedGuest.hpp"
#include <string_view>
namespace circuit::runtime {
struct RecordedGuestFixture {
    std::string name, elf_sha256, script_sha256;
    std::vector<uint8_t> elf;
    std::vector<ButtonInput> input;
    RecordedGuestOptions options;
};
std::vector<std::string> recordedGuestNames();
RecordedGuestFixture recordedGuestFixture(std::string_view name);
std::string runRecordedGuestFixtureJson(const std::string& name, bool caches=false,
                                      bool dram=false, size_t max_cycles=500000,
                                      std::function<void(uint64_t,uint64_t,uint64_t)> progress = {},
                                      size_t quantum_events=100000, bool expanded_core=false,
                                      bool compact_children=true);
}
