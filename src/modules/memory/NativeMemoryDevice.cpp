#include "modules/memory/NativeMemoryDevice.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
using namespace circuit;
using namespace circuit::memory;
NativeDeviceRequest nativeDeviceRequest(size_t width,Technology technology,const EndpointConfig& config) {
    ParameterMap p{{"depth_words",std::to_string(config.depth_words)},
                   {"record_history",config.record_history ? "1" : "0"},
                   {"paged",config.paged ? "1" : "0"},
                   {"max_resident_bytes",std::to_string(config.max_resident_bytes)},
                   {"max_history_bytes",std::to_string(config.max_history_bytes)}};
    std::string id;
    if (technology == Technology::FF) id = "memory.ff.device.";
    else {
        id = "memory." + std::string(technologyName(technology)) + ".device.";
        p["words_per_row"] = std::to_string(config.words_per_row);
        if (technology != Technology::ROM) p["phase_ticks"] = std::to_string(config.phase_ticks);
        if (technology == Technology::ROM) p["image_hex"] = config.image_hex;
        if (technology == Technology::DRAM) p["retention_ticks"] = std::to_string(config.retention_ticks);
        if (technology == Technology::NORFlash) {
            p["block_words"] = std::to_string(config.block_words);
            p["program_ticks"] = std::to_string(config.program_ticks);
            p["erase_ticks"] = std::to_string(config.erase_ticks);
        }
    }
    id += std::to_string(width);
    return {std::move(id),std::move(p)};
}
std::shared_ptr<IOComponent> createBehavioralMemoryDevice(
    const std::string& name,size_t width_,Technology technology_,EndpointConfig config_) {
    auto checked=config_;
    checked.refresh_interval_ticks=0; // Refresh policy belongs to a controller.
    checked.validate(width_,technology_);
    const auto [id,p]=nativeDeviceRequest(width_,technology_,config_);

    return std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(
        {id,name,p,{},{},{},Fidelity::Behavioral},canonicalDefaultProfile()).root);
}
