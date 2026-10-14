# Internal documentation

The public documentation is the MkDocs site in `site-docs/`. This folder holds
maintained guides and reference, a living project status, a changelog, dated
topic records and historical reports. [AGENTS.md](../AGENTS.md)
defines when agents update each one as part of their work. Public component
pages describe current IO, behavior, internals, examples and edge cases.
Finished plans and milestone reports belong in [`archive/`](archive/); they
describe the code as it was when they were written.

| Document | Purpose |
| --- | --- |
| [STATUS.md](STATUS.md) | Current project-wide state, active records and open work |
| [CHANGELOG.md](CHANGELOG.md) | Meaningful completed changes, verification and limits |
| [guides/](guides/README.md) | Procedures and guided explanations |
| [reference/](reference/README.md) | Maintained contracts, architecture and specifications |
| [records/](records/README.md) | Dated evidence and decisions for substantive topics |
| [archive/](archive/README.md) | Indexed historical plans, guides and reports |

## Where to put a document

| Purpose | Location |
| --- | --- |
| How to perform a task or follow an explanation | [guides/](guides/README.md) |
| Current behavior, contracts, structure or verification specification | [reference/](reference/README.md) |
| Work in progress, decisions or validation evidence for a dated topic | [records/](records/README.md) |
| Completed reports, superseded instructions or historical proposals | [archive/](archive/README.md) |

## Historical documents

The [archive index](archive/README.md) lists completed reports, older API
guides, superseded roadmaps and unimplemented proposals. Treat their dates,
metrics and paths as historical evidence. Use current source, tests and public
site pages to establish present behavior.
