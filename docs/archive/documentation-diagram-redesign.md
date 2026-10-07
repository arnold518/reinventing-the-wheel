# Documentation diagram design

The reference pages use static, inline SVG figures that match Material's Noto Sans / Ubuntu Mono typography and light/slate themes. Each figure answers one question. Nesting means ownership; arrows mean a stated flow or logical ordering, not automatically a clock cycle.

The compositions follow the content-first, nested/architecture guidance in [diagram-design](https://github.com/cathrynlavery/diagram-design), reviewed at commit `dc1ace47b99a419e42d01a03cb6ace5346efa8ae`. The user chose the existing documentation theme, so its fonts and colors replace the skill's default skin. The 640px canvas fits the article column at desktop size; 16px primary and 14px secondary labels stay readable instead of shrinking a large graph. Narrow screens scroll the figure locally.

The skill's Mermaid importer could not parse one existing FF component label (`malformed edge at line 765`). These are manual semantic redraws checked against the reference Markdown and component wiring, not claims of a lossless automatic conversion. No skill installation or global style profile is required.

## Content and fidelity ledger

| Figure / page | Question | Reorganization and intentionally collapsed detail |
| --- | --- | --- |
| `memory-storage` / 3.1 | Where does storage live? | Nest device, bank, word/row, and cells. Technology-specific peripheral roles surround the bank; the device-family and layer tables retain distinctions between technologies. |
| `memory-endpoints` / 3.1 | How do device, controller, and endpoint compose? | Nest a controller and one native device inside MemoryEndpoint, below the universal transaction interface. Expanded storage and MemoryContainer are alternative device representations for the selected technology, not parallel devices. Lines denote interfaces, not one-way data flow. No processor-specific paths are shown; the caption explains direct native access and the fused endpoint alternative. |
| `ff-memory` / 3.1.1 | How do N words and W bits compose? | Expand one representative word and three representative cell positions. Address/access/read helper names remain outside the bank; the following table specifies their types and pins. |
| `register32` / 4.3.1 | What storage does one register reuse? | Show the FF word, representative cells, and full mask. Splitters, byte enables, and joining are summarized in the caption; their implementation remains documented under FFWord. |
| `register-file` / 4.3.2 | How do two read ports share 31 stored words? | Show one write decoder, one bank, and two selectors. x0 sits outside the bank. Omit x0-to-selector wires and the full-mask helper from the drawing; preserve them in the connection table. |
| `memory-access` / 4.3.6 | How do request and returned data travel? | Separate outgoing and incoming views of the same four components. Abbreviated names are expanded in the caption. Handshakes, issue, completion, and faults stay in the pin tables. |
| `single-cycle-stages` / 4.4 | What responsibilities occur in logical order? | Use one aligned row. Explicitly distinguish logical order from five clock cycles or a complete data path. Exact instruction, operand, address, and store-data routes are tabulated below. |
| `single-cycle-writeback` / 4.4 | Which values can be written back? | Show the four alternatives and one destination. Make ALU/link bypass of Memory explicit; selection and capture-enable names remain in the caption. |
| `single-cycle-next-pc` / 4.4 | How is choosing a PC different from writing it? | Separate fallthrough/target selection from Fetch's PC storage. Selection inputs and alignment/completion conditions are named below the figure. |
| `single-cycle-completion` / 4.4 | What coordinates architectural completion? | Group inputs and outputs by responsibility instead of drawing every control wire. The groups are not new components. ISSUE, TAKE, address-check inputs, and omitted CLK/RST/ENABLE are explained in the caption and prose. |

No C++ example, IO table, test-row table, or simulator embed was removed. The full wiring remains in the component references, especially 4.4.7. The overview figures deliberately do not reproduce complete netlists.

## Editing and rendering

1. Edit the compositions or shared theme tokens in `tools/render_documentation_diagrams.py`.
2. Run `python tools/render_documentation_diagrams.py` to update the ten self-contained HTML assets in `site-docs/assets/diagrams/`.
3. Reference an asset with a `<!-- diagram: slug -->` marker in Markdown. `tools/mkdocs_diagrams.py` inlines only the marked figure, not the standalone document or its font stylesheet. Both MkDocs configurations enable this hook.
4. Run `python tools/render_documentation_diagrams.py --check` and `python tests/DocumentationDiagramsTest.py`.
5. Run `python tests/DocumentationDiagramsBrowserTest.py http://127.0.0.1:8893/ /tmp/ritw-diagram-review` against the local preview, then inspect the screenshots. A passing geometry check does not replace visual review.

The figures have no runtime renderer, remote diagram service, animation, or iframe. Each SVG has a unique accessible title/description; its scroll region is keyboard-focusable. The HTML assets can also be opened independently. Google Fonts are optional for standalone viewing; the page figures inherit the site's font stack.

## Validation scope

- The source test checks generated-asset freshness, SVG accessibility and local references, page markers, duplicate/malformed IDs, both hook configurations, and preservation of C++/simulator content during inlining.
- The browser test checks every figure at 1440px, 640px, and 360px viewport widths in both themes. It checks label bounds, label/connector overlaps, desktop fit, page overflow, keyboard scrolling, stage order, unique IDs, instant navigation, and rendering with JavaScript/web fonts disabled.
- The four existing Register32, register-file, building-block, and single-cycle documentation tests validate their actual C++ reference examples.
- MkDocs strict builds validate publication. Diagram changes do not require rebuilding the simulator. Browser tests block simulator URLs to avoid allocating multiple WASM heaps.

The external skill's `self_check.py` was also run against all ten standalone assets. It is a one-time external validation, not an undeclared dependency of this repository's tests.
