"""Compile the cache page's binary reference algorithm and exercise its claims."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
COMPILER = sys.argv[1] if len(sys.argv) > 1 else "c++"
source = (ROOT / "site-docs/memory/3.4.md").read_text()
snippet, = re.findall(r"```cpp\n(.*?)\n```", source, re.S)

harness = r'''
#include <cassert>
int main() {
    std::vector<std::uint32_t> ram(16);
    ram[0] = 0x12345678;
    ram[1] = 0x9abcdef0;
    ram[4] = 0x11223344;
    ram[8] = 0x55667788;
    CacheReference<32, 2, 2, 2> cache(ram, true);
    assert(cache.read(0) == 0x12345678 && cache.lower_reads == 2);
    assert(cache.read(4) == 0x9abcdef0 && cache.lower_reads == 2);
    assert(cache.write(0, 0xaabbccdd, 0b0101));
    assert(ram[0] == 0x12bb56dd && cache.lower_writes == 1);
    assert(cache.read(0) == 0x12bb56dd && cache.lower_reads == 2);
    assert(cache.write(16, 0xfedcba98, 0b1111)); // Write miss: no allocation.
    assert(cache.lower_writes == 2 && cache.lower_reads == 2);
    assert(cache.read(16) == 0xfedcba98 && cache.lower_reads == 4);
    assert(cache.read(0) == 0x12bb56dd && cache.lower_reads == 4);
    assert(cache.read(32) == 0x55667788 && cache.lower_reads == 6);
    assert(cache.read(16) == 0xfedcba98 && cache.lower_reads == 8); // LRU victim.
    cache.invalidate();
    assert(cache.read(0) == 0x12bb56dd && cache.lower_reads == 10);

    CacheReference<32, 2, 2, 2> read_only(ram, false);
    assert(!read_only.write(0, 0, 0b1111) && ram[0] == 0x12bb56dd);
    std::vector<std::uint32_t> bytes{0x12, 0x34, 0x56, 0x78};
    CacheReference<8, 1, 1, 1> byte_cache(bytes, true);
    assert(byte_cache.read(0) == 0x12 && byte_cache.lower_reads == 1);
    assert(byte_cache.write(0, 0xab, 1) && byte_cache.read(0) == 0xab);
    std::vector<std::uint32_t> halves{0x1234, 0x5678, 0x9abc, 0xdef0};
    CacheReference<16, 2, 1, 2> half_cache(halves, true);
    assert(half_cache.read(0) == 0x1234 && half_cache.lower_reads == 2);
    assert(half_cache.write(0, 0xabcd, 0b10));
    assert(halves[0] == 0xab34 && half_cache.read(0) == 0xab34);
}
'''

with tempfile.TemporaryDirectory(prefix="cache-doc-cpp-") as directory:
    cpp = Path(directory) / "cache_reference.cpp"
    binary = Path(directory) / "cache_reference"
    cpp.write_text(snippet + "\n" + harness)
    subprocess.run([COMPILER, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-pedantic", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("Cache C++ reference: width, fill, hit, LRU, write and invalidate checks passed.")
