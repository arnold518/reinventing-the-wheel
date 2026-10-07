#include "tests/RV32IMemoryAccessTests.hpp"
#include <cstdint>

namespace {
using namespace circuit::test;
ComponentTestSpec lanesSpec() {
    ActionScenario scenario{"lanes", {}, {}};
    for (unsigned size = 0; size < 4; ++size)
        for (unsigned offset = 0; offset < 4; ++offset)
            for (unsigned sign = 0; sign < 2; ++sign) {
                const uint32_t address = 0x100 + offset;
                const uint32_t word = 0x80ff3412;
                const uint32_t shifted = word >> (8 * offset);
                uint32_t read = size == 0 ? shifted & 255 : size == 1 ? shifted & 65535 : size == 2 ? shifted : 0;
                if (sign && size == 0 && (read & 128)) read |= 0xffffff00;
                if (sign && size == 1 && (read & 32768)) read |= 0xffff0000;
                const bool bad = size == 3 || (size == 1 && (offset & 1)) || (size == 2 && offset);
                const unsigned mask = size == 0 ? 1U << offset : size == 1 ? 3U << (offset & 2) : size == 2 ? 15 : 0;
                scenario.actions.push_back({
                    "size-"+std::to_string(size)+"-lane-"+std::to_string(offset)+"-sign-"+std::to_string(sign),
                    CheckpointKind::Settled,
                    {{"ADDR",logicBits(32,address)}, {"SIZE",logicBits(2,size)},
                     {"SIGN_EXTEND",logicBit(bool(sign))}, {"WORD_DATA",logicBits(32,word)},
                     {"WRITE_DATA",logicBits(32,0xdeadbeef)}},
                    {{"WORD_ADDR",logicBits(32,0x100U | bad)},
                     {"WORD_WRITE_DATA",logicBits(32,uint32_t(0xdeadbeefU << (8 * offset)))},
                     {"READ_DATA",logicBits(32,read)}, {"WRITE_MASK",logicBits(4,mask)}}, true,
                    bad ? "Invalid access retains an unaligned endpoint address." : "Little-endian byte-lane formatting."});
            }
    return {"RV32IMemoryLanesTest","rv32i.memory-lanes","RV32I_MEMORY_LANES_ROOT",{}, {scenario}};
}
ComponentTestSpec requestSpec() {
    ActionScenario scenario{"handshake", {}, {}};
    const auto add = [&](const char* name, NamedValues inputs, NamedValues outputs) {
        scenario.actions.push_back({name, CheckpointKind::Settled, std::move(inputs), std::move(outputs)});
    };
    const auto b = [](bool x) { return logicBit(x); };
    const auto bus = [&](bool valid, bool ready, bool done, uint32_t address) {
        return NamedValues{{"BUS_REQ_VALID", b(valid)}, {"BUS_RSP_READY", b(ready)},
                           {"DONE", b(done)}, {"BUS_REQ_ADDR", logicBits(32,address)}};
    };
    add("reset", {{"CLK",b(false)}, {"RST",b(true)}, {"ISSUE",b(false)}, {"TAKE",b(false)},
        {"TAG",logicBits(32,0)}, {"ADDR",logicBits(32,0x100)}, {"WRITE_DATA",logicBits(32,0x12345678)},
        {"OP",logicBits(3,1)}, {"WRITE_MASK",logicBits(4,15)}, {"BUS_REQ_READY",b(false)},
        {"BUS_RSP_VALID",b(false)}, {"BUS_RSP_READ_DATA",logicBits(32,0)}, {"BUS_RSP_STATUS",logicBits(3,0)}}, bus(false,false,false,0x100));
    add("issue-before-edge", {{"RST",b(false)},{"ISSUE",b(true)}}, bus(true,false,false,0x100));
    add("capture", {{"CLK",b(true)}}, bus(true,false,false,0x100));
    add("payload-held", {{"CLK",b(false)}, {"ISSUE",b(false)}, {"ADDR",logicBits(32,0x200)},
        {"WRITE_DATA",logicBits(32,0)}, {"OP",logicBits(3,0)}, {"WRITE_MASK",logicBits(4,0)}},
        {{"BUS_REQ_ADDR",logicBits(32,0x100)}, {"BUS_REQ_WRITE_DATA",logicBits(32,0x12345678)},
         {"BUS_REQ_OP",logicBits(3,1)}, {"BUS_REQ_WRITE_MASK",logicBits(4,15)}, {"BUS_REQ_VALID",b(true)}});
    add("backpressure-edge", {{"CLK",b(true)}}, bus(true,false,false,0x100));
    add("accept-setup", {{"CLK",b(false)}, {"BUS_REQ_READY",b(true)}}, bus(true,false,false,0x100));
    add("accepted", {{"CLK",b(true)}}, bus(false,false,false,0x100));
    add("response-held", {{"CLK",b(false)}, {"BUS_RSP_VALID",b(true)},
        {"BUS_RSP_READ_DATA",logicBits(32,0x89abcdef)}, {"BUS_RSP_STATUS",logicBits(3,2)}},
        {{"DONE",b(true)}, {"BUS_RSP_READY",b(false)}, {"READ_DATA",logicBits(32,0x89abcdef)}, {"FAULT",b(true)}});
    add("response-held-edge", {{"CLK",b(true)}}, bus(false,false,true,0x100));
    add("take-setup", {{"CLK",b(false)}, {"TAKE",b(true)}}, bus(false,true,true,0x100));
    add("taken", {{"CLK",b(true)}}, bus(false,false,false,0x200));
    add("new-issue-setup", {{"CLK",b(false)}, {"ISSUE",b(true)}, {"TAKE",b(false)},
        {"BUS_REQ_READY",b(false)}, {"BUS_RSP_VALID",b(false)}, {"TAG",logicBits(32,4)}}, bus(true,false,false,0x200));
    add("second-capture", {{"CLK",b(true)}}, bus(true,false,false,0x200));
    add("redirect-before-accept", {{"CLK",b(false)}, {"TAG",logicBits(32,8)}, {"ISSUE",b(false)}}, bus(true,true,false,0x200));
    add("record-abandonment", {{"CLK",b(true)}}, bus(true,true,false,0x200));
    add("tag-returns-ABA", {{"CLK",b(false)}, {"TAG",logicBits(32,4)}, {"BUS_REQ_READY",b(true)}}, bus(true,true,false,0x200));
    add("abandoned-accepted", {{"CLK",b(true)}}, bus(false,true,false,0x200));
    add("stale-response", {{"CLK",b(false)}, {"BUS_RSP_VALID",b(true)}}, bus(false,true,false,0x200));
    add("stale-drained", {{"CLK",b(true)}}, bus(false,false,false,0x200));
    add("ungated-response-wires", {{"CLK",b(false)}, {"BUS_RSP_VALID",b(false)}},
        {{"DONE",b(false)}, {"FAULT",b(true)}, {"READ_DATA",logicBits(32,0x89abcdef)}});
    add("reset-again", {{"RST",b(true)}}, bus(false,false,false,0x200));
    return {"RV32IMemoryRequestTest","rv32i.memory-request","RV32I_MEMORY_REQUEST_ROOT",{}, {scenario}};
}
}
RV32IMemoryLanesTest::RV32IMemoryLanesTest() : ComponentScenarioTest(lanesSpec()) {}
RV32IMemoryRequestTest::RV32IMemoryRequestTest() : ComponentScenarioTest(requestSpec()) {}
