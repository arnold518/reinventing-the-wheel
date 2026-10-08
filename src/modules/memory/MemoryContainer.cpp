#include "modules/memory/MemoryContainer.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
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
std::shared_ptr<IOComponent> createMemoryContainer(
    const std::string& name,size_t width_,Technology technology_,EndpointConfig config_,
    const std::shared_ptr<BuildContext>& context) {
    auto checked=config_;
    checked.refresh_interval_ticks=0; // Refresh policy belongs to a controller.
    checked.validate(width_,technology_);
    const auto [id,p]=nativeDeviceRequest(width_,technology_,config_);

    const auto& implementation=builtinComponentCatalog().implementation(id+".behavioral");
    if(!implementation.supports(p)) throw std::invalid_argument("Unsupported native memory configuration");
    auto device=std::dynamic_pointer_cast<IOComponent>(implementation.factory(name,p,context));
    auto memory=std::dynamic_pointer_cast<MemoryDeviceStorage>(device);
    if(!memory) throw std::logic_error("Native device lacks scalable memory storage");
    memory->setContainerDisplayType("MemoryContainer<"+std::string(technologyName(technology_))+","+std::to_string(width_)+">");
    return device;
}
