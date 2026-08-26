# Active Plan

## Current milestone

**MVP-013 — Activity view**

Target commit:

```text
feat(api): add aggregated activity endpoint
```

## Goal

Add a consumer-neutral activity view composed only from objective state already
present in the immutable snapshot.

## Acceptance criteria

- `GET /v1/activity` returns `runningJobs`, `failedRuns`, `pullRequests`, and
  `issues` groups.
- Groups contain normalized resource payloads and deterministic ordering from
  the snapshot.
- No priority, confidence, score, display, or consumer-specific fields are
  introduced.
- The endpoint returns `503 snapshot_unavailable` before the first complete
  poll and never triggers GitHub traffic.
- Tests cover inclusion, exclusion, schema version, and stale metadata.

## Non-goals

- ranking or prioritization;
- recommendation logic;
- notifications or event streaming;
- local API pagination.

## Expected files

- `include/ghinfo/server.hpp`
- `src/server.cpp`
- `tests/api_test.cpp`
- `docs/API.md`
- `docs/ROADMAP.md`
- `README.md`
- `dev/COMMIT.md`
- `dev/PLAN.md`

## Required skills

- `.agents/skills/modern-cpp/SKILL.md`
- `.agents/skills/api-contract/SKILL.md`
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
LSAN_OPTIONS=detect_leaks=0 cmake --build --preset asan
LSAN_OPTIONS=detect_leaks=0 ctest --preset asan --output-on-failure
git diff --check
```
