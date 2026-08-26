---
name: polling
description: Implement independent background polling, immutable snapshot publication, stale-state handling, backoff, and graceful shutdown.
---

# Polling

## Architecture

HTTP consumers do not trigger GitHub traffic.

The poller:

1. reads configured repositories;
2. refreshes source data;
3. normalizes it;
4. builds a complete candidate snapshot;
5. publishes atomically only when invariants hold.

## Failure rules

- Preserve last-known-good data on transient failure.
- Record poll attempt/success metadata separately.
- Do not publish partially mutated shared state.
- Back off after repeated failures.
- Honor stop tokens and shutdown promptly.
- Avoid repeatedly fetching jobs for irrelevant completed-success runs.

Do not introduce worker pools until measured need exists.
