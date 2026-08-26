# Commit Handoff

## Objective

Expose the normalized snapshot through the stable v1 read API for MVP-012.

## Files changed

- `include/ghinfo/server.hpp`
- `src/server.cpp`
- `tests/api_test.cpp`
- `tests/golden/summary.json`
- `docs/API.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- `/v1/summary` reports deterministic aggregate counts.
- Repository, issue, pull request, workflow run, and workflow job resources
  are exposed as normalized JSON arrays/objects.
- Repository filters and run status/conclusion filters are supported and
  invalid filters return bounded client errors.
- Data endpoints return `503 snapshot_unavailable` without a snapshot.
- Meta includes safe poll and rate-limit state.
- Golden and focused tests cover schema, normalization, nullable fields, and
  filtering.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 33 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  pending before commit; this executor runs under ptrace and otherwise makes
  LeakSanitizer abort during test discovery.
- `git diff --check` — pending before commit.

## Compatibility and security

This is an additive v1 API implementation. Handlers read only immutable
snapshots and operational state; they never invoke GitHub or expose tokens,
raw upstream bodies, or raw exception messages.

## Deferred

Activity aggregation, final container hardening, signal behavior, and release
automation remain assigned to later milestones.
