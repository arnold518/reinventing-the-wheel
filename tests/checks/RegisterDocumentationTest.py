"""Check the RV32I register reference, ownership, and all supported widths."""
from pathlib import Path
from html import unescape
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
compiler = sys.argv[1] if len(sys.argv) > 1 else "c++"
library = sys.argv[2] if len(sys.argv) > 2 else str(ROOT / "build/libSimulatorLib.a")
source = (ROOT / "site-docs/rv32i/4.3/register.md").read_text()
assert source.startswith("# § 4.3.1. `Register<W>`\n")
assert "**1, 2, 3, 4, 5, 8, 16, and 32 bits**" in source
for heading in ("IO pins", "Behavior", "Implementation", "Examples", "Tests"):
    assert f"### {heading}\n" in source
assert "| Component | Description | Structural | Behavioral |" in source
assert "<!-- simulator-examples: register32 RegisterTest -->" in source
assert 'src="../../../simulator/?scenario=register32&amp;embed=1"' in source
assert source.count('class="simulator-embed"') == 1
nav = unescape((ROOT / "mkdocs.yml").read_text())
assert nav.index("4.3.1. Register<W>:") < nav.index("4.3.2. Register File:")
index = (ROOT / "site-docs/rv32i/4.3/index.md").read_text()
assert "4.3.1. Register&lt;W&gt;" in index
memory = (ROOT / "site-docs/memory/3.1.1.md").read_text()
assert "Register<" not in memory
assert "register32" not in memory
assert "## 3.1.1.5. `FFWord<W>`" in memory
assert "## 3.1.1.6. `FFStorageBank<W>`" in memory
assert "## 3.1.1.7. `FFMemory<W>`" in memory
for page in (ROOT / "site-docs/memory").rglob("*.md"):
    text = page.read_text()
    assert not re.search(r"(?m)^#+ .*`Register<", text), page
    assert "| `Register<W>` |" not in text, page
    assert "<!-- simulator-examples: register32 " not in text, page
    assert "register_example" not in text, page
assert "../rv32i/4.3/register.md" in (ROOT / "site-docs/memory/3.2.md").read_text()

# Component identifiers, not substrings of still-current file names:
# MemoryArrayBlocks.hpp remains a real resource.
retired = re.compile(r"\b(?:MemoryBit|Register32|Register32Behavioral|MemoryPayloadRegister|"
                     r"LegacyMemoryContainer|MemoryArraySlice|MemoryArray|MemoryWord|"
                     r"MemoryStorageBank|Memory64Kx32|Memory32x32|Memory4x32|"
                     r"MemoryBusAdapter32|FlipFlopArrayModel|SRAMArrayModel|"
                     r"ALU8V2|RV32ICore|RV32ISystem|BehavioralRV32ICore)\b")
pages = [p for p in (ROOT / "site-docs").rglob("*") if p.suffix in {".md", ".html", ".svg"}]
pages += [ROOT / "README.md", ROOT / "mkdocs.yml"]
for page in pages:
    match = retired.search(page.read_text())
    assert match is None, (page, match.group() if match else None)
snippet, = re.findall(r"```cpp\n(.*?)\n```", source, re.S)
harness = r'''
#include "tests/MemoryComponentTests.hpp"
#include <iostream>
#include <stdexcept>
using namespace register_example;
Bit convert(LogicValue b) {
    return b == LogicValue::LOW ? Bit::L : b == LogicValue::HIGH ? Bit::H : b == LogicValue::HIGH_Z ? Bit::Z : Bit::X;
}
template<size_t W>
void checkWidth() {
    RegisterTest definition;
    // Replay every source bit, including high walking bits, at narrower widths.
    for(size_t offset=0;offset<32;offset+=W) {
    register_example::Register<W> reference;
    Word<W> data; data.fill(Bit::X);
    Bit write=Bit::X, clock=Bit::X, reset=Bit::X;
    const auto initial=reference.evaluate(data,write,clock,reset);
    for(auto bit:initial) if(bit!=Bit::X) throw std::runtime_error("power-up is not unknown");
    size_t count=0;
    for(const auto& action:definition.getSpec().scenarios.front().actions) {
        for(const auto& [pin,value]:action.inputs) {
            if(pin=="D") for(size_t bit=0;bit<W;++bit) data[bit]=convert(value[(offset+bit)%32]);
            else if(pin=="WE") write=convert(value[0]);
            else if(pin=="CLK") clock=convert(value[0]);
            else if(pin=="RST") reset=convert(value[0]);
            else throw std::runtime_error("unhandled input");
        }
        auto result=reference.evaluate(data,write,clock,reset);
        for(const auto& [pin,value]:action.expected_outputs) {
            if(pin!="Q") throw std::runtime_error("unhandled output");
            for(size_t bit=0;bit<W;++bit) if(result[bit]!=convert(value[(offset+bit)%32]))
                throw std::runtime_error(action.checkpoint_id+"/"+pin+"/width="+std::to_string(W));
        }
        count+=action.emit_checkpoint;
    }
    if(count!=56) throw std::runtime_error("incomplete scenario replay");
    }
    std::cout<<"Register<"<<W<<"> reference: all scenario bit lanes passed\n";
}
int main() {
    checkWidth<1>(); checkWidth<2>(); checkWidth<3>(); checkWidth<4>();
    checkWidth<5>(); checkWidth<8>(); checkWidth<16>(); checkWidth<32>();
}
'''
with tempfile.TemporaryDirectory(prefix="ritw-register32-reference-") as directory:
    cpp = Path(directory) / "reference.cpp"
    exe = Path(directory) / "reference"
    cpp.write_text(snippet + "\n" + harness)
    subprocess.run([compiler,"-std=c++20","-O0","-I"+str(ROOT/"include"),str(cpp),library,"-pthread","-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
