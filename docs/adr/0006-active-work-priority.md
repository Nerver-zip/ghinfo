# ADR-0006: Prioritize active workflow work

- Status: Accepted
- Date: 2026-08-30
- Scope: `/v1/activity` priority semantics

## Context

Workflow failures remain visible after the workflow has finished. A recent
failure can therefore stay in the activity projection for days, while a
queued or in-progress workflow represents work happening at the moment of the
snapshot. The bounded default view should communicate current activity before
historical incidents.

## Decision

Classify queued and in-progress workflow runs and jobs as `critical`. This
band always sorts ahead of failures, regardless of the active item's or
failure's timestamp.

Classify failures updated within 7 days as `high` and retain the
`recent_failure` signal. Failures older than 7 and up to 30 days remain
`normal` with `stale_failure`; failures older than 30 days remain excluded
from `activity.items`. Failures with an unparseable timestamp retain the
failure base priority (`high`) without an age signal.

Open pull requests remain `high`; open issues and recent closed pull-request
fallback items remain `normal`. The no-category diversity rules, category
views, incident deduplication, grouped fields, and `/v1/runs` history are
unchanged.

This supersedes only the priority-band mapping described by ADR-0001 and the
temporal priority mapping in ADR-0002. It does not change failure eligibility
or the immutable, read-only snapshot architecture.

## Consequences

The default activity photograph surfaces active execution immediately and
prevents an older failure from outranking work currently in progress. Recent
failures remain available and distinguishable, but recency orders them only
within their `high` band. The public `priority` values remain the same, so no
schema field or API version changes.

Consumers that need the complete retained failure history continue to use
`/v1/runs` and `/v1/jobs`; the activity projection remains bounded and
non-durable.

## Verification

Unit and API golden tests cover the new priority values and ordering, including
an active workflow with an older timestamp competing against a newer failure.
