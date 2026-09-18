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
implementation was validated through dev and ASan/UBSan builds, deterministic
C++ and Python tests, format checks, and secret-safety checks.

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

## Completed mixed-state activity previews

Each workflow, pull-request, and issue category is filled independently. Open
or active items retain priority, while recent closed records or completed
workflow runs fill any remaining slots up to the category limit. Closed issues
and pull requests remain activity-only; open-only resource and grouped activity
contracts remain unchanged. See
[ADR-0007](../docs/adr/0007-mixed-state-activity-previews.md).

## Completed active-work priority

Queued and in-progress workflow runs and jobs are classified as `critical` and
therefore sort ahead of recent failures. Recent failures are `high`, stale
failures remain `normal`, and the existing 7/30-day eligibility policy is
unchanged. See [ADR-0006](../docs/adr/0006-active-work-priority.md).

## Network-outage recovery follow-up

Transport errors now retain libcurl codes for safe operational diagnostics.
Poll failure logs report UTC time, status/code, consecutive failures, and
retry delay without logging exception text or upstream content. Recovery is
logged after a complete successful refresh. Transport backoff is capped at
60 seconds; other failures retain the existing 900-second ceiling and rate
limit policy. HTTP integration coverage verifies that activity reads finish
while an upstream request is blocked and preserve the snapshot after failure.

The reported incident cannot be attributed to DNS, TLS, timeout, or a
particular HTTP status from the old category-only logs. Live checks of the
widget's host returned healthy HTTP 200 responses and no current poll failures.
SSH inspection found 21 transport and 6 HTTP failures in the preceding 24
hours, with zero container restarts. Repeated activity reads took 19–32 ms,
with one earlier outlier near 1 second; GitHub probes inside the container
returned HTTP 200 in 119–159 ms. These observations do not reproduce Android
startup behavior. Production contains pre-existing staged changes; the
source/test patch passes `git apply --check` against that tree. Deployment
and verification on the phone remain pending.

## Lightweight activity response

The full `/v1/activity` response keeps its compatibility grouped arrays. The
additive `/v1/activity/items` route returns only common metadata and the
selected ordered items, with the same `limit` and `category` rules. The
ready-to-import Kustom preset now uses this route for its four WebGet flows;
its formulas and `ghinfo` global shape remain unchanged. This avoids sending
and repeatedly parsing unrelated workflow, issue, and pull-request arrays on
each widget render. The compact route and HTTP behavior are covered by the
API tests. Live endpoint timing remains fast; Android/Kustom rendering still
requires device-side verification.

## Completed Kustom clipboard setup

`setup.py` now provides a credential-free interactive wizard for the widget.
It reads the checked-in native `.kwgt` tree, substitutes the reachable ghinfo
URL and bounded refresh interval, optionally seeds the initial activity
snapshot, and emits complete `##KUSTOMCLIP##` and customized `.kwgt` artifacts
under `dist/`. The generated component preserves the existing globals, Flows,
layout, formulas, touch actions, and bundled font. Deterministic Python tests
cover URL/interval validation, all four endpoint substitutions, marker format,
ZIP integrity, font preservation, and secret absence. A default complete clip
is tracked under `assets/ghinfo-kustom-widget.clip`; personalized output stays
ignored under `dist/`.
