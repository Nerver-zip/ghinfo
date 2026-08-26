# Commit Handoff

## Objective

Add the objective, consumer-neutral `/v1/activity` view for MVP-013.

## Files changed

- `include/ghinfo/server.hpp`
- `src/server.cpp`
- `tests/api_test.cpp`
- `docs/API.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- `/v1/activity` exposes running jobs, failed runs, open pull requests, and
  open issues from the immutable snapshot.
- Activity contains no priority, score, confidence, or UI-specific fields.
- The endpoint preserves the standard version/timestamp/stale envelope and
  returns `503 snapshot_unavailable` before readiness.
- Focused tests cover objective grouping and forbidden presentation fields.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 34 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan` and
  `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` —
  pending before commit; this executor runs under ptrace and otherwise makes
  LeakSanitizer abort during test discovery.
- `git diff --check` — pending before commit.

## Compatibility and security

This is an additive v1 endpoint. It reads only the immutable snapshot and
does not create GitHub traffic or expose credentials.

## Deferred

Failure hardening, final container verification, signal behavior, CI/release
work, and the v0.1.0 audit remain.
