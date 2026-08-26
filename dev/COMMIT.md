# Commit Handoff

## Objective

Run background snapshot refreshes independently of HTTP for MVP-010.

## Files changed

- `include/ghinfo/poller.hpp`
- `src/poller.cpp`
- `src/main.cpp`
- `tests/poller_test.cpp`
- `README.md`
- `docs/ARCHITECTURE.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Acceptance criteria

- `Poller::run` refreshes immediately, publishes immutable snapshots, and
  advances generation on later refreshes.
- Refreshes run in a `std::jthread` owned by `main` and use a stoppable timed
  wait.
- Refresh failures are caught without publishing a candidate or marking the
  store ready.
- Tests prove first publication, normalized resource counts, generation 2, and
  sub-500 ms stop after a one-hour configured interval.

## Validation

- `cmake --build --preset dev --parallel` — passed.
- `ctest --preset dev --output-on-failure` — 26 tests passed.
- `./scripts/validate.sh` — pending before commit.
- `cmake --preset asan` and ASan/UBSan build/tests — pending before commit;
  this executor requires `LSAN_OPTIONS=detect_leaks=0` because it runs under
  ptrace.
- `git diff --check` — pending before commit.

## Compatibility and security

No public data endpoint changed. HTTP handlers still read only the snapshot;
poll failures do not expose credentials or replace existing state.

## Deferred

Backoff/rate-limit retry policy, public resource endpoints, signal handling,
and container hardening remain assigned to later milestones.
