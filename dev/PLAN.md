# Active Plan

## Current milestone

**MVP-007 — Workflow runs**

Target commit:

```text
feat(github): fetch workflow runs
```

## Goal

Fetch a bounded recent workflow-run history per repository and normalize
status, conclusion, branch, commit, event, timestamps, identifiers, and URL.

## Acceptance criteria

- Requests use the repository Actions workflow-runs endpoint with an explicit
  `per_page` equal to the configured history bound.
- No more than the requested history bound is returned or retained.
- Known statuses and conclusions serialize through explicit domain enums;
  upstream values outside the known set become `unknown`.
- Null conclusions remain absent rather than becoming a false success/failure.
- IDs are parsed as 64-bit values and timestamps/URLs are preserved.
- Invalid JSON and invalid payload shapes are explicit non-transport errors.
- Tests use the checked-in runs fixture and a local HTTP response.

## Non-goals

- workflow jobs;
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
- `.agents/skills/cpp-testing/SKILL.md`
- `.agents/skills/cpp-code-review/SKILL.md`

## Validation

```bash
./scripts/validate.sh
git diff --check
```
