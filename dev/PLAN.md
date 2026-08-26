# Active Plan

## Current milestone

**MVP-008 — Workflow jobs**

Target commit:

```text
feat(github): fetch relevant workflow jobs
```

## Goal

Fetch normalized job details only for retained workflow runs whose current
state is useful to inspect, following pagination without allowing historical
growth outside the selected runs.

## Acceptance criteria

- Queued/running runs and completed runs without a successful, skipped, or
  neutral conclusion are eligible for job detail.
- Completed successful, skipped, and neutral runs do not trigger a jobs request.
- Jobs are fetched from the run-specific Actions jobs endpoint with bounded
  pagination.
- Job IDs and run IDs are parsed as 64-bit values; status/conclusion enums and
  optional timestamps are normalized explicitly.
- Invalid JSON and invalid payload shapes are explicit non-transport errors.
- Tests use the checked-in jobs fixture, local paginated responses, and verify
  irrelevant runs make no request.

## Non-goals

- snapshot construction;
- polling, retries, and backoff;
- public data endpoints.

## Expected files

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
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
git diff --check
```
