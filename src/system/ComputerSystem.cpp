#include "system/ComputerSystem.hpp"
#include "system/Bus.hpp"
#include "system/MemorySubsystem.hpp"
#include "system/BusPortChecker.hpp"
#include "components/StructuralLogic.hpp"
#include "system/SystemRegistration.hpp"
#include "modules/device/DeviceConfiguration.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <set>
#include <stdexcept>

namespace circuit::system {
namespace {
constexpr const char* requests[] = {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"};
constexpr const char* responses[] = {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"};

bool clockLike(const std::string& pin) {
    const auto ends = [&](const char* tail) { const std::string t(tail); return pin.size() >= t.size() && pin.compare(pin.size() - t.size(), t.size(), t) == 0; };
    return pin == "CLK" || pin == "RST" || ends("_CLK") || ends("_RST");
}
bool isRst(const std::string& pin) { return pin == "RST" || (pin.size() > 4 && pin.compare(pin.size() - 4, 4, "_RST") == 0); }

// Width of PART.PIN from the part's contract.
size_t pinWidth(const SystemDescription& d, const std::string& port, PinType direction) {
    const auto [part_name, pin] = splitPort(port);
    const auto& part = d.part(part_name);
    for (const auto& p : builtinComponentCatalog().contract(part.family).pinsFor(part.parameters))
        if (p.name == pin && p.direction == direction) return p.width;
    throw std::invalid_argument("System " + d.name + ": part " + part_name + " has no "
                                + (direction == PinType::INPUT ? "input " : "output ") + pin);
}
size_t parameterOr(const ParameterMap& p, const std::string& key, size_t fallback) {
    const auto it = p.find(key);
    return it == p.end() ? fallback : parseUnsigned(it->second, key);
}
}

IOComponent::PinInitFunction systemPins(const SystemDescription& d) {
    std::vector<std::pair<std::string, size_t>> inputs, outputs;
    for (const auto& pin : d.inputs) inputs.emplace_back(pin.name, pinWidth(d, pin.port, PinType::INPUT));
    for (const auto& pin : d.outputs) outputs.emplace_back(pin.name, pinWidth(d, pin.port, PinType::OUTPUT));
    return [inputs, outputs, check = d.check_ports](IOComponent* self) {
        self->addPin("CLK", PinType::INPUT);
        self->addPin("RST", PinType::INPUT);
        for (const auto& [name, width] : inputs) self->addPinDynamic(name, PinType::INPUT, width);
        for (const auto& [name, width] : outputs) self->addPinDynamic(name, PinType::OUTPUT, width);
        if (check) self->addPin("PORT_VIOLATION", PinType::OUTPUT);
    };
}

SystemDescription systemFromParameters(const ParameterMap& p) {
    if (p.size() == 1 && p.contains("machine")) return SystemDescription::parse(builtinSystemJson(p.at("machine")));
    if (p.size() == 1 && p.contains("description")) return SystemDescription::parse(p.at("description"));
    // A built-in games machine with its options.
    if (p.contains("machine")) {
        auto rest = p;
        rest.erase("machine");
        return withGameOptions(SystemDescription::parse(builtinSystemJson(p.at("machine"))), gameMachineOptions(rest));
    }
    throw std::invalid_argument("A computer system takes machine (with options) or description");
}

ComputerSystem::ComputerSystem(std::string name, SystemDescription d)
    : IOComponent(std::move(name), systemPins(d)), description_(std::move(d)) {
    description_.validate();
}

void ComputerSystem::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    for (const auto& p : description_.parts) {
        auto part = b.addByContract({p.family, p.name, p.parameters});
        parts_[p.name] = part;
        if (!core_ && std::dynamic_pointer_cast<RV32IPipelineStateView>(part)) core_ = part;
        if (auto memory = std::dynamic_pointer_cast<MemorySubsystem>(part); memory && !ram_) {
            ram_ = memory->storage();
            ram_limit_ = memory->config().bytes();
            // The program sees main memory through its executable window.
            for (const auto& bus : description_.buses)
                for (const auto& region : bus.regions) {
                    bool reaches = splitPort(region.target).first == p.name;
                    for (const auto& [_, port] : region.targets) reaches = reaches || splitPort(port).first == p.name;
                    if (reaches && region.execute && region.base == 0) ram_limit_ = std::min<uint64_t>(ram_limit_, region.bytes);
                }
        }
        if (p.family == "device.framebuffer" && !display_) {
            display_ = part;
            frame_width_ = parameterOr(p.parameters, "width", 4);
            frame_height_ = parameterOr(p.parameters, "height", 4);
            for (const auto& child : part->getChildren())
                if (child->getName() == "PIXELS") video_ = std::dynamic_pointer_cast<MemoryStateView>(child);
            if (!video_) throw std::logic_error("Display PIXELS has no memory observation capability");
        }
    }
    for (const auto& d : description_.buses) {
        auto bus = b.addByContract({busContractId(d.width), d.name, {{"config", busConfigJson(d)}}});
        buses_[d.name] = bus;
        for (const auto& master : d.masters) {
            const auto [part, prefix] = splitPort(master.port);
            const auto& source = parts_.at(part);
            for (const auto* n : requests) w.connect(source->getOutputPinDynamic(prefix + n), bus->getInputPinDynamic(master.name + "_" + n));
            for (const auto* n : responses) w.connect(bus->getOutputPinDynamic(master.name + "_" + n), source->getInputPinDynamic(prefix + n));
        }
        for (const auto& port : d.targetPorts()) {
            const auto [part, prefix] = splitPort(port);
            const auto& target = parts_.at(part);
            const auto bus_prefix = portPrefix(port);
            for (const auto* n : requests) w.connect(bus->getOutputPinDynamic(bus_prefix + n), target->getInputPinDynamic(prefix + n));
            for (const auto* n : responses) w.connect(target->getOutputPinDynamic(prefix + n), bus->getInputPinDynamic(bus_prefix + n));
        }
    }
    // Port checkers observe each link: the master side's request pins and the
    // answering side's response pins, as wired above.
    std::vector<std::shared_ptr<IOComponent>> checkers;
    if (description_.check_ports)
        for (const auto& d : description_.buses) {
            const auto& bus = buses_.at(d.name);
            const auto observe = [&](const std::string& name, const std::shared_ptr<IOComponent>& requester, const std::string& rq,
                                     const std::shared_ptr<IOComponent>& answerer, const std::string& an) {
                auto checker = b.addByContract({portCheckerContractId(d.width), name, {}});
                for (const auto* n : requests) w.connect(requester->getOutputPinDynamic(rq + n), checker->getInputPinDynamic(n));
                for (const auto* n : responses) w.connect(answerer->getOutputPinDynamic(an + n), checker->getInputPinDynamic(n));
                checkers.push_back(checker);
            };
            for (const auto& master : d.masters) {
                const auto [part, prefix] = splitPort(master.port);
                observe("CHECK_" + d.name + "_" + master.name, parts_.at(part), prefix, bus, master.name + "_");
            }
            for (const auto& port : d.targetPorts()) {
                const auto [part, prefix] = splitPort(port);
                const auto bus_prefix = portPrefix(port);
                observe("CHECK_" + d.name + "_" + bus_prefix.substr(0, bus_prefix.size() - 1), bus, bus_prefix, parts_.at(part), prefix);
            }
        }
    for (const auto& pin : description_.inputs) {
        const auto [part, name] = splitPort(pin.port);
        w.connect(getInputPinDynamic(pin.name), parts_.at(part)->getInputPinDynamic(name));
    }
    for (const auto& pin : description_.outputs) {
        const auto [part, name] = splitPort(pin.port);
        w.connect(parts_.at(part)->getOutputPinDynamic(name), getOutputPinDynamic(pin.name));
    }
    // Clock and reset reach every part and bus input that asks for them.
    std::vector<std::shared_ptr<IOComponent>> children;
    for (const auto& [_, part] : parts_) children.push_back(part);
    for (const auto& [_, bus] : buses_) children.push_back(bus);
    for (const auto& checker : checkers) children.push_back(checker);
    if (description_.check_ports) {
        structural::V flags;
        for (const auto& checker : checkers) flags.push_back(checker->getOutputPin("VIOLATION"));
        structural::Logic any(b);
        any.wire(flags.size() == 1 ? flags.front() : any.any(flags), getOutputPin("PORT_VIOLATION"));
        any.finish();
    }
    for (const auto& child : children)
        for (const auto& [name, _] : child->getAllInputPins())
            if (clockLike(name)) w.connect(getInputPin(isRst(name) ? "RST" : "CLK"), child->getInputPinDynamic(name));
    w.finish();
    // Every part input must be driven: an undriven pin is a description error.
    for (const auto& child : children)
        for (const auto& [name, pin] : child->getAllInputPins())
            if (!pin->getExternalWireBase())
                throw std::invalid_argument("System " + description_.name + ": " + child->getName() + "." + name + " is not connected");
}

std::shared_ptr<IOComponent> ComputerSystem::part(const std::string& name) const {
    const auto it = parts_.find(name);
    if (it == parts_.end()) throw std::invalid_argument("System " + description_.name + " has no part " + name);
    return it->second;
}
std::shared_ptr<IOComponent> ComputerSystem::bus(const std::string& name) const {
    const auto it = buses_.find(name);
    if (it == buses_.end()) throw std::invalid_argument("System " + description_.name + " has no bus " + name);
    return it->second;
}
const MemoryStorage& ComputerSystem::ramStorage() const {
    if (!ram_) throw std::logic_error("System " + description_.name + " has no memory subsystem");
    return ram_->memoryStorage();
}
const MemoryStorage& ComputerSystem::videoStorage() const {
    if (!video_) throw std::logic_error("System " + description_.name + " has no display");
    return video_->memoryStorage();
}
void ComputerSystem::loadElf(const rv32i::RV32IElfImage& elf) {
    if (!ram_) throw std::logic_error("System " + description_.name + " has no memory subsystem");
    elf.requireFitsMemory(ram_limit_);
    if (elf.entryPoint() != 0) throw std::invalid_argument("The system's reset entry is address zero");
    ram_->clearMemoryImage();
    for (const auto& segment : elf.segments()) ram_->initializeMemoryBytes(segment.address, segment.bytes);
    if (video_) video_->clearMemoryImage();
}
void ComputerSystem::setMemoryHistoryRecordingEnabled(bool enabled) {
    const auto visit = [&](const auto& self, const std::shared_ptr<Component>& node) -> void {
        if (auto view = std::dynamic_pointer_cast<MemoryStateView>(node)) view->setMemoryHistoryRecordingEnabled(enabled);
        for (const auto& child : node->getChildren()) self(self, child);
    };
    for (const auto& child : getChildren()) visit(visit, child);
}
rv32i::RV32IArchitecturalState ComputerSystem::snapshotPipelineState(uint32_t pc) const {
    if (!core_) throw std::logic_error("System " + description_.name + " has no core");
    return std::dynamic_pointer_cast<RV32IPipelineStateView>(core_)->snapshotPipelineState(pc);
}
RV32IRetirementObservation ComputerSystem::pendingRetirement() const {
    const auto view = std::dynamic_pointer_cast<RV32IRetirementView>(core_);
    if (!view) throw std::logic_error("System " + description_.name + " has no core with a retirement view");
    return view->pendingRetirement();
}
FrameSnapshot ComputerSystem::captureFrame(SimulationTime time) const {
    if (!video_) throw std::logic_error("System " + description_.name + " has no display");
    FrameSnapshot f;
    f.time = time;
    f.sequence = static_cast<uint32_t>(display_->getOutputPinDynamic("FRAME_SEQUENCE")->getValueAsUInt64());
    f.width = f.stride = frame_width_;
    f.height = frame_height_;
    const auto bits = video_->memoryStorage().readLogic(0, f.width * f.height);
    f.pixels.resize(f.width * f.height);
    f.known.resize(f.pixels.size());
    for (size_t i = 0; i < f.pixels.size(); ++i)
        for (size_t bit = 0; bit < 8; ++bit) {
            const auto v = bits.at(8 * i + bit);
            if (v == LogicValue::HIGH) f.pixels[i] |= 1u << bit;
            if (v == LogicValue::HIGH || v == LogicValue::LOW) f.known[i] |= 1u << bit;
        }
    return f;
}

size_t GameMachineOptions::videoCapacityBytes() const {
    device::FramebufferConfig geometry{width, height, video_bytes};
    return video_bytes ? geometry.capacityBytes() : std::max(size_t{4096}, geometry.capacityBytes());
}
const char* mainMemoryName(MainMemory m) {
    switch (m) {
    case MainMemory::SRAM: return "sram";
    case MainMemory::FF: return "ff";
    case MainMemory::DRAM: return "dram";
    }
    throw std::invalid_argument("Unknown main memory");
}
MainMemory mainMemoryFromName(const std::string& name) {
    for (auto m : {MainMemory::SRAM, MainMemory::FF, MainMemory::DRAM})
        if (name == mainMemoryName(m)) return m;
    throw std::invalid_argument("A games machine's memory is sram, ff or dram, not " + name);
}
void GameMachineOptions::validate() const {
    device::FramebufferConfig{width, height, video_bytes}.validate();
    if ((video_bytes && video_bytes < 4096) || !timer_divisor || !device::validFifoDepth(console_depth)
        || !device::validFifoDepth(button_depth))
        throw std::invalid_argument("Invalid games machine timer, queue or video options");
}
GameMachineOptions gameMachineOptions(const ParameterMap& p) {
    static const std::set<std::string> keys{"caches", "memory", "clock_period", "timer_divisor", "timer_frequency_hz", "width",
                                            "height", "video_bytes", "console_depth", "button_depth"};
    for (const auto& [key, _] : p)
        if (!keys.contains(key)) throw std::invalid_argument("Unknown games machine option " + key);
    GameMachineOptions o;
    const auto flag = [&](const char* key) {
        const auto v = parameterOr(p, key, 0);
        if (v > 1) throw std::invalid_argument(std::string(key) + " must be 0 or 1");
        return v == 1;
    };
    o.caches = flag("caches");
    if (const auto it = p.find("memory"); it != p.end()) o.memory = mainMemoryFromName(it->second);
    o.clock_period = parameterOr(p, "clock_period", 0);
    o.timer_divisor = static_cast<uint32_t>(parameterOr(p, "timer_divisor", 1));
    o.timer_frequency_hz = static_cast<uint32_t>(parameterOr(p, "timer_frequency_hz", 0));
    o.width = parameterOr(p, "width", 64);
    o.height = parameterOr(p, "height", 48);
    o.video_bytes = parameterOr(p, "video_bytes", 0);
    o.console_depth = parameterOr(p, "console_depth", 4);
    o.button_depth = parameterOr(p, "button_depth", 4);
    o.validate();
    return o;
}
SystemDescription withGameOptions(SystemDescription d, const GameMachineOptions& o) {
    o.validate();
    const auto set = [&](const std::string& part, const std::string& key, const std::string& value, const std::string& fallback) {
        for (auto& p : d.parts)
            if (p.name == part) {
                if (value == fallback) p.parameters.erase(key);
                else p.parameters[key] = value;
            }
    };
    for (auto& p : d.parts)
        if (p.family == "memory.subsystem.32") {
            p.parameters["technology"] = mainMemoryName(o.memory);
            p.parameters["caches"] = o.caches ? "default" : "none";
            if (o.clock_period && o.memory != MainMemory::FF) p.parameters["clock_period"] = std::to_string(o.clock_period);
            else p.parameters.erase("clock_period");
        }
    set("TIMER", "divisor", std::to_string(o.timer_divisor), "");
    set("TIMER", "frequency_hz", std::to_string(o.timer_frequency_hz), "");
    set("CONSOLE", "fifo_depth", std::to_string(o.console_depth), "4");
    set("BUTTONS", "fifo_depth", std::to_string(o.button_depth), "4");
    const auto video = o.videoCapacityBytes();
    set("DISPLAY", "width", std::to_string(o.width), "");
    set("DISPLAY", "height", std::to_string(o.height), "");
    set("DISPLAY", "storage_bytes", std::to_string(video), "");
    for (auto& bus : d.buses)
        for (auto& region : bus.regions)
            if (region.name == "VIDEO") region.bytes = video;
    d.validate();
    return d;
}

void registerComputerSystems(ComponentCatalog& catalog) {
    SystemPart<SystemDescription> part;
    part.id = "system.computer";
    part.type = ComputerSystem::TypeName;
    part.test = "ComputerSystemScenarioTest";
    part.parse = systemFromParameters;
    part.pins = systemPins;
    part.structural = [](const std::string& name, const SystemDescription& d, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<ComputerSystem>(context, name, d);
    };
    part.capabilities = {"rv32i-pipeline-state-view", "rv32i-retirement-view", "frame-state-view"};
    registerSystemPart(catalog, std::move(part));
}

} // namespace circuit::system
