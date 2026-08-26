# Active Plan

## Current milestone

**MVP-004 — Conditional request cache**

Target commit:

```text
feat(github): support conditional requests
```

## Goal

Reuse the last successful response for a request path when GitHub returns
`304 Not Modified`, while retaining fresh response headers such as rate-limit
metadata.

## Acceptance criteria

- A successful response with an ETag is cached per request path.
- Later requests send `If-None-Match` with the cached ETag.
- A `304` response returns the cached body and keeps the `304` status visible.
- Current response headers override cached headers; cached headers fill absent
  values needed by consumers.
- A `304` without a cached body is reported as an HTTP failure.
- Cache entries do not cross request paths.
- Tests cover the 200 -> 304 lifecycle and changing rate-limit headers.

## Non-goals

- pagination;
- issues/PRs/runs/jobs parsing;
- polling or retries/backoff;
- public data endpoints.

## Expected files

- `include/ghinfo/github_client.hpp`
- `src/github_client.cpp`
- `tests/github_client_test.cpp`
- `docs/ARCHITECTURE.md`
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
