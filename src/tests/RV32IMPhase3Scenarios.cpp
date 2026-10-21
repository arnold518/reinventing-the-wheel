#include "tests/ComponentTestModel.hpp"
#include "tests/TestRegistry.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <map>
#include <string>

namespace {
using namespace circuit::test;
using Values = std::map<std::string, uint64_t>;

circuit::BuildProfile scenarioProfile() {
    return circuit::BuildProfileBuilder("phase3-recorded")
        .addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral, circuit::ProfileSelector::any()))
        .unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build();
}

// Scenarios retain wire history for the viewer. Large independent sweeps live
// in RV32IMPhase3Tests and do not run when a viewer opens one of these records.
class Sequence {
    std::map<std::string, size_t> widths_;
    NamedValues state_;
public:
    ComponentTestSpec spec;
    Sequence(const std::string& name, const std::string& contract)
        : spec{name, contract, "DUT", {}, {{"contract", {}, {}, {}}}} {
        for (const auto& p : circuit::builtinComponentCatalog().contract(contract).pins) {
            widths_[p.name] = p.width;
            if (p.direction == PinType::INPUT) state_[p.name] = logicBits(p.width, 0);
        }
        Values reset;
        for (const auto& [name, value] : state_)
            if (name == "RST" || name.ends_with("_RST") || name == "ENABLE" || name == "BACK_READY") reset[name] = 1;
        step("reset", reset, {});
        if (state_.contains("CLK")) {
            cycle("reset-edge", {}, {}, false);
            for (auto& [name, value] : reset) if (name != "ENABLE" && name != "BACK_READY") value = 0;
            step("ready", reset, {});
        }
    }
    void step(std::string id, const Values& changes, const Values& expected, bool emit = true) {
        for (const auto& [name, value] : changes) state_.at(name) = logicBits(widths_.at(name), value);
        NamedValues outputs;
        for (const auto& [name, value] : expected) outputs[name] = logicBits(widths_.at(name), value);
        spec.scenarios[0].actions.push_back({std::move(id), CheckpointKind::Settled, state_, outputs, emit, {}});
    }
    void cycle(const std::string& id, Values changes, const Values& expected, bool emit = true) {
        for (const auto& [name, value] : state_) if (name == "CLK" || name.ends_with("_CLK")) changes[name] = 0;
        step("", changes, {}, false);
        Values high, low;
        for (const auto& [name, value] : state_) if (name == "CLK" || name.ends_with("_CLK")) {high[name] = 1; low[name] = 0;}
        step(id, high, expected, emit);
        step("", low, {}, false);
    }
    void access(const std::string& id, bool write, uint32_t address, uint32_t data,
                uint32_t status, uint32_t read = 0, const std::string& prefix = "") {
        cycle("", {{prefix+"REQ_VALID",1}, {prefix+"REQ_OP",write?1u:0u}, {prefix+"REQ_ADDR",address},
                   {prefix+"REQ_WRITE_DATA",data}, {prefix+"REQ_WRITE_MASK",15}, {prefix+"RSP_READY",0}}, {}, false);
        for (int i = 0; i < 12; ++i) cycle("", {{prefix+"REQ_VALID",0}}, {}, false);
        Values expected{{prefix+"RSP_VALID",1}, {prefix+"RSP_STATUS",status}};
        if (!write && status == 0) expected[prefix+"RSP_READ_DATA"] = read;
        step(id, {}, expected);
        cycle("", {{prefix+"RSP_READY",1}}, {}, false);
        cycle("", {{prefix+"RSP_READY",0}}, {}, false);
    }
};

ComponentTestSpec recordedSpec(const std::string& name, const std::string& contract) {
    Sequence s(name, contract);
    if (contract == "arithmetic.multiply-step.4") {
        s.step("add-selected", {{"HI",13},{"A",11},{"BIT",1}}, {{"SUM",8},{"CARRY",1}});
        s.step("skip", {{"BIT",0}}, {{"SUM",13},{"CARRY",0}});
    } else if (contract == "arithmetic.divide-step.4") {
        s.step("subtract", {{"REMAINDER",3},{"DIVISOR",5},{"BIT",1}}, {{"NEXT_REMAINDER",2},{"QUOTIENT_BIT",1}});
        s.step("retain", {{"REMAINDER",1},{"BIT",0}}, {{"NEXT_REMAINDER",2},{"QUOTIENT_BIT",0}});
    } else if (contract == "arithmetic.multiplier.32" || contract == "arithmetic.divider.32" || contract == "rv32im.mul-div.32") {
        bool div = contract == "arithmetic.divider.32", combined = contract == "rv32im.mul-div.32";
        Values launch{{"A",13},{"B",17},{"START",1}};
        if (combined) launch["OP"] = 0;
        s.cycle("launch", launch, {{"BUSY",1},{"DONE",0},{"READY",0}});
        for (int i = 0; i < 32; ++i) s.cycle("", {{"START",0}}, {}, false);
        Values result{{"DONE",1},{"BUSY",0},{"READY",0}};
        if (combined) result["RESULT"] = 221;
        else if (div) {result["QUOTIENT"] = 0; result["REMAINDER"] = 13;}
        else {result["LO"] = 221; result["HI"] = 0;}
        s.step("complete", {}, result);
        s.cycle("held", {{"A",99},{"B",0}}, result);
        s.cycle("take", {{"TAKE",1}}, {{"READY",1},{"DONE",0},{"BUSY",0}});
    } else if (contract == "rv32im.decode-control") {
        s.step("multiply", {{"INSTRUCTION",0x022081b3}}, {{"LEGAL",1},{"REG_WRITE",1},{"USES_RS1",1},{"USES_RS2",1},{"RS1_ADDR",1},{"RS2_ADDR",2},{"RD_ADDR",3},{"MEM_READ",0},{"MEM_WRITE",0},{"TRAP_REQUEST",0}});
        s.step("reserved", {{"INSTRUCTION",0x04000033}}, {{"LEGAL",0}});
    } else if (contract == "rv32im.pipeline.stage.decode") {
        s.cycle("writeback", {{"WB_VALID",1},{"WB_REG_WRITE",1},{"WB_REGISTER_WRITE",1},{"WB_RD",1},{"WB_VALUE",13}}, {});
        s.step("multiply-decode", {{"WB_VALID",0},{"WB_REG_WRITE",0},{"WB_REGISTER_WRITE",0},{"IN_VALID",1},{"IN_INSTRUCTION",0x022081b3}}, {{"OUT_RS1_VALUE",13},{"OUT_RS2_VALUE",0},{"OUT_CONTROL",0x1383},{"TERMINAL",0}});
    } else if (contract == "rv32im.pipeline.stage.execute") {
        s.step("wait", {{"IN_VALID",1},{"IN_CONTROL",0x1383},{"IN_INSTRUCTION",0x022081b3},{"IN_REGISTER_ADDRESSES",1|(2<<5)|(3<<10)},{"IN_RS1_VALUE",13},{"IN_RS2_VALUE",17}}, {{"EXECUTION_WAIT",1}});
        for (int i = 0; i < 33; ++i) s.cycle("", {}, {}, false);
        s.step("multiply-result", {}, {{"EXECUTION_WAIT",0},{"OUT_ALU_RESULT",221},{"PRETERMINAL",0}});
    } else if (contract == "rv32im.pipeline.coordinator") {
        s.step("move", {{"IMEM_READY",1},{"DMEM_READY",1}}, {{"COMMIT_ENABLE",1},{"EX_MEM_WRITE",1}});
        s.step("arithmetic-hold", {{"EXECUTION_WAIT",1}}, {{"EXECUTION_STALL",1},{"COMMIT_ENABLE",0},{"EX_MEM_WRITE",0},{"MEM_WB_WRITE",0}});
        s.step("older-fault", {{"MEMORY_FAULT",1}}, {{"EXECUTION_STALL",0},{"EX_MEM_FLUSH",1}});
    } else if (contract == "rv32im.core.educational-five-stage") {
        for (int i=0;i<4;++i) s.cycle("", {}, {}, false);
        s.step("fetch-backpressure", {}, {{"IMEM_REQ_VALID",1},{"IMEM_REQ_ADDR",0},{"COMMIT_VALID",0},{"HALTED",0},{"TRAPPED",0}});
    } else if (contract == "device.counter.4") {
        s.cycle("one", {}, {{"COUNT",1},{"TICK",1}});
        s.cycle("two", {}, {{"COUNT",2},{"TICK",1}});
        s.cycle("paused", {{"ENABLE",0}}, {{"COUNT",2},{"TICK",0}});
    } else if (contract == "device.fifo.8") {
        s.cycle("push-a", {{"PUSH",1},{"PUSH_DATA",65}}, {{"COUNT",1},{"DATA",65},{"VALID",1},{"OVERFLOW",0}});
        s.cycle("push-b", {{"PUSH_DATA",66}}, {{"COUNT",2},{"DATA",65}});
        s.cycle("pop-a", {{"PUSH",0},{"POP",1}}, {{"COUNT",1},{"DATA",66}});
        s.cycle("empty", {}, {{"COUNT",0},{"VALID",0},{"DATA",0}});
    } else if (contract == "video.indexed-pixel.32") {
        for (unsigned i=0;i<4;++i) {
            const uint32_t index[]{0xe0,0x1c,3,255}, color[]{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff};
            s.step("pixel-"+std::to_string(i), {{"WORD",0xff031ce0},{"OFFSET",i}}, {{"INDEX",index[i]},{"ARGB",color[i]}});
        }
    } else if (contract == "device.register-port.32") {
        s.step("backend", {{"BACK_READ_DATA",0x12345678}}, {});
        s.access("sampled-read", false, 0, 0, 0, 0x12345678);
        s.access("unaligned", false, 1, 0, 1);
    } else if (contract == "bus.error-target.32") {
        s.access("unmapped", false, 0, 0, 2);
    } else if (contract == "bus.address-router.32" || contract == "bus.instruction-guard.32") {
        s.access("device-fetch-or-unmapped", false, contract == "bus.instruction-guard.32" ? 0x10000000 : 0xffff0000, 0, 2);
    } else if (contract == "device.console.32") {
        s.access("queue-a", true, 0, 65, 0);
        s.step("byte-offered", {}, {{"TX_VALID",1},{"TX_DATA",65}});
        s.cycle("byte-accepted", {{"TX_READY",1}}, {{"TX_VALID",0}});
    } else if (contract == "device.timer.32") {
        s.access("divisor", false, 4, 0, 0, 1);
        s.access("read-only", true, 0, 0, 3);
    } else if (contract == "device.buttons.32") {
        s.cycle("press", {{"INPUT_VALID",1},{"INPUT_BUTTON",2},{"INPUT_DOWN",1}}, {});
        s.cycle("release", {{"INPUT_DOWN",0}}, {});
        s.step("input-idle", {{"INPUT_VALID",0}}, {});
        s.access("released-level", false, 0, 0, 0, 0);
        s.access("tap-retained", false, 4, 0, 0, 0x102);
        s.access("pop-press", true, 16, 1, 0);
        s.access("release-retained", false, 4, 0, 0, 2);
    } else if (contract == "device.framebuffer.32") {
        s.access("width", false, 0, 0, 0, 4);
        s.access("write-pixels", true, 0, 0xff031ce0, 0, 0, "FB_");
        s.access("read-pixels", false, 0, 0, 0, 0xff031ce0, "FB_");
        s.access("present", true, 20, 1, 0);
        s.step("frame-sequence", {}, {{"FRAME_SEQUENCE",1},{"PRESENT",0}});
    }
    return std::move(s.spec);
}
}
void appendRV32IMPhase3Scenarios(std::vector<TestRegistryEntry>& entries) {
    const std::vector<std::pair<std::string,std::string>> cases{
        {"MultiplyStep","arithmetic.multiply-step.4"},{"DivideStep","arithmetic.divide-step.4"},
        {"Multiplier32","arithmetic.multiplier.32"},{"Divider32","arithmetic.divider.32"},{"MulDivUnit32","rv32im.mul-div.32"},
        {"RV32IMDecodeControl","rv32im.decode-control"},{"RV32IMDecodeStage","rv32im.pipeline.stage.decode"},
        {"RV32IMExecuteStage","rv32im.pipeline.stage.execute"},{"RV32IMPipelineCoordinator","rv32im.pipeline.coordinator"},
        {"RV32IMFiveStageCore","rv32im.core.educational-five-stage"},
        {"ClockedCounter","device.counter.4"},{"SynchronousFifo","device.fifo.8"},
        {"DeviceRegisterPort32","device.register-port.32"},{"BusErrorTarget32","bus.error-target.32"},
        {"AddressRouter32","bus.address-router.32"},{"InstructionAccessGuard32","bus.instruction-guard.32"},
        {"ConsoleDevice32","device.console.32"},{"TimerDevice32","device.timer.32"},{"ButtonDevice32","device.buttons.32"},
        {"IndexedPixelSelector32","video.indexed-pixel.32"},{"FramebufferDisplay32","device.framebuffer.32"}};
    for (const auto& [base,contract] : cases) {
        auto name=base+"ScenarioTest";
        const bool combinational=contract.starts_with("arithmetic.multiply-step.")||contract.starts_with("arithmetic.divide-step.")||
            contract=="rv32im.decode-control"||contract=="rv32im.pipeline.coordinator"||contract=="video.indexed-pixel.32";
        const auto kind=contract=="rv32im.core.educational-five-stage"?RegisteredTestKind::Program:
            combinational?RegisteredTestKind::TruthTable:RegisteredTestKind::Sequential;
        entries.push_back({name,base+"Test",kind,{contract},"contract",{"simulation","phase3-scenario"},
            [name,contract]{return std::make_unique<ComponentScenarioTest>(recordedSpec(name,contract),"contract",scenarioProfile());},true});
    }
}
