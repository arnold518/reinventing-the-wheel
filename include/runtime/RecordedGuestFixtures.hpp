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
// Its machine options (screen, button queue), inputs and budgets. A short
// fixture queues presses at fixed cycles; a long one paces a batch of button
// changes after each frame (options.presentation_inputs).
RecordedGuestFixture recordedGuestFixture(std::string_view name);
// Move a fixture's presses to another clock period, keeping their cycles.
void retime(RecordedGuestFixture&, SimulationTime period);
std::string runRecordedGuestFixtureJson(const std::string& name, bool caches=false,
                                      bool dram=false, size_t max_cycles=500000,
                                      std::function<void(uint64_t,uint64_t,uint64_t)> progress = {},
                                      size_t quantum_events=100000, bool expanded_core=false,
                                      // 0: the fixture's own screen
                                      bool compact_children=true, size_t width=0, size_t height=0,
                                      std::string fidelity_profile={},
                                      // Empty: the system the guest was built for.
                                      std::string system={});
}
