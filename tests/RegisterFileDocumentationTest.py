"""Compile the exact 4.2.2 reference and check all declared register-file actions."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else "c++"
library = sys.argv[2] if len(sys.argv) > 2 else str(ROOT / "build/libSimulatorLib.a")
source = (ROOT / "site-docs/rv32i/4.2/register-file.md").read_text()
assert not (ROOT / "site-docs/memory/3.4.md").exists()
assert source.startswith("# § 4.2.2. Register File\n")
assert '(../../memory/3.1.1.md)' in source
assert 'src="../../../simulator/?scenario=rv32i-register-file&amp;embed=1"' in source
index = (ROOT / "site-docs/rv32i/4.2/index.md").read_text()
assert index.startswith("# § 4.2. RV32I Building Blocks\n")
assert '(register-file.md)' in index
for section in range(1, 8):
    assert f'| [4.2.{section}.' in index
nav = (ROOT / "mkdocs.yml").read_text()
assert '4.2.2. Register File: rv32i/4.2/register-file.md' in nav
assert 'memory/3.4.md' not in nav
snippet, = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
for heading in ("IO pins", "Behavior", "Implementation", "Examples", "Tests"):
    assert f"### {heading}\n" in source
assert "| Component | Description | Structural | Behavioral |" in source
assert "<!-- simulator-examples: rv32i-register-file RegisterFile32x32Test -->" in source
assert source.count('class="simulator-embed"') == 1
harness = r'''
#include "tests/MemoryComponentTests.hpp"
#include <iostream>
#include <stdexcept>
using namespace register_file_example;
Bit convert(LogicValue b) {
    return b == LogicValue::LOW ? Bit::L : b == LogicValue::HIGH ? Bit::H : b == LogicValue::HIGH_Z ? Bit::Z : Bit::X;
}
int main() {
    RegisterFile32x32Test test;
    const auto& scenario = test.getSpec().scenarios.front();
    RegisterFile reference;
    Address rs1, rs2, rd; Word data;
    rs1.fill(Bit::X);rs2.fill(Bit::X);rd.fill(Bit::X);data.fill(Bit::X);
    Bit write=Bit::X, clock=Bit::X, reset=Bit::X;
    size_t count=0;
    const auto assign = [](auto& dest, const auto& src) {
        for(size_t i=0;i<src.size();++i) dest[i]=convert(src[i]);
    };
    for(const auto& action:scenario.actions) {
        for(const auto& [pin,value]:action.inputs) {
            if(pin=="RS1_ADDR") assign(rs1,value);
            else if(pin=="RS2_ADDR") assign(rs2,value);
            else if(pin=="RD_ADDR") assign(rd,value);
            else if(pin=="WRITE_DATA") assign(data,value);
            else if(pin=="REG_WRITE") write=convert(value[0]);
            else if(pin=="CLK") clock=convert(value[0]);
            else if(pin=="RST") reset=convert(value[0]);
            else throw std::runtime_error("unhandled input");
        }
        auto result=reference.evaluate(rs1,rs2,rd,data,write,clock,reset);
        for(const auto& [pin,value]:action.expected_outputs) {
            const auto& actual=pin=="RS1_DATA"?result.first:result.second;
            for(size_t i=0;i<32;++i) if(actual[i]!=convert(value[i]))
                throw std::runtime_error(action.checkpoint_id+"/"+pin);
        }
        count+=action.emit_checkpoint;
    }
    std::cout<<"4.2.2 C++ reference: all "<<count<<" declared checkpoints passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix="ritw-register-reference-") as directory:
    cpp = Path(directory) / "reference.cpp"
    exe = Path(directory) / "reference"
    cpp.write_text(snippet + "\n" + harness)
    subprocess.run([compiler,"-std=c++20","-O0","-I"+str(ROOT/"include"),str(cpp),library,"-pthread","-o",str(exe)], check=True)
    subprocess.run([str(exe)],check=True)

layout = json.loads((ROOT/"visualizer-v2/layout.json").read_text())
types = layout["type_layouts"]
bank = types["MemoryStorageBank<FFArray,32,31>"]["children"]
for i in range(31):
    p = bank[f"WORD_{i}"]["rel_pos"]
    if i % 4:
        q = bank[f"WORD_{i-1}"]["rel_pos"]
        assert p[0] > q[0] and abs(p[1]-q[1]) < 1e-9
    if i >= 4:
        assert p[1] > bank[f"WORD_{i-4}"]["rel_pos"][1]
print("Register bank defaults are ordered left-to-right, top-to-bottom.")
