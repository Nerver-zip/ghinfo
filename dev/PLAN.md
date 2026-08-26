# Active Plan

## Current milestone

**MVP-010 — Background poller**

Target commit:

```text
feat(poller): add periodic background refresh
```

## Goal

Run complete snapshot refreshes independently of HTTP requests, publish the
first successful snapshot atomically, repeat at the configured interval, and
stop promptly through `std::stop_token`.

## Acceptance criteria

- `Poller::run` performs an initial refresh without waiting for the interval.
- Each successful refresh publishes a complete immutable snapshot and advances
  generation.
- The poller uses `std::stop_token` and a stoppable timed wait.
- A failed refresh does not publish a candidate or mark the store ready.
- `main` owns the poller in a `std::jthread`; API handlers remain read-only.
- Tests prove first publication, generation, and prompt stop with a local
  hermetic GitHub server.

## Non-goals

- exponential backoff or stale metadata;
- public resource endpoints;
- signal/container lifecycle hardening beyond normal jthread ownership.

## Expected files

- `include/ghinfo/poller.hpp`
- `src/poller.cpp`
- `src/main.cpp`
- `tests/poller_test.cpp`
- `README.md`
- `docs/ARCHITECTURE.md`
- `dev/COMMIT.md`

## Required skills

- `.agents/skills/modern-cpp/SKILL.md`
- `.agents/skills/polling/SKILL.md`
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
git diff --check
```
