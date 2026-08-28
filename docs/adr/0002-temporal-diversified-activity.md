# ADR-0002: Temporal and diversified activity selection

- Status: Accepted
- Date: 2026-08-28
- Scope: additive `/v1/activity` projection behavior

## Context

The initial prioritized projection gave every failed run and failed job the
`critical` band indefinitely. A bounded view could therefore be dominated by
old workflow failures, hiding current pull requests, issues, and running
work. The service also needs predictable category diversity for small widget
views without becoming a durable notification queue.

`Snapshot::generated_at` is already a deterministic timestamp for a complete
poll. It can age failures without adding a clock dependency, first-seen state,
or persistence.

## Decision

### Temporal relevance

For failed workflow runs, use `updatedAt`. For failed jobs, use
`completedAt`, falling back to `startedAt`. Relative to `generatedAt`:

- age up to 7 days: retain `critical` and add `recent_failure`;
- age over 7 and up to 30 days: retain as `normal` and add `stale_failure`;
- age over 30 days: omit from `activity.items`.

Future timestamps are treated as current. If a timestamp cannot be evaluated,
the item is retained at its base priority because normalized GitHub payloads
have already been validated and retaining attention is safer than silently
discarding it. Queued or in-progress workflow runs and jobs remain relevant;
open pull requests and issues retain their existing priorities.

This filtering applies only to `activity.items`. `/v1/runs`, `/v1/jobs`, and
the grouped activity arrays continue to expose the complete current snapshot.
Workflow history is still bounded by `GHINFO_RUN_HISTORY` per repository;
“all history” means all retained history, not every run stored by GitHub.

### Diversity

Eligible items are partitioned into:

1. jobs and workflows;
2. open pull requests;
3. open issues.

Each partition keeps the global deterministic ordering. For limits 1 and 2,
the best global items are selected. For limits at least 3, the selector first
allocates `floor(limit / 3)` slots to every category, redistributes slots when
a category is short, and fills any remainder with the best unselected global
candidates. Thus limits 3, 6, and larger multiples attempt 1/1/1, 2/2/2,
and equal rounds respectively; limits 4 and 5 start with one from each
category before filling the remainder globally.

The selected list follows priority, recency, repository, kind, and ID order.
When an alternative exists, the first three selected items do not contain both
the failed run and a failed job for the same repository and workflow run; the
duplicate is deterministically deferred beyond the protected top three. The
two kinds remain distinct and are available at larger limits or when no
alternative exists.

## Compatibility

The change is additive within v1. Existing grouped fields and their source
data are preserved. `runningRuns` and `running_run` expose already normalized
workflow state; existing consumers can ignore unknown additive fields and
values. No score, acknowledgement, queue, persistence, webhook, or
consumer-specific field is introduced.

## Consequences

Small consumers receive a more useful current photograph while larger
consumers can still inspect all eligible activity items. Selection remains
linear over in-memory data and is repeated consistently for every read. Old
failures remain discoverable through `/v1/runs` until they leave the configured
run history, but no longer crowd the bounded activity view.

The projection is still not durable notification history. A restart loses the
in-memory snapshot, and `firstSeenAt` remains intentionally deferred.
