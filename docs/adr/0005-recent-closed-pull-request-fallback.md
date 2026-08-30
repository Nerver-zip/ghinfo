# ADR-0005: Recent closed pull-request activity fallback

- Status: Accepted
- Date: 2026-08-30
- Scope: GitHub pull-request collection and prioritized activity projection

## Context

The primary pull-request view is intentionally open-only. That is useful while
there is active review work, but it leaves the activity projection without a
pull-request item when a user has no open pull requests. A recent completed
review can still be useful context for a compact current-state photograph.

The fallback must not turn `/v1/pulls` into a mixed-state endpoint, create an
unbounded history query, or introduce consumer-specific presentation logic.

## Decision

During complete snapshot construction, if and only if no open pull request was
collected across the selected repositories, request up to 3 closed pull
requests per repository from GitHub, using `state=closed`, `sort=updated`, and
`direction=desc`. Store these records separately from open pull requests.

Expose the fallback records only through the prioritized `activity.items`
projection. They remain `kind: "pull_request"`, have `normal` priority, and
carry the `recent_closed_pull_request` signal. The fallback is suppressed when
at least one open pull request exists in the complete snapshot.

Keep `/v1/pulls`, repository pull-request arrays, summary counts, and grouped
`activity.pullRequests` open-only. Keep the service in-memory, read-only, and
consumer-agnostic.

## Consequences

The activity view remains useful when review queues are empty while existing
open-PR consumers retain their contract. The additional collection is bounded
to one page and three records per repository, but it adds one request per
repository only for snapshots without open pull requests. Closed fallback
items have lower priority than current failures, active work, and open PRs.

The fallback is not a notification history and does not persist first-seen
state. A closed PR can appear again in a later snapshot if it remains among the
three most recently updated closed PRs and no open PR exists.

## Alternatives rejected

- Mixing closed PRs into `/v1/pulls` or grouped activity would break their
  established open-only meanings.
- Fetching all closed PRs would increase request cost and duplicate history
  responsibilities that belong to bounded resources.
- Adding a Kustom-specific field or endpoint would violate the consumer-neutral
  API boundary.

## Verification

The GitHub client test checks the bounded closed-PR query and ordering. Snapshot
tests verify fallback activation only when there are no open PRs, and API tests
verify that fallback items stay confined to prioritized activity.
