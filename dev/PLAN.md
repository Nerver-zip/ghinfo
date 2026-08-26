# Active Plan

## Current milestone

**MVP-009 — Snapshot builder**

Target commit:

```text
feat(poller): build repository snapshots
```

## Goal

Build a complete immutable candidate snapshot from all configured repositories
and normalized GitHub resources before any future publication.

## Acceptance criteria

- Repository metadata, open issues, open pull requests, bounded workflow runs,
  and relevant jobs are combined into one `Snapshot`.
- A repository refresh failure aborts candidate construction rather than
  producing a partial snapshot.
- Generation and supplied UTC timestamp are stored in the candidate.
- Rate-limit headers are normalized into optional snapshot metadata.
- Collections have deterministic ordering independent of response arrival.
- Tests exercise all resource endpoints through a local HTTP server and verify
  complete counts, generation, timestamp, ordering, and rate limit.

## Non-goals

- background thread lifecycle;
- retry/backoff or stale-state policy;
- public resource endpoints.

## Expected files

- `include/ghinfo/model.hpp`
- `include/ghinfo/snapshot.hpp`
- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `include/ghinfo/snapshot_builder.hpp`
- `src/snapshot_builder.cpp`
- `tests/snapshot_builder_test.cpp`
- `CMakeLists.txt`
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
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
git diff --check
```
