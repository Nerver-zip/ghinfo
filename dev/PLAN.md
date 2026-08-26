# Active Plan

## Current milestone

**MVP-011 — Resilience and backoff**

Target commit:

```text
feat(poller): preserve last-known-good state
```

## Goal

Track polling attempts and failures independently from immutable snapshots,
preserve the last successful data, and schedule bounded retries using GitHub
throttling hints when available.

## Acceptance criteria

- A failed refresh never replaces or clears the last published snapshot.
- Poll state records attempt time, stale state, consecutive failures, safe
  failure category, and next retry metadata without raw errors/secrets.
- Successful refresh resets stale/failure state and keeps generation monotonic.
- Exponential retry delay is bounded; `Retry-After` takes precedence for 403/429
  and the rate-limit reset hint is honored when present.
- Stop tokens interrupt retry waits promptly.
- Tests prove last-known-good preservation, stale transition, backoff bounds,
  rate-limit hints, and reset after recovery.

## Non-goals

- public status serialization beyond existing endpoints;
- resource filtering or activity endpoint;
- container/release changes.

## Expected files

- `include/ghinfo/model.hpp`
- `include/ghinfo/snapshot.hpp`
- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `include/ghinfo/poller.hpp`
- `src/poller.cpp`
- `tests/github_client_test.cpp`
- `tests/poller_test.cpp`
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `dev/COMMIT.md`

## Required skills

- `.agents/skills/modern-cpp/SKILL.md`
- `.agents/skills/github-rest/SKILL.md`
- `.agents/skills/polling/SKILL.md`
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan
LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure
git diff --check
```
