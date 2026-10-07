#pragma once
#include "modules/memory/MemoryProtocol.hpp"
#include "modules/memory/MemoryDeviceStorage.hpp"
#include "components/capabilities/MemoryStateView.hpp"
#include <memory>

namespace circuit::memory {
struct FFModel { static constexpr auto technology = Technology::FF; };
struct SRAMModel { static constexpr auto technology = Technology::SRAM; };
struct DRAMModel { static constexpr auto technology = Technology::DRAM; };
struct ROMModel { static constexpr auto technology = Technology::ROM; };
struct NORFlashModel { static constexpr auto technology = Technology::NORFlash; };
}

// Catalog contract ID and parameters of the native device described by an
// endpoint configuration. Shared by containers and expanded endpoints.
struct NativeDeviceRequest {
    std::string contract_id;
    circuit::ParameterMap parameters;
};
NativeDeviceRequest nativeDeviceRequest(
    size_t width, circuit::memory::Technology, const circuit::memory::EndpointConfig&);
// A container is the selected native behavioral device, not another hierarchy
// or a second timing model. The factory calls Component::createWithContext on
// the existing tested implementation and gives the leaf its container label.
std::shared_ptr<IOComponent> createMemoryContainer(
    const std::string& name, size_t width, circuit::memory::Technology,
    circuit::memory::EndpointConfig = {},
    const std::shared_ptr<circuit::BuildContext>& context = {});

template<size_t W, class Model>
std::shared_ptr<IOComponent> createMemoryContainer(
    const std::string& name, circuit::memory::EndpointConfig config = {}) {
    return createMemoryContainer(name,W,Model::technology,std::move(config));
}
