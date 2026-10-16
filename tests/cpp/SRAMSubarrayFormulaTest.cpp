// The on-chip SRAM size formula (sramSubarrayEstimate) against a held-out
// size proven from gates: fitted on the tables at 64, 128 and 256 words (1, 2
// and 4 subarrays) with the global wire taken from the profile, it must predict the table proven at 512 words (8
// subarrays) within 10 % for every delay, in every timing profile. Larger
// SRAMs, which cannot be built from gates, run on this formula.
#include "components/selection/ProvenPartDelays.hpp"
#include "components/TimingProfile.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <string>

using namespace circuit;

int main() {
    const std::map<std::string, const std::map<std::string, PartDelays>*> profiles = {
        {"unit", &provenPartDelaysUnit()}, {"embedded", &provenPartDelaysEmbedded()}, {"desktop", &provenPartDelaysDesktop()}};
    size_t compared = 0, failures = 0;
    double worst = 0;
    for (const auto& [profile, tables] : profiles)
        for (const std::string part : {"memory.sram.device.8.behavioral", "memory.sram.endpoint.8.behavioral"}) {
            const auto at = [&](size_t depth) -> const PartDelays* {
                const auto found = tables->find(part + "@depth=" + std::to_string(depth));
                return found == tables->end() ? nullptr : &found->second;
            };
            if (!at(64) || !at(128) || !at(256) || !at(512)) {
                std::cerr << profile << " " << part << ": missing a proven size\n";
                ++failures;
                continue;
            }
            const auto estimate = sramSubarrayEstimate(*at(64), *at(128), *at(256), 8, timing::timingProfile(profile).memory.sram_wire_per_side);
            const auto check = [&](const std::string& what, size_t predicted, size_t proven) {
                ++compared;
                const double deviation = proven ? std::abs(double(predicted) - double(proven)) / double(proven) : (predicted ? 1.0 : 0.0);
                worst = std::max(worst, deviation);
                if (deviation > 0.10) {
                    ++failures;
                    std::cerr << profile << " " << part << " " << what << ": formula " << predicted << ", proven " << proven << '\n';
                }
            };
            for (const auto& arc : at(512)->arcs)
                for (const auto& predicted : estimate.arcs)
                    if (predicted.input == arc.input && predicted.output == arc.output)
                        check(arc.input + "->" + arc.output, predicted.delay.maximum, arc.delay.maximum);
            for (const auto& [output, range] : at(512)->launches)
                if (const auto found = estimate.launches.find(output); found != estimate.launches.end())
                    check("launch " + output, found->second.maximum, range.maximum);
        }
    std::cout << compared << " delays compared at 8 subarrays; largest deviation " << std::round(worst * 1000) / 10 << " %\n";
    if (failures) { std::cout << "SRAMSubarrayFormulaTest failed: " << failures << " outside 10 %\n"; return 1; }
    std::cout << "SRAMSubarrayFormulaTest passed\n";
    return 0;
}
