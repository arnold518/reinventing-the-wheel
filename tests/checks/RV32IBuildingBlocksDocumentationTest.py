"""Compile/replay exact 4.3 C++ examples and check live pin/fidelity schemas.

Serial probes and one compiler; no full cores or full test suite.
"""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROBE = sys.argv[1] if len(sys.argv)>1 else str(ROOT/"build/tests/VisualizerSessionProbe")
COMPILER = sys.argv[2] if len(sys.argv)>2 else "c++"
LIBRARY = sys.argv[3] if len(sys.argv)>3 else str(ROOT/"build/libSimulatorLib.a")
sys.path.insert(0,str(ROOT/"tools"))
from mkdocs_examples import render_table

snippets, outputs = [], {}
for number,page in enumerate(("alu32","instruction-decoder","memory-access","execution-status"),3):
    document=(ROOT/f"site-docs/rv32i/4.3/{page}.md").read_text()
    assert document.startswith(f"# § 4.3.{number}.")
    assert f"rv32i/4.3/{page}.md" in (ROOT/"mkdocs.yml").read_text()
    assert f"({page}.md)" in (ROOT/"site-docs/rv32i/4.3/index.md").read_text()
    assert "| Component | Description | Structural | Behavioral |" in document
    components=re.findall(r"(?m)^\| `([^`]+)` \| [^|\n]+ \| (Yes|No) \| (Yes|No) \|$",document)
    parts=re.split(r"(?m)^## 4\.3\.\d+\.\d+\. ",document)[1:]
    assert len(components)==len(parts)==document.count('class="simulator-embed"')
    snippets+=re.findall(r"```cpp\n(.*?)\n```",document,re.S)
    for (name,structural,behavioral),part in zip(components,parts):
        for heading in ("IO pins","Behavior","Implementation","Examples","Tests"):
            assert f"### {heading}\n" in part,(name,heading)
        assert "#### C++ example\n" in part and "#### Unknown inputs and timing\n" in part
        alias,test=re.findall(r"<!-- simulator-examples: (\S+) (\S+) -->",part)[0]
        assert f'src="../../../simulator/?scenario={alias}&amp;embed=1"' in part
        topology=json.loads(subprocess.check_output([PROBE,"topology",test,alias],text=True,timeout=120))
        examples=json.loads(subprocess.check_output([PROBE,"examples",test,alias],text=True,timeout=120))
        root=next(c for c in topology["components"] if c["id"]==topology["rootId"])
        assert root["type"]==name,(name,root["type"])
        assert set(root["availableFidelities"])=={f for f,v in (("structural",structural),("behavioral",behavioral)) if v=="Yes"}
        documented={}
        pin_table=part.split("### IO pins\n",1)[1].split("### Behavior\n",1)[0]
        for names,direction,width in re.findall(r"(?m)^\| ([^|]+) \| (Input|Output) \| (\d+)(?: each)? \|",pin_table):
            for pin in re.findall(r"`([^`]+)`",names):
                assert pin not in documented
                documented[pin]=(direction.lower(),int(width))
        actual={p["name"]:(direction,p["width"]) for key,direction in (("inputs","input"),("outputs","output")) for p in examples[key]}
        assert documented==actual,(name,documented,actual)
        assert len(examples["rows"])==len(topology["checkpoints"])
        if test=="RV32IExecutionControlStatusUnitTest":
            assert {"transaction-complete","instruction-commit","after-edge","settled"} <= {r["kind"] for r in examples["rows"]}
        assert 'class="example-table-scroll"' in render_table(examples,alias,test)
        for row,cp in zip(examples["rows"],topology["checkpoints"]):
            assert (row["checkpoint"],row["time"],row["timelineIndex"])==(cp["label"],cp["time"],cp["index"])
        outputs[test]=[p["name"] for p in examples["outputs"]]
        print(f"{name}: pins, fidelities, {len(examples['rows'])} checkpoints verified",flush=True)

harness=r'''
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "rv32i/RV32IControl.hpp"
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
using U=std::map<std::string,uint32_t>;
bool binary(const circuit::test::LogicVector& bits) {
    for(auto b:bits) if(b!=LogicValue::LOW && b!=LogicValue::HIGH) return false;
    return true;
}
uint32_t number(const circuit::test::LogicVector& bits) {
    uint32_t result=0;
    for(size_t k=0;k<bits.size();++k) if(bits[k]==LogicValue::HIGH) result|=uint32_t{1}<<k;
    return result;
}
template<class Factory> void replay(const std::string& name, Factory factory, bool sequential=false) {
    auto test=createTestByName(name);
    const auto& spec=dynamic_cast<circuit::test::ComponentScenarioTest&>(*test).getSpec();
    size_t checked=0,skipped=0;
    for(const auto& scenario:spec.scenarios) {
        auto evaluate=factory(scenario);
        circuit::test::NamedValues held;
        for(const auto& action:scenario.actions) {
            for(const auto& [pin,value]:action.inputs) held[pin]=value;
            bool known=true; U inputs;
            for(const auto& [pin,value]:held) { known&=binary(value); inputs[pin]=number(value); }
            if(!known) {
                if(sequential) throw std::runtime_error(name+": cannot skip state-changing X/Z");
                ++skipped; continue;
            }
            const auto result=evaluate(inputs);
            for(const auto& [pin,value]:action.expected_outputs) {
                if(!binary(value)) throw std::runtime_error(name+": nonbinary expectation");
                if(result.at(pin)!=number(value)) throw std::runtime_error(name+"/"+action.checkpoint_id+"/"+pin+
                    " expected="+std::to_string(number(value))+" actual="+std::to_string(result.at(pin)));
            }
            checked+=action.emit_checkpoint;
        }
    }
    std::cout<<name<<": C++ reference "<<checked<<" binary checkpoints passed, "<<skipped<<" X/Z actions outside reference scope\n";
}
'''

def output_map(test):
    return "U{"+",".join('{"'+p+'",r.'+p+'}' for p in outputs[test])+"}"

def args(names):
    return ",".join(f'v.at("{p}")' for p in names.split())

models={
    "AddSub32Test":("alu32_example::addsub","A B SUB"),
    "Logic32Test":("alu32_example::logic","A B"),
    "Shifter32Test":("alu32_example::shift","A B LEFT ARITHMETIC"),
    "ALU32ControlDecoderTest":("alu32_example::control","OP"),
    "ALU32CompareUnitTest":("alu32_example::compare","DIFF CARRY_IN OVERFLOW_IN ENABLE"),
    "ALU32ResultSelectorTest":("alu32_example::select","ARITHMETIC_RESULT AND_RESULT OR_RESULT XOR_RESULT SHIFT_RESULT SLT_RESULT SLTU_RESULT A B LOCAL_OP HIGH_HALF OUT_OF_RANGE"),
    "ALU32FlagUnitTest":("alu32_example::flags","RESULT ARITHMETIC_ENABLE ARITH_CARRY ARITH_OVERFLOW"),
    "ALU32Test":("alu32_example::alu","A B OP"),
    "ALU32CoreTest":("alu32_example::core","A B OP"),
    "RV32IDecodeControlUnitTest":("decode_example::decode","INSTRUCTION"),
    "RV32IMemoryLanesTest":("memory_access_example::lanes","ADDR WRITE_DATA WORD_DATA SIZE SIGN_EXTEND"),
    "RV32IMemoryAlignmentUnitTest":("memory_access_example::misaligned","ADDRESS SIZE"),
}
harness+="\nint main() {\n"
harness+=r'''
for(const auto* contract : {"rv32i.memory-lanes", "rv32i.memory-request"}) {
    std::set<std::string> owners;
    for(const auto& entry:getTestRegistry())
        for(const auto& id:entry.contract_ids) if(id==contract) owners.insert(entry.logical_test_id);
    if(owners.size()!=1) throw std::runtime_error(std::string(contract)+": ambiguous logical ownership");
}
'''
for test,(function,inputs) in models.items():
    scalar=test in ("Shifter32Test","ALU32ResultSelectorTest","RV32IMemoryAlignmentUnitTest")
    result='U{{"'+outputs[test][0]+'",r}}' if scalar else output_map(test)
    harness+=f'replay("{test}",[](const auto&) {{ return [](const U& v) {{ const auto r={function}({args(inputs)}); return {result}; }}; }});\n'
harness+=r'''
replay("RV32IBitPatternMatcherTest",[](const auto& scenario) {
    const auto mask=uint32_t(std::stoull(scenario.parameters.at("mask"),nullptr,0));
    const auto value=uint32_t(std::stoull(scenario.parameters.at("value"),nullptr,0));
    return [mask,value](const U& v) { return U{{"MATCH",decode_example::match(v.at("INPUT"),mask,value)}}; };
});
try { decode_example::match(0,0,0); throw std::runtime_error("zero mask accepted"); }
catch(const std::invalid_argument&) {}
'''
stateful=[
    ("RV32IMemoryRequestTest","memory_access_example::Request","memory_access_example::RequestInputs",
     "CLK RST ISSUE TAKE TAG ADDR WRITE_DATA OP WRITE_MASK BUS_REQ_READY BUS_RSP_VALID BUS_RSP_READ_DATA BUS_RSP_STATUS"),
    ("RV32IExecutionControlStatusUnitTest","status_example::Status","status_example::Inputs",
     "CLK RST ENABLE LEGAL REG_WRITE MEM_READ MEM_WRITE HALT_REQUEST TRAP_REQUEST DECODE_TRAP_CAUSE ALU_ADDRESS MEM_SIZE PC_MISALIGNED TARGET_MISALIGNED IMEM_READY IMEM_FAULT DMEM_READY DMEM_FAULT"),
]
for test,model,input_type,inputs in stateful:
    assign=" ".join(f'i.{p}=v.at("{p}");' for p in inputs.split())
    harness+=f'replay("{test}",[](const auto&) {{ return [state={model}{{}}](const U& v) mutable {{ {input_type} i{{}}; {assign} const auto r=state.evaluate(i); return {output_map(test)}; }}; }},true);\n'

actual={
    "RS1_ADDR":"d.rs1","RS2_ADDR":"d.rs2","RD_ADDR":"d.rd","IMM":"uint32_t(d.immediate)",
    "ALU_OP":"c.alu_op","ALU_A_SEL":"aluSourceA(c.alu_a)","ALU_B_SEL":"aluSourceB(c.alu_b)",
    "WRITEBACK_SEL":"writeback(c.writeback)","MEM_SIZE":"memorySize(c.mem_size)",
    "BRANCH_TYPE":"branch(c.branch)","JUMP_TYPE":"jump(c.jump)","DECODE_TRAP_CAUSE":"decodeTrapCause(c.trap_cause)",
    "LEGAL":"c.legal","REG_WRITE":"c.reg_write","MEM_READ":"c.mem_read","MEM_WRITE":"c.mem_write",
    "LOAD_SIGN_EXTEND":"c.load_sign_extend","USES_RS1":"c.uses_rs1","USES_RS2":"c.uses_rs2",
    "HALT_REQUEST":"c.halt","TRAP_REQUEST":"c.trap",
}
harness+=r'''
const auto check_decode=[](uint32_t word) {
    using namespace rv32i::component_encoding;
    const auto r=decode_example::decode(word);
    const auto d=rv32i::RV32IDecoder::decode(word);
    const auto c=rv32i::RV32IControl::fromDecoded(d);
'''
for field in outputs["RV32IDecodeControlUnitTest"]:
    harness+=f'if(r.{field}!={actual[field]}) throw std::runtime_error("decoder sweep/{field}/word="+std::to_string(word));\n'
harness+=r'''
};
std::mt19937 random(0x42);
for(uint32_t op=0;op<128;++op) for(uint32_t f3=0;f3<8;++f3) for(uint32_t f7=0;f7<128;++f7)
    check_decode(op|(f3<<12)|(f7<<25)|(random()&0x01ff8f80));
for(unsigned i=0;i<20000;++i) check_decode(random());
check_decode(0x00000073); check_decode(0x00100073);
std::cout<<"Decoder: 131072 field combinations + 20000 random words + exact system encodings passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix="ritw-building-blocks-doc-") as temp:
    source=Path(temp)/"examples.cpp"
    binary=Path(temp)/"examples"
    source.write_text("\n\n".join(snippets)+harness)
    subprocess.run([COMPILER,"-std=c++20","-O0","-I"+str(ROOT/"include"),str(source),LIBRARY,"-pthread","-o",str(binary)],check=True,timeout=120)
    subprocess.run([str(binary)],check=True,timeout=120)
assert len(outputs)==15
print("All 15 component references checked.")
