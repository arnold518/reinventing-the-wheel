"""Check 4.4 pin tables and compile/replay its actual C++ reference examples.

Serial native probes and one compiler. No full structural CPU sweep or rebuild.
"""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROBE = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build/tests/VisualizerSessionProbe")
COMPILER = sys.argv[2] if len(sys.argv) > 2 else "c++"
LIBRARY = sys.argv[3] if len(sys.argv) > 3 else str(ROOT / "build/libSimulatorLib.a")
sys.path.insert(0, str(ROOT / "tools"))
from mkdocs_examples import render_table

STAGES = (("fetch", "Fetch"), ("decode", "Decode"), ("execute", "Execute"),
          ("memory", "Memory"), ("writeback", "Writeback"), ("next-pc", "NextPC"))
snippets, fields, inputs = [], {}, {}
for page in ("alu32", "instruction-decoder", "memory-access", "execution-status"):
    document = (ROOT / f"site-docs/rv32i/4.3/{page}.md").read_text()
    snippets += re.findall(r"```cpp\n(.*?)\n```", document, re.S)

navigation = (ROOT / "mkdocs.yml").read_text()
index = (ROOT / "site-docs/rv32i/4.4/index.md").read_text()
for number, (slug, stage) in enumerate((*STAGES, ("core", "Core")), 1):
    document = (ROOT / f"site-docs/rv32i/4.4/{slug}.md").read_text()
    assert document.startswith(f"# § 4.4.{number}.")
    assert f"rv32i/4.4/{slug}.md" in navigation and f"({slug}.md)" in index
    assert "| Component | Description | Structural | Behavioral |" in document
    for heading in ("IO pins", "Behavior", "Implementation", "Examples", "Tests"):
        assert f"### {heading}\n" in document, (slug, heading)
    for heading in ("C++ example", "Unknown inputs and timing", "Structural implementation",
                    "Behavioral implementation", "Embedded simulation"):
        assert f"#### {heading}\n" in document, (slug, heading)
    assert document.count('class="simulator-embed"') == 1
    snippets += re.findall(r"```cpp\n(.*?)\n```", document, re.S)
    if stage == "Core":
        test, alias, name = "RV32ISingleCycleCoreTest", "rv32i-core", "RV32ISingleCycleCore"
    else:
        alias, test = re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->", document)[0]
        name = f"RV32ISingleCycle{stage}Stage"
        assert f'src="../../../simulator/?scenario={alias}&amp;embed=1"' in document
    topology = json.loads(subprocess.check_output([PROBE, "topology", test, alias], text=True, timeout=90))
    root = next(c for c in topology["components"] if c["id"] == topology["rootId"])
    assert root["type"] == name and set(root["availableFidelities"]) == {"structural", "behavioral"}
    pins = {p["name"]: (p["type"], p["width"]) for p in topology["pins"] if p["componentId"] == root["id"]}
    documented = {}
    pin_table = document.split("### IO pins\n", 1)[1].split("### Behavior\n", 1)[0]
    for names, direction, width in re.findall(r"(?m)^\| ([^|]+) \| (Input|Output) \| (\d+)(?: each)? \|", pin_table):
        for pin in re.findall(r"`([^`]+)`", names):
            assert pin not in documented, pin
            documented[pin] = (direction.lower(), int(width))
    assert documented == pins, (name, documented, pins)
    fields[stage] = [pin for pin, (direction, _) in pins.items() if direction == "output"]
    inputs[stage] = [pin for pin, (direction, _) in pins.items() if direction == "input"]
    if stage != "Core":
        examples = json.loads(subprocess.check_output([PROBE, "examples", test, alias], text=True, timeout=90))
        assert len(examples["rows"]) == len(topology["checkpoints"])
        assert 'class="example-table-scroll"' in render_table(examples, alias, test)
        for row, cp in zip(examples["rows"], topology["checkpoints"]):
            assert (row["checkpoint"], row["time"], row["timelineIndex"]) == (cp["label"], cp["time"], cp["index"])
        print(f"{name}: all pins, both fidelities, {len(examples['rows'])} rows verified", flush=True)

harness = r'''
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/RV32IProgramCases.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using U=std::map<std::string,uint32_t>;
void require(bool ok,const std::string& message) { if(!ok) throw std::runtime_error(message); }
uint32_t number(const circuit::test::LogicVector& bits) {
    uint32_t n=0;
    for(size_t b=0;b<bits.size();++b) {
        require(bits[b]==LogicValue::LOW || bits[b]==LogicValue::HIGH,"Example outside binary scope");
        if(bits[b]==LogicValue::HIGH) n|=uint32_t{1}<<b;
    }
    return n;
}
template<class Factory> void replay(const std::string& name,Factory factory) {
    auto test=createTestByName(name);
    const auto& spec=dynamic_cast<circuit::test::ComponentScenarioTest&>(*test).getSpec();
    size_t count=0;
    for(const auto& scenario:spec.scenarios) {
        auto model=factory(); U held;
        for(const auto& action:scenario.actions) {
            for(const auto& [pin,value]:action.inputs) held[pin]=number(value);
            const auto result=model(held);
            for(const auto& [pin,value]:action.expected_outputs)
                require(result.at(pin)==number(value),name+"/"+action.checkpoint_id+"/"+pin);
            count+=action.emit_checkpoint;
        }
    }
    std::cout<<name<<": exact C++ example passed "<<count<<" checkpoints\n";
}

// Direct test memory: reads are combinational; masked writes use the edge.
struct Endpoint {
    rv32i::RV32IFunctionalMemory memory;
    std::map<uint32_t,uint8_t> writes;
    explicit Endpoint(size_t bytes):memory(bytes) {}
    bool fault(uint32_t word) const { return uint64_t(word)*4+4>memory.sizeBytes(); }
    uint32_t read(uint32_t word) const { return fault(word)?0:memory.readU32(word*4); }
    void edge(bool enable,uint32_t word,uint32_t data,unsigned mask) {
        // Universal endpoint: an error response rejects the requested store.
        if(!enable || fault(word)) return;
        for(unsigned byte=0;byte<4;++byte) if(mask&(1u<<byte)) {
            const uint8_t value=uint8_t(data>>(byte*8));
            memory.writeU8(word*4+byte,value);writes[word*4+byte]=value;
        }
    }
};
void check_program(const RV32ISystemProgramCase& fixture) {
    using namespace single_cycle_example;
    require(fixture.initial_pc==0 && fixture.initial_registers==std::array<uint32_t,32>{},"Unsupported fixture setup");
    Endpoint imem(fixture.memory_size_bytes),dmem(fixture.memory_size_bytes);
    rv32i::RV32IFunctionalMemory oracle_i(fixture.memory_size_bytes),oracle_d(fixture.memory_size_bytes);
    imem.memory.loadProgram(fixture.program,fixture.program_base);
    oracle_i.loadProgram(fixture.program,fixture.program_base);
    for(const auto& init:fixture.initial_data) {
        dmem.memory.loadBytes(init.address,init.bytes);oracle_d.loadBytes(init.address,init.bytes);
    }
    rv32i::RV32IState expected;
    Core core;
    CoreInputs i{}; i.RST=true; core.evaluate(i);i.RST=false;
    bool terminal=false;
    for(size_t cycle=0;cycle<fixture.max_instructions*2;++cycle) {
        i.CLK=false;i.ENABLE=cycle%11!=5;
        const auto pc=core.observe(i).IMEM_REQ_ADDR/4;
        i.IMEM_RSP_READ_DATA=imem.read(pc);i.IMEM_RSP_STATUS=imem.fault(pc)?2:0;
        const auto address=core.evaluate(i).DMEM_REQ_ADDR/4;
        i.DMEM_RSP_READ_DATA=dmem.read(address);i.DMEM_RSP_STATUS=dmem.fault(address)?2:0;
        const auto before=core.evaluate(i);
        const auto old_registers=core.registers();
        require(before.INSTRUCTION_ATTEMPT==i.ENABLE,fixture.name+": instruction needs extra cycle");
        dmem.edge(before.DMEM_REQ_VALID&&before.DMEM_REQ_OP==1,before.DMEM_REQ_ADDR/4,before.DMEM_REQ_WRITE_DATA,before.DMEM_REQ_WRITE_MASK);
        i.CLK=true;
        const auto after=core.evaluate(i);
        if(before.INSTRUCTION_ATTEMPT) rv32i::RV32IInstructionOracle::step(expected,oracle_i,oracle_d);
        else require(after.PC==before.PC && core.registers()==old_registers,fixture.name+": early state write");
        require(after.PC==expected.pc && core.registers()==expected.x,fixture.name+": PC/register mismatch");
        require(after.HALTED==expected.halted && after.TRAPPED==expected.trapped &&
                after.TRAP_CAUSE==unsigned(expected.trap_cause),fixture.name+": status mismatch");
        if(after.HALTED || after.TRAPPED) { terminal=true;break; }
    }
    require(terminal,fixture.name+": did not finish");
    require(dmem.memory.readBytes(0,fixture.memory_size_bytes)==oracle_d.readBytes(0,fixture.memory_size_bytes),
            fixture.name+": memory mismatch");
    verifyRV32IProgramExpectedResult(fixture,expected,dmem.writes,fixture.name);
    std::cout<<fixture.name<<": composed C++ core matches oracle at "<<expected.instruction_count<<" attempts\n";
}
int main() {
std::cout<<std::unitbuf;
'''
for _, stage in STAGES:
    assign = " ".join(f'i.{pin}=v.at("{pin}");' for pin in inputs[stage])
    output_map = "U{" + ",".join('{"' + pin + '",r.' + pin + "}" for pin in fields[stage]) + "}"
    capture = f"state=single_cycle_example::{stage}{{}}" if stage in ("Fetch", "Decode", "Memory") else ""
    call = "state.evaluate(i)" if capture else f"single_cycle_example::{dict(Execute='execute',Writeback='writeback',NextPC='next_pc')[stage]}(i)"
    harness += f'replay("RV32ISingleCycle{stage}StageTest",[] {{return [{capture}](const U& v) mutable {{ single_cycle_example::{stage}Inputs i{{}}; {assign} const auto r={call}; return {output_map}; }};}});\n'
harness += r'''
for(size_t n=1;n<=RV32IProgramCaseCount;++n) check_program(rv32iProgramCase(n));
RV32ISystemProgramCase overlap;
overlap.name="no-reading-back";
overlap.program=rv32i::RV32IProgram::fromWords({0x10000093,0x0000a083,0x00208093,
    0x01800293,0x000282e7,0x00000073,0x001080b3,0x0010a023,0x00100073});
overlap.initial_data={{0x100,{0,2,0,0}}};overlap.max_instructions=12;
overlap.expected_result.defined=true;overlap.expected_result.pc=32;
overlap.expected_result.instruction_count=8;overlap.expected_result.halted=true;
overlap.expected_result.registers={{1,1028},{5,20}};
overlap.expected_result.bus_writes={{0x404,4},{0x405,4},{0x406,0},{0x407,0}};
check_program(overlap);
}
'''
with tempfile.TemporaryDirectory(prefix="ritw-single-cycle-doc-") as temp:
    source, binary = Path(temp) / "examples.cpp", Path(temp) / "examples"
    source.write_text("\n\n".join(snippets) + harness)
    subprocess.run([COMPILER, "-std=c++20", "-O0", "-I" + str(ROOT / "include"),
                    str(source), LIBRARY, "-pthread", "-o", str(binary)], check=True, timeout=120)
    subprocess.run([str(binary)], check=True, timeout=120)
print("All seven 4.4 references and their C++ examples checked.")
