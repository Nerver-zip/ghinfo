# Active Plan

## Current state

The implementation milestones through MVP-017, the prioritized activity
projection, and its temporal/diversified follow-up are complete in the local
checkout. The projection is additive, in-memory, read-only, and covered by
the current API, snapshot, parser, and sanitizer tests. The `v0.1.0` tag has
been published remotely; the rewritten main branch still requires external
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
implementation was validated through dev and ASan/UBSan builds, 55 deterministic
tests, format checks, and Gitleaks.

## Completed post-MVP milestone

The prioritized activity projection is implemented and documented. Its ADRs,
API extension, immutable snapshot derivation, explicit temporal policy,
category-balanced selection, compatibility fields, parser validation, golden
contract, HTTP coverage, stale-read behavior, and deterministic unit tests are
present in the local checkout.

The implementation should remain in-memory and should not add SQLite, a broker,
notifications, or per-consumer acknowledgement state.

Build acceleration is separate from application persistence: CI and the
container builder may use ccache/BuildKit caches, but runtime images and
snapshots remain unchanged.
