#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include <memory>

// Model tags remain for the compact endpoint reference used by timing tests.
namespace circuit::memory {
struct FFModel { static constexpr auto technology = Technology::FF; };
struct SRAMModel { static constexpr auto technology = Technology::SRAM; };
struct DRAMModel { static constexpr auto technology = Technology::DRAM; };
struct ROMModel { static constexpr auto technology = Technology::ROM; };
struct NORFlashModel { static constexpr auto technology = Technology::NORFlash; };
}

// Catalog contract ID and parameters of the native device described by an
// endpoint configuration. Shared by standalone devices and endpoints.
struct NativeDeviceRequest {
    std::string contract_id;
    circuit::ParameterMap parameters;
};
NativeDeviceRequest nativeDeviceRequest(
    size_t width, circuit::memory::Technology, const circuit::memory::EndpointConfig&);
// Build the technology's native device at behavioral fidelity. This is a
// construction helper, not a component or a separate memory contract.
std::shared_ptr<IOComponent> createBehavioralMemoryDevice(
    const std::string& name, size_t width, circuit::memory::Technology,
    circuit::memory::EndpointConfig = {});
