# Commit Handoff

## Objective

Preserve the last-known-good snapshot and expose safe operational poll state
for MVP-011.

## Files changed

- `include/ghinfo/github_client.hpp`
- `include/ghinfo/model.hpp`
- `include/ghinfo/poller.hpp`
- `include/ghinfo/snapshot.hpp`
- `src/github_client.cpp`
- `src/poller.cpp`
- `src/server.cpp`
- `tests/poller_test.cpp`
- `tests/snapshot_test.cpp`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- A failed refresh never replaces or clears the last published snapshot.
- Poll state records attempt time, last success, stale status, consecutive
  failures, safe error category, and next retry time.
- A successful refresh resets failure state and advances snapshot generation.
- Retry delay uses bounded exponential backoff, with `Retry-After` and rate
  limit reset hints taking precedence when available.
- Stop tokens interrupt normal and failure waits promptly.
- Tests prove preservation, stale transition, recovery, bounded backoff, and
  rate-limit hint precedence.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 29 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  pending before commit; this executor runs under ptrace and otherwise makes
  LeakSanitizer abort during test discovery.
- `git diff --check` — pending before commit.

## Compatibility and security

No public data endpoint changed. HTTP handlers still read only the snapshot;
poll failures do not expose credentials or replace existing state. Poll state
stores only bounded categories and timestamps, never raw GitHub error bodies.

## Deferred

Public resource endpoints and status serialization, signal handling, and
container hardening remain assigned to later milestones.
