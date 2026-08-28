# ADR-0001: Prioritized activity projection

- Status: Accepted
- Date: 2026-08-26
- Scope: additive `/v1/activity` extension

Temporal aging, category diversity, and incident handling are subsequent
additive refinements recorded in [ADR-0002](0002-temporal-diversified-activity.md).

## Context

The current activity endpoint exposes separate groups of running jobs, failed
runs, open pull requests, and open issues. Consumers that need a bounded view
of what deserves attention would otherwise have to duplicate relevance and
ordering rules. The service already owns a complete immutable snapshot, so it
can publish a deterministic projection without becoming a notification queue.

The projection must remain read-only, consumer-agnostic, in-memory, and safe
when the last-known-good snapshot is stale. It must not infer historical
events, acknowledgement state, or consumer-specific presentation.

## Decision

Extend `GET /v1/activity` additively with `activity.items`. The existing
`runningJobs`, `failedRuns`, `pullRequests`, and `issues` arrays remain in the
response unchanged. The item list is derived while a complete snapshot is
constructed and is stored as immutable snapshot data. HTTP handlers only
validate the requested limit and select from that immutable list.

### Request contract

- `limit` is optional and defaults to `20`.
- Values from `1` through `100` are valid.
- Empty, non-numeric, zero, negative, overflowing, or greater-than-100 values
  return `400` with `{"error":"invalid_limit"}`.
- Reads never acknowledge, remove, reserve, or mutate items.

### Item kinds and priority bands

Every item has a stable string `kind`, `priority`, `signals` array,
`repository`, `id`, nullable `updatedAt`, and `url`. Kind-specific fields are
included only when applicable:

- `failed_job`: workflow job with `conclusion=failure`; `critical` priority;
  includes `runId`, `name`, `status`, and `conclusion`.
- `failed_run`: workflow run with `conclusion=failure`; `critical` priority;
  includes `name`, `status`, and `conclusion`.
- `running_job`: queued or in-progress workflow job; `high` priority; includes
  `runId`, `name`, and `status`.
- `pull_request`: open pull request; `high` priority; includes `number` and
  `title`.
- `issue`: open issue; `normal` priority; includes `number` and `title`.

The `signals` array contains stable explanations such as `failed_job`,
`failed_run`, `running_job`, `open_pull_request`, or `open_issue`. No opaque
numeric score is exposed. The base priority classification above is subject
to the temporal failure policy in ADR-0002.

### Ordering

Eligible items are ordered by priority band (`critical`, `high`, `normal`),
then by effective timestamp descending, repository ascending, kind ascending,
and stable numeric ID ascending. Effective timestamps are `updatedAt` for
issues, pull requests, and workflow runs; a job uses `completedAt`, then
`startedAt`. Jobs without either timestamp sort after timestamped items in
their priority band. The limit selector's category balancing and top-three
incident handling are defined by ADR-0002.

The projection is deterministic for equal inputs. The priority band is an
explicit urgency classification; recency is an explicit ordering signal rather
than an unstable band promotion.

### Compatibility and deferrals

This is an additive v1 schema change. Existing grouped fields remain available
for consumers that do not use `items`. The extension does not add
`firstSeenAt`, event history, persistence, acknowledgements, per-consumer
state, SQLite, webhooks, GraphQL, brokers, or dashboard-specific fields.

## Consequences

Consumers can request a bounded, already ordered view without implementing
GitHub-specific relevance rules. Snapshot construction performs a small,
linear projection over data already retained in memory. Repeated reads are
cheap and identical for the same snapshot, but the service still does not
provide durable notification semantics or tell consumers what changed since a
previous poll.

## Rejected alternatives

- An opaque numeric score would make compatibility and debugging difficult.
- A consuming queue or acknowledgement state would require per-consumer
  identity and persistence decisions outside the MVP boundary.
- Computing the projection through GitHub calls in a request handler would
  violate the polling architecture and make latency/rate limits unpredictable.
