#include "modules/rv32i/RV32IALU.hpp"
#include "rv32im/RV32IMInstruction.hpp"
#include "RV32IMScenarioProfile.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/PortRule.hpp"
#include "tests/TestRegistry.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <map>
#include <set>
#include <bit>
#include <string>
#include <tuple>

namespace {
using namespace circuit::test;
using Values = std::map<std::string, uint64_t>;

// Scenarios retain wire history for the viewer. Large independent sweeps live
// in RV32IMPhase3Tests and do not run when a viewer opens one of these records.


class Sequence {
    std::map<std::string, size_t> widths_;
    NamedValues state_;
public:
    ComponentTestSpec spec;
    Sequence(const std::string& name, const std::string& contract, const std::string& instance,
             circuit::ParameterMap parameters = {})
        : spec{name, contract, instance, parameters, {{"contract", {}, {}, {}}}} {
        for (const auto& p : circuit::builtinComponentCatalog().contract(contract).pinsFor(parameters)) {
            widths_[p.name] = p.width;
            if (p.direction == PinType::INPUT) state_[p.name] = logicBits(p.width, 0);
        }
        Values reset;
        for (const auto& [name, value] : state_)
            if (name == "RST" || name.ends_with("_RST") || name == "ENABLE" || name == "BACK_READY") reset[name] = 1;
        const auto [held, idle] = circuit::test::portRuleExpectations(circuit::builtinComponentCatalog().contract(contract).pinsFor(parameters));
        step("reset", reset, held);
        if (state_.contains("CLK")) {
            cycle("reset-edge", {}, {}, false);
            for (auto& [name, value] : reset) if (name != "ENABLE" && name != "BACK_READY") value = 0;
            step("ready", reset, idle);
        }
        edges = 0;
    }
    unsigned edges = 0;   // rising clock edges since reset was released
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
        ++edges;
    }
    void access(const std::string& id, bool write, uint32_t address, uint32_t data,
                uint32_t status, uint32_t read = 0, const std::string& prefix = "", uint32_t mask = 15) {
        cycle("", {{prefix+"REQ_VALID",1}, {prefix+"REQ_OP",write?1u:0u}, {prefix+"REQ_ADDR",address},
                   {prefix+"REQ_WRITE_DATA",data}, {prefix+"REQ_WRITE_MASK",mask}, {prefix+"RSP_READY",0}}, {}, false);
        for (int i = 0; i < 12; ++i) cycle("", {{prefix+"REQ_VALID",0}}, {}, false);
        Values expected{{prefix+"RSP_VALID",1}, {prefix+"RSP_STATUS",status}};
        if (!write && status == 0) expected[prefix+"RSP_READ_DATA"] = read;
        step(id, {}, expected);
        cycle("", {{prefix+"RSP_READY",1}}, {}, false);
        cycle("", {{prefix+"RSP_READY",0}}, {}, false);
    }
};

ComponentTestSpec recordedSpec(const std::string& name, const std::string& contract, const std::string& instance) {
    // The timer runs with a divisor and a declared clock (the 4.5 ns machine).
    const circuit::ParameterMap parameters = contract == "device.timer"
        ? circuit::ParameterMap{{"divisor", "4"}, {"frequency_hz", "222000000"}} : circuit::ParameterMap{};
    Sequence s(name, contract, instance, parameters);
    if (contract == "arithmetic.multiply-step.4") {
        s.step("add-selected", {{"HI",13},{"A",11},{"BIT",1}}, {{"SUM",8},{"CARRY",1}});
        s.step("skip", {{"BIT",0}}, {{"SUM",13},{"CARRY",0}});
        s.step("zero",{{"HI",0},{"A",0},{"BIT",1}},{{"SUM",0},{"CARRY",0}});
        s.step("maximum-carry",{{"HI",15},{"A",15}},{{"SUM",14},{"CARRY",1}});
        s.step("carry-boundary",{{"HI",15},{"A",1}},{{"SUM",0},{"CARRY",1}});
        s.step("maximum-skip",{{"BIT",0}},{{"SUM",15},{"CARRY",0}});
    } else if (contract == "arithmetic.divide-step.4") {
        s.step("subtract", {{"REMAINDER",3},{"DIVISOR",5},{"BIT",1}}, {{"NEXT_REMAINDER",2},{"QUOTIENT_BIT",1}});
        s.step("retain", {{"REMAINDER",1},{"BIT",0}}, {{"NEXT_REMAINDER",2},{"QUOTIENT_BIT",0}});
        s.step("equal",{{"REMAINDER",2},{"DIVISOR",5},{"BIT",1}},{{"NEXT_REMAINDER",0},{"QUOTIENT_BIT",1}});
        s.step("high-remainder",{{"REMAINDER",15},{"DIVISOR",15}},{{"NEXT_REMAINDER",0},{"QUOTIENT_BIT",1}});
        s.step("zero-divisor",{{"REMAINDER",0},{"DIVISOR",0},{"BIT",0}},{{"NEXT_REMAINDER",0},{"QUOTIENT_BIT",1}});
        s.step("below-divisor",{{"DIVISOR",15},{"BIT",1}},{{"NEXT_REMAINDER",1},{"QUOTIENT_BIT",0}});
    } else if (contract == "arithmetic.iterative.multiplier.32" || contract == "arithmetic.iterative.divider.32" || contract == "arithmetic.iterative.mul-div.32") {
        const bool div=contract=="arithmetic.iterative.divider.32", combined=contract=="arithmetic.iterative.mul-div.32";
        const auto transaction=[&](const std::string& id,uint32_t a,uint32_t b,unsigned op,bool pause=false) {
            Values launch{{"A",a},{"B",b},{"START",1},{"TAKE",0},{"ENABLE",1},{"CANCEL",0}};
            if(combined)launch["OP"]=op;
            else if(div)launch["SIGNED"]=op;
            else {launch["SIGNED_A"]=op&1;launch["SIGNED_B"]=(op>>1)&1;}
            s.cycle(id+"-launch",launch,{{"BUSY",1},{"DONE",0},{"READY",0}});
            if(pause)for(unsigned i=0;i<3;++i)s.cycle(id+"-paused",{{"START",0},{"ENABLE",0}},{{"BUSY",1},{"DONE",0},{"READY",0}},i==2);
            for(unsigned i=0;i<32;++i)s.cycle("",{{"START",0},{"ENABLE",1}}, {},false);
            Values result{{"DONE",1},{"BUSY",0},{"READY",0}};
            using rv32im::MOperation;
            if(combined)result["RESULT"]=rv32im::referenceResult(static_cast<MOperation>(op),a,b);
            else if(div){
                result["QUOTIENT"]=rv32im::referenceResult(op?MOperation::DIV:MOperation::DIVU,a,b);
                result["REMAINDER"]=rv32im::referenceResult(op?MOperation::REM:MOperation::REMU,a,b);
            }else{
                result["LO"]=static_cast<uint32_t>(uint64_t(a)*b);
                const int64_t aa=(op&1)?int64_t(std::bit_cast<int32_t>(a)):int64_t(a);
                const int64_t bb=(op&2)?int64_t(std::bit_cast<int32_t>(b)):int64_t(b);
                const uint64_t product=op?static_cast<uint64_t>(aa*bb):uint64_t(a)*b;
                result["HI"]=product>>32;
            }
            s.step(id+"-complete",{},result);
            // A second launch cannot replace an unconsumed completion.
            s.cycle(id+"-held",{{"A",99},{"B",0},{"START",1},{"ENABLE",0}},result);
            s.cycle(id+"-take",{{"START",0},{"ENABLE",1},{"TAKE",1}},{{"READY",1},{"DONE",0},{"BUSY",0}});
        };
        if(combined){
            for(unsigned op=0;op<8;++op)transaction(rv32im::name(static_cast<rv32im::MOperation>(op)),0xfffffff9,3,op,op==0);
            transaction("divide-zero",13,0,4);
            transaction("signed-overflow",0x80000000,0xffffffff,4);
        }else if(div){
            transaction("unsigned",100,7,0,true);
            transaction("negative-dividend",0xfffffff9,3,1);
            transaction("negative-divisor",7,0xfffffffd,1);
            transaction("divide-zero",13,0,1);
            transaction("signed-overflow",0x80000000,0xffffffff,1);
        }else{
            transaction("unsigned",13,17,0,true);
            transaction("signed",0xfffffff9,3,3);
            transaction("mixed-sign",0x80000000,0xffffffff,1);
            transaction("signed-minimum",0x80000000,0xffffffff,3);
            transaction("unsigned-high",0xffffffff,0xffffffff,0);
        }
        s.cycle("cancel-launch",{{"A",13},{"B",17},{"TAKE",0},{"START",1}},{{"BUSY",1},{"DONE",0}});
        s.cycle("cancel-while-paused",{{"START",0},{"CANCEL",1},{"ENABLE",0}},{{"BUSY",0},{"DONE",0},{"READY",0}});
        s.step("canceled-ready",{{"CANCEL",0},{"ENABLE",1}},{{"BUSY",0},{"DONE",0},{"READY",1}});
        s.cycle("reset-launch",{{"START",1}},{{"BUSY",1}});
        s.step("reset-in-flight",{{"RST",1},{"START",0}},{{"BUSY",0},{"DONE",0},{"READY",0}});
        s.step("reset-released",{{"RST",0}},{{"BUSY",0},{"DONE",0},{"READY",1}});
    } else if (contract == "rv32im.decode-control") {
        // An M instruction: register fields pass through, WRITEBACK_SEL = 1,
        // MEM_SIZE = 2, every other control field clear (§ 5.1.2.1).
        const Values mControl{{"LEGAL",1},{"REG_WRITE",1},{"USES_RS1",1},{"USES_RS2",1},{"RS1_ADDR",1},{"RS2_ADDR",2},{"RD_ADDR",3},
            {"WRITEBACK_SEL",1},{"MEM_SIZE",2},{"IMM",0},{"ALU_OP",0},{"ALU_A_SEL",0},{"ALU_B_SEL",0},{"MEM_READ",0},{"MEM_WRITE",0},
            {"LOAD_SIGN_EXTEND",0},{"BRANCH_TYPE",0},{"JUMP_TYPE",0},{"HALT_REQUEST",0},{"TRAP_REQUEST",0},{"DECODE_TRAP_CAUSE",0}};
        for(unsigned op=0;op<8;++op)s.step(rv32im::name(static_cast<rv32im::MOperation>(op)),{{"INSTRUCTION",0x022081b3|(op<<12)}},mControl);
        s.step("integer-add",{{"INSTRUCTION",0x002081b3}},{{"LEGAL",1},{"REG_WRITE",1},{"RD_ADDR",3}});
        s.step("x0-destination",{{"INSTRUCTION",0x02208033}},{{"LEGAL",1},{"RD_ADDR",0}});
        s.step("reserved", {{"INSTRUCTION",0x04000033}}, {{"LEGAL",0}});
        s.step("illegal",{{"INSTRUCTION",0xffffffff}},{{"LEGAL",0}});
    } else if (contract == "rv32im.pipeline.stage.decode") {
        s.cycle("writeback-rs1",{{"WB_VALID",1},{"WB_REG_WRITE",1},{"WB_REGISTER_WRITE",1},{"WB_RD",1},{"WB_VALUE",13}},{});
        s.cycle("writeback-rs2",{{"WB_RD",2},{"WB_VALUE",17}},{});
        for(unsigned op=0;op<8;++op)s.step(rv32im::name(static_cast<rv32im::MOperation>(op)),
            {{"WB_VALID",0},{"WB_REG_WRITE",0},{"WB_REGISTER_WRITE",0},{"IN_VALID",1},{"IN_PC",64},{"IN_INSTRUCTION",0x022081b3|(op<<12)}},
            {{"OUT_RS1_VALUE",13},{"OUT_RS2_VALUE",17},{"OUT_CONTROL",0x1383},{"OUT_REGISTER_ADDRESSES",1|(2<<5)|(3<<10)},{"OUT_PC",64},{"OUT_VALID",1},{"TERMINAL",0}});
        s.step("writeback-bypass",{{"WB_VALID",1},{"WB_REG_WRITE",1},{"WB_REGISTER_WRITE",1},{"WB_RD",1},{"WB_VALUE",42}},{{"OUT_RS1_VALUE",42}});
        s.step("x0-source",{{"IN_INSTRUCTION",0x022001b3},{"WB_RD",0},{"WB_VALUE",99}},{{"OUT_RS1_VALUE",0},{"OUT_RS2_VALUE",17}});
        s.step("invalid-packet",{{"IN_VALID",0}},{{"OUT_VALID",0},{"LOAD_USE_HAZARD",0},{"TERMINAL",0}});
    } else if (contract == "rv32im.pipeline.stage.execute") {
        Values packet{{"IN_VALID",1},{"IN_CONTROL",0x1383},{"IN_INSTRUCTION",0x022081b3},{"IN_REGISTER_ADDRESSES",1|(2<<5)|(3<<10)},{"IN_RS1_VALUE",13},{"IN_RS2_VALUE",17}};
        s.step("multiply-wait",packet,{{"EXECUTION_WAIT",1}});
        for(unsigned i=0;i<33;++i)s.cycle("",{}, {},false);
        s.step("multiply-result",{},{{"EXECUTION_WAIT",0},{"OUT_ALU_RESULT",221},{"PRETERMINAL",0},{"OUT_VALID",1},
            {"OUT_INSTRUCTION",0x022081b3},{"OUT_CONTROL",0x1383},{"OUT_REGISTER_ADDRESSES",1|(2<<5)|(3<<10)},{"OUT_EXECUTION_STATUS",0},
            {"REDIRECT",0},{"IS_BRANCH",0},{"CONTROL_TAKEN",0}});
        s.cycle("completion-held",{{"ENABLE",0}},{{"EXECUTION_WAIT",0},{"OUT_ALU_RESULT",221}});
        s.cycle("take-multiply",{{"ENABLE",1},{"TAKE",1}},{});
        s.step("invalid-packet",{{"TAKE",0},{"IN_VALID",0}},{{"EXECUTION_WAIT",0}});
        s.step("older-load-pending",{{"IN_VALID",1},{"IN_RS1_VALUE",99},{"EX_MEM_VALID",1},{"EX_MEM_REG_WRITE",1},{"EX_MEM_RD",1},{"EX_MEM_RESULT_READY",0},{"EX_MEM_VALUE",13}},{{"EXECUTION_WAIT",1}});
        for(unsigned i=0;i<5;++i)s.cycle("",{}, {},false);
        s.step("pending-still-waits",{},{{"EXECUTION_WAIT",1}});
        s.step("older-load-forwarded",{{"EX_MEM_RESULT_READY",1}},{{"EXECUTION_WAIT",1}});
        for(unsigned i=0;i<33;++i)s.cycle("",{}, {},false);
        s.step("forwarded-result",{},{{"EXECUTION_WAIT",0},{"OUT_ALU_RESULT",221}});
        s.cycle("take-forwarded",{{"TAKE",1}},{});
        s.step("faulted-packet",{{"TAKE",0},{"IN_FETCH_STATUS",2}},{{"PRETERMINAL",1},{"EXECUTION_WAIT",0}});
        s.step("signed-divide",{{"IN_FETCH_STATUS",0},{"EX_MEM_VALID",0},{"IN_INSTRUCTION",0x0220c1b3},{"IN_RS1_VALUE",0xfffffff9},{"IN_RS2_VALUE",3}},{{"EXECUTION_WAIT",1}});
        for(unsigned i=0;i<33;++i)s.cycle("",{}, {},false);
        s.step("divide-result",{},{{"EXECUTION_WAIT",0},{"OUT_ALU_RESULT",0xfffffffe}});
        s.cycle("cancel-completion",{{"CANCEL",1},{"ENABLE",0}},{{"EXECUTION_WAIT",0}});
        s.step("canceled-invalid",{{"CANCEL",0},{"IN_VALID",0},{"ENABLE",1}},{{"EXECUTION_WAIT",0}});
    } else if (contract == "rv32im.pipeline.coordinator") {
        s.step("move", {{"IMEM_READY",1},{"DMEM_READY",1}}, {{"COMMIT_ENABLE",1},{"IF_ID_WRITE",1},{"ID_EX_WRITE",1},{"EX_MEM_WRITE",1},
            {"MEM_WB_WRITE",1},{"FETCH_PC_WRITE",1},{"EXECUTION_STALL",0}});
        // The hold suppresses every packet write and flush, fetch advance and
        // response consumption, and retirement.
        s.step("arithmetic-hold", {{"EXECUTION_WAIT",1}}, {{"EXECUTION_STALL",1},{"COMMIT_ENABLE",0},
            {"IF_ID_WRITE",0},{"ID_EX_WRITE",0},{"EX_MEM_WRITE",0},{"MEM_WB_WRITE",0},
            {"IF_ID_FLUSH",0},{"ID_EX_FLUSH",0},{"EX_MEM_FLUSH",0},{"MEM_WB_FLUSH",0},{"FETCH_PC_WRITE",0},{"FETCH_RESPONSE_TAKE",0}});
        s.step("older-fault", {{"MEMORY_FAULT",1}}, {{"EXECUTION_STALL",0},{"EX_MEM_FLUSH",1}});
        s.step("older-terminal",{{"MEMORY_FAULT",0},{"WB_TERMINAL",1}},{{"EXECUTION_STALL",0}});
        s.step("resume",{{"WB_TERMINAL",0},{"EXECUTION_WAIT",0}},{{"COMMIT_ENABLE",1},{"EX_MEM_WRITE",1},{"EXECUTION_STALL",0}});
        s.step("disabled",{{"ENABLE",0}},{{"COMMIT_ENABLE",0},{"EX_MEM_WRITE",0}});
        s.step("halted",{{"ENABLE",1},{"HALTED",1},{"EXECUTION_WAIT",1}},{{"COMMIT_ENABLE",0},{"EXECUTION_STALL",0}});
    } else if (contract == "rv32im.core.educational-five-stage") {
        for(int i=0;i<4;++i)s.cycle("",{}, {},false);
        s.step("fetch-backpressure",{},{{"IMEM_REQ_VALID",1},{"IMEM_REQ_ADDR",0},{"COMMIT_VALID",0},{"HALTED",0},{"TRAPPED",0}});
        for(int i=0;i<4;++i)s.cycle("",{}, {},false);
        s.step("request-retained",{},{{"IMEM_REQ_VALID",1},{"IMEM_REQ_ADDR",0},{"COMMIT_VALID",0},{"HALTED",0},{"TRAPPED",0}});
        s.step("disabled",{{"ENABLE",0}},{{"COMMIT_VALID",0},{"HALTED",0},{"TRAPPED",0}});
        s.step("reset-request",{{"RST",1}},{{"COMMIT_VALID",0},{"HALTED",0},{"TRAPPED",0}});
        s.step("restart",{{"RST",0},{"ENABLE",1}},{{"HALTED",0},{"TRAPPED",0}});
    } else if (contract == "device.counter.4") {
        s.cycle("one", {}, {{"COUNT",1},{"TICK",1}});
        s.cycle("two", {}, {{"COUNT",2},{"TICK",1}});
        s.cycle("paused", {{"ENABLE",0}}, {{"COUNT",2},{"TICK",0}});
        s.cycle("resumed", {{"ENABLE",1}}, {{"COUNT",3},{"TICK",1}});
        for(int i=0;i<12;++i)s.cycle("",{}, {},false);
        s.step("maximum", {}, {{"COUNT",15}});
        s.cycle("wraps", {}, {{"COUNT",0},{"TICK",1}});
    } else if (contract == "device.fifo.8") {
        s.cycle("push-a", {{"PUSH",1},{"PUSH_DATA",65}}, {{"COUNT",1},{"DATA",65},{"VALID",1},{"OVERFLOW",0}});
        s.cycle("push-b", {{"PUSH_DATA",66}}, {{"COUNT",2},{"DATA",65}});
        s.cycle("pop-a", {{"PUSH",0},{"POP",1}}, {{"COUNT",1},{"DATA",66}});
        s.cycle("empty", {}, {{"COUNT",0},{"VALID",0},{"DATA",0}});
        // Fill all four places, then push once more: the byte is lost and flagged.
        for(unsigned i=1;i<=4;++i)s.cycle(i==4?"full":"",{{"POP",0},{"PUSH",1},{"PUSH_DATA",i}},
            i==4?Values{{"COUNT",4},{"FULL",1},{"PUSH_READY",0},{"DATA",1},{"OVERFLOW",0}}:Values{},i==4);
        s.cycle("overflow", {{"PUSH_DATA",5}}, {{"COUNT",4},{"OVERFLOW",1},{"DATA",1}});
        s.cycle("pop-after-overflow", {{"PUSH",0},{"POP",1}}, {{"COUNT",3},{"DATA",2},{"FULL",0},{"PUSH_READY",1},{"OVERFLOW",1}});
        s.cycle("clear-overflow", {{"POP",0},{"CLEAR_OVERFLOW",1}}, {{"COUNT",3},{"OVERFLOW",0}});
    } else if (contract == "device.word.register-port.32") {
        // The request is held on the accepting edge. In the next cycle the
        // strobe is high and the answer is already offered (the response
        // buffer falls through while empty); the buffer holds it after that.
        const auto take=[&](const std::string& id){
            s.cycle(id,{{"RSP_READY",1}},{{"RSP_VALID",0},{"REQ_READY",1}});
            s.step("",{{"RSP_READY",0}},{},false);
        };
        s.step("backend", {{"BACK_READ_DATA",0x12345678}}, {{"READ_STROBE",0},{"WRITE_STROBE",0}});
        s.cycle("read-accepted", {{"REQ_VALID",1},{"REQ_OP",0},{"REQ_ADDR",8},{"RSP_READY",0}},
            {{"REQ_READY",0},{"READ_STROBE",1},{"WRITE_STROBE",0},{"IS_READ",1},{"IS_WRITE",0},{"ADDRESS",8},
             {"RSP_VALID",1},{"RSP_STATUS",0},{"RSP_READ_DATA",0x12345678}});
        s.cycle("read-held", {{"REQ_VALID",0}}, {{"READ_STROBE",0},{"RSP_VALID",1},{"RSP_STATUS",0},{"RSP_READ_DATA",0x12345678}});
        s.step("backend-changes", {{"BACK_READ_DATA",0}}, {{"RSP_VALID",1},{"RSP_READ_DATA",0x12345678}});
        take("read-taken");
        s.cycle("write-waits", {{"REQ_VALID",1},{"REQ_OP",1},{"REQ_ADDR",4},{"REQ_WRITE_DATA",0xcafef00d},{"REQ_WRITE_MASK",15},{"BACK_READY",0}},
            {{"WRITE_STROBE",0},{"IS_WRITE",1},{"ADDRESS",4},{"WRITE_DATA",0xcafef00d},{"WRITE_MASK",15},{"REQ_READY",0}});
        s.cycle("write-still-waits", {{"REQ_VALID",0}}, {{"WRITE_STROBE",0},{"RSP_VALID",0}});
        s.step("backend-ready", {{"BACK_READY",1}}, {{"WRITE_STROBE",1},{"RSP_VALID",1},{"RSP_STATUS",0}});
        s.cycle("write-answered", {}, {{"WRITE_STROBE",0},{"RSP_VALID",1},{"RSP_STATUS",0}});
        take("write-taken");
        s.cycle("empty-mask", {{"REQ_VALID",1},{"REQ_WRITE_MASK",0}}, {{"WRITE_STROBE",0}});
        s.cycle("empty-mask-answered", {{"REQ_VALID",0}}, {{"RSP_VALID",1},{"RSP_STATUS",0}});
        take("empty-mask-taken");
        s.cycle("partial-mask", {{"REQ_VALID",1},{"REQ_WRITE_MASK",3}}, {{"WRITE_STROBE",0}});
        s.cycle("partial-mask-refused", {{"REQ_VALID",0}}, {{"RSP_VALID",1},{"RSP_STATUS",6}});
        take("partial-mask-taken");
        s.cycle("unaligned", {{"REQ_VALID",1},{"REQ_OP",0},{"REQ_ADDR",1}}, {{"READ_STROBE",0}});
        s.cycle("unaligned-refused", {{"REQ_VALID",0}}, {{"RSP_VALID",1},{"RSP_STATUS",1}});
        take("unaligned-taken");
        s.cycle("unknown-operation", {{"REQ_VALID",1},{"REQ_OP",2},{"REQ_ADDR",0}}, {{"READ_STROBE",0},{"WRITE_STROBE",0}});
        s.cycle("unknown-refused", {{"REQ_VALID",0}}, {{"RSP_VALID",1},{"RSP_STATUS",4}});
        take("unknown-taken");
    } else if (contract == "device.console") {
        s.access("room", false, 4, 0, 0, 1);
        s.access("queue-empty", false, 8, 0, 0, 0);
        s.access("queue-a", true, 0, 65, 0);
        s.step("byte-offered", {}, {{"TX_VALID",1},{"TX_DATA",65}});
        s.access("one-waiting", false, 8, 0, 0, 1);
        s.cycle("byte-accepted", {{"TX_READY",1}}, {{"TX_VALID",0}});
        s.step("", {{"TX_READY",0}}, {}, false);
        s.access("queue-drained", false, 8, 0, 0, 0);
        for(unsigned c:{66u,67u,68u,69u})s.access(std::string("queue-")+char(c+32),true,0,c,0);
        s.access("queue-full", false, 8, 0, 0, 4);
        s.access("no-room", false, 4, 0, 0, 0);
        s.step("oldest-first", {}, {{"TX_VALID",1},{"TX_DATA",66}});
        s.access("status-read-only", true, 4, 1, 3);
    } else if (contract == "device.timer") {
        s.access("divisor", false, 4, 0, 0, 4);
        s.access("frequency-declared", false, 8, 0, 0, 222000000);
        s.access("count-read-only", true, 0, 0, 3);
        s.access("divisor-read-only", true, 4, 8, 3);
        // The count is sampled on the edge after the accepting edge: one tick
        // per four clock cycles since reset was released.
        s.access("count", false, 0, 0, 0, (s.edges + 1) / 4);
        s.access("count-later", false, 0, 0, 0, (s.edges + 1) / 4);
        for (int i = 0; i < 9; ++i) s.cycle("", {}, {}, false);
        s.access("count-after-idle", false, 0, 0, 0, (s.edges + 1) / 4);
    } else if (contract == "device.buttons") {
        // A host input event changes state without answering the bus.
        const Values quiet{{"RSP_VALID",0},{"REQ_READY",1}};
        s.access("no-events", false, 12, 0, 0, 0);
        s.cycle("press", {{"INPUT_VALID",1},{"INPUT_BUTTON",2},{"INPUT_DOWN",1}}, quiet);
        s.step("press-idle", {{"INPUT_VALID",0}}, quiet);
        s.access("held-level", false, 0, 0, 0, 4);
        s.cycle("release", {{"INPUT_VALID",1},{"INPUT_DOWN",0}}, quiet);
        s.step("input-idle", {{"INPUT_VALID",0}}, quiet);
        s.access("released-level", false, 0, 0, 0, 0);
        s.access("event-waits", false, 12, 0, 0, 1);
        s.access("two-events", false, 8, 0, 0, 2);
        s.access("tap-retained", false, 4, 0, 0, 0x102);
        s.access("pop-press", true, 16, 1, 0);
        s.access("release-retained", false, 4, 0, 0, 2);
        s.access("pop-release", true, 16, 1, 0);
        s.access("queue-empty", false, 8, 0, 0, 0);
        // Five presses on five edges into a four-place queue: the fifth is
        // lost and flagged, its level still changes.
        for (unsigned button = 0; button < 5; ++button)
            s.cycle(button == 4 ? "fifth-press-lost" : "", {{"INPUT_VALID",1},{"INPUT_BUTTON",button},{"INPUT_DOWN",1}}, quiet, button == 4);
        s.step("presses-idle", {{"INPUT_VALID",0}}, quiet);
        s.access("overflow-flagged", false, 12, 0, 0, 3);
        s.access("queue-full", false, 8, 0, 0, 4);
        s.access("oldest-kept", false, 4, 0, 0, 0x100);
        s.access("five-levels", false, 0, 0, 0, 0x1f);
        s.access("clear-overflow", true, 20, 1, 0);
        s.access("overflow-cleared", false, 12, 0, 0, 1);
    } else if (contract == "device.framebuffer") {
        s.access("width", false, 0, 0, 0, 4);
        s.access("height", false, 4, 0, 0, 4);
        s.access("stride", false, 8, 0, 0, 4);
        s.access("format-rgb332", false, 12, 0, 0, 1);
        s.access("no-frames-yet", false, 16, 0, 0, 0);
        // A 4 x 4 picture, one word per row: red around a green middle.
        s.access("row-0-red", true, 0, 0xe0e0e0e0, 0, 0, "FB_");
        s.access("row-1-red-green", true, 4, 0xe01c1ce0, 0, 0, "FB_");
        s.access("row-2-red-green", true, 8, 0xe01c1ce0, 0, 0, "FB_");
        s.access("row-3-red", true, 12, 0xe0e0e0e0, 0, 0, "FB_");
        s.access("read-row-1", false, 4, 0, 0, 0xe01c1ce0, "FB_");
        s.access("present", true, 20, 1, 0);
        s.step("frame-1", {}, {{"FRAME_SEQUENCE",1},{"PRESENT",0}});
        // One pixel by byte mask: (1, 1) turns blue, the row's other bytes stay.
        s.access("pixel-1-1-blue", true, 4, 0x00000300, 0, 0, "FB_", 0x2);
        s.access("read-row-1-again", false, 4, 0, 0, 0xe01c03e0, "FB_");
        s.access("present-again", true, 20, 1, 0);
        s.access("frame-sequence-2", false, 16, 0, 0, 2);
        s.access("past-storage", false, 16, 0, 2, 0, "FB_");
        s.access("width-read-only", true, 0, 8, 3);
    }
    return std::move(s.spec);
}
}
void appendRV32IMPhase3Scenarios(std::vector<TestRegistryEntry>& entries) {
    const std::vector<std::tuple<std::string,std::string,std::string>> cases{
        {"MultiplyStep","arithmetic.multiply-step.4","MULTIPLY_STEP_ROOT"},{"DivideStep","arithmetic.divide-step.4","DIVIDE_STEP_ROOT"},
        {"MultiplierWidth32","arithmetic.iterative.multiplier.32","MULTIPLIER_W32_ROOT"},{"DividerWidth32","arithmetic.iterative.divider.32","DIVIDER_W32_ROOT"},{"MulDivUnitWidth32","arithmetic.iterative.mul-div.32","MULDIV_W32_ROOT"},
        {"RV32IMDecodeControl","rv32im.decode-control","RV32IM_DECODE_CONTROL_ROOT"},{"RV32IMDecodeStage","rv32im.pipeline.stage.decode","RV32IM_DECODE_STAGE_ROOT"},
        {"RV32IMExecuteStage","rv32im.pipeline.stage.execute","RV32IM_EXECUTE_STAGE_ROOT"},{"RV32IMPipelineCoordinator","rv32im.pipeline.coordinator","RV32IM_PIPELINE_COORDINATOR_ROOT"},
        {"RV32IMFiveStageCore","rv32im.core.educational-five-stage","RV32IM_FIVE_STAGE_CORE_ROOT"},
        {"ClockedCounter","device.counter.4","CLOCKED_COUNTER_ROOT"},{"SynchronousFifo","device.fifo.8","SYNCHRONOUS_FIFO_ROOT"},
        {"DeviceRegisterPortWidth32","device.word.register-port.32","DEVICE_REGISTER_PORT_W32_ROOT"},
        {"ConsoleDevice","device.console","CONSOLE_DEVICE_ROOT"},{"TimerDevice","device.timer","TIMER_DEVICE_ROOT"},{"ButtonDevice","device.buttons","BUTTON_DEVICE_ROOT"},
        {"FramebufferDisplay","device.framebuffer","FRAMEBUFFER_DISPLAY_ROOT"}};
    for (const auto& [base,contract,instance] : cases) {
        auto name=base+"ScenarioTest";
        const bool combinational=contract.starts_with("arithmetic.multiply-step.")||contract.starts_with("arithmetic.divide-step.")||
            contract=="rv32im.decode-control"||contract=="rv32im.pipeline.coordinator";
        const auto kind=contract=="rv32im.core.educational-five-stage"?RegisteredTestKind::Program:
            combinational?RegisteredTestKind::TruthTable:RegisteredTestKind::Sequential;
        entries.push_back({name,base+"Test",kind,{contract},"contract",{"simulation","phase3-scenario"},
            [name,contract,instance]{return std::make_unique<ComponentScenarioTest>(recordedSpec(name,contract,instance),"contract",rv32imInspectionProfile());},true});
    }
}
