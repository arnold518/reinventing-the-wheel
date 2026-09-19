"""Compile the actual register reference and replay its declared scenario."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = sys.argv[1] if len(sys.argv) > 1 else "c++"
library = sys.argv[2] if len(sys.argv) > 2 else str(ROOT / "build/libSimulatorLib.a")
source = (ROOT / "site-docs/rv32i/4.2/register.md").read_text()
assert source.startswith("# § 4.2.1. Register32\n")
for heading in ("IO pins", "Behavior", "Implementation", "Examples", "Tests"):
    assert f"### {heading}\n" in source
assert "| Component | Description | Structural | Behavioral |" in source
assert "<!-- simulator-examples: register32 Register32Test -->" in source
assert 'src="../../../simulator/?scenario=register32&amp;embed=1"' in source
assert source.count('class="simulator-embed"') == 1
nav = (ROOT / "mkdocs.yml").read_text()
assert nav.index("4.2.1. Register32:") < nav.index("4.2.2. Register File:")
snippet, = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
harness = r'''
#include "tests/MemoryComponentTests.hpp"
#include <iostream>
#include <stdexcept>
using namespace register32_example;
Bit convert(LogicValue b) {
    return b == LogicValue::LOW ? Bit::L : b == LogicValue::HIGH ? Bit::H : b == LogicValue::HIGH_Z ? Bit::Z : Bit::X;
}
int main() {
    Register32Test definition;
    Register reference;
    Word data; data.fill(Bit::X);
    Bit write=Bit::X, clock=Bit::X, reset=Bit::X;
    const auto initial=reference.evaluate(data,write,clock,reset);
    for(auto bit:initial) if(bit!=Bit::X) throw std::runtime_error("power-up is not unknown");
    size_t count=0;
    for(const auto& action:definition.getSpec().scenarios.front().actions) {
        for(const auto& [pin,value]:action.inputs) {
            if(pin=="D") for(size_t bit=0;bit<32;++bit) data[bit]=convert(value[bit]);
            else if(pin=="WE") write=convert(value[0]);
            else if(pin=="CLK") clock=convert(value[0]);
            else if(pin=="RST") reset=convert(value[0]);
            else throw std::runtime_error("unhandled input");
        }
        auto result=reference.evaluate(data,write,clock,reset);
        for(const auto& [pin,value]:action.expected_outputs) {
            if(pin!="Q") throw std::runtime_error("unhandled output");
            for(size_t bit=0;bit<32;++bit) if(result[bit]!=convert(value[bit]))
                throw std::runtime_error(action.checkpoint_id+"/"+pin);
        }
        count+=action.emit_checkpoint;
    }
    std::cout<<"Register32 reference: all "<<count<<" checkpoints passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix="ritw-register32-reference-") as directory:
    cpp = Path(directory) / "reference.cpp"
    exe = Path(directory) / "reference"
    cpp.write_text(snippet + "\n" + harness)
    subprocess.run([compiler,"-std=c++20","-O0","-I"+str(ROOT/"include"),str(cpp),library,"-pthread","-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
