# Active Plan

## Current milestone

**MVP-012 — Core read API**

Target commit:

```text
feat(api): expose normalized status resources
```

## Goal

Expose stable, consumer-agnostic JSON for the complete immutable snapshot,
with deterministic normalization, safe readiness behavior, and bounded
resource filters.

## Acceptance criteria

- `/v1/summary` reports deterministic repository, issue, pull request, and
  workflow counts.
- `/v1/repos` and `/v1/repos/{owner}/{repo}` expose normalized repository
  state and retained related resources.
- `/v1/issues`, `/v1/pulls`, `/v1/runs`, and `/v1/jobs` expose normalized arrays.
- `repo` filtering works for issue, pull, run, and job resources; run status
  and conclusion filters accept only documented enum strings.
- Data endpoints return `503 snapshot_unavailable` before the first complete
  poll and never call GitHub.
- Public enum values are explicit strings; nullable upstream values remain
  JSON `null`, not fabricated values.
- Golden and focused API tests cover schema, normalization, filtering, and
  unavailable state.

## Non-goals

- activity aggregation;
- pagination of the local API;
- authentication for consumers;
- presentation-specific priority or UI fields.

## Expected files

- `include/ghinfo/server.hpp`
- `src/server.cpp`
- `tests/api_test.cpp`
- `tests/golden/summary.json`
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
