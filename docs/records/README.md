# Topic records

Create a record automatically when substantive design, implementation,
investigation, audit or validation work begins. Reuse an open record for the
same topic. Name it `YYYY-MM-DD-topic.md` using the topic's start date; work
that spans days stays in that file. List every open record under "In progress"
in [STATUS.md](../STATUS.md).

Use this template:

```markdown
# Short topic title

Started: YYYY-MM-DD. Status: open. Code state: `<branch>` at `<starting commit>`.

## Summary

Current conclusion and next step in a few lines.

## YYYY-MM-DD — Investigation

What was observed, with source paths, commands, key results and uncertainty.

## YYYY-MM-DD — Decision and verification

What changed and why; checks run, results, limits and remaining work.
```

The header and short summary may be rewritten as the work progresses. Dated
sections are append-only. Correct an earlier assessment in a new dated
section and name what it replaces. Copy important numbers and failure details
into the tracked record; an ignored `build/` artifact alone is insufficient.
State whether a check was run, failed or remains unrun.

When the topic is finished, change its status to `closed`, update the summary
and the [status file](../STATUS.md), and freeze the record. Follow-up work gets
a new record that links to the closed one. Put completed change summaries in
the [changelog](../CHANGELOG.md) and current usage instructions in the public
site or current internal guidance.
