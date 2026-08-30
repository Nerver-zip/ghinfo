# Active Plan

## Current state

The implementation milestones through MVP-017, the prioritized activity
projection, and its temporal/diversified follow-up are complete in the local
checkout. The projection is additive, in-memory, read-only, and covered by
the current API, snapshot, parser, and sanitizer tests. The `v0.3.0` tag is
the current release target; the rewritten main branch still requires external
synchronization.

## Final audit criteria

- All planned HTTP routes are exercised over a local server, including filters
  and `400`/`404`/`503` responses.
- Canonical dev and sanitizer validation remain green after the audit fix.
- Every commit has post-commit status, whitespace, and relevant test evidence.
- Docker image build and remote GitHub Actions/release state are verified on a
  host with Docker and a configured GitHub remote.
- Upstream timestamps and repository identity are rejected when they violate
  the normalized domain contract.
- Automatic repository discovery is paginated, validated, and integrated into
  complete snapshot construction.
- Activity failures are aged against deterministic snapshot time, expired
  failures are omitted only from `activity.items`, and the retained run
  history remains exposed by `/v1/runs`.
- Activity limits provide deterministic category diversity and incident
  deduplication in the top three without consumer state.
- Future tags or remote releases require those external gates.

## External verification

The Docker image and remote workflow/release state remain external evidence.
The local environment has no active Docker daemon, so the current activity
implementation was validated through dev and ASan/UBSan builds, 65 deterministic
tests, format checks, and Gitleaks.

## Completed post-MVP milestone

The prioritized activity projection is implemented and documented. Its ADRs,
API extension, immutable snapshot derivation, explicit temporal policy,
category-balanced selection, compatibility fields, parser validation, golden
contract, HTTP coverage, stale-read behavior, and deterministic unit tests are
present in the local checkout.

The implementation should remain in-memory and should not add SQLite, a broker,
notifications, or per-consumer acknowledgement state.

## Completed activity category views

`GET /v1/activity` now accepts the optional `category` filter with the values
`workflows`, `pull_requests`, and `issues`. Category views provide bounded,
consumer-agnostic lists for independent widget controls while preserving the
existing no-category diversity behavior and response envelope. Filtering is
performed over immutable snapshot data after failure-age eligibility, with the
same priority, recency, deterministic tie-break, and workflow incident
deduplication rules. Invalid categories return `invalid_category` with HTTP
400.

Build acceleration is separate from application persistence: CI and the
container builder may use ccache/BuildKit caches, but runtime images and
snapshots remain unchanged.

## Current follow-up

The job-detail collection is bounded independently from workflow-run history.
`GHINFO_RUN_HISTORY` preserves the configured run window for `/v1/runs`, while
`GHINFO_JOB_RUN_HISTORY` defaults to 10 and expands jobs only for the newest
runs plus active runs per repository. This allows recent failed jobs inside a
successful workflow to reach `/v1/activity` without querying all retained
history. See [ADR-0003](../docs/adr/0003-bounded-job-expansion.md).

## Completed recent closed pull-request fallback

When a complete snapshot has no open pull requests, the collector requests up
to 3 recently updated closed pull requests per repository. They are retained
separately and exposed only as normal-priority `pull_request` activity items
with the `recent_closed_pull_request` signal. Open-only resource and grouped
activity contracts remain unchanged. See
[ADR-0005](../docs/adr/0005-recent-closed-pull-request-fallback.md).

## Completed active-work priority

Queued and in-progress workflow runs and jobs are classified as `critical` and
therefore sort ahead of recent failures. Recent failures are `high`, stale
failures remain `normal`, and the existing 7/30-day eligibility policy is
unchanged. See [ADR-0006](../docs/adr/0006-active-work-priority.md).
