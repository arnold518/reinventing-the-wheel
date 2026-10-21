#pragma once
#include "simulator/SimulationTime.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

// Immutable observation of a persistent indexed surface. Capture at a settled
// PRESENT boundary, before advancing to the next guest write. No scanout/DMA.
struct FrameSnapshot {
    SimulationTime time = 0;
    uint32_t sequence = 0;
    size_t width = 0, height = 0, stride = 0;
    std::vector<uint8_t> pixels, known;
};
class FrameStateView {
public:
    virtual ~FrameStateView() = default;
    virtual FrameSnapshot captureFrame(SimulationTime time) const = 0;
};
