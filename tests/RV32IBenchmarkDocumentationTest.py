"""Keep the benchmark manual aligned with the native catalogue and moved pages.

Read-only except for the native runner's short catalogue queries. No circuits,
external builds/downloads, or benchmark campaigns are run by this test.
"""
import json
import re
import subprocess
import sys
import unittest
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "site-docs"
CHAPTER = DOCS / "rv32i/4.2"
BINARY = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "build/rv32i_experiment"
EMBENCH = {
    "aha-mont64", "crc32", "cubic", "edn", "huffbench", "matmult-int",
    "minver", "nbody", "nettle-aes", "nettle-sha256", "nsichneu", "picojpeg",
    "qrduino", "sglib-combined", "slre", "st", "statemate", "ud", "wikisort",
}


def read(path):
    return path.read_text(encoding="utf-8")


class BenchmarkDocumentation(unittest.TestCase):
    def test_native_catalogue_and_program_identity(self):
        rows = json.loads(subprocess.check_output(
            [str(BINARY), "--list-benchmark-catalog"], text=True, timeout=15))
        names = json.loads(subprocess.check_output(
            [str(BINARY), "--list-workloads"], text=True, timeout=15))
        self.assertEqual(len(rows), 37)
        self.assertEqual(len(names), 15)
        actual = {row["id"] for row in rows}
        pages = {p.stem for p in (CHAPTER / "programs").glob("program-*.md")}
        pages |= {p.stem for p in (CHAPTER / "workloads").glob("*.md") if p.stem != "index"}
        self.assertEqual(pages, actual)
        for number in range(1, 23):
            page = CHAPTER / f"programs/program-{number:02}.md"
            source = read(page)
            self.assertIn(f"`program-{number:02}`", source)
            self.assertIn(f"--program {number}", source)
            self.assertIn(f"rv32i-program{number} RV32ISingleCycleSystemTest/program-{number:02}", source)
            self.assertEqual(re.findall(r"^## (.+)$", source, re.M), [
                "Purpose", "Setup", "Assembly", "Execution", "Expected result", "Simulation and checkpoints"])
        for name in names:
            source = read(CHAPTER / f"workloads/{name}.md")
            self.assertIn(f"`{name}`", source)
            self.assertIn(f"--workload {name}", source)
            self.assertEqual(re.findall(r"^## (.+)$", source, re.M), [
                "Purpose", "Setup", "Implementation", "Expected result", "Measurement and running", "Source and tests"])
        index = read(CHAPTER / "workloads/index.md")
        self.assertEqual(index.count("| Legacy loop |"), 3)
        self.assertEqual(index.count("| Parameterized kernel |"), 12)

    def test_compiled_source_catalogue_is_not_a_pass_list(self):
        lock = read(ROOT / "tools/rv32i-benchmarks/embench.lock")
        revision = re.search(r"^EMBENCH_COMMIT=(\w+)$", lock, re.M)[1]
        pages = {p.stem for p in (CHAPTER / "compiled").glob("*.md")}
        self.assertEqual(pages, EMBENCH | {"index", "smoke", "compiled-equivalence", "middle-equivalence"})
        for name in EMBENCH:
            source = read(CHAPTER / f"compiled/{name}.md")
            self.assertIn(f"src/{name}/", source)
            self.assertIn(revision, source)
            self.assertIn(f"build-embench.sh {name}", source)
            self.assertIn("not", source.lower())
            self.assertEqual(re.findall(r"^## (.+)$", source, re.M), [
                "Purpose", "Setup", "Implementation", "Expected result", "Measurement and running", "Source and tests"])
        index = read(CHAPTER / "compiled/index.md")
        self.assertIn("Source catalogue is not a pass list", index)
        self.assertIn("libc/libm", index)
        self.assertIn("--allow-incomplete", index)
        middle = read(CHAPTER / "compiled/middle-equivalence.md")
        self.assertIn("--preset compiled-middle", middle)
        self.assertIn("not full Embench", middle)
        self.assertIn("RV32IMiddleBenchmarkTest", middle)
        for name in ("crc32", "nettle-sha256", "sglib-combined"):
            self.assertIn(name, middle)
        self.assertEqual(re.findall(r"^## (.+)$", middle, re.M), [
            "Purpose", "Setup", "Implementation", "Expected result", "Measurement and running", "Source and tests"])

    def test_navigation_and_section_moves(self):
        nav = read(ROOT / "mkdocs.yml")
        for page in CHAPTER.rglob("*.md"):
            relative = str(page.relative_to(DOCS))
            self.assertEqual(nav.count(relative), 1, relative)
        for number, title in ((3, "RV32I Building Blocks"), (4, "Single-Cycle Core"),
                              (5, "Baseline Pipeline"), (6, "Advanced Pipeline")):
            source = read(DOCS / f"rv32i/4.{number}/index.md")
            self.assertTrue(source.startswith(f"# § 4.{number}. {title}"), source[:80])
        self.assertFalse((DOCS / "rv32i/4.1/programs").exists())
        implementation = read(DOCS / "rv32i/4.1/implementation.md")
        self.assertIn("<!-- rv32i-step-examples -->", implementation)
        self.assertIn("<!-- rv32i-edge-examples -->", implementation)
        self.assertIn("historical", read(DOCS / "rv32i/experiments.md").lower())

    def test_relative_document_links(self):
        # Markdown page links only. MkDocs handles HTML embeds and anchors at build time.
        for page in (DOCS / "rv32i").rglob("*.md"):
            for match in re.finditer(r"\]\(([^\s)]+)\)", read(page)):
                target = urlsplit(match[1])
                if target.scheme or target.netloc or not target.path or target.path.startswith("/"):
                    continue
                if not target.path.endswith(".md"):
                    continue
                path = (page.parent / unquote(target.path)).resolve()
                self.assertTrue(path.exists(), f"{page.relative_to(DOCS)} -> {match[1]}")

    def test_source_references_and_documented_suites(self):
        for page in CHAPTER.rglob("*.md"):
            for source in re.findall(r"`((?:include|src|tests|tools)/[^`\n]+)`", read(page)):
                if any(c in source for c in " *{}"):  # commands or explicitly parameterized examples
                    continue
                if source.startswith("src/") and source.split("/")[1] in EMBENCH:
                    continue  # pinned upstream paths, linked explicitly on the compiled pages
                self.assertTrue((ROOT / source).exists(), f"{page.relative_to(DOCS)}: {source}")
        text = "\n".join(read(p) for p in (CHAPTER / "verification").glob("*.md"))
        for suite in ("BenchmarkModelTest", "BenchmarkCampaignCLITest", "RV32IBenchmarkTest",
                      "RV32IBenchmarkCLITest", "RV32IExperimentTest", "RV32IExperimentCLITest",
                      "RV32IDifferentialTest", "RV32IProfileToggleSweepTest", "RV32IBinaryClockCLITest",
                      "SafeClockTimingTest", "MemoryStateTimingTest", "MemoryFeedbackTimingTest",
                      "MemoryEndpointSafeClockTest", "BinaryStartupTimingTest", "RV32IElfLoaderTest",
                      "RV32IExternalValidationSmokeTest", "RV32IReferenceDocumentationTest"):
            self.assertIn(suite, text)

    def test_moved_cpp_example_includes(self):
        paths = [ROOT / "tests/RV32IBaselineExamplesTest.cpp",
                 ROOT / "tests/RV32IAdvancedExamplesTest.cpp",
                 DOCS / "rv32i/4.6/examples.hpp"]
        for path in paths:
            for include in re.findall(r'^#include "(\.\./[^"]+)"', read(path), re.M):
                self.assertTrue((path.parent / include).is_file(), f"{path}: {include}")


if __name__ == "__main__":
    unittest.main()
