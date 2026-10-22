#include "modules/rv32im/RV32IMDeviceSystem.hpp"
#include "modules/rv32im/RV32IMCore.hpp"
#include "modules/device/DeviceBus.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/memory/Cache.hpp"
#include "modules/memory/TwoPortMemoryArbiter.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/StructuralNetBuilder.hpp"
#include "components/selection/BuildContext.hpp"
#include "modules/device/DeviceLogic.hpp"
#include "RegisterComponent.hpp"
#include <bit>

namespace {
void pins(IOComponent* p) {
    for (auto n : {"CLK", "RST", "ENABLE", "TX_READY", "INPUT_VALID", "INPUT_DOWN"}) p->addPin(n, PinType::INPUT);
    p->addPin<5>("INPUT_BUTTON", PinType::INPUT);
    for (auto n : {"TX_VALID", "PRESENT", "HALTED", "TRAPPED", "COMMIT_VALID"}) p->addPin(n, PinType::OUTPUT);
    p->addPin<8>("TX_DATA", PinType::OUTPUT);
    p->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    p->addPin<32>("FRAME_SEQUENCE", PinType::OUTPUT);
}
rv32im::DeviceSystemConfig parse(const circuit::ParameterMap& p) {
    const std::set<std::string> keys{"caches", "dram", "timer_divisor", "timer_frequency_hz", "width", "height", "video_bytes", "console_depth", "button_depth"};
    if (!circuit::device::detail::supportsParameters(p, keys)) throw std::invalid_argument("Invalid device-system parameters");
    using circuit::device::detail::parameter;
    rv32im::DeviceSystemConfig c;
    const auto caches = parameter(p, "caches", 0), dram = parameter(p, "dram", 0);
    if (caches > 1 || dram > 1) throw std::invalid_argument("Boolean device-system parameters must be 0 or 1");
    c.caches = caches; c.dram = dram;
    c.timer_divisor = parameter(p, "timer_divisor", 1);
    c.timer_frequency_hz = parameter(p, "timer_frequency_hz", 0);
    c.width = parameter(p, "width", 64); c.height = parameter(p, "height", 48);
    c.video_bytes = parameter(p,"video_bytes",0);
    c.console_depth = parameter(p,"console_depth",4); c.button_depth = parameter(p,"button_depth",4);
    c.validate(); return c;
}
}
namespace circuit::families {
const ComponentFamily RV32IMDeviceSystem{"rv32im.system.devices", "RV32IMDeviceSystem", pins,
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) {
        return Component::createWithContext<::RV32IMDeviceSystem>(c, n, c ? parse(c->parameters()) : rv32im::DeviceSystemConfig{});
    }};
}
size_t rv32im::DeviceSystemConfig::videoCapacityBytes() const {
    circuit::device::FramebufferConfig geometry{width,height,video_bytes};
    return video_bytes ? geometry.capacityBytes() : std::max(size_t{4096},geometry.capacityBytes());
}
void rv32im::DeviceSystemConfig::validate() const {
    circuit::device::FramebufferConfig{width,height,video_bytes}.validate();
    if ((video_bytes && video_bytes<4096) || !timer_divisor || !circuit::device::validFifoDepth(console_depth) || !circuit::device::validFifoDepth(button_depth))
        throw std::invalid_argument("Invalid device-system timer/queue parameters");
}
RV32IMDeviceSystem::RV32IMDeviceSystem(std::string n, rv32im::DeviceSystemConfig c)
    : IOComponent(std::move(n), pins), config_(c) { c.validate(); }
void RV32IMDeviceSystem::buildInternals(ComponentBuilder& b) {
    circuit::detail::StructuralWiring w(b);
    core_ = b.add(circuit::families::RV32IMFiveStageCore, "CORE");
    router_ = b.add(circuit::device::busFamily(circuit::device::BusUnit::Router,32), "DATA_ROUTER",(config_.videoCapacityBytes()==4096?circuit::ParameterMap{}:circuit::ParameterMap{{"video_bytes",std::to_string(config_.videoCapacityBytes())}}));
    auto guard = b.add(circuit::device::busFamily(circuit::device::BusUnit::InstructionGuard,32), "FETCH_GUARD");
    std::shared_ptr<IOComponent> arbiter = b.addNewComponent<circuit::memory::TwoPortMemoryArbiter<32>>("RAM_ARBITER");
    rv32i::MemoryConfiguration mc;
    mc.technology = config_.dram ? circuit::memory::Technology::DRAM : circuit::memory::Technology::FF;
    mc.device.depth_words = 65536;
    ram_ = rv32i::buildMemory(b, *this, "RAM", mc);
    ram_view_ = rv32i::memoryView(ram_);
    peripherals_["console"] = b.add(circuit::families::ConsoleDevice, "CONSOLE",(config_.console_depth==4?circuit::ParameterMap{}:circuit::ParameterMap{{"fifo_depth",std::to_string(config_.console_depth)}}));
    peripherals_["timer"] = b.add(circuit::families::TimerDevice, "TIMER",
        {{"divisor", std::to_string(config_.timer_divisor)}, {"frequency_hz", std::to_string(config_.timer_frequency_hz)}});
    peripherals_["buttons"] = b.add(circuit::families::ButtonDevice, "BUTTONS",(config_.button_depth==4?circuit::ParameterMap{}:circuit::ParameterMap{{"fifo_depth",std::to_string(config_.button_depth)}}));
    auto display = b.add(circuit::families::FramebufferDisplay, "DISPLAY",
        {{"width", std::to_string(config_.width)}, {"height", std::to_string(config_.height)}, {"storage_bytes",std::to_string(config_.videoCapacityBytes())}});
    peripherals_["display"] = display;
    // Contract capability belongs to PIXELS, separate from the register bank.
    for (const auto& child : display->getChildren())
        if (child->getName() == "PIXELS") video_view_ = std::dynamic_pointer_cast<MemoryStateView>(child);
    if (!video_view_) throw std::logic_error("Display PIXELS has no memory observation capability");
    auto clocked = [&](const std::shared_ptr<IOComponent>& c) {
        w.connect(getInputPin("CLK"), c->getInputPin("CLK"));
        w.connect(getInputPin("RST"), c->getInputPin("RST"));
    };
    // Bus payload/handshake only. Clock/reset are supplied once per target.
    auto bus = [&](const std::shared_ptr<IOComponent>& from, const std::string& fp,
                   const std::shared_ptr<IOComponent>& to, const std::string& tp = "") {
        for (auto n : {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"})
            w.connect(from->getOutputPinDynamic(fp+n), to->getInputPinDynamic(tp+n));
        for (auto n : {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"})
            w.connect(to->getOutputPinDynamic(tp+n), from->getInputPinDynamic(fp+n));
    };
    for (auto c : {core_, router_, guard, ram_, display, peripherals_["console"], peripherals_["timer"], peripherals_["buttons"]}) clocked(c);
    // The existing arbiter has separate I/D protocol clock/reset inputs.
    // Both ports belong to this one global clock/reset domain.
    for (const auto& prefix : {std::string("I_"),std::string("D_")}) {
        w.connect(getInputPin("CLK"),arbiter->getInputPin(prefix+"CLK"));
        w.connect(getInputPin("RST"),arbiter->getInputPin(prefix+"RST"));
    }
    w.connect(getInputPin("CLK"), display->getInputPin("FB_CLK"));
    w.connect(getInputPin("RST"), display->getInputPin("FB_RST"));
    w.connect(getInputPin("ENABLE"), core_->getInputPin("ENABLE"));
    bus(core_, "IMEM_", guard); bus(core_, "DMEM_", router_);
    auto ramRoute = [&](const std::shared_ptr<IOComponent>& from, const std::string& prefix, const std::string& port, bool rw) {
        if (!config_.caches) { bus(from, prefix, arbiter, port); return; }
        circuit::memory::CacheConfig cc; cc.sets = 4; cc.block_bytes = 16; cc.backing_depth_words = 65536;
        cc.read_write = rw; cc.scalable_storage = true;
        auto cache = b.addNewComponent<circuit::memory::Cache<32>>(rw ? "DATA_CACHE" : "INSTRUCTION_CACHE", cc);
        clocked(cache); w.connect(w.constant(rw ? "NO_D_INV" : "NO_I_INV", false), cache->getInputPin("INV_VALID"));
        bus(from, prefix, cache); bus(cache, "MEM_", arbiter, port);
    };
    ramRoute(guard, "MEM_", "I_", false); ramRoute(router_, "RAM_", "D_", true); bus(arbiter, "MEM_", ram_);
    bus(router_, "CONSOLE_", peripherals_["console"]); bus(router_, "TIMER_", peripherals_["timer"]);
    bus(router_, "BUTTONS_", peripherals_["buttons"]); bus(router_, "DISPLAY_", display); bus(router_, "VIDEO_", display, "FB_");
    w.connect(getInputPin("TX_READY"), peripherals_["console"]->getInputPin("TX_READY"));
    for (auto n : {"INPUT_VALID", "INPUT_BUTTON", "INPUT_DOWN"}) w.connect(getInputPinDynamic(n), peripherals_["buttons"]->getInputPinDynamic(n));
    for (auto n : {"TX_VALID", "TX_DATA"}) w.connect(peripherals_["console"]->getOutputPinDynamic(n), getOutputPinDynamic(n));
    for (auto n : {"PRESENT", "FRAME_SEQUENCE"}) w.connect(display->getOutputPinDynamic(n), getOutputPinDynamic(n));
    for (auto n : {"HALTED", "TRAPPED", "TRAP_CAUSE", "COMMIT_VALID"}) w.connect(core_->getOutputPinDynamic(n), getOutputPinDynamic(n));
    w.finish();
}
std::shared_ptr<IOComponent> RV32IMDeviceSystem::peripheral(const std::string& n) const { return peripherals_.at(n); }
void RV32IMDeviceSystem::loadElf(const rv32i::RV32IElfImage& elf) {
    elf.requireFitsMemory(0x40000);
    if (elf.entryPoint() != 0) throw std::invalid_argument("Device system reset entry is zero");
    ram_view_->clearMemoryImage();
    for (const auto& segment : elf.segments()) ram_view_->initializeMemoryBytes(segment.address, segment.bytes);
    video_view_->clearMemoryImage();
}
void RV32IMDeviceSystem::setMemoryHistoryRecordingEnabled(bool enabled) {
    // Include optional cache banks as well as backing RAM/VRAM. Simulator
    // wire history and native-device memory history are separate policies.
    const auto visit = [&](const auto& self, const std::shared_ptr<Component>& node) -> void {
        if (auto view = std::dynamic_pointer_cast<MemoryStateView>(node)) view->setMemoryHistoryRecordingEnabled(enabled);
        for (const auto& child : node->getChildren()) self(self, child);
    };
    for (const auto& child : getChildren()) visit(visit, child);
}
rv32i::RV32IArchitecturalState RV32IMDeviceSystem::snapshotPipelineState(uint32_t pc) const {
    return std::dynamic_pointer_cast<RV32IPipelineStateView>(core_)->snapshotPipelineState(pc);
}
RV32IRetirementObservation RV32IMDeviceSystem::pendingRetirement() const {
    return std::dynamic_pointer_cast<RV32IRetirementView>(core_)->pendingRetirement();
}
FrameSnapshot RV32IMDeviceSystem::captureFrame(SimulationTime time) const {
    FrameSnapshot f; f.time = time; f.sequence = static_cast<uint32_t>(getOutputPin<32>("FRAME_SEQUENCE")->getValueAsUInt64());
    f.width = f.stride = config_.width; f.height = config_.height;
    const auto bits = video_view_->memoryStorage().readLogic(0, f.width * f.height);
    f.pixels.resize(f.width*f.height); f.known.resize(f.pixels.size());
    for (size_t i=0;i<f.pixels.size();++i) for (size_t bit=0;bit<8;++bit) {
        const auto v = bits.at(8*i+bit);
        if (v==LogicValue::HIGH) f.pixels[i] |= 1u<<bit;
        if (v==LogicValue::HIGH || v==LogicValue::LOW) f.known[i] |= 1u<<bit;
    }
    return f;
}
void rv32im::registerDeviceSystem(circuit::ComponentCatalog& c) {
    detail::registerStructural(c, circuit::families::RV32IMDeviceSystem, "RV32IMDeviceSystemTest", "clock-boundary",
        [](const circuit::ParameterMap& p) { try { (void)parse(p); return true; } catch (const std::exception&) { return false; } },
        {"rv32i-pipeline-state-view", "rv32i-retirement-view", "frame-state-view"}, "known-binary");
}
