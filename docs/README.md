# Internal documentation

The public documentation is the MkDocs site in `site-docs/`. This folder holds
working notes for people changing the code. Finished plans and milestone
reports are in [`archive/`](archive/); they describe the code as it was when
they were written.

## Start here

| Document | Use it for |
| --- | --- |
| [adding-a-component.md](adding-a-component.md) | The current checklist for adding a part |
| [unified-component-profiles.md](unified-component-profiles.md) | Families, structural/behavioral selection, profiles |
| [test-organization.md](test-organization.md) | The scenario registry, CTest and browser/WASM checks |
| [benchmark-system.md](benchmark-system.md) | Workloads, runs, campaigns and equivalence checks |
| [rv32i-cleanup.md](rv32i-cleanup.md) | What has been retired or renamed, and why |

## Current notes by area

| Area | Documents |
| --- | --- |
| RV32I cores | [rv32i-single-cycle-stages.md](rv32i-single-cycle-stages.md), [rv32i-single-cycle-documentation-notes.md](rv32i-single-cycle-documentation-notes.md), [rv32i-memory-integration.md](rv32i-memory-integration.md) |
| Timing and performance | [circuit-timing-analyzer.md](circuit-timing-analyzer.md), [rv32i-circuit-timing-optimization.md](rv32i-circuit-timing-optimization.md), [rv32i-logic-timing-optimization.md](rv32i-logic-timing-optimization.md), [rv32i-core-gap-benchmarks.md](rv32i-core-gap-benchmarks.md), [rv32i-external-benchmark-report.md](rv32i-external-benchmark-report.md) |
| Visualizer and layout | [visualizer-v2-profile-explorer.md](visualizer-v2-profile-explorer.md), [automatic-layout-refresh.md](automatic-layout-refresh.md), [single-cycle-stage-layout.md](single-cycle-stage-layout.md), [wire-routing-algorithms.md](wire-routing-algorithms.md), [visualizer-quick-test.md](visualizer-quick-test.md) |
| Site and deployment | [webassembly-github-pages-report.md](webassembly-github-pages-report.md) |
| Planning | [rv32i-roadmap.md](rv32i-roadmap.md) |

## Kept for reference, describing older code

| Document | Note |
| --- | --- |
| [complete-circuit-workflow.md](complete-circuit-workflow.md) | Walkthrough of the December 2025 API; superseded by the checklist above |
| [building-circuits.md](building-circuits.md) | Early quick guide to gates and composites |
| [behavioral-rv32i-beginners-guide.md](behavioral-rv32i-beginners-guide.md) | Beginner's tour of the behavioral RV32I machine (July 2026) |
| [general-memory-container.md](general-memory-container.md) | The retired legacy array container; current memory design is site sections 3.1 to 3.3 |
