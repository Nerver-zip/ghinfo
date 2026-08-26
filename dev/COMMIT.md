# Commit Handoff

## Objective

Build complete normalized repository snapshots for MVP-009.

## Files changed

- `CMakeLists.txt`
- `include/ghinfo/model.hpp`
- `include/ghinfo/snapshot.hpp`
- `include/ghinfo/github_client.hpp`
- `include/ghinfo/snapshot_builder.hpp`
- `src/github_client.cpp`
- `src/snapshot_builder.cpp`
- `tests/snapshot_builder_test.cpp`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- Every configured repository contributes repository, issues, pulls, bounded
  runs, and relevant jobs to one candidate snapshot.
- Candidate construction is all-or-nothing for the configured refresh.
- Generation, supplied UTC timestamps, deterministic collection ordering, and
  normalized rate-limit metadata are present.
- Snapshot-builder tests exercise both configured repositories and all local
  resource endpoints.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 25 tests passed.
- `LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan --parallel` — passed.
- `LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure` — 25 tests passed.
- Unmodified ASan discovery is unavailable in this executor because LeakSanitizer
  refuses to run under ptrace; the limitation is recorded, not hidden.
- `./scripts/validate.sh` — pending before commit.
- `git diff --check` — pending before commit.

## Compatibility and security

No public HTTP schema changed. The builder publishes no state itself and does
not expose raw upstream payloads or credentials.

## Deferred

Background polling, stale-state preservation, retries/backoff, and public data
endpoints remain assigned to later roadmap milestones.
